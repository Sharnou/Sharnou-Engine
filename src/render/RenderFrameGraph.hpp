#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shn::render {

enum class PassState : std::uint8_t { Pending, Running, Complete };
struct RenderPass { std::string name; std::vector<std::string> reads; std::vector<std::string> writes; PassState state{PassState::Pending}; std::function<void()> execute; };

class RenderFrameGraph {
    std::vector<RenderPass> passes_;
    std::unordered_map<std::string,std::size_t> producer_;
public:
    std::size_t add(std::string name, std::vector<std::string> reads, std::vector<std::string> writes, std::function<void()> execute) {
        const auto id=passes_.size();
        for (const auto& w:writes) producer_[w]=id;
        passes_.push_back({std::move(name),std::move(reads),std::move(writes),PassState::Pending,std::move(execute)});
        return id;
    }
    bool compile(std::vector<std::size_t>& order) const {
        order.clear(); order.reserve(passes_.size()); std::vector<bool> done(passes_.size());
        while (order.size()<passes_.size()) {
            bool progress=false;
            for (std::size_t i=0;i<passes_.size();++i) if (!done[i]) {
                bool ready=true;
                for (const auto& r:passes_[i].reads) { auto it=producer_.find(r); if (it!=producer_.end() && !done[it->second]) { ready=false; break; } }
                if (ready) { done[i]=true; order.push_back(i); progress=true; }
            }
            if (!progress) return false;
        }
        return true;
    }
    bool execute() {
        std::vector<std::size_t> order; if (!compile(order)) return false;
        for (auto i:order) { passes_[i].state=PassState::Running; if (passes_[i].execute) passes_[i].execute(); passes_[i].state=PassState::Complete; }
        return true;
    }
    std::size_t size() const noexcept { return passes_.size(); }
};

}
