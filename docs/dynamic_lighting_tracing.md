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

All 60 focused harness/reporter tests passed, including traversal, cache reset,
missing models, ignored owners, vehicle flags, malformed leaves, overflow and
negative controls. Normal Windows and diagnostic/capture builds succeeded.
Local Ninja has no Android/web targets; their required validation uses CI.

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
Their results and the final sustained matrix are pending.
