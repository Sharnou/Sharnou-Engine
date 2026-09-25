#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace shn::render {
struct Plane { float x{},y{},z{},d{}; float distance(float px,float py,float pz) const noexcept{return x*px+y*py+z*pz+d;} };
struct Frustum { std::array<Plane,6> planes{};
    bool sphereVisible(float x,float y,float z,float radius) const noexcept { for(const auto&p:planes) if(p.distance(x,y,z)<-radius) return false; return true; }
};
enum class LOD : std::uint8_t { Ultra, High, Medium, Low, Impostor, Culled };
class LODSelector {
    std::array<float,5> thresholds_{};
public:
    LODSelector() noexcept { thresholds_ = {24.f,48.f,96.f,180.f,360.f}; }
    void setThresholds(float a,float b,float c,float d,float e){thresholds_={a,b,c,d,e};}
    LOD select(float distance,float radius=1.f) const noexcept { const float d=std::max(0.f,distance-radius); if(d<thresholds_[0])return LOD::Ultra; if(d<thresholds_[1])return LOD::High; if(d<thresholds_[2])return LOD::Medium; if(d<thresholds_[3])return LOD::Low; if(d<thresholds_[4])return LOD::Impostor; return LOD::Culled; }
};
}
