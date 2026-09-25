# Sharnou Engine

Sharnou Engine is a Windows 10 x64 native C++23 engine foundation for a 3D HD MMORPG/ARPG. Its design goal is a much smaller, specialized runtime than general-purpose engines, while concentrating performance engineering on ECS data locality, bounded concurrency, GPU-driven rendering, asynchronous world streaming, fast startup, Direct3D 11, and AVIF-only texture assets.

## Fixed platform

- Operating system: Windows 10 x64 only
- Compiler/IDE: Microsoft Visual Studio Community 2022
- Language: strict modern C++23 with MSVC /std:c++latest
- Graphics: hardware Direct3D 11
- Texture source format: .avif only
- Build system: CMake + vcpkg

## Architecture

The runtime deliberately follows the common-layer spirit of engineerOfLies/gfc without copying its source files. The upstream project describes itself as Game Framework Common, a common library for Game Framework 2D and 3D, and is MIT-licensed. See ATTRIBUTION.md.

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
13. A Hi-Z depth-pyramid foundation for hierarchical occlusion testing.
14. A dependency-aware render frame graph for ordered GPU passes.
15. A thread-safe streaming-to-GPU upload queue.
16. A deterministic character motor with acceleration, gravity and jump state.

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

## Renderer stage

`src/render/GpuDrivenScene.hpp` provides the D3D11 compute-culling foundation: structured instance data, a 64-thread compute shader, a GPU visible-index buffer and a `DrawIndexedInstancedIndirect` argument buffer.

`src/render/HiZOcclusion.hpp` provides a hierarchical depth representation with conservative max-depth reduction. It is a CPU-side contract layer today and is deliberately isolated so the same resource layout can be moved to a D3D11 compute-generated pyramid when the renderer binds a depth SRV/UAV path.

`src/render/RenderFrameGraph.hpp` provides dependency-aware pass scheduling. A production frame can now be expressed as resource dependencies rather than hard-coded submission order.

The next renderer layers are camera frustum planes, GPU Hi-Z generation, hierarchical occlusion, LOD/HLOD selection, material/mesh batching, GPU skeletal skinning, shadow-caster culling, transient GPU allocation and timestamp profiling.

## World streaming stage

`src/world/AsyncWorldStreaming.hpp` provides worker-backed cell scheduling with `Unloaded`, `Queued`, `Loading`, `Resident` and `Evicting` states.

`src/world/StreamingUploadQueue.hpp` provides the boundary between background asset preparation and the render thread's GPU upload stage. The production cell package format should remain versioned and contain cell manifests, dependency data, mesh records, collision records, AVIF material references and GPU upload records.

The intended frame path is gameplay tick -> streaming requests -> completed background work -> GPU upload -> frame graph -> GPU visibility -> indirect geometry -> lighting/shadows -> UI -> present.

## Gameplay stage

`src/gameplay/CharacterMotor.hpp` supplies a deterministic lightweight character controller contract for movement, acceleration, gravity, ground state and jump impulses. It is intended to be shared conceptually by client prediction and server simulation, while authoritative collision remains a server concern.

## Size target

The engineering target is a runtime package below 1 GiB. The repository contains a size guard, but final installed size depends on codec linkage and Windows runtime dependencies. No unsupported claim is made that the engine is already faster or more capable than Unity or Unreal across all workloads; the target is a specialized architecture with lower baseline overhead and controllable runtime costs.

## Validation

`SharnouRuntimeSelfTest` now validates base runtime contracts, asynchronous streaming, GPU-culling shader structure, Hi-Z construction, frame-graph ordering, streaming upload handoff and character motor behavior. Actual D3D11 device execution and performance profiling still require a Windows machine with a Direct3D 11-capable adapter.

## Roadmap

The immediate production renderer stage is GPU Hi-Z generation, camera frustum/occlusion culling, LOD/HLOD, GPU skeletal skinning, material/mesh batching, shadow-caster culling, transient GPU allocation and GPU timestamp profiling. The following MMO stage expands authoritative simulation, interest management, delta snapshots, persistence transactions, shard/zone ownership and crash recovery.
