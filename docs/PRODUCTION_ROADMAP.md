# Sharnou Engine Production Roadmap

Status: Proposed canonical implementation roadmap
Product: Sharnou Engine for Honour War
Primary target: Windows 10/11 x64, Direct3D 11 / DXGI
Integration: Sharnou IDE -> SPP -> Sharnou Engine -> Honour War
External compiler policy: prohibited; Sharnou must own the production compiler and linker path

This plan expands the existing runtime and GPU-streaming architecture into a dependency-ordered production engine programme. It does not claim that the present source tree is a finished renderer, a complete MMO server or a buildable production runtime.

## 1. Product objective

Build a specialized, high-performance 3D MMORPG/ARPG engine for Honour War. Its runtime must provide deterministic gameplay foundations, a real geometry renderer, imported asset support, scalable world streaming, animation, physics, networking, persistence, profiling and a reliable dedicated server.

The engine is a product of the Sharnou toolchain. Its final runtime target remains Build/Runtime/SharnouEngine.exe. The engine must not silently fall back to Visual Studio, MSVC, Clang, MinGW, MSBuild, CMake or a downloaded compiler.

## 2. Audited repository baseline

| Area | Present in repository | Limitation that remains |
|---|---|---|
| Runtime foundations | C++ source for runtime, world, asset, gameplay, networking and server subsystems | Presence of headers/source is not proof of production correctness, concurrency safety or load capacity. |
| Runtime self-test | src/RuntimeSelfTest.cpp | Source-level self-test must be run by the actual built executable; it cannot prove an unbuilt executable works. |
| GPU-driven scene | GPU instance/visibility and indirect draw path are documented | Current culling stage is distance-oriented; it is not yet complete frustum + Hi-Z occlusion + LOD/material binning. |
| World streaming | Worker-backed async cell scheduler and explicit residency states are documented | A production cell package, dependency manifest, IO/decode/upload integration and recovery rules remain to be completed. |
| Renderer | D3D11/DXGI and GPU subsystem foundations exist | A clear/present loop or compute-contract test is not proof of complete geometry, materials, lighting, animation or world rendering. |
| Build path | Build script expects an approved sharnou-cxx.exe or sharnoucc.exe | The currently pinned compiler release discussed by open PR #1 is a PowerShell bootstrap for .sharnou and has no C++ frontend/native C++ compiler. The Engine build therefore remains fail-closed/blocked until this mismatch is resolved. |
| CI | Windows policy and contract checks are present | Existing policy success is not a successful Engine compile, actual GPU test or production runtime test. |

The build blocker must remain visible. Never report a successful Engine build unless the exact repository compiler produces the executable and the produced executable passes the runtime tests.

## 3. E0 - Resolve the engine source/compiler architecture

The Engine repository currently contains C++23 source, while the Sharnou Compiler roadmap describes a purpose-built Sharnou language and does not promise C++ compatibility. The Engine build script expects a Sharnou C++ compiler executable. These facts are incompatible today.

Before implementing more Engine functionality, record and approve one source architecture decision:

**Recommended path:** incrementally port the Engine from C++ prototype/reference sources to native Sharnou language modules as the compiler gains the required types, pointers/handles, arrays, structs, function pointers, imports and ABI support. Keep the existing C++ files as reference material until each subsystem has been ported and equivalence-tested. The final build must be performed by the native self-hosted Sharnou compiler.

Alternative path: retain C++23 as the production language only if Sharnou commits to implementing and testing its own C++ frontend, semantic model, ABI and linker path. This is a much larger compiler programme and must be explicitly approved as a compiler scope change.

No external C++ compiler may be used as a temporary production route.

E0 acceptance:
- Architecture decision record identifies the canonical source language and supported subset.
- Build contract names the actual compiler artifact, version, source identity, target and invocation.
- The current C++ build cannot report success when the required Sharnou compiler is missing.
- CI tests verify that no external compiler or linker is invoked.
- Porting work has subsystem-by-subsystem parity tests and retains the reference implementation until parity is demonstrated.

## 4. E1 - Native toolchain, runtime and platform lifecycle

Deliverables:
- Build and package the native SharnouEngine.exe at Build/Runtime/SharnouEngine.exe from the approved Sharnou compiler.
- Explicit Windows x64 ABI bindings for Win32, COM, DXGI and D3D11; compiler-owned import table generation and validation.
- Versioned compiler, runtime, Engine and SPP compatibility contracts.
- Centralized ownership of HWND, DXGI factory/swap chain, D3D11 device/context, back buffer, RTV, depth buffers and device-dependent resources.
- Correct resize, minimize, restore, DPI, focus, shutdown and device-removed/reset behavior.
- HRESULT/error propagation and structured diagnostics for every platform resource operation.
- Deterministic ownership and release tests for COM and OS handles.
- Separate client and renderer-free dedicated-server entry points.

Acceptance:
- Clean Windows x64 checkout builds using only the pinned Sharnou compiler and documented Windows runtime APIs.
- No external compiler, linker, SDK installer or build system is downloaded.
- Repeated startup/shutdown and resize/device-reset tests show no leaks or use-after-free.
- The runtime's PE imports are audited against the actual executable import table.
- Both client self-test and dedicated-server self-test execute successfully.

## 5. E2 - First real geometry renderer

Deliverables:
- Vertex and index buffer creation, immutable/dynamic update policies and bounds checks.
- Explicit vertex formats, input layouts, shader reflection/metadata and constant buffers.
- Stable row/column matrix convention and camera coordinate contract.
- Depth testing, viewport, rasterizer, blend and sampler states.
- Indexed triangle rendering, followed by a procedural mesh and a simple imported model.
- Resource lifetime tracking and device-loss recreation.
- Real runtime frame capture from the built executable.

Acceptance:
- A real indexed triangle and a nontrivial 3D mesh render in a D3D11 runtime scene.
- Captured frame contains verified non-clear geometry pixels; a blank clear-only window does not pass.
- Image evidence includes executable SHA-256, source commit, scene, resolution, settings, GPU and driver.
- Tests run on an actual D3D11-capable Windows desktop runner. Headless runners report this lane as unavailable rather than pass.

## 6. E3 - Production asset intake and runtime formats

The canonical runtime format roles are:

| Asset role | Format | Runtime requirement |
|---|---|---|
| 3D scene/model structure | glTF 2.x / GLB | Parse nodes, transforms, meshes, indices, accessors, skins, animations and declared extensions with strict bounds checking. |
| 3D material textures | KTX2 | Validate KTX 2.0 and implement a supported Basis Universal transcode path for target GPU formats. |
| 2D/UI raster visuals | AVIF | Validate and decode with a bundled or explicitly verified decoder; do not assume OS codec availability. |
| Source/interchange | FBX, OBJ and registered inputs | Import through the IDE pipeline and convert to the canonical runtime representation; do not ship unprocessed authoring sources as runtime dependencies. |

Deliverables:
- Versioned asset contract shared with Sharnou IDE and Honour War.
- Strict glTF/GLB parser, required-extension handling and KHR_texture_basisu validation.
- KTX2 metadata, mip, supercompression and payload-range validation plus transcode tests.
- AVIF parser/decoder capability verification, colour-space and alpha handling.
- Materials, dependency manifests, content hashes and relative-path resolution.
- Atomic asset cooking, SHA-256 verification, content-addressed cache and corruption recovery.
- Streaming and offline operation when every required asset is already verified in cache.
- Negative fixtures for truncated files, invalid offsets, integer overflow, missing extensions and malicious paths.

Acceptance:
- A clean imported model scene displays geometry and KTX2-backed materials in the real Engine.
- AVIF UI visuals decode in the runtime through the approved decoder path.
- Assets are verified before use; a cache hit is accepted only for matching hashes and contract versions.
- All files stay within the declared project/content roots.
- IDE SPP, Engine integration and Honour War manifest agree on a single contract version.

## 7. E4 - Materials, lighting and shadows

Deliverables:
- Metallic-roughness physically based material model with documented units and colour-space rules.
- Base colour, normal, roughness, metallic, ambient occlusion and emissive maps.
- Directional, point and spot lights with explicit count and range budgets.
- Cascaded directional shadows, stable shadow camera fitting, bias and filtering.
- Material/mesh batching and render-state sorting.
- GPU resource lifetime and memory budget telemetry.

Acceptance:
- Reference scenes validate material channels, colour space, lighting falloff, shadow stability and transparency.
- Screenshot comparisons use fixed camera, assets, settings and hardware; tolerances are documented.
- No claim of physically based rendering is made from base-colour-only output.

## 8. E5 - Character, animation, physics and gameplay foundations

Deliverables:
- Skeleton and joint validation, inverse bind matrices, skin weights and attachment sockets.
- Animation clips, interpolation, blending, state machines, root-motion policy and animation LOD.
- GPU skinning with a CPU/reference path for correctness comparison.
- Fixed-step physics, broadphase spatial hashing, swept character collision, floor/step handling and sleeping.
- Navigation mesh/grid integration and server-authoritative movement validation.
- Third-person camera, click-to-move input and accessibility/configurable control support.
- Reusable combat primitives: hitboxes, hurtboxes, stamina/cooldown timing, telegraphs, invulnerability windows and deterministic damage resolution.

Acceptance:
- Animation and physics reference scenes run at fixed simulation rates.
- CPU and GPU skinning outputs match within specified tolerances.
- Collision and navigation tests cover boundaries, tunnelling, slopes, moving obstacles and invalid input.
- Gameplay systems are tested independently before Honour War content depends on them.

## 9. E6 - Large-world streaming and GPU visibility

Deliverables:
- Versioned region/cell package format with dependencies, compression and hashes.
- Async file IO, decode, mesh preparation and GPU upload queues separated from the render thread.
- Explicit cell lifecycle and cancellation/eviction semantics.
- Camera frustum culling, Hi-Z pyramid and occlusion culling.
- Per-instance LOD/HLOD, mesh/material binning, indirect draws and shadow-caster culling.
- Transient GPU allocator, frame resource lifetime tracking and GPU timestamps.
- Bounded memory/IO/decode/upload queues and backpressure.

Acceptance:
- Deterministic streaming tests cover rapid travel, cancellation, eviction, missing/corrupt cells and low-memory pressure.
- GPU culling tests compare visible sets against a CPU reference.
- Performance reports include scene size, instance counts, resolution, GPU, driver, frame budget and measurement method.
- No performance claim is based only on source-level shader inspection.

## 10. E7 - MMO simulation, networking and persistence

Deliverables:
- Dedicated server process with no renderer dependency.
- Authoritative simulation tick, deterministic state transitions and bounded per-tick work.
- Client/server protocol with version negotiation, authentication boundary, message validation and rate limits.
- Reliable and unreliable channels, delta compression, interest management and spatial replication.
- Client prediction and server reconciliation for approved movement/combat state.
- Character persistence with transactions, journal/WAL recovery, schema migrations and idempotent writes.
- Login/session gateway, world/shard services, zone simulation, chat/social service boundaries and graceful shard migration.
- Abuse controls, anti-replay, input validation, observability and incident diagnostics.

Acceptance:
- Network simulation tests cover packet loss, latency, reordering, reconnects, version mismatch and malformed packets.
- Persistence tests cover process crash at transaction boundaries and migration from prior schema versions.
- Server load tests report concurrent sessions, entity counts, tick latency, memory, bandwidth and failure rates.
- The server remains renderer-independent and can restart or migrate without duplicating character state.

## 11. E8 - Sharnou IDE integration and content authoring

Deliverables:
- IDE-owned project/Spp build, asset import/cook, Engine launch and test control.
- Version-pinned compiler and Engine contracts.
- Scene/world-cell, material, character/animation and navigation authoring tools.
- Asset dependency graph and incremental cooker.
- Runtime log viewer, profiler and real frame capture integration.
- Reproducible project manifests and automated Honour War smoke tests.

Acceptance:
- The complete workflow is initiated and observed through Sharnou IDE.
- The IDE launches only the approved SharnouEngine.exe whose hash matches the build manifest.
- Project, Engine and asset-contract mismatches are rejected before launch.

## 12. E9 - Packaging, performance and production operations

Deliverables:
- Reproducible runtime package and signed/verified manifest.
- Small Windows bootstrap with separately cached content packs and SHA-256 verification.
- Offline launch from verified cached assets.
- Versioned patch/delta delivery, rollback and interrupted-update recovery.
- Crash dumps, structured logs, CPU/GPU telemetry and health reporting.
- Memory, CPU, GPU, bandwidth, asset streaming and server tick budgets.
- Accessibility, controller, UI scaling, localization and remapping validation.
- Security review for parsers, network protocol, account/session flow and saved data.

Acceptance:
- Clean install/update/rollback/offline tests pass on supported Windows configurations.
- Runtime package size is measured from the actual artifact.
- 30/60/120-minute client and server soak tests complete without unbounded memory growth, deadlocks or unrecovered errors.
- Release evidence includes source SHA, compiler SHA, runtime SHA-256, manifest, test report, hardware matrix and known limitations.

## 13. CI and automation model

Every Engine PR must run:
- Repository policy and forbidden-tool scan.
- Compiler contract and build fail-closed tests.
- Source/language syntax tests for the selected canonical implementation language.
- Asset parser and manifest validation.
- Runtime self-test when a genuine approved executable is available.

Scheduled/nightly validation must add:
- Full CPU/runtime regression suite.
- Streaming, asset corruption and cache recovery tests.
- Dedicated-server deterministic/network/persistence suites.
- Actual D3D11 execution and real frame capture on a suitable interactive Windows runner.
- Client/server soak and performance budgets.

A missing compiler, secret, D3D11 GPU runner or runtime binary is a visible BLOCKED/UNAVAILABLE result. It must never be converted into a green test by skipping the operation.

## 14. Definition of done

Sharnou Engine is production-capable when it builds reproducibly through the approved self-hosted Sharnou toolchain; launches a real Windows runtime; renders imported glTF/GLB scenes with KTX2 materials and AVIF UI assets; provides tested PBR, lighting, shadows, animation, physics and world streaming; runs an authoritative dedicated MMO server with validated networking and persistence; and is fully controlled by Sharnou IDE.

A source-level subsystem, a successful policy scan, a clear-only window, a shader text check or an unlaunched executable is not sufficient evidence of a production feature.