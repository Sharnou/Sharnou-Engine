#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace shn::render {

struct HiZLevel {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<float> depth;
};

class HiZPyramid {
    std::vector<HiZLevel> levels_;
public:
    void build(const float* depth, std::uint32_t width, std::uint32_t height) {
        levels_.clear();
        if (!depth || width == 0 || height == 0) return;
        HiZLevel base{width, height, std::vector<float>(depth, depth + static_cast<std::size_t>(width) * height)};
        levels_.push_back(std::move(base));
        while (levels_.back().width > 1 || levels_.back().height > 1) {
            const auto& src = levels_.back();
            const std::uint32_t w = std::max(1u, (src.width + 1) / 2);
            const std::uint32_t h = std::max(1u, (src.height + 1) / 2);
            HiZLevel dst{w, h, std::vector<float>(static_cast<std::size_t>(w) * h, 1.0f)};
            for (std::uint32_t y=0; y<h; ++y) for (std::uint32_t x=0; x<w; ++x) {
                float z = 0.0f;
                for (std::uint32_t oy=0; oy<2; ++oy) for (std::uint32_t ox=0; ox<2; ++ox) {
                    const auto sx=std::min(src.width-1, x*2+ox), sy=std::min(src.height-1, y*2+oy);
                    z=std::max(z, src.depth[static_cast<std::size_t>(sy)*src.width+sx]);
                }
                dst.depth[static_cast<std::size_t>(y)*w+x]=z;
            }
            levels_.push_back(std::move(dst));
        }
    }
    bool occluded(std::uint32_t minX, std::uint32_t minY, std::uint32_t maxX, std::uint32_t maxY, float nearestDepth) const {
        if (levels_.empty()) return false;
        const auto& l=levels_.back();
        const float sampled=l.depth[0];
        (void)minX; (void)minY; (void)maxX; (void)maxY;
        return nearestDepth > sampled;
    }
    std::size_t levelCount() const noexcept { return levels_.size(); }
};

}
