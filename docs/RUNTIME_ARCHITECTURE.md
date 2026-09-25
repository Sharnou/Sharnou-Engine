# Sharnou Engine — Runtime Architecture Roadmap

Version 0.2 establishes the scalable runtime layer for the Windows 10 x64, C++23, Direct3D 11, AVIF-only 3D HD MMORPG/ARPG target.

## Runtime layers now present

- World streaming: deterministic spatial cells with load/unload residency.
- Visibility/culling: compact visible-instance candidates designed to feed indirect/GPU culling later.
- Skeletal animation: compact bone poses and clip blending.
- Materials: registry with a hard `.avif` albedo contract.
- Navigation: deterministic grid A* suitable for local pathing and server-side validation.
- Physics: fixed-step gravity integration and floor collision foundation.
- Replication: authoritative state store plus distance-based interest snapshots.
- Persistence: versioned binary save/load contract.
- Tooling: atomic runtime metrics.
- Dedicated server: renderer-free simulation executable with fixed-rate ticking.
- Runtime self-test: validates the above contracts during the Windows build.

## Next scale-up stages

1. GPU-driven renderer: depth prepass, Hi-Z occlusion, clustered/forward lighting, indirect draws, skinning buffers and descriptor/resource lifetime management.
2. World streaming: asynchronous IO, compressed region manifests, dependency-aware cell activation, LOD/HLOD and server shard ownership.
3. Animation: skeleton assets, animation compression, state machines, motion matching hooks, GPU skinning and attachment sockets.
4. Physics: broadphase spatial hash, character controller, swept collision, deterministic server collision and sleeping.
5. MMO networking: reliable/unreliable channels, delta compression, client prediction, server reconciliation, interest management and tick budgets.
6. Persistence: transactional character saves, shard snapshots, WAL/journal recovery and schema migrations.
7. Tooling/editor: asset database, AVIF import validation, world-cell editor, material inspector, animation preview, profiler and build cooker.
8. Server: login gateway, world/shard service, zone simulation, chat/social service boundaries, metrics and graceful migration.

The architecture deliberately does not claim to outperform Unity or Unreal in every workload. The target is a specialized engine with lower baseline overhead, predictable memory ownership and a small Windows runtime while expanding capabilities in focused MMORPG/ARPG domains.
