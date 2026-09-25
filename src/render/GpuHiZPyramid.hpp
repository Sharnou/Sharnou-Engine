#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace shn::render {
using Microsoft::WRL::ComPtr;

inline constexpr char HiZShader[] = R"(
Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> Destination : register(u0);
cbuffer HiZFrame : register(b0) { uint2 SourceSize; uint2 DestinationSize; };
[numthreads(8,8,1)] void CopyDepth(uint3 id:SV_DispatchThreadID) {
    if(id.x>=DestinationSize.x||id.y>=DestinationSize.y)return;
    uint2 p=min(id.xy,SourceSize-1);
    Destination[id.xy]=SourceDepth.Load(int3(p,0));
}
[numthreads(8,8,1)] void ReduceDepth(uint3 id:SV_DispatchThreadID) {
    if(id.x>=DestinationSize.x||id.y>=DestinationSize.y)return;
    uint2 p=id.xy*2;
    float z=0;
    for(uint oy=0;oy<2;++oy)for(uint ox=0;ox<2;++ox)z=max(z,SourceDepth.Load(int3(min(p+uint2(ox,oy),SourceSize-1),0)));
    Destination[id.xy]=z;
}
)";

struct HiZConstants { std::uint32_t sourceWidth{},sourceHeight{},destinationWidth{},destinationHeight{}; };

class GpuHiZPyramid {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    std::vector<ComPtr<ID3D11ShaderResourceView>> mipSrv_;
    std::vector<ComPtr<ID3D11UnorderedAccessView>> mipUav_;
    ComPtr<ID3D11ComputeShader> copyShader_,reduceShader_;
    ComPtr<ID3D11Buffer> constants_;
    std::uint32_t width_{},height_{},levels_{};

    bool shader(ID3D11ComputeShader** out,const char* entry,std::string& error){
        ComPtr<ID3DBlob> code,diag;
        if(FAILED(D3DCompile(HiZShader,sizeof(HiZShader)-1,"SharnouHiZ",nullptr,nullptr,entry,"cs_5_0",0,0,&code,&diag))){
            error=diag?std::string(static_cast<const char*>(diag->GetBufferPointer()),diag->GetBufferSize()):"Hi-Z shader compilation failed."; return false;
        }
        return SUCCEEDED(device_->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,out));
    }
public:
    bool initialize(ID3D11Device* device,ID3D11DeviceContext* context,std::uint32_t width,std::uint32_t height,std::string& error){
        if(!device||!context||!width||!height){error="Invalid Hi-Z dimensions or D3D11 device.";return false;}
        device_=device;context_=context;width_=width;height_=height;levels_=1+static_cast<std::uint32_t>(std::floor(std::log2(static_cast<double>(std::max(width,height)))));
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=levels_;d.ArraySize=1;d.Format=DXGI_FORMAT_R32_FLOAT;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&texture_))){error="Hi-Z texture creation failed.";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Format=DXGI_FORMAT_R32_FLOAT;sv.Texture2D.MostDetailedMip=0;sv.Texture2D.MipLevels=levels_;if(FAILED(device_->CreateShaderResourceView(texture_.Get(),&sv,&srv_))){error="Hi-Z SRV creation failed.";return false;}
        mipSrv_.resize(levels_);mipUav_.resize(levels_);
        for(std::uint32_t mip=0;mip<levels_;++mip){
            D3D11_SHADER_RESOURCE_VIEW_DESC ms{};ms.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;ms.Format=DXGI_FORMAT_R32_FLOAT;ms.Texture2D.MostDetailedMip=mip;ms.Texture2D.MipLevels=1;if(FAILED(device_->CreateShaderResourceView(texture_.Get(),&ms,&mipSrv_[mip]))){error="Hi-Z mip SRV creation failed.";return false;}
            D3D11_UNORDERED_ACCESS_VIEW_DESC u{};u.ViewDimension=D3D11_UAV_DIMENSION_TEXTURE2D;u.Format=DXGI_FORMAT_R32_FLOAT;u.Texture2D.MipSlice=mip;if(FAILED(device_->CreateUnorderedAccessView(texture_.Get(),&u,&mipUav_[mip]))){error="Hi-Z mip UAV creation failed.";return false;}
        }
        if(!shader(&copyShader_,"CopyDepth",error)||!shader(&reduceShader_,"ReduceDepth",error))return false;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=16;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(device_->CreateBuffer(&cb,nullptr,&constants_))){error="Hi-Z constant buffer creation failed.";return false;}
        return true;
    }
    bool generate(ID3D11ShaderResourceView* sourceDepth){
        if(!context_||!sourceDepth||levels_<1)return false;
        HiZConstants c{width_,height_,width_,height_};context_->UpdateSubresource(constants_.Get(),0,nullptr,&c,0,0);
        ID3D11ShaderResourceView* s[]={sourceDepth};ID3D11UnorderedAccessView* u[]={mipUav_[0].Get()};context_->CSSetShader(copyShader_.Get(),nullptr,0);context_->CSSetShaderResources(0,1,s);context_->CSSetUnorderedAccessViews(0,1,u,nullptr);context_->CSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->Dispatch((width_+7)/8,(height_+7)/8,1);
        for(std::uint32_t mip=1;mip<levels_;++mip){const std::uint32_t sw=std::max(1u,width_>>(mip-1)),sh=std::max(1u,height_>>(mip-1)),dw=std::max(1u,width_>>mip),dh=std::max(1u,height_>>mip);c={sw,sh,dw,dh};context_->UpdateSubresource(constants_.Get(),0,nullptr,&c,0,0);ID3D11ShaderResourceView* src[]={mipSrv_[mip-1].Get()};ID3D11UnorderedAccessView* dst[]={mipUav_[mip].Get()};context_->CSSetShader(reduceShader_.Get(),nullptr,0);context_->CSSetShaderResources(0,1,src);context_->CSSetUnorderedAccessViews(0,1,dst,nullptr);context_->CSSetConstantBuffers(0,1,constants_.GetAddressOf());context_->Dispatch((dw+7)/8,(dh+7)/8,1);}
        ID3D11ShaderResourceView* ns[]={nullptr};ID3D11UnorderedAccessView* nu[]={nullptr};context_->CSSetShaderResources(0,1,ns);context_->CSSetUnorderedAccessViews(0,1,nu,nullptr);context_->CSSetShader(nullptr,nullptr,0);return true;
    }
    ID3D11ShaderResourceView* shaderResource() const noexcept{return srv_.Get();}
    std::uint32_t levels() const noexcept{return levels_;}
};
}
