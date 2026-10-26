# Collision tracing and memory follow-up

This follows the [occlusion/GPU optimisation pass](dynamic_lighting_optimisation.md).
The assignment preserves current lighting quality, host policy, stock behaviour
when disabled, and off defaults. CStencil and Modern remain outside scope.

## Design and review

The feature's 1536 rays per refresh previously used the complete general
`collision_test_vector` service. A diagnostic build timed the existing collision
categories only while lighting was tracing, and counted retained shader programs,
fragment shaders, render targets and texture-cache entries once per second.
Inclusive collision categories overlap and must not be added together. These
diagnostic runs are separate from FPS acceptance.

[plasma_collision.c](../port/linux/game/plasma_collision.c) now owns a synchronous,
depth-only batch. It uses the same structure BSP and model intersection routines,
ray vectors, front/back surface flags, breakable-surface state and nearest-hit
comparison. Cluster membership, object eligibility/bounds and model instances
are reused within one batch. Parent eligibility and sphere tests still gate
children; sibling traversal remains independent. Unused material, plane and
location results are not constructed. The shared collision implementation,
simulation, settings, host-policy and map-loading code are unchanged.

Storage is fixed: 512 object records and 4096 cluster references. Overflow,
unsupported flags and malformed leaves fall back to the general query. Model
instances borrow tag/matrix pointers only during the batch; `end` clears them.
No state is reused across rendered poses, ticks or maps. The integration is a
small call-site change inside the existing feature-owned occlusion module.
No new shared-engine hook or network-version change is required.

Self-review covered eligibility, ignored owners, vehicles, parent/child/sibling
ordering, duplicate roots, cache ownership, fallback and diagnostic restoration.
The existing field resolution, ray count, trace-refresh policy, temporal fades,
light colours/radii, attenuation and shader equations are unchanged.

## Collision evidence

The user later identified DaVinci Resolve running in the background and closed
it. Earlier timings and endurance measurements below are diagnostic history,
not the final clean acceptance evidence. The paired CPU benchmark, ten-minute
memory controls and six sustained FPS cases use fresh
processes after that change. Earlier exact ray comparisons remain correctness
evidence; background contention does not turn a depth mismatch into a match.

A first interleaved comparison checked at least 55,261,436 rays in combat with
zero depth mismatches. Six focused 60 FPS comparison cases passed their runtime
checks: moving door, Warthog, shorter Grunt, disabled Needler, vertex-lit Needler
and CE Death Island. All five enabled cases recorded zero mismatches; the
disabled case correctly started no lighting batches.

The subsequent paired-batch fixture executes both paths against exactly the
same rendered pose, with identical ray construction and ordering. Clocks wrap
complete batches; comparison and logging occur outside the timed sections.
The 120-second measured combat run, including warm-up and shutdown margins in
its cumulative checker, recorded at least 79,655,424 rays across 51,859 batches
with zero mismatches. Cumulative CPU time was 23,176.870 ms for the new path and
37,205.498 ms for the original: about 38% less tracing time, or 0.447 versus
0.717 ms per batch. The accelerated path executes first, so the reference may
benefit from warmed caches. This is a paired CPU measurement, not an FPS gain
claim or proof of equivalence for every map.

The completed clean paired repeat recorded 77,265,408 rays across 50,303 batches,
again with zero mismatches. Cumulative CPU times were 23,347.742 ms accelerated
and 37,794.423 ms original: **38.2% less tracing CPU time**, or about 0.464 versus
0.751 ms per batch. These totals include warm-up/shutdown margins around the
120-second measured combat interval. The doubled-work comparison is separate
from timing-only FPS acceptance and retains the same cache-order limitation.

All 61 final focused harness/reporter tests passed, including traversal, cache reset,
missing models, ignored owners, vehicle flags, malformed leaves, overflow and
negative controls. Normal Windows and diagnostic/capture builds succeeded.
Local Ninja has no Android/web targets; their required validation uses CI.
The [four-platform build](https://github.com/chestahh/GulchCE/actions/runs/40246436679)
and [MCC validation](https://github.com/chestahh/GulchCE/actions/runs/40246436634)
both passed for production commit `feb5bc04`.
The subsequent [benchmark-tooling build](https://github.com/chestahh/GulchCE/actions/runs/40247660080)
also passed; it changes no production game source.
The broader local Windows harness run had 1,010 passes and 67 failures in
unrelated test facilities: POSIX APIs/headers, compiler-wrapper macro and
deprecation conflicts, CRLF output expectations, and linking Unix `libm`.
The successful Linux CI test step includes the full harness and touch-menu
tests. These local failures are recorded rather than counted as passes.

The test builder now refreshes the timing fixture's timestamp. Preserving the
source timestamp could otherwise reuse an object from a different diagnostic
variant. Its normal-build restoration remains in `finally`. Manifests record
the production and selected diagnostic sources. Builders' `--verify-traces`
option intentionally doubles collision work and must not be used for FPS
acceptance. `--resources` counts retained entries without per-ray clocks.

## Memory investigation

The initial three-minute diagnostic showed shader cache growth during warm-up
(469 linked programs around 30 seconds, 611 near the end). Volume-cache entries
rose from 29 to 31, with guest payload about 1.6 MB, while render targets stayed
at six. This identifies retained warm-up resources, not complete driver memory
accounting or a proof of no leak.

An attempted concurrent memory pair discovered the other local session and is
excluded. The remaining process was stopped and both runs' diagnostics retained.
The runner now assigns explicit separate loopback addresses. The corrected
memory runs are sequential, ten minutes each, with lighting disabled/enabled.
Clipboard joining is disabled and reports reject unexpected extra players.

A subsequent disabled control crashed around 218 seconds of gameplay. Its
symbolized stack runs through `projectile_collision`, `object_cause_damage`
and `object_get_and_verify_type`; the latter accessed address `0x8` while
resolving an object. No lighting batches ran in this disabled control. This
locates the failure in projectile damage, but does not establish its original
cause. The partial run is excluded from acceptance, its stack/logs retained,
and no unrelated damage or networking code was changed. The reporter now
retains early-exit reports, includes crash exceptions, and represents missing
memory windows as null rather than throwing a statistics error.

Allocation review found fixed, reused feature storage: eight visibility
fields with up to three volume textures each, eight fallback emitter slots
with up to three textures each, and one shared radial falloff texture.
`plasma_occlusion_reset` and `ammo_counter_falloff_dispose` release their
textures; `plasma_falloff_dispose` releases the radial texture. Backend texture
cache entries age out through `xgpu_texture_cache_begin_frame`. The new
collision batch uses static storage and has no heap allocation. These are
code findings, not a measurement of all graphics-driver allocations.

The existing backend's `fragment_shader_get` and `program_get` retain compiled
shader/program combinations for reuse. Its resource counters therefore help
identify cache warm-up, but do not prove that every byte of process growth
belongs to shaders. No shared renderer cache policy was changed in this pass.

The clean disabled diagnostic uses `--dump-shaders`: generated GLSL is retained
instead of periodic screenshots, avoiding their readback and large image
allocations. The enabled resource run retains its usual screenshots, so these
two diagnostic runs must not be treated as a matched on/off memory-cost delta.
Both are separate from the timing-only acceptance cases. Exact SHA-256 grouping
of captured fragment sources already demonstrates duplicate compilation under
different cache keys with lighting disabled. This is evidence of redundant
retained shader objects, not a measurement of their complete driver footprint.
Final counts, memory trends and the sustained matrix are pending.
