# Sharnou Engine Texture Policy

Sharnou Engine uses a two-track runtime texture policy.

## 3D material textures — .ktx2

KTX 2.0 is the required shipped texture container for 3D materials used by glTF assets.

Use KTX2 for:
- Albedo/base color
- Normal
- Roughness/metallic
- Ambient occlusion
- Emissive
- Other GPU-facing 3D material textures

glTF assets should reference KTX2 through the Khronos KHR_texture_basisu extension when using Basis Universal textures.

## 2D visual assets — .avif

AVIF is the required shipped raster format for:
- UI images
- Menu/background artwork
- 2D icons
- 2D promotional/distribution artwork
- Other non-material raster imagery

## Model/scene containers

- .gltf and .glb are approved glTF 2.x scene/model containers.
- FBX/OBJ are authoring/interchange inputs and are not shipped runtime texture formats.
- KTX2 is the texture payload for 3D materials.
- AVIF is the raster payload for 2D/UI imagery.

## Rejected shipped texture formats

Do not ship PNG, JPG/JPEG, WebP, GIF, BMP, TGA, or DDS textures in the runtime asset tree.

The format split is intentional: glTF carries scene/model structure, KTX2 carries GPU-oriented 3D texture data, and AVIF carries compact 2D imagery.
