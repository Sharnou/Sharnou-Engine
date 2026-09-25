#pragma once
#include "FrustumLOD.hpp"
#include <cstdint>
#include <vector>
namespace shn::render {
struct ShadowCaster { std::uint32_t instance{}; float x{},y{},z{},radius{1}; };
class ShadowCuller {
public:
    std::vector<std::uint32_t> visible(const Frustum& lightFrustum,const std::vector<ShadowCaster>& casters) const {
        std::vector<std::uint32_t> out; out.reserve(casters.size());
        for(const auto&c:casters) if(lightFrustum.sphereVisible(c.x,c.y,c.z,c.radius)) out.push_back(c.instance);
        return out;
    }
};
}
