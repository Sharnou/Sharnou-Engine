#include "runtime/RuntimeStack.hpp"
#include "net/MMOShardRuntime.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(){shn::runtime::DedicatedServer s;s.start();shn::net::MMOShardRuntime shards(4);shards.connect(1,0,0,0,0,200);for(int i=0;i<120;++i){s.tick(1.f/60.f);shards.tick();if((i%10)==0){const std::uint32_t id=std::uint32_t(i/10+1);s.replication().upsert({id,{float(i),0,0},0});shards.upsert({id,float(i),0,0,0});}std::this_thread::sleep_for(std::chrono::milliseconds(1));}auto snap=s.replication().snapshot({0,0,0},1000);auto deltas=shards.shardFor(1).snapshot(1);s.stop();std::cout<<"Sharnou Dedicated Server stopped cleanly; entities="<<snap.entities.size()<<" physicsSteps="<<s.tooling().metrics().physicsSteps.load()<<" shards="<<shards.shardCount()<<" interestDeltas="<<deltas.size()<<"\n";return 0;}
