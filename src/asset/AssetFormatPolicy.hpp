#pragma once
#include <cstdint>
#include <string_view>

namespace shn::asset {

enum class AssetRole : std::uint8_t {
    Scene3D, Texture3D, Raster2D, SourceModel, Unknown
};

inline constexpr std::string_view lowerExtension(std::string_view path) noexcept {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos || dot < slash) return {};
    return path.substr(dot);
}

inline constexpr bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}

inline constexpr AssetRole classify(std::string_view path) noexcept {
    const auto ext = lowerExtension(path);
    if (equalsIgnoreCase(ext, ".gltf") || equalsIgnoreCase(ext, ".glb")) return AssetRole::Scene3D;
    if (equalsIgnoreCase(ext, ".ktx2")) return AssetRole::Texture3D;
    if (equalsIgnoreCase(ext, ".avif")) return AssetRole::Raster2D;
    if (equalsIgnoreCase(ext, ".fbx") || equalsIgnoreCase(ext, ".obj")) return AssetRole::SourceModel;
    return AssetRole::Unknown;
}

inline constexpr bool isRuntimeAsset(std::string_view path) noexcept {
    const auto role = classify(path);
    return role == AssetRole::Scene3D || role == AssetRole::Texture3D || role == AssetRole::Raster2D;
}

inline constexpr bool isApprovedRuntimeTexture(std::string_view path) noexcept {
    const auto role = classify(path);
    return role == AssetRole::Texture3D || role == AssetRole::Raster2D;
}

} // namespace shn::asset
