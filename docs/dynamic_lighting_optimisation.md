# Dynamic lighting performance follow-up

This follows the [first sustained matrix](dynamic_lighting_performance.md).
The target is less redundant work with unchanged visual quality, defaults and
host policy. Field size (16 cubed), 1536 rays per refresh, eight admitted fields,
trace invalidation, temporal fades, light radii/colours and shader equations are
unchanged. CStencil and Modern remain outside scope.

## Profile and changes

`tools/lighting_performance_build.py --profile` adds CPU timers around tracing,
field reprojection, texture generation/upload and model binding. Binding time
includes any of the other operations it invokes and must not be added to them.
The test-only GPU fixture wraps draws in `GL_TIME_ELAPSED` queries every 32nd
frame. It polls availability before reading and skips sampling if its bounded
ring fills, rather than synchronising the CPU with the GPU. Query semantics are
from the [Khronos timer-query specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt);
the fixture is independently implemented. GPU rows classify other draws (0),
models with selected lights (1), selected world diffuse passes (2), and selected
specular passes (3). Model timing includes the complete material draw, not just
the additional lighting instructions. Query overhead can perturb rendering;
the sustained matrix uses a separate build without GPU queries or phase timers.

Two 120-second profiles after 30 seconds of warm-up used native 2560x1440,
the same quality settings as the first matrix, and fifteen Heroic Carousel bots.
Before optimisation, mean trace/field/texture CPU times were 3.63/0.64/0.32 ms
per rendered frame. Sampled GPU model/diffuse/specular draws averaged
0.217/0.118/0.075 ms per sampled frame; other draws averaged 4.44 ms.
This identifies CPU tracing as a larger target than changing lighting shaders.

Three conservative changes remove repeated work:

- `plasma_occlusion.c` prepares history coordinates once per axis rather than
  per cell. It keeps the exact coordinate arithmetic, sample accumulation order
  and out-of-range fallback. Existing collision and fade logic is retained.
- `plasma_occlusion_gl.c` resolves uniforms once per linked program and uses
  one immutable sampler instead of repeatedly looking up uniforms, assigning
  sampler units and setting texture parameters on each model draw. A small
  record in the backend's program entry is necessary because the backend owns
  shader linking and lifetime; a feature-global cache keyed only by GL names
  would be unsafe if names were reused. The feature owns the implementation.
- Equal diffuse/specular radii now share the same attenuation/visibility texture
  upload. Their expressions are identical, including directional masks. Other
  radii retain separate textures, and per-view invalidation still applies.

The follow-up profile measured 2.09/0.39/0.25 ms for trace/field/texture work.
It also had fewer selected lights and refreshes, so neither those frame averages
nor the higher profile FPS establish an isolated speedup. Field CPU time per
update fell from about 0.107 to 0.086 ms in these samples; scenes still differ.
Sampled GPU model/diffuse/specular draws averaged 0.057/0.093/0.077 ms, with
3.26 ms in other draws. No lower-quality shader or reduced sampling budget was
introduced to obtain these numbers.
These profiles precede the final identical-texture sharing change; sustained
and final regression checks validate the combined implementation.

## Acceptance

The combined implementation is `e9fe5090`. Final self-review covered the complete
CPU, GL binding, integration, profiling and test changes, including allocation
fallback, cache ownership, per-view invalidation and teardown. No changes to
`source/`, settings defaults, host-policy code, network data or map loaders were
needed. There is no network-version change.

The normal and capture/timing-only Windows release builds passed. All 58 focused
regression tests passed, including exact history-sample equivalence, equal versus
different specular extents, allocation fallback, program-owned uniform locations,
reserved texture units and the existing fade/door/chassis/activation contracts.
Android and web targets are absent from the local Ninja configuration; their
builds require CI and are not represented as local successes.

All 21 final focused visual cases passed their runtime assertions and frame-cap
checks. At 30/60/120 FPS they cover crouched Needler holders, a shorter Grunt,
first-person AR lighting, Warthog chassis blocking, a moving door and enhanced
overshield charge. Additional 60 FPS cases cover disabled lighting, vertex-lit
models and CE Death Island. Contact sheets for all cases were inspected:
spill remains attached to the holders, the distant floor beneath them stays
dark, chassis blocking remains visible, and green/pink shield lighting follows
the animation. These are sampled 640x480 diagnostic captures, not a guarantee
of every frame or every map. Sustained tests use native 2560x1440 separately.

The initial door attempts combined incompatible fixture modes and spawned no
projectiles. The initial CE attempt used a stale fixture filename. Those four
attempts are excluded, corrected and superseded by the final cases. The capture
runner now accepts both the original CE map filename and its historical fixture
name, without changing any engine loading path.

A normal-build two-peer Windows smoke test completed with 37/33 two-player
snapshots and eight scripted firing events per peer, without logged assertions,
exceptions or shader errors. The host requested glow/overshield while both peers'
local shield preferences were off; other dynamic lighting was on for the host
and off for the client. This supplements the policy harness and unchanged policy
source; it is not internet crossplay or simulation-equivalence proof.

Xbox c40 -> MCC Mercury Falling -> CE Death Island -> Xbox Blood Gulch
transitions passed with lighting off at 30 FPS and on at 120 FPS. Each run
completed all three switch commands, logged the MCC load and CE conversion,
returned to Blood Gulch and exited without exceptions. These are bounded
render/lifecycle checks, not full campaign compatibility claims.

The [platform workflow](https://github.com/chestahh/GulchCE/actions/runs/40240939785)
passed Linux, Windows, Android and web builds and their configured checks.
The separate [MCC validation workflow](https://github.com/chestahh/GulchCE/actions/runs/40240939807)
also passed. Visual observations remain the Windows GPU captures described above.

## Sustained follow-up

The matrix uses the same native 2560x1440 setup as the first pass: an Intel
Core i7-14700K and NVIDIA RTX 5070 Ti, 8x MSAA, 16x anisotropic filtering,
1024-pixel shadows, per-pixel lighting and highest model detail. Each cap has
one disabled control and one enabled run, with fifteen Heroic bots and a
scripted player in Carousel. Each run has thirty seconds of warm-up followed
by five measured minutes. Runs are sequential, with no competing builds or
other test instances. Original player settings and saves are not modified.

The timing-only build was produced before the implementation commit was made;
its manifest therefore names the parent commit. All seven recorded production
source/header hashes match `e9fe5090`, and its `profile` field is false. The
normal executable has been restored without the test instrumentation.

The report retains raw timing and separately excludes the fixture's explicitly
marked screenshot frames from its pacing summary. Other stalls remain in the
reported results. Bot combat is not an identical replay, so comparisons with
earlier runs describe observed workloads, not an isolated percentage speedup.

| Cap | Lighting | Average FPS | p99 frame time (ms) | 1% low FPS | Frames over 50 ms | Peak private memory (MiB) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 30 | Off | 30.00 | 33.532 | 29.55 | 0 | 701.6 |
| 30 | On | 30.00 | 33.539 | 28.71 | 0 | 719.6 |
| 60 | Off | 60.00 | 16.828 | 56.53 | 0 | 699.0 |
| 60 | On | 59.98 | 19.528 | 45.09 | 0 | 724.2 |
| 120 | Off | 119.63 | 10.298 | 86.86 | 0 | 717.6 |
| 120 | On | 105.96 | 16.387 | 52.80 | 1 | 765.5 |

Each measured span is approximately 300 seconds. Fifteen marked capture frames
are excluded per run; raw summaries remain in each report. The 120-on case's
60.298 ms stall is retained, and no case recorded a frame over 100 ms. Bot logs
show continued combat, kills and respawns. All six cases exited normally with
no logged assertions, exceptions or shader errors. All six native-resolution
contact sheets were inspected: geometry, weapons, characters and combat effects
remain intact, with no obvious corruption. Sparse stills cannot establish
continuous temporal smoothness; the focused visual matrix supplements them.

The earlier enabled runs averaged 30.00/58.79/84.56 FPS at 30/60/120 caps, with
p99 times of 33.402/21.788/22.282 ms. This follow-up demonstrates better observed
60/120 throughput and tails, but the disabled 120 control also improved from
103.68 to 119.63 FPS. Different scenes and execution conditions prevent
attributing the complete difference to these code changes.

Thirty FPS remains supported by this workload. Sixty FPS is now sustained on
average, but its 19.53 ms p99 and 45.09 FPS 1% low leave frame-pacing acceptance
open. At 120, enabled per-minute averages were 100.95, 109.67, 109.02, 104.66
and 105.51 FPS: there is still a material gap to the target. Do not mark
Milestone 3 or the 1.0 performance baseline fully accepted from these results.

Disabled cases recorded zero selected feature lights and zero occlusion work.
Enabled cases reached the eight-field limit, selected up to 27/29/26 lights
at 30/60/120 respectively, and selected lights in 96.9/98.5/95.3 percent of
frames. Mean CPU occlusion updates were 5.01/4.57/2.46 ms. Those averages depend
on scene content and cache reuse; lower per-frame cost at 120 does not establish
better efficiency. The earlier phase profiles identify collision tracing as
the next area to investigate, without reducing ray coverage or changing fades.

Memory also needs longer-duration investigation. The 120-on process's first
and last sampled minute averaged 706.0 and 762.2 MiB private memory, compared
with 699.4 and 715.3 MiB for its disabled control. These windows include
startup/shutdown margins and are not the gameplay timing window. Both enabled
and disabled runs grew, and neither allocation provenance nor a steady-state
plateau has been established. This is not a leak-free endurance claim.

## Evidence and cleanup

Local evidence is under `build/lighting-performance/optimised-{cap}-{off,on}`
and `profile-{before,after}-120`: frame/phase/GPU CSVs where applicable,
settings and build manifests, memory samples, reports, logs and PNG captures.
The combined sustained report is `build/lighting-optimisation-sustained.json`;
the visual, network and transition reports use the same optimisation prefix.
Retail data and captures are not committed.

Final audit verified all recorded production hashes, restored normal-build
instrumentation state, local documentation links, six full run durations,
disabled-work counters and the eight-field bound. The implementation's final
58-test regression run and CI results above apply to the unchanged production
commit; subsequent edits only record acceptance evidence.

Cleanup removed staged maps and generated map caches after each run. The final
audit removed 247 remaining disposable binaries, fixture objects and superseded
captures (131.8 MiB), preserving the earlier attempt logs and contact sheets.
All 618 archived inactive test-save files (about 1060 MiB loose) were verified
against their path/hash manifests. About 327 MiB of evidence and compressed
saves remains; `build/lighting-optimisation-cleanup.json` records the inventory.
No test game remains running. Original maps/saves, previous milestone evidence,
toolchains and the normal Windows build are preserved.
