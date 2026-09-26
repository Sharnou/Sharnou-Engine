#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace shn::asset {

inline constexpr std::uint8_t Ktx2Identifier[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

inline bool isKtx2(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < sizeof(Ktx2Identifier)) return false;
    for (std::size_t i = 0; i < sizeof(Ktx2Identifier); ++i) {
        if (bytes[i] != Ktx2Identifier[i]) return false;
    }
    return true;
}

inline std::uint32_t readU32LE(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

inline std::uint32_t ktx2VkFormat(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= 20 ? readU32LE(bytes, 12) : 0;
}

inline std::uint32_t ktx2Width(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= 24 ? readU32LE(bytes, 20) : 0;
}

inline std::uint32_t ktx2Height(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= 28 ? readU32LE(bytes, 24) : 0;
}

inline std::uint32_t ktx2LevelCount(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= 44 ? readU32LE(bytes, 40) : 0;
}

inline bool hasValidKtx2Header(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < 80 || !isKtx2(bytes)) return false;
    const auto width = ktx2Width(bytes);
    const auto height = ktx2Height(bytes);
    const auto levels = ktx2LevelCount(bytes);
    return width > 0 && height > 0 && levels > 0;
}

} // namespace shn::asset
