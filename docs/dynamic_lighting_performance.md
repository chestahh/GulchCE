# Dynamic lighting: sustained performance acceptance

This extends the short [visual acceptance runs](dynamic_lighting_acceptance.md).
The subsequent [profiling and optimisation pass](dynamic_lighting_optimisation.md)
records changes and follow-up measurements; the values here remain the original
baseline.
It measures a sustained combat workload; the earlier capture averages alone
were not evidence of sustained performance. Results are specific to the tested
hardware, quality settings and scene.

## Method

The Windows portable release configuration is `--portable --release --lto=off
--pgo=off`, based on `dynamic-lighting-test` at `409235b0`. The machine has an
Intel Core i7-14700K and NVIDIA RTX 5070 Ti. Native resolution is 2560x1440.
The test copies the user's relevant graphics preferences into an isolated
configuration: 8x MSAA, 16x anisotropic filtering, 1024 shadows, per-pixel model
lighting, highest model detail, HDR off and classic_honeycomb2 shield appearance.
The user's original configuration is read, never edited.

Each case uses one hidden game instance, fifteen Heroic bots and the scripted
local player in a Carousel Slayer match. The score limit is raised to 10,000
to prevent an early match end. Real bot movement, weapon selection, firing,
damage, deaths and respawns provide combat; no fixture forces light placement
or changes gameplay. This is a crowded multiplayer workload, not a claim that
every Covenant emitter or campaign encounter is exercised.

Each frame cap (30, 60 and 120) is compared with all nine dynamic lighting
options off and on. Animated Overshield follows the comparison switch too.
Glow Intensity remains DEFAULT. Local matches use local preferences, leaving
the existing network host-policy implementation unchanged. V-sync is disabled
to let the selected cap control pacing. Thirty seconds of warm-up precede
five measured minutes. Cases run sequentially to avoid competing for GPU time.

The [test builder](../tools/lighting_performance_build.py) installs the
[passive timing fixture](../tools/fixtures/lighting_performance.c) temporarily.
It records frame intervals using the engine's microsecond performance counter,
selected lights, scene lights and occlusion update work into a buffered CSV.
Occlusion work timing covers collision traces and visibility-field updates;
it excludes texture upload and GPU rendering. These are elapsed CPU-side times,
not GPU timer-query results. Occlusion update counts refer to the preceding
render window. A manifest records production source and executable hashes.
The builder restores all production sources and rebuilds the normal executable
in its finally block; ordinary builds contain none of this instrumentation.

The [runner](../tools/lighting_performance_run.py) samples process private
memory and working set every five seconds and captures a frame about once a
minute. Both raw pacing and pacing excluding the capture frame plus two following
frames are reported. Real stalls are retained. The 1% low is 1000 divided by
the mean duration of the slowest 1% of frames; p95/p99 are frame durations.
Per-minute results expose deterioration hidden by a single overall average.
Configuration, frame data, memory samples, bot telemetry and images are retained
with each case. Bot matches can diverge, so these are comparable workloads,
not frame-for-frame deterministic image or speed comparisons.

```sh
python tools/lighting_performance_build.py
python tools/lighting_performance_run.py --data-root <game-data> --settings <config.toml> --fps 120 --enabled 1 --label combat-120-on
```

Desktop configuration belongs beside the executable. The runner stages its own
executable and configuration, uses separate data/save paths and removes its map
copies and binaries after each run. Test saves are archived losslessly with
original relative paths and verified SHA-256 hashes before loose copies are
removed. Original game data and saves are preserved. Windows local bot matches
still require working loopback networking. The initial sandboxed/configuration
pilot is excluded from acceptance.

## Results

The six normal-quality cases completed on 25 October 2026: thirty measured
minutes of combat, plus warm-up. Values below exclude fifteen marked capture
frames per case; the raw results remain in each report. Frame budgets are
33.33, 16.67 and 8.33 ms respectively.

| Cap | Lights | Average FPS | p99 ms | 1% low FPS | Frames over 50 ms |
| --- | --- | ---: | ---: | ---: | ---: |
| 30 | Off | 29.92 | 33.44 | 23.39 | 1 |
| 30 | On | 30.00 | 33.40 | 29.78 | 0 |
| 60 | Off | 60.00 | 16.73 | 59.25 | 0 |
| 60 | On | 58.79 | 21.79 | 44.26 | 0 |
| 120 | Off | 103.68 | 14.83 | 54.53 | 3 |
| 120 | On | 84.56 | 22.28 | 41.37 | 1 |

**Steady 30 FPS is supported for this workload. Locked 60 and 120 FPS are not.**
The off result also misses 120, so the entire deficit cannot be assigned to
dynamic lighting. Different bot battles prevent treating the on/off difference
as an exact isolated cost. The 30-off run includes an 839 ms stall, and 120-off
includes three stalls over 100 ms. They occurred outside capture exclusions and
remain in the results; they do not establish that enabling lighting cures stalls.

The 120-on per-minute averages were 92.7, 98.1, 82.0, 65.0 and 85.0 FPS.
Recovery in the last minute argues against a simple continuously worsening
slowdown, but does not prove thermal stability or absence of leaks. Peak private
memory was 710.5 MiB at 30-on, 707.4 MiB at 60-on and 765.2 MiB at 120-on.
The 120-on first/last sampling windows rose from approximately 686.8 to 763.7 MiB;
120-off rose from 735.7 to 746.6 MiB. These memory windows use process elapsed
time and include startup/shutdown margins, unlike the gameplay-relative frame
window. Resource residency and longer-duration memory behaviour need separate
investigation before claiming a leak-free endurance result.

Enabled runs reached the eight-field occlusion update bound; selected lights
peaked at thirty in the 120-on case and were active in about 95% of its frames.
All off cases recorded zero selected feature lights and zero occlusion work.
Mean CPU-side occlusion update time was 3.91 ms at 30-on, 4.44 ms at 60-on and
2.85 ms at 120-on. The latter is a substantial part of an 8.33 ms budget, before
other CPU work or GPU rendering. Lower per-frame work at 120 is not evidence of
greater efficiency: scenes differ and the existing trace cache can be reused.

Bot logs confirm ongoing combat, kills and respawns in every measured minute.
Every case exited normally with no logged assertion, exception or shader error.
The six contact sheets were inspected, including disabled controls at every cap.
They show intact geometry, weapon/character rendering, projectiles and combat
effects, with no obvious corruption in the sampled frames. Sparse stills do not
prove continuous temporal smoothness, every damage colour, or every emitter's
appearance; the focused visual cases remain documented in milestone 2.

### Follow-up and validation limits

A seventh five-minute diagnostic kept native resolution and lighting enabled,
but disabled MSAA in its isolated configuration. It averaged 84.87 FPS, with
22.29 ms p99 and a 39.97 FPS 1% low; occlusion updates averaged 3.17 ms.
Thus disabling MSAA alone did not demonstrate a remedy for this workload.
Different combat sequences still prevent an exact MSAA cost comparison.
Its captures show no obvious rendering corruption. The case is retained as
`diagnostic-120-aa-off`, separate from the normal-quality matrix.

Keep performance acceptance open at 60/120. Profile the CPU occlusion update
and GPU lighting passes separately, then repeat the same sustained matrix after
any optimisation. In particular, retain the existing corner/chassis blocking
and smooth transitions when reducing update work; simply lowering the budget
without visual regression checks is not an accepted solution.

This assignment adds reproducible measurement tools and evidence, with no
production rendering change. Review covered fixture ownership, source restoration,
timing boundaries, capture exclusions, process lifetime and cleanup. The small
fixture stays outside ordinary build discovery; its temporary hooks collect
data without changing light selection, simulation or host policy. No speculative
production refactor was justified by these measurements alone. Lighting remains
isolated and off by default. CStencil and Modern remain outside scope.

The evidence lives locally under `build/lighting-performance/baseline-{cap}-{off,on}`:
`report.json`, `frames.csv`, `memory.json`, settings/build manifests, logs and
native-resolution PNGs. Retail maps and screenshots are not committed. The
instrumented Windows build and restored normal Windows build both succeeded.
Cross-platform and network behaviour are unchanged; this local performance
workload does not repeat or replace the milestone 2 platform, host-policy,
two-peer and map-family transition checks.

Final validation passed all three reporter regression tests, local documentation
link checks and production-source hash restoration checks. The normal executable
does not contain the timing CSV marker. Review found no further necessary
production changes within this measurement assignment.

Cleanup removed staged maps, generated map caches, per-run executables/DLLs,
the dedicated measurement binary and its leftover object. The 337 inactive
test-save files (200.2 MiB loose) were archived with paths and verified hashes.
Approximately 204.6 MiB of raw results, logs, native captures and compressed
saves remains for diagnosis; `build/m3-cleanup.json` records the final inventory.
No benchmark process remains. Original data, user saves, earlier evidence and
the normal Windows build were preserved.
