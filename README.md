# Sharnou Engine

Sharnou Engine is a Windows 10 x64 native C++23 engine foundation for a 3D HD MMORPG/ARPG. Its design goal is a much smaller, specialized runtime than general-purpose engines, while concentrating performance engineering on ECS data locality, bounded concurrency, GPU-driven rendering, asynchronous world streaming, fast startup, Direct3D 11, and AVIF-only texture assets.

## Fixed platform

- Operating system: Windows 10 x64 only
- Compiler/IDE: Microsoft Visual Studio Community 2022
- Language: strict modern C++23 with MSVC /std:c++latest
- Graphics: hardware Direct3D 11
- Texture source format: .avif only
- Build system: CMake + vcpkg

Direct3D 11 is a native Windows/C++ graphics API and Windows 10 includes the Direct3D 11.3 API. See Microsoft documentation: https://learn.microsoft.com/en-us/windows/win32/direct3darticles/direct3d11-deployment

## Architecture

The runtime deliberately follows the common-layer spirit of engineerOfLies/gfc without copying its source files. The upstream project describes itself as Game Framework Common, a common library for Game Framework 2D and 3D, and is MIT-licensed. See ATTRIBUTION.md and https://github.com/engineerOfLies/gfc.

Current engine layers include:

1. Dense ECS-style component pools using sparse-to-dense entity indexing.
2. A bounded MPMC task queue with jthread workers and condition-variable wakeups.
3. Real AVIF decoding through libavif, with compressed-file and decoded-surface guards.
4. Direct3D 11 hardware device creation, depth buffering, HLSL shader compilation and a 3D test render path.
5. RAII COM resource ownership through Microsoft::WRL::ComPtr.
6. Strict Windows 10 x64 runtime boundaries.
7. A runtime texture boundary that rejects non-AVIF texture files.
8. Asynchronous world-cell scheduling with explicit residency states.
9. GPU-driven instance buffers, compute visibility testing and D3D11 indirect draw arguments.
10. Skeletal animation, material registry, navigation, fixed-step physics, replication and persistence foundations.
11. A renderer-independent dedicated server target.
12. Windows CI and a 1 GiB runtime-binary size guard.

## Build on Windows 10

Install Visual Studio Community 2022 with Desktop development with C++, CMake tools and the Windows 10 SDK. Install vcpkg and set VCPKG_ROOT to the vcpkg directory.

From the repository root in PowerShell:

    ./tools/build.ps1

Or configure manually:

    cmake --preset windows-msvc-release
    cmake --build build/vs2022-release --config Release --parallel

The vcpkg manifest selects libavif with dav1d for decoding.

## AVIF contract

Only files with the .avif extension are accepted by the runtime texture loader. Other texture extensions are rejected. Production textures belong under assets/textures.

Decoded RGBA data is uploaded into an immutable D3D11 shader resource. The engine does not ship a large demo texture pack, which keeps the repository and first-run footprint small.

## GPU-driven rendering stage

`src/render/GpuDrivenScene.hpp` now provides a D3D11 compute-culling foundation: structured instance data, a 64-thread compute shader, a GPU visible-index buffer and a `DrawIndexedInstancedIndirect` argument buffer. The current visibility predicate is distance-based. Frustum planes, Hi-Z occlusion, LOD selection, material binning, GPU skinning and shadow culling are the next renderer layer.

## Asynchronous world streaming stage

`src/world/AsyncWorldStreaming.hpp` provides worker-backed cell scheduling with `Unloaded`, `Queued`, `Loading`, `Resident` and `Evicting` states. The production cell package format is intentionally not invented yet; a later asset-cooking stage should define versioned cell manifests, dependency data, mesh data, collision data and GPU-upload records.

The intended frame path is gameplay tick -> streaming requests -> completed background work -> GPU upload -> GPU visibility -> indirect geometry -> lighting/shadows -> UI -> present.

## Size target

The engineering target is a runtime package below 1 GiB. The repository contains a size guard, but final installed size depends on codec linkage and the Windows runtime dependencies. No unsupported claim is made that the engine is already faster or more capable than Unity or Unreal across all workloads; the target is a specialized architecture with lower baseline overhead and controllable runtime costs.

## Validation

`SharnouRuntimeSelfTest` validates the base runtime contracts, asynchronous streaming contract and GPU-culling shader contract. Actual D3D11 device execution and performance profiling still require a Windows machine with a Direct3D 11-capable adapter.

## Roadmap

The next major stage is a production renderer: camera frustum culling, Hi-Z occlusion, LOD/HLOD, GPU skeletal skinning, material/mesh batching, shadow-caster culling, transient GPU allocation and GPU timestamp profiling. After that, networking/persistence can be expanded into production MMO shard, interest-management and crash-recovery infrastructure.
