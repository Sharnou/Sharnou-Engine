# Sharnou Engine

Sharnou Engine is a Windows 10 x64 native C++23 engine foundation for a 3D HD MMORPG/ARPG. Its design goal is a much smaller, specialized runtime than general-purpose engines, while concentrating performance engineering on ECS data locality, bounded concurrency, fast startup, Direct3D 11, and AVIF-only texture assets.

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

The first engine slice contains:

1. Dense ECS-style component pools using sparse-to-dense entity indexing.
2. A bounded MPMC lock-free task queue with jthread workers and condition-variable wakeups.
3. Real AVIF decoding through libavif, with a 256 MiB compressed-file guard and 8192x8192 decoded-surface guard.
4. Direct3D 11 hardware device creation, depth buffering, HLSL shader compilation and a small 3D cube render path.
5. RAII COM resource ownership through Microsoft::WRL::ComPtr.
6. Fixed Windows subsystem entry point and no cross-platform runtime branches.
7. A runtime texture boundary that rejects non-AVIF texture files.
8. A Windows CI workflow and a 1 GiB runtime-binary size guard.

libavif is used because AVIF decoding is not correctly implemented by merely inspecting a file header; libavif provides actual AV1/AVIF decode and YUV/RGB conversion APIs. Its current project documentation recommends tagged releases and exposes Windows installation through vcpkg.

## Build on Windows 10

Install Visual Studio Community 2022 with Desktop development with C++, CMake tools and the Windows 10 SDK. Install vcpkg and set VCPKG_ROOT to the vcpkg directory.

Then from the repository root in PowerShell:

    ./tools/build.ps1

Or configure manually:

    cmake --preset windows-msvc-release
    cmake --build build/vs2022-release --config Release --parallel

The vcpkg manifest selects libavif with dav1d for decoding. The current libavif package exposes AV1 decoder features through its vcpkg port.

## AVIF contract

Only files with the .avif extension are accepted by the runtime texture loader. Other texture extensions are rejected. Production textures belong under assets/textures.

Decoded RGBA data is uploaded into an immutable D3D11 shader resource. The engine does not ship a large demo texture pack, which keeps the repository and first-run footprint small.

## Size target

The engineering target is a runtime package comfortably below 1 GiB, with source and engine architecture designed to stay much smaller than a general-purpose engine. The repository contains a size guard, but the final installed size still depends on the chosen libavif codec linkage and the Windows redistributable/dependency packaging. No unsupported claim is made that the engine is already faster or more capable than Unity or Unreal across all workloads; the intended advantage is specialization, lower baseline overhead and controllable architecture.

## Current runtime test

Launching the Release executable opens a native 1280x720 window, creates the Direct3D 11 device, compiles the HLSL pipeline, instantiates 2,000 ECS entities and renders a lit rotating 3D cube. If an AVIF file is present under assets/textures, the first valid AVIF is decoded and bound to the cube.

This is the foundation layer, not the finished MMORPG. The next architecture layers should add scene streaming, visibility/culling, animation, skeletal meshes, materials, navigation, physics, replication, persistence, editor tooling and the authoritative server runtime without changing the Windows/C++23/AVIF constraints.
