#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>
namespace shn::render {
using Microsoft::WRL::ComPtr;
struct GpuTiming { std::string name; double milliseconds{}; bool ready{}; };
class GpuTimestampProfiler {
    struct Slot { ComPtr<ID3D11Query> start,end; std::string name; };
    ComPtr<ID3D11Device> device_; ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Query> disjoint_;
    std::vector<Slot> slots_;
    bool open_{};
public:
    bool initialize(ID3D11Device* d,ID3D11DeviceContext* c,std::uint32_t maxScopes=64) {
        if(!d||!c||!maxScopes) return false; device_=d; context_=c; slots_.resize(maxScopes);
        D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
        return SUCCEEDED(device_->CreateQuery(&q,&disjoint_));
    }
    void beginFrame(){ if(context_&&disjoint_){ context_->Begin(disjoint_.Get()); open_=true; } slots_.clear(); }
    bool begin(const char* name){
        if(!open_||!device_||!context_) return false;
        Slot s{}; D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP,0};
        if(FAILED(device_->CreateQuery(&q,&s.start))) return false;
        if(FAILED(device_->CreateQuery(&q,&s.end))) return false;
        s.name=name?name:"scope"; context_->End(s.start.Get()); slots_.push_back(std::move(s)); return true;
    }
    void end(){ if(open_&&!slots_.empty()) context_->End(slots_.back().end.Get()); }
    void endFrame(){ if(open_&&context_&&disjoint_){ context_->End(disjoint_.Get()); open_=false; } }
    std::vector<GpuTiming> collect(){
        std::vector<GpuTiming> out; if(!context_||!disjoint_||open_) return out;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};
        if(context_->GetData(disjoint_.Get(),&d,sizeof(d),0)!=S_OK||d.Disjoint||!d.Frequency) return out;
        for(auto& s:slots_){ UINT64 a{},b{}; if(context_->GetData(s.start.Get(),&a,sizeof(a),0)!=S_OK||context_->GetData(s.end.Get(),&b,sizeof(b),0)!=S_OK){out.push_back({s.name,0,false});continue;} out.push_back({s.name,double(b-a)*1000.0/d.Frequency,true}); }
        return out;
    }
};
}