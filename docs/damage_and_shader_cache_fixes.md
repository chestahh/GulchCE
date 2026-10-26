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
