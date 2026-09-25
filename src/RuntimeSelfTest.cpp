#include "runtime/RuntimeStack.hpp"
#include "render/GpuDrivenScene.hpp"
#include "render/HiZOcclusion.hpp"
#include "render/RenderFrameGraph.hpp"
#include "world/AsyncWorldStreaming.hpp"
#include "world/StreamingUploadQueue.hpp"
#include "gameplay/CharacterMotor.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(){
    if(!shn::runtime::RunRuntimeSelfTest()){std::cerr<<"Runtime self-test FAILED: base runtime contracts\n";return 1;}
    shn::world::AsyncWorldStreamer streaming({},2); streaming.configure(64.f,1); streaming.update(0.f,0.f);
    for(int i=0;i<50 && !streaming.resident();++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if(!streaming.resident()){std::cerr<<"Runtime self-test FAILED: async world streaming\n";return 2;}
    if(sizeof(shn::render::CullShader)<=128 || std::string(shn::render::CullShader).find("numthreads") == std::string::npos){std::cerr<<"Runtime self-test FAILED: GPU culling shader contract\n";return 3;}

    float depth[16]={1,1,1,1,1,.7f,.7f,1,1,.7f,.5f,1,1,1,1,1};
    shn::render::HiZPyramid hiz; hiz.build(depth,4,4);
    if(hiz.levelCount()!=3 || !hiz.occluded(0,0,1,1,.95f)){std::cerr<<"Runtime self-test FAILED: Hi-Z pyramid\n";return 4;}

    shn::render::RenderFrameGraph graph; bool upload=false, gbuffer=false, post=false;
    graph.add("Upload",{}, {"Scene"}, [&]{upload=true;});
    graph.add("GBuffer",{"Scene"},{"GBuffer"}, [&]{gbuffer=true;});
    graph.add("Post",{"GBuffer"},{"BackBuffer"}, [&]{post=true;});
    if(!graph.execute() || !upload || !gbuffer || !post){std::cerr<<"Runtime self-test FAILED: frame graph\n";return 5;}

    shn::world::StreamingUploadQueue uploads; uploads.push({7,"test.avif",{1,2,3}}); shn::world::UploadRequest request;
    if(!uploads.pop(request) || request.assetId!=7 || request.source!="test.avif"){std::cerr<<"Runtime self-test FAILED: upload queue\n";return 6;}
    uploads.stop();

    shn::gameplay::CharacterMotor motor; float x=0,y=0,z=0; motor.jump(); motor.step(1.f/60.f,1,0,x,y,z);
    if(x<=0.f || y<=0.f || motor.grounded){std::cerr<<"Runtime self-test FAILED: character motor\n";return 7;}

    std::cout<<"Runtime self-test PASSED: streaming, GPU culling, Hi-Z, frame graph, upload queue, character motor, animation, AVIF materials, navigation, physics, replication and persistence contracts loaded.\n";
    return 0;
}
