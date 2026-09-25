#include "runtime/RuntimeStack.hpp"
#include "render/GpuDrivenScene.hpp"
#include "world/AsyncWorldStreaming.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main(){
    if(!shn::runtime::RunRuntimeSelfTest()){std::cerr<<"Runtime self-test FAILED: base runtime contracts\n";return 1;}
    shn::world::AsyncWorldStreamer streaming({},2); streaming.configure(64.f,1); streaming.update(0.f,0.f);
    for(int i=0;i<50 && !streaming.resident();++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if(!streaming.resident()){std::cerr<<"Runtime self-test FAILED: async world streaming\n";return 2;}
    if(sizeof(shn::render::CullShader)<=128 || std::string(shn::render::CullShader).find("numthreads") == std::string::npos){std::cerr<<"Runtime self-test FAILED: GPU culling shader contract\n";return 3;}
    std::cout<<"Runtime self-test PASSED: streaming, GPU-culling contract, animation, AVIF materials, navigation, physics, replication and persistence contracts loaded.\n";
    return 0;
}
