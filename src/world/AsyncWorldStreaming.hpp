#pragma once
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace shn::world {
struct CellId { std::int32_t x{},z{}; bool operator==(const CellId& o) const noexcept{return x==o.x&&z==o.z;} };
struct CellHash { std::size_t operator()(CellId c) const noexcept{return (std::uint64_t(std::uint32_t(c.x))<<32)^std::uint32_t(c.z);} };
struct CellManifest { CellId id{}; std::string asset; std::uint32_t generation{}; };
enum class State : std::uint8_t { Unloaded, Queued, Loading, Resident, Evicting };
struct Record { CellManifest manifest; State state=State::Unloaded; };

class AsyncWorldStreamer {
    using Job=std::function<void()>;
    std::unordered_map<CellId,Record,CellHash> records_;
    std::queue<Job> jobs_;
    std::vector<std::jthread> workers_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::atomic<bool> stopping_{false};
    std::filesystem::path root_;
    float cellSize_=128.f;
    std::int32_t radius_=2;
    CellId cell(float x,float z) const noexcept { return {static_cast<std::int32_t>(std::floor(x/cellSize_)),static_cast<std::int32_t>(std::floor(z/cellSize_))}; }
    void enqueueLocked(CellId id) {
        auto it=records_.find(id); if(it!=records_.end() && it->second.state!=State::Unloaded) return;
        auto& r=records_[id]; r.manifest.id=id; r.manifest.asset=(root_/((std::to_string(id.x)+"_"+std::to_string(id.z))+".cell")).string(); r.state=State::Queued;
        jobs_.push([this,id]{
            std::string path;
            { std::scoped_lock lock(mutex_); auto it=records_.find(id); if(it==records_.end())return; it->second.state=State::Loading; path=it->second.manifest.asset; }
            std::error_code ec; (void)std::filesystem::exists(path,ec);
            std::scoped_lock lock(mutex_); auto it=records_.find(id); if(it==records_.end())return; it->second.state=State::Resident; ++it->second.manifest.generation;
        });
    }
public:
    explicit AsyncWorldStreamer(std::filesystem::path root={},std::uint32_t workers=2):root_(std::move(root)){
        workers=workers?workers:1; workers_.reserve(workers);
        for(std::uint32_t i=0;i<workers;++i) workers_.emplace_back([this](std::stop_token st){Job job;while(!st.stop_requested()&&!stopping_.load()){ {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stopping_.load()||!jobs_.empty()||st.stop_requested();});if(stopping_.load()||st.stop_requested())return;job=std::move(jobs_.front());jobs_.pop();}if(job)job();}});
    }
    ~AsyncWorldStreamer(){stopping_=true;wake_.notify_all();for(auto&w:workers_)w.request_stop();}
    void configure(float cellSize,std::int32_t radius){cellSize_=cellSize>0?cellSize:128.f;radius_=radius<0?0:radius;}
    void update(float x,float z){const CellId c=cell(x,z);{std::scoped_lock lock(mutex_);for(std::int32_t dz=-radius_;dz<=radius_;++dz)for(std::int32_t dx=-radius_;dx<=radius_;++dx)enqueueLocked({c.x+dx,c.z+dz});for(auto&[id,r]:records_)if(std::abs(id.x-c.x)>radius_+1||std::abs(id.z-c.z)>radius_+1)r.state=State::Unloaded;}wake_.notify_all();}
    std::size_t resident() const {std::scoped_lock lock(mutex_);std::size_t n=0;for(const auto&[id,r]:records_)n+=r.state==State::Resident;return n;}
    std::size_t tracked() const {std::scoped_lock lock(mutex_);return records_.size();}
};
}
