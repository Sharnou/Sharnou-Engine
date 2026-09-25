#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>

namespace shn::render {
using Microsoft::WRL::ComPtr;

struct GpuInstance { float x{},y{},z{},radius{1.f}; std::uint32_t mesh{}; };
struct IndirectDraw { std::uint32_t indexCount{},instanceCount{},startIndex{},baseVertex{},startInstance{}; };

inline constexpr char CullShader[] = R"(
struct Instance { float4 sphere; uint mesh; };
StructuredBuffer<Instance> Input : register(t0);
RWStructuredBuffer<uint> Visible : register(u0);
RWByteAddressBuffer Args : register(u1);
cbuffer Frame : register(b0) { float4x4 ViewProj; uint Count; float MaxDistance; float2 Pad; float3 Camera; float Pad2; };
[numthreads(64,1,1)] void CS(uint3 id:SV_DispatchThreadID) {
  if (id.x >= Count) return;
  Instance a = Input[id.x];
  float3 d = a.sphere.xyz - Camera;
  bool visible = dot(d,d) <= (MaxDistance+a.sphere.w)*(MaxDistance+a.sphere.w);
  if (!visible) return;
  uint slot; Args.InterlockedAdd(4, 1, slot); Visible[slot] = id.x;
}
)";

class GpuDrivenScene {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11ComputeShader> cullShader_;
    ComPtr<ID3D11Buffer> instances_;
    ComPtr<ID3D11Buffer> visible_;
    ComPtr<ID3D11Buffer> args_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11ShaderResourceView> instanceSrv_;
    ComPtr<ID3D11UnorderedAccessView> visibleUav_;
    ComPtr<ID3D11UnorderedAccessView> argsUav_;
    std::uint32_t capacity_{};
public:
    bool initialize(ID3D11Device* device, ID3D11DeviceContext* context, std::uint32_t capacity, std::string& error) {
        if (!device || !context || !capacity) { error="Invalid GPU scene initialization parameters."; return false; }
        device_ = device; context_ = context; capacity_ = capacity;
        ComPtr<ID3DBlob> bytecode, diagnostics;
        if (FAILED(D3DCompile(CullShader, sizeof(CullShader)-1, "SharnouGpuCull", nullptr, nullptr, "CS", "cs_5_0", 0, 0, &bytecode, &diagnostics))) {
            error = diagnostics ? std::string(static_cast<const char*>(diagnostics->GetBufferPointer()), diagnostics->GetBufferSize()) : "GPU culling shader compilation failed.";
            return false;
        }
        if (FAILED(device_->CreateComputeShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &cullShader_))) { error="Compute shader creation failed."; return false; }
        D3D11_BUFFER_DESC b{}; b.Usage=D3D11_USAGE_DEFAULT; b.ByteWidth=capacity_*20; b.BindFlags=D3D11_BIND_SHADER_RESOURCE; b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; b.StructureByteStride=20;
        if (FAILED(device_->CreateBuffer(&b,nullptr,&instances_))) { error="Instance buffer creation failed."; return false; }
        D3D11_SHADER_RESOURCE_VIEW_DESC srv{}; srv.ViewDimension=D3D11_SRV_DIMENSION_BUFFER; srv.Format=DXGI_FORMAT_UNKNOWN; srv.Buffer.FirstElement=0; srv.Buffer.NumElements=capacity_;
        if (FAILED(device_->CreateShaderResourceView(instances_.Get(),&srv,&instanceSrv_))) { error="Instance SRV creation failed."; return false; }
        b.ByteWidth=capacity_*4; b.BindFlags=D3D11_BIND_UNORDERED_ACCESS; b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; b.StructureByteStride=4;
        if (FAILED(device_->CreateBuffer(&b,nullptr,&visible_))) { error="Visible buffer creation failed."; return false; }
        D3D11_UNORDERED_ACCESS_VIEW_DESC u{}; u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; u.Format=DXGI_FORMAT_UNKNOWN; u.Buffer.FirstElement=0; u.Buffer.NumElements=capacity_;
        if (FAILED(device_->CreateUnorderedAccessView(visible_.Get(),&u,&visibleUav_))) { error="Visible UAV creation failed."; return false; }
        b.ByteWidth=20; b.MiscFlags=0; b.StructureByteStride=0;
        if (FAILED(device_->CreateBuffer(&b,nullptr,&args_))) { error="Indirect argument buffer creation failed."; return false; }
        D3D11_UNORDERED_ACCESS_VIEW_DESC au{}; au.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; au.Format=DXGI_FORMAT_R32_TYPELESS; au.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_RAW; au.Buffer.FirstElement=0; au.Buffer.NumElements=5;
        b.BindFlags=D3D11_BIND_UNORDERED_ACCESS; b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS; b.StructureByteStride=0;
        if (FAILED(device_->CreateBuffer(&b,nullptr,&args_))) { error="Indirect raw argument buffer creation failed."; return false; }
        if (FAILED(device_->CreateUnorderedAccessView(args_.Get(),&au,&argsUav_))) { error="Indirect argument UAV creation failed."; return false; }
        b.ByteWidth=96; b.Usage=D3D11_USAGE_DEFAULT; b.BindFlags=D3D11_BIND_CONSTANT_BUFFER; b.MiscFlags=0;
        if (FAILED(device_->CreateBuffer(&b,nullptr,&constants_))) { error="Culling constant buffer creation failed."; return false; }
        return true;
    }
    bool ready() const noexcept { return cullShader_ && instances_ && visible_ && args_ && constants_; }
    std::uint32_t capacity() const noexcept { return capacity_; }
};
}
