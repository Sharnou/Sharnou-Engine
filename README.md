# Sharnou Engine

Sharnou Engine is the canonical custom engine for **Honour War**, a 3D HD MMORPG/ARPG.

## Permanent integration

**Sharnou-IDE → SPP → SharnouEngine → Honour War**

- Canonical game project: `honour-war`
- Game repository: `https://github.com/Sharnou/Honour-War`
- Authoritative IDE: `https://github.com/Sharnou/Sharnou-IDE`
- Engine ID: `SharnouEngine`
- Project protocol: Sharnou Project Protocol (SPP)
- Movement contract: Ragnarok Online-style click-to-move; no WASD
- Generated raster visual format: `.avif` only

Sharnou-IDE is the sole project-authoring and runtime-control entry point. Existing IDE/project metadata is migration input and is automatically converted to the Sharnou-IDE SPP contract. Legacy IDEs are not runtime controllers.

## Rejected development dependencies

Honour War/SharnouEngine does not use or bootstrap:

- Visual Studio
- MSBuild
- Windows SDK development installations
- CMake
- vcpkg
- Unity
- Unreal Engine
- automatic external programming-tool downloads

No network download is performed to obtain a compiler, SDK, IDE or build system.

## Runtime-first model

Sharnou-IDE validates the canonical project and engine contracts, compiles SPP authoring data to engine-consumable bytecode, then launches only an already-existing approved SharnouEngine runtime. Runtime evidence must come from the actual executable; static validation is not runtime evidence.

The repository's historical native implementation contains platform-specific renderer/source work from the earlier bootstrap stage. That code is retained as engineering source while the active project control/build policy is migrated to Sharnou-IDE. It must not be used as a reason to restore a rejected IDE or toolchain.

## Asset policy

Model intake remains FBX/OBJ. GLB/GLTF is rejected.

All newly generated or converted raster textures/visuals use `.avif` only. Historical reference images are allowed as reference material but are not new generated assets.

See `SHARNOU_IDE_INTEGRATION.json` for the machine-readable integration contract.

## Validation

The authoritative validation sequence is:

1. Sharnou-IDE canonical project identity check.
2. SPP validation/compilation.
3. SharnouEngine contract validation.
4. Existing-runtime self-test.
5. Actual runtime gameplay test.
6. Real gameplay screenshot only after executable runtime evidence exists.
