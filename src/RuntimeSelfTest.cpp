#include "runtime/RuntimeStack.hpp"
#include "asset/AssetFormatPolicy.hpp"
#include "asset/Ktx2Container.hpp"
#include "render/GpuDrivenScene.hpp"
#include "render/HiZOcclusion.hpp"
#include "render/RenderFrameGraph.hpp"
#include "render/FrustumLOD.hpp"
#include "render/MaterialBatcher.hpp"
#include "render/GpuSkinning.hpp"
#include "render/ShadowCulling.hpp"
#include "render/TransientGpuAllocator.hpp"
#include "render/GpuTimestampProfiler.hpp"
#include "render/ProductionGpuVisibility.hpp"
#include "world/AsyncWorldStreaming.hpp"
#include "world/StreamingUploadQueue.hpp"
#include "gameplay/CharacterMotor.hpp"
#include "net/MMOShardRuntime.hpp"
#include <array>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <string>
#include <type_traits>
#include <thread>

int main(){
    if(!shn::runtime::RunRuntimeSelfTest()){std::cerr<<"Runtime self-test FAILED: base runtime contracts\n";return 1;}
    shn::world::AsyncWorldStreamer streaming({},2); streaming.configure(64.f,1); streaming.update(0.f,0.f);
    for(int i=0;i<50 && !streaming.resident();++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if(!streaming.resident()){std::cerr<<"Runtime self-test FAILED: async world streaming\n";return 2;}
    if(sizeof(shn::render::CullShader)<=128 || std::string(shn::render::CullShader).find("numthreads") == std::string::npos){std::cerr<<"Runtime self-test FAILED: GPU culling shader contract\n";return 3;}
    float depth[16]={.5f,.5f,.5f,.5f,.5f,.7f,.7f,.5f,.5f,.7f,.5f,.5f,.5f,.5f,.5f,.5f};
    shn::render::HiZPyramid hiz; hiz.build(depth,4,4);
    if(hiz.levelCount()!=3 || !hiz.occluded(0,0,1,1,.95f)){std::cerr<<"Runtime self-test FAILED: Hi-Z pyramid\n";return 4;}
    shn::render::RenderFrameGraph graph; bool upload=false, gbuffer=false, post=false;
    graph.add("Upload",{}, {"Scene"}, [&]{upload=true;}); graph.add("GBuffer",{"Scene"},{"GBuffer"}, [&]{gbuffer=true;}); graph.add("Post",{"GBuffer"},{"BackBuffer"}, [&]{post=true;});
    if(!graph.execute() || !upload || !gbuffer || !post){std::cerr<<"Runtime self-test FAILED: frame graph\n";return 5;}
    shn::world::StreamingUploadQueue uploads; uploads.push({7,"test.avif",{1,2,3}}); shn::world::UploadRequest request;
    if(!uploads.pop(request) || request.assetId!=7 || request.source!="test.avif"){std::cerr<<"Runtime self-test FAILED: upload queue\n";return 6;} uploads.stop();
    shn::gameplay::CharacterMotor motor; float x=0,y=0,z=0; motor.jump(); motor.step(1.f/60.f,1,0,x,y,z); if(x<=0.f || y<=0.f || motor.grounded){std::cerr<<"Runtime self-test FAILED: character motor\n";return 7;}
    shn::render::Frustum fr{};
    fr.planes = {
        shn::render::Plane{1,0,0,1}, shn::render::Plane{-1,0,0,1},
        shn::render::Plane{0,1,0,1}, shn::render::Plane{0,-1,0,1},
        shn::render::Plane{0,0,1,1}, shn::render::Plane{0,0,-1,1}
    };
    if(!fr.sphereVisible(0,0,0,.5f) || fr.sphereVisible(-3,0,0,.5f)){std::cerr<<"Runtime self-test FAILED: frustum culling\n";return 8;}
    shn::render::LODSelector lod; if(lod.select(10)!=shn::render::LOD::Ultra || lod.select(500)!=shn::render::LOD::Culled){std::cerr<<"Runtime self-test FAILED: LOD selection\n";return 9;}
    shn::render::MaterialBatcher batcher; auto batches=batcher.build({{1,2,0,1},{1,2,1,2},{2,2,0,3}}); if(batches.size()!=2){std::cerr<<"Runtime self-test FAILED: material batching\n";return 10;}
    if(shn::render::SkinningShader.find("numthreads") == std::string_view::npos || shn::render::SkinningShader.find("Bones") == std::string_view::npos){std::cerr<<"Runtime self-test FAILED: GPU skinning shader\n";return 11;}
    if(std::string_view(shn::render::ProductionVisibilityShader).find("InterlockedAdd") == std::string_view::npos || std::string_view(shn::render::ProductionVisibilityShader).find("HiZ.SampleLevel") == std::string_view::npos){std::cerr<<"Runtime self-test FAILED: production GPU visibility shader\n";return 12;}
    if(sizeof(shn::render::VisibilityConstants)%16!=0){std::cerr<<"Runtime self-test FAILED: GPU visibility constant-buffer alignment\n";return 13;}
    shn::render::ShadowCuller shadow; auto shadowIds=shadow.visible(fr,{{4,0,0,0,1},{5,-4,0,0,1}}); if(shadowIds.size()!=1 || shadowIds[0]!=4){std::cerr<<"Runtime self-test FAILED: shadow culling\n";return 14;}
    shn::render::TransientGpuAllocator arena(4096); auto a=arena.allocate(256), b=arena.allocate(512); if(a.size!=256 || b.size!=512 || arena.used()==0){std::cerr<<"Runtime self-test FAILED: transient GPU allocator\n";return 15;}
    static_assert(std::is_same_v<decltype(std::declval<shn::render::GpuTimestampProfiler>().collect()),std::vector<shn::render::GpuTiming>>);
    shn::render::GpuTimestampProfiler profiler;
    if(shn::asset::classify("hero.gltf") != shn::asset::AssetRole::Scene3D ||
       shn::asset::classify("hero.glb") != shn::asset::AssetRole::Scene3D ||
       shn::asset::classify("hero_albedo.ktx2") != shn::asset::AssetRole::Texture3D ||
       shn::asset::classify("ui/login.avif") != shn::asset::AssetRole::Raster2D ||
       shn::asset::classify("source/hero.fbx") != shn::asset::AssetRole::SourceModel ||
       shn::asset::classify("legacy.dds") != shn::asset::AssetRole::Unknown){
        std::cerr<<"Runtime self-test FAILED: asset format policy\\n"; return 17;
    }
    std::array<std::uint8_t,80> ktx2{};
    for(std::size_t i=0;i<12;++i) ktx2[i]=shn::asset::Ktx2Identifier[i];
    ktx2[20]=1; ktx2[40]=1;
    ktx2[24]=1; ktx2[28]=1;
    if(!shn::asset::hasValidKtx2Header(ktx2)){
        std::cerr<<"Runtime self-test FAILED: KTX2 header validation\\n"; return 18;
    }
    shn::net::MMOShardRuntime shards(4); shards.connect(1,0,0,0,0,100); shards.upsert({1,10,0,0,1}); shards.upsert({2,500,0,0,1}); const auto deltas=shards.shardFor(1).snapshot(1); if(deltas.size()!=1 || deltas[0].id!=1){std::cerr<<"Runtime self-test FAILED: MMO shard interest\n";return 19;}
    std::cout<<"Runtime self-test PASSED: world streaming, GPU culling, Hi-Z, frame graph, frustum/LOD, batching, GPU skinning, shadows, transient GPU allocation, character motor, MMO shard interest, animation, AVIF 2D materials, KTX2 3D textures, glTF runtime containers, navigation, physics, replication and persistence contracts loaded.\n";
    return 0;
}
