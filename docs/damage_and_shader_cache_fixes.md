# Projectile owner lifetime and shader cache follow-up

This follows the [collision optimisation investigation](dynamic_lighting_tracing.md).
The scope is two existing-engine defects: a stale damage-owner lookup, then
redundant compilation of byte-identical fragment shader sources. Lighting
settings/defaults, visual equations, host policy and Xbox/CE/MCC loading paths
must remain unchanged.

## Projectile-damage crash

Both original failures assert that an object datum is "unused or changed":
`0x8107004a` in the disabled control and `0x8e8b0048` in the enabled control.
The subsequent null dereference is in `object_get_and_verify_type`, reached
from `object_cause_damage`'s gametype multiplier lookup. In
`get_player_index_from_object_or_parents`, a failed `unit_try_and_get` was
followed by a strict `object_get` on the same handle.

The engine permits a projectile to outlive its owner. Its deleted-object
handler clears a homing target but does not retire the owner handle. The
damage structure also carries player/team ownership separately. Therefore the
fix checks the object before following its parent and returns `NONE` if it no
longer exists, rather than erasing ownership or suppressing damage. Datum
generation checking prevents a reused slot from being treated as the old
owner. Valid units and parent chains retain their original results.

This is a general object-lifetime defect reproduced on an Xbox multiplayer
map with lighting off and on, not an adaptation to MCC data. Only this lookup
in `source/objects/damage.c` changes; it carries a `port:` comment. Host damage
admission still precedes calculation. No wire format, prediction rule or
valid-input damage calculation changes, so the pinned OpenCE and MCC network
versions remain unchanged. A departed player's separate damage attribution
still follows the existing validation and network rules.

The combined owner-lookup, stale-player-damage and MCC-damage harness run passed
52 cases, including negative controls that restore the strict lookup and fail
on deleted/reused objects and missing parents. The normal Windows release
build passed. Twenty protocol/admission checks also passed. Two hidden
six-minute measured Carousel combat runs with 15 Heroic bots completed normally,
with lighting off/on respectively. A test-only diagnostic detected no retired
owner encounters in those particular runs: they are stability evidence, while
the harness supplies the deterministic stale-owner regression. They ran together
at 640x480/30 FPS and are not performance benchmarks. No diagnostic logging is
present in the normal build.
The damage commit's [four-platform CI](https://github.com/chestahh/GulchCE/actions/runs/40254648830)
and [MCC validation](https://github.com/chestahh/GulchCE/actions/runs/40254648722)
passed.

## Fragment shader duplication

The full-state key remains unchanged. On its miss, the existing translator
still generates the exact same GLSL; `fragment_source_get` in `d3d8_gl.c` hashes
the bytes and compares the complete string before sharing a compiled shader.
This fixes a shared renderer cache inefficiency already demonstrated with
dynamic lighting disabled, not an MCC compatibility requirement. No map-family
adapter or shader translator is changed. The shared backend owns the GL objects,
so this general renderer fix belongs there rather than in a lighting module.

The source entry owns the generated string and shader result for the existing
GL context lifetime. Key entries reference the shader ID, and the unchanged
program cache can reuse links for identical vertex/fragment pairs. Per-program
uniform state and serial tracking remain unchanged. Failed compilation stays
cached; failed cache allocation falls back to the original per-key ownership.
Map unloads retain these context resources, with no map/tag pointers retained.
The existing device is created once, and `D3DDevice_Release` does not destroy it;
this change does not introduce or claim context-recreation support.

The new harness compiles the actual source/full-key cache functions and covers
identical versus different/prefix sources, forced hash collisions, recent-key
eviction, source ownership, compilation failure and allocation failure. Fourteen
tests passed, including deliberately broken equality/sharing controls. The
normal Windows build passed. Diagnostic resource CSVs now distinguish full-key
entries (`fragments`) from unique source entries and their retained string bytes;
key-entry counts alone no longer represent compiled fragment-object counts.

The AMD/NVIDIA visibility implementation was reviewed separately. AMD's CPU-only
query readback, typed query pool, buffer binding restoration, NVIDIA's buffered
delivery, delayed fences and post-swap nonblocking polling are unchanged. Their
30 contract/harness tests passed before the cache change and in the combined
128-test final regression run (30.37 seconds). That run also covers damage,
protocol/admission, shader sharing, occlusion binding/reprojection and model
shader translation. Source comparison confirms the visibility functions,
compile/link functions, `program_get`, `prepare_draw`, presentation and shader
translators remain unchanged. No new AMD hardware run is available; GPU runtime
observations here use the NVIDIA RTX 5070 Ti.

## Real-renderer cache checks

Two isolated 180-second measured Carousel runs (30-second warm-up, 15 Heroic
bots, 640x480/30 FPS) completed normally with no detected runtime errors. They
ran concurrently to check correctness/resource counts, not comparative FPS or
memory cost. GLSL dumping replaced screenshot readback in these diagnostics.

| Lighting | Full-state keys | Distinct fragment sources | Duplicate compilations avoided | Linked programs | Retained source bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Off | 466 | 64 | 402 (86.3%) | 82 | 381,681 |
| On | 538 | 65 | 473 (87.9%) | 85 | 386,104 |

Counts are the final samples, including warm-up/shutdown margins. Independent
SHA-256 grouping of every dumped fragment source exactly matched the source
cache counts, and file counts matched full-state entries. Each distinct source
is compiled once; key aliases reuse that object. Percentages describe avoided
fragment compilation/object duplication in these runs, not FPS or memory savings.
The new source strings occupy about 0.36-0.37 MiB. The previous diagnostic
compiled one object per key; its different workload/resolution prevents an
honest before/after process-memory or program-count percentage claim.

This repairs the confirmed duplication, but does not prove a steady-state
memory plateau or attribute every remaining allocation. Vertex shaders and
other retained renderer resources remain outside this focused change.

## Combined validation

The shader commit's [four-platform workflow](https://github.com/chestahh/GulchCE/actions/runs/40255510975)
passed Windows, Linux, Android and web builds and configured tests; its
[MCC workflow](https://github.com/chestahh/GulchCE/actions/runs/40255510976) passed.
Android/web builds were performed in CI, not represented as local hardware runs.

Nine final visual cases passed their runtime checks. Contact sheets were
inspected for crouched Needler holders at 30 FPS, a shorter Grunt and
first-person AR at 60, Warthog and shield charge/depletion at 120, a moving door
at 60, and disabled/vertex-lit/CE controls at 60. No new visual regression was
observed in these samples. They are 640x480 still captures, not every case at
every cap, continuous animation review, or full campaign/material coverage.

Xbox c40 -> MCC Mercury Falling -> CE Death Island -> Xbox Blood Gulch
completed with lighting off at 30 FPS and on at 120 FPS. Each sequence logged
all requested map loads and exited normally without exceptions. These are
bounded render/lifecycle checks rather than proof of full map compatibility.

A normal-build two-player Windows loopback session also exited normally, with
36 host and 32 client two-player snapshots, eight scripted firing events per
peer and no detected runtime errors. Other lighting was enabled on the host
and disabled on the client; the host requested `glow_overshield` while the
client's local policy was `stock`. This supplements the unchanged policy and
admission tests, not internet crossplay or comprehensive co-op validation.

The combined review covered the complete damage lookup, source cache,
integration, diagnostics, tests and documentation. No broader refactoring was
needed. Host policy, optional lighting defaults, shader equations, AMD/NVIDIA
query delivery and map-family loading/conversion remain unchanged. These fixes
do not by themselves establish locked 120 FPS or finish the 1.0 performance
acceptance milestone.

All recorded production source hashes matched the restored sources, and the
normal Windows binary contains neither damage-owner logging nor capture/timing
fixtures. No owned game processes remain. Cleanup removed temporary maps and
generated caches after runs and another 63,314,236 bytes of diagnostic binaries
and fixture objects at completion. All 332 archived test-save files
(356,663,960 bytes uncompressed) were verified against their SHA-256 manifests.
About 55 MiB of diagnostic evidence remains; original maps/saves, toolchains
and the normal Windows build were preserved.

Local evidence includes `build/damage-owner-runtime.json`,
`build/shader-cache-runtime.json`, `build/shader-cache-counts.json`,
`build/shader-cache-final-visual.json`, the transition/network report folders,
`build/damage-shader-final-regression.log` and `build/damage-shader-cleanup.json`.
Retail data/captures are not committed. No further endurance rerun was queued.
