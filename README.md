# Sharnou Engine

Sharnou Engine is the canonical custom engine for **Honour War**, a 3D HD MMORPG/ARPG.

## Permanent integration

**Sharnou-IDE -> SPP -> SharnouEngine -> Honour War**

- Canonical game project: honour-war
- Game repository: https://github.com/Sharnou/Honour-War
- Authoritative IDE: https://github.com/Sharnou/Sharnou-IDE
- Engine ID: SharnouEngine
- Movement contract: Ragnarok Online-style click-to-move; no WASD
- Runtime 3D scenes: .gltf / .glb
- Runtime 3D textures: .ktx2 using KHR_texture_basisu
- Runtime 2D/raster visuals: .avif

Sharnou-IDE is the sole project-authoring and runtime-control entry point. Existing IDE/project metadata is migration input and is converted to the Sharnou-IDE SPP contract. Legacy IDEs are not runtime controllers.

## Rejected development dependencies

Honour War/SharnouEngine does not use or bootstrap Visual Studio, MSBuild, Windows SDK development installations, CMake, vcpkg, Unity, Unreal Engine, or automatic external programming-tool downloads.

No compiler, SDK, IDE, or build system is downloaded automatically.

## Runtime-first model

Sharnou-IDE validates the canonical project and engine contracts, compiles SPP authoring data to engine-consumable bytecode, then launches only an already-existing approved SharnouEngine runtime. Runtime evidence must come from the actual executable; static validation is not runtime evidence.

## Asset roles

The IDE intake boundary may accept registered source formats. Canonical runtime delivery uses glTF/GLB for 3D scene/model structure, KTX2 for GPU-facing 3D textures, and AVIF for shipped 2D/raster visuals. FBX/OBJ remain source/interchange inputs and are converted before runtime packaging.

## Validation sequence

1. Sharnou-IDE canonical project identity check.
2. SPP validation/compilation.
3. SharnouEngine contract validation.
4. Existing-runtime self-test.
5. Actual runtime gameplay test.
6. Real gameplay screenshot only after executable runtime evidence exists.
