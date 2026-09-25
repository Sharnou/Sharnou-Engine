#pragma once
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace shn::net {
struct NetEntity { std::uint32_t id{}; float x{},y{},z{}; std::uint32_t revision{}; };
struct ClientView { std::uint64_t clientId{}; float x{},y{},z{}; float radius=128.f; std::uint32_t lastRevision{}; };
struct Delta { std::uint32_t id{}; std::uint32_t revision{}; float x{},y{},z{}; std::uint8_t op{}; };

class Shard {
    std::uint32_t id_{};
    std::uint32_t tick_{};
    std::unordered_map<std::uint32_t,NetEntity> entities_;
    std::unordered_map<std::uint64_t,ClientView> clients_;
public:
    explicit Shard(std::uint32_t id=0):id_(id){}
    void tick(){++tick_;}
    void upsert(NetEntity e){e.revision=++tick_;entities_[e.id]=e;}
    void connect(ClientView c){clients_[c.clientId]=c;}
    void disconnect(std::uint64_t id){clients_.erase(id);}
    std::vector<Delta> snapshot(std::uint64_t clientId) const {
        std::vector<Delta> out; auto c=clients_.find(clientId); if(c==clients_.end()) return out;
        const float r2=c->second.radius*c->second.radius;
        for(const auto& [id,e]:entities_){
            const float dx=e.x-c->second.x,dy=e.y-c->second.y,dz=e.z-c->second.z;
            if(dx*dx+dy*dy+dz*dz<=r2 && e.revision>c->second.lastRevision) out.push_back({e.id,e.revision,e.x,e.y,e.z,0});
        }
        return out;
    }
    std::uint32_t id() const noexcept{return id_;}
    std::size_t entityCount() const noexcept{return entities_.size();}
};

class MMOShardRuntime {
    std::vector<Shard> shards_;
public:
    explicit MMOShardRuntime(std::uint32_t count=4){count=std::max(1u,count);shards_.reserve(count);for(std::uint32_t i=0;i<count;++i)shards_.emplace_back(i);}
    Shard& shardFor(std::uint32_t entityId){return shards_[entityId%shards_.size()];}
    const Shard& shardFor(std::uint32_t entityId) const{return shards_[entityId%shards_.size()];}
    void upsert(NetEntity e){shardFor(e.id).upsert(e);}
    void connect(std::uint64_t clientId,std::uint32_t preferredShard,float x,float y,float z,float radius){shards_[preferredShard%shards_.size()].connect({clientId,x,y,z,radius,0});}
    std::size_t shardCount() const noexcept{return shards_.size();}
    void tick(){for(auto& s:shards_)s.tick();}
};
}
