# Sharnou Engine — GPU + Streaming Stage

## Scope

This stage establishes the performance path needed before the engine grows into a large MMORPG/ARPG renderer: asynchronous world-cell work and a Direct3D 11 GPU-driven visibility path.

## GPU-driven scene

`src/render/GpuDrivenScene.hpp` contains:

- structured GPU instance data;
- a D3D11 compute shader with 64-thread groups;
- distance visibility testing on the GPU;
- a GPU visible-instance index buffer;
- an indirect draw argument buffer;
- `DrawIndexedInstancedIndirect` support;
- explicit UAV/SRV unbinding after the compute pass;
- AVIF-independent rendering infrastructure so texture decoding remains in the existing asset boundary.

The current compute test is distance-based. It is intentionally a first GPU-driven stage rather than a claim of complete UE5-style occlusion culling. The next renderer pass should add camera frustum planes, hierarchical depth/Hi-Z occlusion, LOD selection and material batching.

## Asynchronous world streaming

`src/world/AsyncWorldStreaming.hpp` adds a worker-backed cell scheduler. World cells have explicit `Unloaded`, `Queued`, `Loading`, `Resident` and `Evicting` states. A configurable radius determines which cells are scheduled around the player's current cell.

The worker queue is separate from the render thread. Cell work can therefore evolve into file IO, AVIF decode, mesh decompression, collision cooking and GPU upload without making the frame loop perform blocking disk work.

The current cell loader intentionally does not invent a binary cell format. A later asset-cooking stage should define the production `.cell` package, versioning and dependency manifest.

## Performance path

The intended client frame sequence is:

1. input and gameplay tick;
2. update streaming residency requests;
3. consume completed background cell work;
4. upload changed instance data;
5. GPU visibility/culling compute pass;
6. indirect opaque geometry;
7. shadows and lighting;
8. transparent/UI passes;
9. present.

The dedicated server remains renderer-independent.

## Validation

`SharnouRuntimeSelfTest` now checks the existing runtime contracts, asynchronous world streaming, and the GPU-culling shader contract. A Windows GPU execution test still requires a Windows 10/11 machine with a Direct3D 11-capable adapter; source-level self-test success is not a substitute for that hardware validation.

## Next renderer work

The next implementation stage should add:

- frustum-plane GPU culling;
- Hi-Z depth pyramid generation;
- occlusion culling;
- per-instance LOD selection;
- material/mesh binning;
- GPU skeletal skinning;
- shadow-caster culling;
- streaming completion -> GPU upload queues;
- frame allocator and transient descriptor/resource tracking;
- GPU timestamp profiling.
