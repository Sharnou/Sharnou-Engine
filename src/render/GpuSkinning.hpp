#pragma once
#include <cstdint>
#include <string_view>
namespace shn::render {
inline constexpr std::string_view SkinningShader = R"HLSL(
struct Vertex { float3 position; float3 normal; float4 weights; uint4 bones; };
StructuredBuffer<float4x4> Bones : register(t0);
StructuredBuffer<Vertex> Input : register(t1);
RWStructuredBuffer<Vertex> Output : register(u0);
[numthreads(64,1,1)]
void CS(uint3 id:SV_DispatchThreadID){
    uint i=id.x; Vertex v=Input[i];
    float4 p=float4(0,0,0,1); float3 n=0;
    [unroll] for(uint k=0;k<4;k++){ float w=v.weights[k]; p += mul(float4(v.position,1),Bones[v.bones[k]])*w; n += mul(v.normal,(float3x3)Bones[v.bones[k]])*w; }
    v.position=p.xyz; v.normal=normalize(n); Output[i]=v;
}
)HLSL";
}
