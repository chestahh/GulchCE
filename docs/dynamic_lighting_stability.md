# Reduced dynamic-lighting stability check

The approved 20-minute measured combat plan completed at revision
`14c1ebe1ab3d8c0371b245131276378596b45e45`: five minutes each at 30 and 60 FPS,
then ten minutes at 120 FPS. All three processes exited normally with no
detected runtime errors. The earlier, larger matrix was not executed.

30 and 60 FPS were sustained in this workload. **120 FPS is not accepted as
locked**, and memory is not proven to have reached a plateau. This pass makes
no production changes and does not complete the 1.0 performance milestone.

## Method and boundaries

The runs used the Windows timing build, an NVIDIA RTX 5070 Ti (driver 617.42),
2560x1440, 8x MSAA, 16x anisotropic filtering, 1024 shadowmaps, per-pixel
lighting, highest model detail and 90-degree world/viewmodel FOV. HDR and HDR
output were off. Each fresh process ran Carousel with 15 Heroic bots and a
scripted player, with a 30-second warm-up before its measured interval.

Runs were sequential in hidden windows with isolated data/save folders.
Resolve/Fusion and other game instances were absent at preflight. No builds
or harness tests ran concurrently with the measurements. The environment was
not otherwise continuously controlled.

All optional dynamic-lighting settings were enabled only in these isolated
sessions. The build collected frame timing and aggregate occlusion timing;
profiling, per-ray diagnostics, shader-resource diagnostics and trace
verification were disabled. Process memory was sampled every five seconds.
Periodic screenshots excluded three marked frames per capture from the
reported pacing; raw timings remain available. Memory measurements include
capture allocations and do not attribute allocations to a subsystem.

There was no matched lighting-off control or deterministic replay. Differences
between caps or earlier sessions cannot be attributed solely to the recent
fixes, and this pass does not measure their FPS or memory savings.

## Frame pacing

| Cap | Measured seconds | Average FPS | 1% low FPS | p99 frame ms | Worst frame ms | Frames over 100 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 30 | 299.97 | 30.00 | 29.80 | 33.406 | 34.252 | 0 |
| 60 | 299.98 | 60.00 | 58.41 | 16.751 | 27.577 | 0 |
| 120 | 599.99 | 116.30 | 64.01 | 13.140 | 412.305 | 2 |

Neither the 30 nor 60 FPS run had a measured non-capture frame over 50 ms.
The 120 FPS run's per-minute averages ranged from 114.60 to 118.34 FPS;
1.66% of its measured frames exceeded 1.5 times the frame budget.

The two 120 FPS stalls were consecutive, at frame-log times 408.965 and
409.187 seconds (including warm-up), lasting 412.305 and 221.453 ms. Both
were unmarked for capture exclusion and remain in the results. Their aggregate
occlusion times were 191.428 and 3.052 ms respectively, with six updates each.
The first therefore overlaps a large occlusion timing sample, but these wall
times alone do not distinguish expensive tracing, scheduling stalls or other
causes. No causal attribution is made from this run.

Mean occlusion time was 2.952, 2.890 and 1.555 ms at the three respective caps.
The maximum selected-light counts were 26, 27 and 26; the eight-update-per-frame
occlusion limit held in every run. Lower mean per-frame occlusion time at a
higher cap is not by itself evidence of lower total tracing cost.

## Process memory

Values below are process private bytes in MiB. Early means cover process
seconds 90-150; late means cover 240-300 for the five-minute runs and 540-600
for the ten-minute run. These fixed windows precede shutdown and use the
process sampler's clock, not the frame-log clock.

| Cap | Early mean MiB | Late mean MiB | Change MiB | Peak MiB |
| --- | ---: | ---: | ---: | ---: |
| 30 | 645.92 | 644.48 | -1.44 | 657.93 |
| 60 | 644.24 | 644.86 | +0.62 | 656.94 |
| 120 | 641.93 | 647.07 | +5.14 | 650.33 |

The 120 FPS run's private-byte regression slope over process minutes 5-10
was approximately +0.89 MiB/minute. No large runaway increase was observed
within the approved interval, but the modest upward trend prevents a claim
of flat memory or leak-free operation. These measurements do not identify
whether the retained memory is useful caching, fragmentation or a leak.

## Preservation, review and validation

This is validation of the existing [damage-owner and shader-cache fixes](damage_and_shader_cache_fixes.md),
not another renderer change. Production source hashes matched the diagnostic
manifest and approved plan, and `source/`, `port/` and `tools/` had no diff from
the recorded revision. The normal Windows binary was restored without the
timing fixture. Lighting defaults, host policy, AMD/NVIDIA visibility-query
paths and Xbox/CE/MCC boundaries remain unchanged.

The reporter's four tests passed before the runs. Final report checks verified
three complete intervals, normal exits, no reported runtime errors or
unexpected network players, enabled test lighting and bounded occlusion
updates. Contact sheets from all three runs were inspected: sampled combat
showed coloured lighting with no obvious new visual regression. Still images
do not establish continuous animation quality or exhaustive material coverage.

The preceding fix report records 128 regression tests, four-platform CI,
map-family transitions and a two-player host-policy smoke test. That evidence
applies to the unchanged implementation; it is not represented as newly rerun
here. This pass used NVIDIA hardware only and did not repeat AMD hardware,
Xbox/CE/MCC transition, internet crossplay or comprehensive co-op tests.

The final review covered the measurement setup, capture exclusions, memory
windows, source restoration and report limits. No production refactoring was
needed for this validation-only assignment. The remaining performance question
is the 120 FPS shortfall and its stalls; memory plateau remains unproven.
No additional endurance runs are queued.

## Evidence and cleanup

Local evidence is under `build/lighting-performance/stability-reduced-30-on`,
`stability-reduced-60-on` and `stability-reduced-120-on`, with the approved plan,
aggregate results, summary and cleanup report in `build/lighting-stability-*`.
Retail captures and test data are not committed.

No owned game processes remained at cleanup. Each runner removed its staged
maps, caches and executable copies. Final cleanup removed another 42,107,741
bytes of diagnostic executables/configuration and the fixture object, including
the unused resource-diagnostic build prepared for the larger plan. All 111
archived test-save files (62,968,332 bytes uncompressed) were verified against
their SHA-256 manifests. About 108 MiB of useful evidence remains (59 files),
including logs, timings, captures and save archives. Original maps/saves,
toolchains and the normal Windows build were preserved.

## Focused stall and memory investigation

A subsequent investigation used the same production code at `d84409ac`, on
the same NVIDIA hardware, resolution, quality settings and Carousel bot
workload. It ran eight measured minutes with lighting on, then four with it
off, sequentially, each with a 30-second warm-up. Both exited normally without
detected runtime errors or unexpected players. No further endurance matrix
was launched.

The Windows-only test builder now accepts `--stalls`, recording rendering
thread CPU time and shader compilation/linking time (including status-query
synchronization), alongside `--resources` counts. Resource diagnostics also
count vertex-layout objects, samplers, mirror buffers and framebuffers.
The runner's `--no-captures` disables image allocations and readback without
dumping shaders. These are diagnostic measurements, not clean FPS acceptance
runs. No GPU query path or production behaviour was changed.

### Findings

| Lighting | Measured minutes | Average FPS | p99 frame ms | Worst frame ms | Frames over 100 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| On | 8 | 115.98 | 12.978 | 25.092 | 0 |
| Off | 4 | 119.91 | 9.900 | 51.229 | 0 |

The original 412/221 ms pair did not reproduce. The enabled run still missed
the sustained 120 FPS target even without large hitches, so those two issues
must not be conflated. The runs have different combat histories and durations;
their averages are not a precise measurement of lighting cost.

The disabled run recorded a 51.229 ms frame at frame-log time 267.881 seconds.
It included one shader compilation (10.977 ms) and one program link
(36.805 ms), with 46.875 ms of thread CPU time. Thus a newly encountered
shader/program caused a concrete hitch independently of optional lighting in
this sample. This does not establish the cause of the earlier, much larger
pair. CPU accounting is coarse (15.625 ms steps observed); it is useful for
large stalls, not a precise per-frame CPU budget.

The enabled run performed no further shader compilation or program linking
after frame-log second 120. From then until the end, programs stayed at 89,
distinct fragment sources at 78 and retained source text at 530,663 bytes.
Full-state key entries increased from 547 to 592: 45 additional structs,
about 12 KiB before allocator overhead, based on the current 32-bit layout.
Those aliases do not compile more fragment shaders. Render targets (6),
vertex-layout objects (10), samplers (29), mirror buffers (4), framebuffers
(11) and immediate-vertex capacity (512) stayed constant throughout the
measured enabled interval. Texture entries varied between 226 and 290;
their guest payload was lower at the end than at the start. Counts and guest
payload do not measure driver-internal allocations.

| Lighting | Early mean MiB | Late mean MiB | Change MiB | Process peak MiB |
| --- | ---: | ---: | ---: | ---: |
| On | 645.88 | 649.51 | +3.64 | 652.23 |
| Off | 645.16 | 645.15 | -0.00 | 674.17 |

Early memory windows are process seconds 90-150. Late windows are 420-480
for enabled and 180-240 for disabled; peaks include startup and the post-measurement
tail before exit. Private memory
fluctuated by several MiB, including downward steps, rather than increasing
monotonically. The enabled run's fitted slope over process seconds 240-480
was +1.52 MiB/minute. Comparing the same early/180-240 windows gives +1.48 MiB
enabled and approximately zero disabled. The shorter control cannot establish
whether later growth is feature-specific.

The control's flat comparison windows are not a flat whole session: private
memory rose from 647.08 MiB at process second 275.23 to 673.91 MiB at 280.25,
then stayed high until exit (674.17 MiB at 295.25). This late approximately
27 MiB jump was near the compile/link hitch; the frame and process logs have
different origins. Compiler/driver allocation is therefore a concrete target
for an allocation trace, but temporal proximity does not prove ownership.
It also cannot explain the enabled run's later growth without compilation.

Removing captures did not remove the enabled run's upward trend. The later
increase was not caused by new shader compilation/linking, and the measured
cache entries do not account for its magnitude. Allocation ownership remains
unresolved: these observations distinguish neither allocator retention from
a leak nor engine allocations from driver allocations. The lighting collision
batch itself has fixed storage. Also, Windows write tracking deliberately
uses protected-page faults, so the earlier process fault-count spike is not
evidence of disk paging by itself.

The useful next memory diagnostic is an allocation-stack trace spanning an
early and late window, rather than another broad FPS matrix. The concrete
compile/link hitch provides a separate target for evaluating shader warm-up;
no prewarming, asynchronous compilation, eviction or rendering changes are
justified as a fix for the original pair by this evidence alone.

### Tool correction, validation and cleanup

The old fixture marked periodic capture slots even when the screenshot
directory was empty. The game correctly skipped image allocation/readback,
but the reporter still excluded those frames. The builder now checks the
directory before marking captures, and the reporter retains all frames when
captures are disabled, including old binary markers. A regression test keeps
a real 200 ms hitch in a marked slot with captures disabled.

Both investigation runs used the same diagnostic binary, built before that
marker guard was corrected. Capture-free summaries were recomputed from all
raw frames and saved separately as `capture-free-pacing.json`; no stall was
filtered away and no rerun was needed for that reporting correction.

The final combined review covered diagnostic ownership, clock boundaries,
cache traversal, source restoration and capture-free reporting. All five
reporter tests passed; the final Windows diagnostic build and restored normal
Windows build succeeded. Production source hashes matched their manifests,
`source/` and `port/` were unchanged, and the normal binary contains no stall
fixture. These Windows diagnostic tools are not compiled into Android/web
or shipped in normal builds. No fresh non-Windows, AMD hardware, CE/MCC
transition or network run is claimed; unchanged implementation evidence from
the preceding fix report remains applicable. Host policy, off defaults and
AMD/NVIDIA and Xbox/CE/MCC paths remain unchanged.

Evidence is in `build/lighting-performance/stall-investigation-120-1` and
`stall-investigation-120-0`, with `build/lighting-stall-summary.json`, build/test
logs and the cleanup report. All owned game processes exited. The runners
removed staged maps/caches and per-run binaries; final cleanup removed another
21,072,779 bytes of diagnostic executable/configuration and fixture object.
All 74 archived save files (41,978,888 bytes uncompressed) passed SHA-256
verification. About 10.4 MiB of evidence remains in 32 files. Original maps,
saves, toolchains and the normal Windows build were preserved.
