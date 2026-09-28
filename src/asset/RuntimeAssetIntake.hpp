#pragma once
#include "Ktx2Container.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace shn::asset {

struct BootstrapVertex {
    float position[3]{};
    float normal[3]{0.f, 1.f, 0.f};
    float uv[2]{};
};

struct BootstrapScene {
    std::vector<BootstrapVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::filesystem::path textureKtx2;
    bool valid{};
    std::string error;
};

inline std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

inline bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

inline bool decodeBase64(std::string_view input, std::vector<std::uint8_t>& out) {
    std::array<std::uint8_t, 256> table{};
    table.fill(0xFF);
    for (std::uint8_t i=0;i<26;++i) {
        table[static_cast<unsigned char>('A'+i)] = i;
        table[static_cast<unsigned char>('a'+i)] = static_cast<std::uint8_t>(26+i);
    }
    for (std::uint8_t i=0;i<10;++i) {
        table[static_cast<unsigned char>('0'+i)] = static_cast<std::uint8_t>(52+i);
    }
    table[static_cast<unsigned char>('+')] = 62;
    table[static_cast<unsigned char>('/')] = 63;

    std::uint32_t buffer = 0;
    unsigned bits = 0;
    out.clear();
    for (unsigned char ch : input) {
        if (ch == '=') break;
        const auto v = table[ch];
        if (v == 0xFF) continue;
        buffer = (buffer << 6u) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFFu));
        }
    }
    return !out.empty();
}

inline BootstrapScene loadBootstrapGltf(const std::filesystem::path& path) {
    BootstrapScene scene;
    const auto bytes = readAllBytes(path);
    if (bytes.empty()) {
        scene.error = "glTF scene could not be read.";
        return scene;
    }

    const std::string json(bytes.begin(), bytes.end());
    if (!contains(json, "\"asset\"") || !contains(json, "\"version\": \"2.0\"")) {
        scene.error = "glTF 2.0 asset declaration missing.";
        return scene;
    }
    if (!contains(json, "\"extensionsUsed\"") || !contains(json, "KHR_texture_basisu")) {
        scene.error = "KHR_texture_basisu declaration missing.";
        return scene;
    }

    constexpr std::string_view marker = "data:application/octet-stream;base64,";
    const auto markerPos = json.find(marker);
    if (markerPos == std::string::npos) {
        scene.error = "glTF bootstrap buffer URI is missing.";
        return scene;
    }

    const auto begin = markerPos + marker.size();
    auto end = json.find('"', begin);
    if (end == std::string::npos) end = json.size();

    std::vector<std::uint8_t> geometry;
    if (!decodeBase64(std::string_view(json).substr(begin, end-begin), geometry) || geometry.size() < 42) {
        scene.error = "glTF bootstrap geometry buffer is invalid.";
        return scene;
    }

    scene.vertices.resize(3);
    for (std::size_t i=0;i<3;++i) {
        std::memcpy(scene.vertices[i].position, geometry.data() + i*12, 12);
        scene.vertices[i].uv[0] = i == 1 ? 1.f : 0.f;
        scene.vertices[i].uv[1] = i == 2 ? 1.f : 0.f;
    }

    constexpr std::size_t indexOffset = 36;
    for (std::size_t i=0;i<3;++i) {
        const auto* p = geometry.data() + indexOffset + i*2;
        scene.indices.push_back(
            static_cast<std::uint32_t>(p[0]) |
            (static_cast<std::uint32_t>(p[1]) << 8u));
    }

    const std::string imageMarker = "\"uri\": \"";
    const auto imagePos = json.find(imageMarker);
    if (imagePos != std::string::npos) {
        const auto imageBegin = imagePos + imageMarker.size();
        const auto imageEnd = json.find('"', imageBegin);
        if (imageEnd != std::string::npos) {
            scene.textureKtx2 =
                path.parent_path() / std::filesystem::path(
                    json.substr(imageBegin, imageEnd-imageBegin));
        }
    }

    scene.valid = true;
    return scene;
}

inline bool validateKtx2File(const std::filesystem::path& path, std::string& error) {
    const auto bytes = readAllBytes(path);
    if (!hasValidKtx2Header(bytes)) {
        error = "KTX2 texture is missing or has an invalid KTX 2.0 header.";
        return false;
    }
    return true;
}

} // namespace shn::asset
