#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace shn::render {
using Microsoft::WRL::ComPtr;
struct VisibilityInstance { float x{},y{},z{},radius{1.f}; std::uint32_t mesh{}; };
struct LODDrawArgs { std::uint32_t indexCount{},instanceCount{},startIndex{},baseVertex{},startInstance{}; };
inline constexpr char ProductionVisibilityShader[] =
"struct Instance { float4 sphere; uint mesh; };\n"
"StructuredBuffer<Instance> Input : register(t0);\n"
"RWStructuredBuffer<uint> Visible : register(u0);\n"
"RWByteAddressBuffer Args : register(u1);\n"
"Texture2D<float> HiZ : register(t1);\n"
"SamplerState PointClamp : register(s0);\n"
"cbuffer Frame : register(b0) { float4 Planes[6]; float4x4 ViewProj; float3 Camera; float MaxDistance; float4 LodNearFar0; float4 LodNearFar1; uint Count; uint Capacity; uint HiZWidth; uint HiZHeight; float HiZBias; float UseHiZ; float2 Pad; };\n"
"float PlaneDist(float4 p,float3 q){return dot(p.xyz,q)+p.w;}\n"
"uint PickLOD(float d){if(d<LodNearFar0.x)return 0;if(d<LodNearFar0.y)return 1;if(d<LodNearFar0.z)return 2;if(d<LodNearFar0.w)return 3;if(d<LodNearFar1.x)return 4;return 5;}\n"
"[numthreads(64,1,1)] void CS(uint3 id:SV_DispatchThreadID){ if(id.x>=Count)return; Instance a=Input[id.x]; float3 p=a.sphere.xyz; for(uint i=0;i<6;++i) if(PlaneDist(Planes[i],p)<-a.sphere.w)return; float3 d=p-Camera; float dist=length(d); if(dist-a.sphere.w>MaxDistance)return; uint lod=PickLOD(dist); if(lod>=5)return; if(UseHiZ>.5){float4 clip=mul(float4(p,1),ViewProj); if(clip.w<=0)return; float2 uv=clip.xy/clip.w*.5+.5; if(any(uv<float2(0,0))||any(uv>float2(1,1)))return; float scene=HiZ.SampleLevel(PointClamp,uv,0); float depth=clip.z/clip.w; if(depth>scene+HiZBias)return;} uint slot; Args.InterlockedAdd(lod*20+4,1,slot); Visible[lod*Capacity+slot]=id.x; }\n";
struct VisibilityConstants {
    float planes[24]{};
    float viewProj[16]{};
    float camera[3]{};
    float maxDistance{};
    float lodNearFar0[4]{};
    float lodNearFar1[4]{};
    std::uint32_t count{},capacity{},hiZWidth{},hiZHeight{};
    float hizBias{.001f},useHiZ{};
    float pad[2]{};
};
class ProductionGpuVisibility {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11ComputeShader> shader_;
    ComPtr<ID3D11Buffer> instances_,visible_,args_,constants_;
    ComPtr<ID3D11ShaderResourceView> instanceSrv_;
    ComPtr<ID3D11UnorderedAccessView> visibleUav_,argsUav_;
    ComPtr<ID3D11SamplerState> sampler_;
    std::uint32_t capacity_{};
public:
    bool initialize(ID3D11Device* d,ID3D11DeviceContext* c,std::uint32_t capacity,std::string& error){
        if(!d||!c||!capacity){error="Invalid GPU visibility parameters.";return false;}
        device_=d; context_=c; capacity_=capacity;
        ComPtr<ID3DBlob> code,diag;
        if(FAILED(D3DCompile(ProductionVisibilityShader,sizeof(ProductionVisibilityShader)-1,"SharnouVisibility",nullptr,nullptr,"CS","cs_5_0",0,0,&code,&diag))){error=diag?std::string(static_cast<const char*>(diag->GetBufferPointer()),diag->GetBufferSize()):"GPU visibility shader compilation failed.";return false;}
        if(FAILED(device_->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader_))){error="GPU visibility shader creation failed.";return false;}
        D3D11_BUFFER_DESC b{}; b.Usage=D3D11_USAGE_DEFAULT; b.ByteWidth=capacity_*20; b.BindFlags=D3D11_BIND_SHADER_RESOURCE; b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; b.StructureByteStride=20;
        if(FAILED(device_->CreateBuffer(&b,nullptr,&instances_))){error="GPU visibility instance buffer failed.";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{}; sv.ViewDimension=D3D11_SRV_DIMENSION_BUFFER; sv.Format=DXGI_FORMAT_UNKNOWN; sv.Buffer.NumElements=capacity_;
        if(FAILED(device_->CreateShaderResourceView(instances_.Get(),&sv,&instanceSrv_))){error="GPU visibility instance SRV failed.";return false;}
        b.ByteWidth=capacity_*5*4; b.BindFlags=D3D11_BIND_UNORDERED_ACCESS; b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; b.StructureByteStride=4;
        if(FAILED(device_->CreateBuffer(&b,nullptr,&visible_))){error="GPU visibility visible buffer failed.";return false;}
        D3D11_UNORDERED_ACCESS_VIEW_DESC uv{}; uv.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; uv.Format=DXGI_FORMAT_UNKNOWN; uv.Buffer.NumElements=capacity_*5;
        if(FAILED(device_->CreateUnorderedAccessView(visible_.Get(),&uv,&visibleUav_))){error="GPU visibility visible UAV failed.";return false;}
        b.ByteWidth=100; b.BindFlags=D3D11_BIND_UNORDERED_ACCESS; b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS|D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS; b.StructureByteStride=0;
        if(FAILED(device_->CreateBuffer(&b,nullptr,&args_))){error="GPU visibility argument buffer failed.";return false;}
        uv={}; uv.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; uv.Format=DXGI_FORMAT_R32_TYPELESS; uv.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_RAW; uv.Buffer.NumElements=25;
        if(FAILED(device_->CreateUnorderedAccessView(args_.Get(),&uv,&argsUav_))){error="GPU visibility argument UAV failed.";return false;}
        b.ByteWidth=sizeof(VisibilityConstants); b.BindFlags=D3D11_BIND_CONSTANT_BUFFER; b.MiscFlags=0; b.StructureByteStride=0;
        if(FAILED(device_->CreateBuffer(&b,nullptr,&constants_))){error="GPU visibility constants failed.";return false;}
        D3D11_SAMPLER_DESC sd{}; sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT; sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        if(FAILED(device_->CreateSamplerState(&sd,&sampler_))){error="GPU visibility sampler failed.";return false;}
        return true;
    }
    bool uploadInstances(const std::vector<VisibilityInstance>& v){ if(!context_||!instances_||v.empty()||v.size()>capacity_)return false; context_->UpdateSubresource(instances_.Get(),0,nullptr,v.data(),0,0); return true; }
    void dispatch(const VisibilityConstants& c,const std::array<LODDrawArgs,5>& initialArgs,ID3D11ShaderResourceView* hiz=nullptr){
        if(!shader_||c.count>capacity_)return;
        context_->UpdateSubresource(constants_.Get(),0,nullptr,&c,0,0); context_->UpdateSubresource(args_.Get(),0,nullptr,initialArgs.data(),0,0);
        ID3D11ShaderResourceView* srvs[2]={instanceSrv_.Get(),hiz}; ID3D11UnorderedAccessView* uavs[2]={visibleUav_.Get(),argsUav_.Get()};
        context_->CSSetShader(shader_.Get(),nullptr,0); context_->CSSetShaderResources(0,2,srvs); context_->CSSetUnorderedAccessViews(0,2,uavs,nullptr); context_->CSSetConstantBuffers(0,1,constants_.GetAddressOf()); context_->CSSetSamplers(0,1,sampler_.GetAddressOf());
        context_->Dispatch((c.count+63)/64,1,1);
        ID3D11ShaderResourceView* ns[2]={nullptr,nullptr}; ID3D11UnorderedAccessView* nu[2]={nullptr,nullptr}; context_->CSSetShaderResources(0,2,ns); context_->CSSetUnorderedAccessViews(0,2,nu,nullptr); context_->CSSetShader(nullptr,nullptr,0);
    }
    void drawLod(ID3D11DeviceContext* c,std::uint32_t lod){if(c&&args_&&lod<5)c->DrawIndexedInstancedIndirect(args_.Get(),lod*20);}
};
}