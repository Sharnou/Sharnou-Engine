#pragma once
#include <cstdint>
#include <map>
#include <vector>

namespace shn::render {
struct DrawItem { std::uint32_t mesh{}, material{}, lod{}, instance{}; };
class MaterialBatcher {
public:
    std::vector<std::vector<DrawItem>> build(const std::vector<DrawItem>& items) const {
        std::map<std::uint64_t,std::vector<DrawItem>> groups;
        for(const auto&i:items) groups[(std::uint64_t(i.material)<<32)|std::uint64_t(i.mesh)].push_back(i);
        std::vector<std::vector<DrawItem>> out; out.reserve(groups.size());
        for(auto&[key,batch]:groups){(void)key;out.push_back(std::move(batch));} return out;
    }
};
}
