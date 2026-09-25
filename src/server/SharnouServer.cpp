#include "runtime/RuntimeStack.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(){shn::runtime::DedicatedServer s;s.start();for(int i=0;i<120;++i){s.tick(1.f/60.f);if((i%20)==0)s.replication().upsert({std::uint32_t(i/20+1),{float(i),0,0},0});std::this_thread::sleep_for(std::chrono::milliseconds(1));}auto snap=s.replication().snapshot({0,0,0},1000);s.stop();std::cout<<"Sharnou Dedicated Server stopped cleanly; entities="<<snap.entities.size()<<" physicsSteps="<<s.tooling().metrics().physicsSteps.load()<<"\n";return 0;}
