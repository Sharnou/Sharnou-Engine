#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace shn::render {
struct GpuSlice { std::size_t offset{}, size{}; };
class TransientGpuAllocator {
    std::vector<std::uint8_t> memory_; std::size_t cursor_{};
public:
    explicit TransientGpuAllocator(std::size_t capacity=16*1024*1024):memory_(capacity){}
    void beginFrame() noexcept {cursor_=0;}
    GpuSlice allocate(std::size_t bytes,std::size_t alignment=256) noexcept { const std::size_t aligned=(cursor_+alignment-1)&~(alignment-1); if(aligned>memory_.size()||bytes>memory_.size()-aligned)return{}; cursor_=aligned+bytes; return{aligned,bytes}; }
    std::size_t used() const noexcept{return cursor_;} std::size_t capacity() const noexcept{return memory_.size();}
};
}
