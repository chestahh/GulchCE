# E5 map-load preparation and combat performance

The original 1024 E5 target was signed off on 24 November 2026 for Windows playtest
`0ca83059d` on the i7-14700K and RTX 5070 Ti. Across four foreground
Two Betrayals runs at 1440p with Hard and Soft 1024, the slowest of 480
one-second windows was 274 FPS. The hidden-window Auto controls recorded
three isolated seconds below 240 in Hard and two in Soft. Sustained 240 FPS
was the accepted 1024 target.

E5 closed on 25 November 2026. In the final snowy Two Betrayals comparison,
fixed Hard 1024 averaged 267.22 FPS and Soft 1024 averaged 271.15 FPS. Each
used all eight local slots and cleared 240 in every measured second of its
two-minute Windows run. Fixed 2048 averaged 249.37 FPS in Hard and 242.74 FPS
in Soft, with dips lasting up to 7 and 32 seconds respectively. Sustained 240
at fixed 2048 and the sustained 360 FPS stretch goal are deferred.
[Matched combat measurements](#matched-1024-and-2048-combat-dips)
[Foreground measurements](#fresh-windows-launches) Ã‚Â· [Hidden-window controls](#repeatable-and-indoor-controls)

Modern Hard and Soft apply when a gameplay map loads. The menu may save a new
mode during play. The current level and checkpoint reverts keep the mode that
loaded with the map. Classic remains the default. This change governs Shadow
Mode in each menu style. The existing host lighting policy still applies.

## Fixed 2048 combat

The fixed 2048 investigation tested Hard and Soft independently in the snowy
Two Betrayals fight on the reference Windows PC. Its targets were sustained
240+ FPS (4.17 ms/frame) and a stretch goal of 360+ FPS (2.78 ms/frame). Keep all eight local
slots available, Shared flashlight, independent sun and fixture budgets, full
dynamic lighting, the sun fade and close-player shadow detail. Use fixed
2048 throughout each run with adaptive quality off. Retain fixed 1024
as the performance and visual regression control. Pursue a gain there too.
Each optimisation must remove work from a path shared by the supported
resolutions. Use 2048 to expose scaling costs and run matched before/after
at both 1024 and 2048. Report a flat or inconclusive result when a gain at
1024 cannot be measured. The experimental 4096 tier uses the same code.
Its frame rate is outside this qualification.

Alternate reference and candidate in repeated fresh launches. Report full-run
and heavier-minute FPS, the number of seconds below each target, the longest
continuous stretch and individual frame hitches. Check the same scoped
passes at both resolutions with asynchronous GPU timers and CPU phase
timings. Confirm any saving with the ordinary passive benchmark. Retain the
Death Island player silhouette, M1 peripheral foliage and Mercury Rising II
fade checks, plus thin contacts, cutouts and moving casters in matched views.

### Reopening baseline, 24 November 2026

The corrected radial playtest on the working tree based on `f2e1a5294`
used the same 19-enemy Xbox encounter, fixed camera, 1440p, AA off, P-cores
and hidden window for these capture-free runs. Each reached eight local
slots including the Shared flashlight. Both new 2048 modes used the same
passive timing executable. The earlier Soft 2048 run used the same
instrumented source inputs. Live combat still varies between launches.

| Fixed shadow mode | Two-minute average FPS | Heavier minute FPS | Slowest second FPS | Seconds below 240 / longest streak | Seconds below 360 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 control | 265.6 | 262.7 | 227 | 1 / 1 | 120 |
| Soft 1024 control | 266.6 | 265.1 | 246 | 0 / 0 | 120 |
| Hard 2048 | 211.5 | 211.2 | 187 | 120 / 120 | 120 |
| Soft 2048 | 213.2 | 210.1 | 183 | 120 / 120 | 120 |
| Earlier Soft 2048 | 216.7 | 214.8 | 199 | 117 / 64 | 120 |

The fresh runs require 0.52 to 0.56 ms less per frame on average to reach
240 FPS. In each mode's slowest second the gap is 1.18 to 1.30 ms per
frame. At 360 FPS the corresponding gaps are 1.91 to 1.95 ms on average
and 2.57 to 2.69 ms in the slowest second. These are elapsed frame-time
budgets. CPU submission and GPU execution can overlap. Local evidence is
in `build/e5-2048-baseline/baseline.json` and the individual
`build/lighting-performance/e5-reopen-*-2048/report.json` files.

Separate 60-second Soft profiles used the same checkpoint and camera. GPU
queries sampled 485 frames at 1024 and 393 at 2048. Values below are GPU
milliseconds per sampled frame. The environmental row covers the timed model
depth pass only.

| GPU pass | Soft 1024 | Soft 2048 | Increase |
| --- | ---: | ---: | ---: |
| Local cube setup, clears and copies | 0.082 | 0.131 | 0.049 |
| Local world geometry depth | 0.110 | 0.302 | 0.192 |
| Local world-space model depth | 0.170 | 0.612 | 0.442 |
| Environmental model depth | 0.268 | 0.421 | 0.153 |

Local model depth draws averaged 134 and 133 per sampled frame respectively.
Start by identifying which models, material passes and cube faces account
for that GPU time. Look for redundant depth draws or coverage that can be
excluded exactly. Preserve animated cutouts, changing poses and off-camera
blockers. Apply the same rule at every tier, compare full-resolution shadow
depth and matched final frames, then time Hard and Soft at both resolutions.
The 0.442 ms increase measures scaling cost. A candidate's recoverable
frame time needs its own A/B measurement.

Alongside depth work, profile the shared BSP lighting and renderer paths.
CPU submission of diffuse and specular BSP draws totalled about 0.83 ms
per frame at both tiers. World and cluster ray queries averaged 0.48 and
0.56 ms. These CPU measurements can include driver waits and overlap GPU
work. The existing OpenCE Classic, GulchCE Classic and lighting-only
controls help identify which changes also benefit lower resolutions.
Local profile evidence is in `build/e5-2048-baseline/depth-comparison.json`.

### Local model draws and shared BSP lighting submission, 24 November 2026

An isolated Windows experiment tested two changes on the same 19-enemy snowy
checkpoint. The first tightened the existing cube-face test with bounds for
the vertices influenced by each bone in a model part. The second uploaded a
complete BSP lighting index list on its first eligible slice when the list
was at most 1,024 bytes. This byte limit was identical at every shadow
resolution. Diffuse and specular draws kept their material order.

The NVIDIA correctness runs at Hard 1024 and Soft 2048 marked 29,652 of
1,468,453 candidate model-face draws for omission. The GPU
replayed 29,198 of them with depth comparison forced to pass and depth writes
disabled. None produced a sample. GPU readback also matched the original
indices in all 44,620 checked material slices. These checks cover this
encounter. They do not establish final-image equivalence across all maps.

Each mode and resolution ran for two minutes twice. Four variants rotated
in five-second blocks inside the same fight. The second launch reversed the
assignments so the reference and combined variant each saw the same encounter
intervals, as did the two individual variants. The first second of each
block was excluded for transitions, leaving 48 measured seconds per variant.
All runs reached eight local slots and used sunlight, full dynamic lighting,
the Shared flashlight, and a fixed 1440p view.

| Mode | Reference FPS | Tighter bounds FPS | Earlier index upload FPS | Both FPS | Seconds below 240, reference / both |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 265.6 | 265.3 | 262.6 | 262.9 | 0 / 1 |
| Soft 1024 | 266.6 | 265.6 | 265.5 | 263.3 | 0 / 1 |
| Hard 2048 | 214.2 | 210.9 | 211.4 | 211.6 | 48 / 48 |
| Soft 2048 | 208.8 | 210.0 | 208.5 | 212.3 | 48 / 48 |

The following frame-time differences average six matched 20-second rounds
across the two orderings. Each individual effect averages over the other
change being enabled and disabled. Negative values mean faster frames.

| Mode | Tighter bounds | Earlier index upload | Both against reference |
| --- | ---: | ---: | ---: |
| Hard 1024 | +0.000 ms | +0.038 ms | +0.039 ms |
| Soft 1024 | +0.023 ms | +0.024 ms | +0.047 ms |
| Hard 2048 | +0.032 ms | +0.023 ms | +0.055 ms |
| Soft 2048 | -0.057 ms | -0.022 ms | -0.079 ms |

Defer both changes. Together they removed roughly 69Ã¢â‚¬â€œ72 BSP index uploads
per frame and about three local model-face draws. The Soft 2048 gain was
0.079 ms in the paired rounds, with a descriptive interval ranging from a
0.177 ms gain to a 0.010 ms loss. Both 1024 modes slowed, and every sampled
2048 second fell below 240 FPS with every variant. Keep the current renderer.
The next candidate is a separate depth-only path for opaque model parts.
Measure them against alpha-tested parts first, then check exact depth and
thin silhouettes before keeping any shader change.

Counts, one-second results and paired round estimates are recorded locally in
`build/e5-face-batch/comparison.json`. The per-frame inputs are in
`build/lighting-performance/e5-face-batch-*/`.

### Opaque and alpha-tested local model draws

In the same snowy Xbox checkpoint, asynchronous GPU timers sampled one in 32
frames during a 60-second combat window after 25 seconds of warm-up. Each
transition between draw categories started a timer in the existing draw order.
The categories use the alpha texture actually bound by the
[local shadow renderer](../../port/linux/src/modern_shadows_gl.c).
Only local model depth draws are counted here. All four runs reached eight
local slots, including the Shared flashlight, with environmental sunlight active.

| Setting | Sampled frames | Opaque draws | Alpha-tested draws | Opaque GPU ms/frame | Alpha-tested GPU ms/frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 481 | 0 | 63,125 | <0.001 | 0.174 |
| Hard 2048 | 388 | 10 | 50,686 | <0.001 | 0.601 |
| Soft 1024 | 478 | 10 | 62,294 | <0.001 | 0.167 |
| Soft 2048 | 383 | 20 | 50,152 | <0.001 | 0.607 |

The draw columns are totals across sampled frames. GPU times are averages
over all sampled frames, including those with zero opaque draws. Defer the
opaque shader A/B. Even eliminating every measured opaque draw would save
less than 0.001 GPU ms per frame on average in this encounter. The experiment
changed profiling only and supplies no new sustained-FPS or visual qualification.

For model shaders, `material` in the [caster selection code](../../port/linux/game/modern_shadows.c)
uses the material flags and the active bitmap permutation. The depth shader
tests the base texture against 127/255 alpha. The timers measure that draw
path, including vertex and raster work. They do not measure how many texels
fail the test or how much time the texture lookup alone takes. The next useful
measurement is to group the expensive draws by their bound texture and check
whether any can be proved fully opaque at upload time. Keep the existing
cutout path for unresolved textures and those with genuine holes.

Local evidence is in `build/e5-opaque-depth/profile-comparison.json` and the
per-run `gpu.csv.zip` files in `build/lighting-performance/e5-opaque-profile-*/`.

### Ranking local model depth draws by base texture

The 1440p Windows Two Betrayals profile grouped asynchronous GPU timers by
model part and bound base-texture upload, in submission order. Soft 1024
and 2048 used the same 19-enemy checkpoint. Readback inspected every texel
of the base level and the channel mapped to alpha. The shadow sampler uses
linear filtering at that level with repeating edges. If every source texel
clears the cutoff, interpolation also clears it. The experiment required
a small margin above 127/255 and renewed the proof after a texture upload.
The tested passing textures had minimum alpha 1.0.

| Base texture | GPU ms/frame at 1024 | GPU ms/frame at 2048 | Cutoff result |
| --- | ---: | ---: | --- |
| cyborg | 0.1801 | 0.6202 | Always passes |
| plasma pistol | 0.0290 | 0.0291 | Always passes |
| jackal head | 0.0048 | 0.0049 | 5,773 texels at or below cutoff |
| jackal legs | 0.0024 | 0.0025 | Always passes |
| jackal torso | 0.0024 | 0.0024 | Always passes |
| jackal arms | 0.0017 | 0.0017 | Always passes |

Eighteen base textures passed at each resolution. Besides the Jackal head,
Elite teeth (1,447 texels) and the shotgun ammunition bitmap (3,793 texels,
sampled at 1024) need the alpha test. The single most expensive cyborg mesh
part accounted for 0.609 GPU ms/frame at 2048. Timing includes its
geometry and raster work and the overhead of splitting timers at part boundaries.

The candidate used a depth-only program with the same vertex shader for
proven model draws. All other draws used their original path. Replaying
the same posed draw lists through both variants produced identical 32-bit
depth for 256 world-receiver cube faces across Hard and Soft at both resolutions
(671,088,640 texels). 22 faces included certified geometry and genuine
cutouts together. These captures cover the tested Xbox encounter on NVIDIA.

For each setting, two hidden two-minute launches used opposite A/B assignments
in four 30-second blocks (ABBA, BAAB). Excluding the first second of every
block left the same 116 encounter seconds for each variant. Sunlight stayed
active and all runs reached eight local slots with the Shared flashlight.

| Setting | Reference FPS | Candidate FPS | FPS gain, forward / reversed | Seconds below 240, reference / candidate | Slowest second, reference / candidate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 253.4 | 258.8 | +6.3 / +4.6 | 18 / 11 | 82 / 231 |
| Soft 1024 | 260.1 | 262.5 | +3.0 / +1.8 | 2 / 1 | 230 / 225 |
| Hard 2048 | 213.5 | 239.7 | +26.2 / +26.3 | 115 / 53 | 196 / 227 |
| Soft 2048 | 209.4 | 238.2 | +30.4 / +27.1 | 116 / 64 | 186 / 218 |

Carry this candidate forward. All four settings gained average FPS in both
launch orders. At 2048, Hard gained 26.2 and 26.3 FPS and Soft gained 30.4
and 27.1. Every matched 29-second interval improved at 2048. The longest
observed below-240 stretch fell from 29 to 5 seconds in Hard and from 29
to 12 in Soft. Sustained 240 remains open. The Hard 1024 reference had an
82-FPS second with 11 frames above 16.7 ms. Its measured gain was 5.5 FPS,
or 4.0 FPS after excluding that second from both variants.

The isolated prototype inspected uploaded GPU textures on first use. The
production implementation computes its opacity proof during upload.

Local evidence includes `build/e5-alpha-ranked/ranked-draws.csv`,
`ranked-textures.csv`, `ranking.json` and `ab.json` in the same directory.
The per-run depth comparisons and timing inputs are under
`build/lighting-performance/e5-ranked-*/`.

### Upload-time opacity for local model shadows

The shadow sampler filters the base texture level. When every texel has full
alpha, every filtered sample passes the cutoff. An opaque model part can then
use the same skinned vertices and write depth without sampling the texture.
For example, the player armour takes this path while a Jackal head with
transparent texels keeps the sampled shader.

[Texture upload](../../port/linux/src/xbox_textures.c) owns the pixels and
knows the uploaded format and channel order. Its `base_level_opaque` checks
the actual DXT blocks or converted pixels once while uploading. The proof
belongs to that texture entry. A dirty upload clears it before any early exit.
Palette variants, replacements and remapped alpha channels retain sampling.
An [MCC entry](../../port/linux/src/mcc_texture_bridge.c) records the same
proof when its checked upload succeeds and releases it with the map.

[Local depth submission](../../port/linux/src/modern_shadows_gl.c) reads this
flag only for a textured model draw. If texture lookup changes before the
draw list is rendered, that draw uses the sampled shader. Both paths share
the same pose, cube faces, depth projection and eight-slot selection.

On the Windows RTX 5070 Ti, a separate GPU comparison replayed the same
caster poses through both shaders. All 671,088,640 depth texels across 256 world
cube faces matched bit for bit in the snowy Xbox Two Betrayals encounter.
A Hard host and Soft client also matched on 71 sampled faces across Xbox,
CE and MCC in the Rat Race, Death Island, Digsite and Blood Gulch rotation.
These checks covered world and first-person receivers.

Separate capture-free runs measured 120 seconds after a 25-second warmup at
2560 x 1440 on performance cores. Each run replayed the same 19-enemy combat
save, kept environmental shadows active and reached eight local shadow slots
with the flashlight sharing the budget. The heavier minute is the slower
of the two consecutive 60-second intervals.

| Mode | Average FPS | Heavier minute FPS | Slowest second FPS | Seconds below 240 / 120 | Longest stretch below 240 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 272.7 | 271.2 | 254 | 0 | 0 s |
| Soft 1024 | 261.0 | 258.6 | 239 | 1 | 1 s |
| Hard 2048 | 238.0 | 237.2 | 209 | 72 | 19 s |
| Soft 2048 | 238.0 | 235.7 | 207 | 64 | 6 s |

A second Soft 1024 run averaged 271.8 FPS. The slower minute averaged
269.0 FPS and the slowest second recorded 230 FPS. It spent
2 of 120 seconds below 240, with a longest stretch of 2 seconds.

Neither mode sustained 240 FPS at fixed 2048 in those runs. The raw timing
reports and depth comparisons are under `build/lighting-performance/e5-ranked-upload-*/`.
The consolidated results are in `build/e5-upload-opacity/results.json`. AMD
hardware and native Linux gameplay were outside this Windows run.

### Where fixed 2048 drops below 240 FPS

In the passive runs above, Hard's longest stretch below 240 ran from second
78 up to second 97. Soft had two six-second stretches, starting at seconds
22 and 42. Only 32 one-second windows fell below 240 in both replays.
Track each run's own slow windows because live combat differs between launches.

Four fresh Windows profiles used those same settings, checkpoint and fixed
camera. The CPU timers measure elapsed time on the render thread. The GPU
timers sampled every 32nd frame and associated each result with its submission
frame. Each replay kept the sun active and reached eight local shadows with
the Shared flashlight. Times below are GPU milliseconds per sampled frame.

| Setting | Sampled frames | Local world depth | Local model depth | Environmental model depth |
| --- | ---: | ---: | ---: | ---: |
| Hard 1024 | 909 | 0.178 | 0.090 | 0.339 |
| Hard 2048 | 850 | 0.363 | 0.199 | 0.392 |
| Soft 1024 | 915 | 0.175 | 0.089 | 0.340 |
| Soft 2048 | 825 | 0.361 | 0.205 | 0.400 |

World geometry in the local lights' depth maps had the largest measured
increase among these depth passes. A second pair of 2048 profiles split
world draws by whether they actually used an alpha texture. Draws without
one took 0.367 GPU ms/frame in Hard and
0.364 in Soft. Alpha-tested world geometry took
less than 0.001 in each. A focused A/B can route the already-opaque world
draws through the local depth-only program while preserving projection,
polygon offset, cube faces and draw order. Compare the resulting depths
before timing it. The measured pass time sets an upper bound on the
work available there.

The strongest repeatable GPU increase within each 2048 replay was in the
regular draws submitted by the [late transparent pass](../../source/rasterizer/rasterizer_transparent_geometry.c).
Comparing the 30 slowest seconds with the 30 fastest, these draws went from
0.243 to 0.333 ms/frame in Hard
and 0.265 to 0.359 in Soft.
The cost also rose by 0.034 and
0.046 ms when the windows were
chosen from the independent passive runs. Average draw counts in the fast
and slow quarters were 100 and 103 in Hard and
104 and 111 in Soft.
Rank the queued materials and measure screen coverage before changing
submission. Those timers cover a mix of draws and do not identify a
particular effect. The GPU time for decals fell in the slow quarter of
both replays. The GPU cost of local world depth was comparatively steady.

A final pair of replays timed the CPU stages in
[render_window](../../source/render/render.c) with one record per second and
no per-draw timers or GPU queries. Each stage record matched the actual
number of completed frames in the same second. The slower of the two
60-second intervals supplies the heavier-minute result.

| Setting | Average FPS | Heavier minute FPS | Slowest second FPS | Seconds below 240 / longest stretch in seconds |
| --- | ---: | ---: | ---: | ---: |
| Hard 2048 | 239.3 | 238.4 | 222 | 56 / 6 |
| Soft 2048 | 234.5 | 232.3 | 217 | 90 / 17 |

In the same runs, both object rendering and lighting the map geometry
cost more render-thread time in the below-target seconds.

| CPU stage (ms/frame) | Hard at or above 240 | Hard below 240 | Soft at or above 240 | Soft below 240 |
| --- | ---: | ---: | ---: | ---: |
| Draw objects | 1.184 | 1.236 | 1.136 | 1.185 |
| Diffuse and specular world lights | 0.855 | 0.894 | 0.901 | 0.939 |

Lighting visibility updates can run inside these stages. The four detailed
profiles recorded zero shader compiles or links during the measured fight.

Use the passive and low-overhead runs for frame pacing. The detailed
profilers change it appreciably. CPU stage times include driver calls and
can overlap GPU execution. The captured pass costs do not establish an
achievable FPS gain. The experiment changed instrumentation only.
The reproducible one-second data and scripts are in
`build/e5-low-windows/`, with the original traces under
`build/lighting-performance/e5-low-*/`.

### Transparent materials and opaque world depth

The Windows RTX 5070 Ti survey timed every 32nd frame of the late transparent
pass. It attributed draws to the shader tag, or to the bound base bitmap for
widgets. An independent replay counted distinct passing screen pixels per
material. It copied the live depth and stencil, replayed the original draws
into a private mask, and counted each marked pixel once per material per
sampled frame. This measures raster coverage after depth, stencil and
shader discard tests. Blending can still give a passing pixel zero visible
colour. Each material received 142 or 143 sampled frames per mode. One
sampled draw in each coverage replay fell outside the 32 ranked materials.

| Material | GPU ms/frame, Hard / Soft | Distinct screen area, Hard / Soft | Passing samples including overlap, Hard / Soft |
| --- | ---: | ---: | ---: |
| glow | 0.077 / 0.092 | 1.51 / 1.45% | 12.45 / 18.36% |
| snow flakes_alpha | 0.051 / 0.051 | 94.57 / 95.25% | 339.88 / 335.72% |
| smoke warthog gun | 0.049 / 0.046 | 38.65 / 36.99% | 76.86 / 72.77% |
| needler contrail | 0.014 / 0.017 | 0.12 / 0.12% | 0.11 / 0.36% |

`rasterizer\glow` is a shared bitmap. Its 22Ã¢â‚¬â€œ26 draws per frame together cost
the most GPU time. Snow took one draw and had the largest screen footprint.
The smoke bitmap had the largest increase from the fastest to the slowest
quarter of each replay, 0.019 ms in Hard and 0.007 ms in Soft. A bitmap name
identifies the resource used and can represent several effects.

The isolated candidate selected the existing [opaque depth program](../../port/linux/src/modern_shadows_gl.c)
for untextured BSP draws into local light cubes at submission. The same-pose
comparison covered all six faces and all eight primary cubes in every setting.
All 1,006,632,960 depth values matched bit for bit, including 691,548,388
populated texels. The sampled faces included 36 with alpha-tested casters.
Separate first-person cubes were absent from the captured comparisons.

The passive comparison used fixed 1440p output, Shared flashlight, sun,
fixtures and all eight local slots. Every 120-second run covered 3,600
simulation ticks from the same snowy combat core with 19 enemies. Combat
timing varied across launches. The second 2048 pair ran in the opposite order.

| Mode and resolution | Pair | Average FPS, control Ã¢â€ â€™ candidate | Heavier minute FPS, control Ã¢â€ â€™ candidate | Seconds below 240, control Ã¢â€ â€™ candidate | Longest stretch in seconds, control Ã¢â€ â€™ candidate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 1 | 269.6 Ã¢â€ â€™ 277.1 | 268.2 Ã¢â€ â€™ 276.1 | 0 Ã¢â€ â€™ 0 | 0 Ã¢â€ â€™ 0 |
| Soft 1024 | 1 | 272.2 Ã¢â€ â€™ 275.0 | 271.5 Ã¢â€ â€™ 274.1 | 0 Ã¢â€ â€™ 1 | 0 Ã¢â€ â€™ 1 |
| Hard 2048 | 1 | 240.1 Ã¢â€ â€™ 248.4 | 237.5 Ã¢â€ â€™ 246.9 | 46 Ã¢â€ â€™ 4 | 6 Ã¢â€ â€™ 2 |
| Hard 2048 | 2 | 244.1 Ã¢â€ â€™ 246.5 | 242.6 Ã¢â€ â€™ 244.1 | 16 Ã¢â€ â€™ 11 | 3 Ã¢â€ â€™ 2 |
| Soft 2048 | 1 | 242.2 Ã¢â€ â€™ 245.2 | 240.8 Ã¢â€ â€™ 241.5 | 38 Ã¢â€ â€™ 21 | 5 Ã¢â€ â€™ 4 |
| Soft 2048 | 2 | 238.9 Ã¢â€ â€™ 246.8 | 238.0 Ã¢â€ â€™ 244.9 | 57 Ã¢â€ â€™ 11 | 7 Ã¢â€ â€™ 2 |

Four separate 60-second GPU profiles alternated shader selection in 32-frame
blocks, using control, candidate, candidate, control. Timers sampled one
frame per block. These timings include instrumentation. The passive table
above supplies the frame-rate comparison.

| Setting | Opaque world GPU ms/frame, control Ã¢â€ â€™ candidate | Opaque world draws/frame, control Ã¢â€ â€™ candidate | Local depth submission CPU ms/frame, control Ã¢â€ â€™ candidate |
| --- | ---: | ---: | ---: |
| Hard 1024 | 0.174 Ã¢â€ â€™ 0.044 | 502.3 Ã¢â€ â€™ 502.8 | 0.030 Ã¢â€ â€™ 0.029 |
| Soft 1024 | 0.174 Ã¢â€ â€™ 0.045 | 502.2 Ã¢â€ â€™ 502.9 | 0.030 Ã¢â€ â€™ 0.028 |
| Hard 2048 | 0.375 Ã¢â€ â€™ 0.080 | 502.8 Ã¢â€ â€™ 502.4 | 0.030 Ã¢â€ â€™ 0.031 |
| Soft 2048 | 0.364 Ã¢â€ â€™ 0.079 | 502.7 Ã¢â€ â€™ 503.1 | 0.031 Ã¢â€ â€™ 0.030 |

Both 2048 pairs improved in each mode. Hard spent 15 of 240 candidate
seconds below target, and Soft spent 32. Both modes still had dips at fixed 2048. The 1024 results have one passive pair per mode.

The branch now uses the opaque program for untextured world geometry in local
shadow maps. Alpha-tested world materials retain their discard test. The sun
uses its existing program. A Windows NVIDIA two-player run with a Hard host
and Soft client cycled through Xbox Rat Race, CE Death Island, MCC Digsite and
Xbox Blood Gulch. Both peers rendered all four rounds and agreed on a player
respawn in each. Paired GPU readbacks of 247 faces, including 116 first-person
faces and 83 with cutout casters, matched across all
258,998,272 depth values. This sampled comparison used 1024. AMD hardware
remains unverified.

The local evidence is in `build/e5-transparent-world/` under
`material-ranking.csv`, `depth-results.json`, `performance.csv`, `scoped.json`
and `opaque-world.patch`. The per-peer comparisons are in
`build/e5-world-glow/family-depth.json`.

### Glow and smoke submission

The late transparent pass was sampled every 32nd frame in the snowy Xbox
Two Betrayals fight at 1440p. At 2048, all 24,332 sampled Hard glow draws
and 20,271 Soft glow draws came from `light_volume_render`. Each draw
already grouped about 18 sprites. Around a quarter immediately followed a
glow draw with matching sampled render state and vertex constants. The
ordering, per-volume projection, blending and stencil still matter. Separate
timing of the full light-volume render callback gave 0.071 ms per frame in Hard and 0.070 ms in Soft. Immediate
draw submission accounted for 0.026 ms of that in each mode. The final
index and draw calls accounted for about 0.007 ms.

The Warthog gun smoke bitmap averaged 1.02 draws per frame in each mode.
Its mean GPU cost rose from 0.020 to 0.129 ms between the smallest and
largest screen-coverage quarters in Hard, and from 0.012 to 0.116 ms in
Soft. Coverage counts passing samples, including overlapping sprites.
CPU draw submission averaged 0.0026 ms per frame in both modes.

An isolated [renderer](../../port/linux/src/d3d8_gl.c) experiment uploaded
the sequential quad indices once and reused a prefix when issuing otherwise
identical triangle draws. A GPU buffer readback matched all 253,260
reference indices across the 290 distinct quad counts sampled. Each setting
ran twice from the same combat checkpoint. Five-second control and candidate
blocks followed an ABBA pattern, inverted on the second launch. Each column
covers 120 one-second windows per path across the two launches.

| Setting | Average FPS, original Ã¢â€ â€™ reusable indices | Seconds below 240, original Ã¢â€ â€™ reusable indices |
| --- | ---: | ---: |
| Hard 1024 | 274.2 Ã¢â€ â€™ 277.1 | 0 Ã¢â€ â€™ 0 |
| Soft 1024 | 276.6 Ã¢â€ â€™ 279.8 | 0 Ã¢â€ â€™ 0 |
| Hard 2048 | 241.3 Ã¢â€ â€™ 242.8 | 36 Ã¢â€ â€™ 33 |
| Soft 2048 | 250.1 Ã¢â€ â€™ 250.0 | 7 Ã¢â€ â€™ 8 |

The gain repeated at 1024 and in Hard 2048. Soft 2048 gained 1.55 FPS in
one launch and lost 1.68 in the other. An initial Hard 2048 launch started
128 simulation ticks earlier than the comparison route and was excluded.
Its replacement started at tick 927 and ran for 3,600 ticks.
Four additional full Soft 2048 runs compared fixed submission paths in both
orders. Reuse increased average FPS from 247.8 to 253.6 and from 246.1 to
248.0 in the reversed pair. Seconds below 240 fell from ten to three in the
first pair and stayed at fifteen in the second. All four reached eight local
shadow slots.

The desktop renderer now creates one 192 KiB element buffer during
`gl_initialize`. `draw_sequential_quads` uses the prefix for the requested
number of quads. Draws with supplied indices keep their original ordering.
A failed buffer allocation or an oversized list uses the original streamed
indices. GLES keeps its streamed and rebased paths.

The following uninterrupted 120-second replays measured the opaque world
shader with the original quad submission, before integrating reusable indices. Each replay used the
same 19-enemy combat core, advanced 3,600 simulation ticks, and reached eight
local shadows with the flashlight sharing the budget.

| Setting | Average FPS | Heavier minute FPS | Seconds below 240 | Longest below 240 |
| --- | ---: | ---: | ---: | ---: |
| Hard 1024 | 275.9 | 274.6 | 0 | 0 s |
| Soft 1024 | 271.5 | 270.7 | 0 | 0 s |
| Hard 2048 | 243.2 | 243.0 | 32 | 4 s |
| Soft 2048 | 244.5 | 243.1 | 19 | 3 s |

Both 1024 runs sustained 240 FPS in every one-second window. The two
2048 runs averaged above target, but both had consecutive seconds below it.
The local evidence is in `build/e5-world-glow/`. `submission.json` and
`particle-cpu.json` contain the GPU and CPU profiles. `quad-performance.json`
and `soft-qualification.json` contain the controlled comparisons.
`branch-pacing.json` records the original-submission baseline.

The production implementation was replayed against the streamed indices on the
same GPU draw in Hard and Soft. The probe copied the current colour, depth
and stencil attachments before drawing, then compared the two outputs across
the entire target. All 47 pairs matched, including nine visibly contributing
draws per material per mode. The two-peer Hard/Soft cycle and the separate CE
Off/Hard run also passed with the final renderer.

| Final renderer | Average FPS | Heavier minute FPS | Seconds below 240 | Longest below 240 |
| --- | ---: | ---: | ---: | ---: |
| Hard 1024 | 280.8 | 279.9 | 0 | 0 s |
| Soft 1024 | 273.8 | 273.3 | 0 | 0 s |
| Hard 2048 | 249.4 | 247.8 | 6 | 1 s |
| Soft 2048 | 248.8 | 246.7 | 6 | 1 s |

The Windows NVIDIA measurements use the same fixed 1440p fight and 120
one-second windows per setting. `accepted-pacing.json` records the final
frame pacing and `framebuffer-parity.json` records the GPU comparisons.
AMD hardware and native Linux runtime performance remain unverified.

### Shared rendering time in slow combat windows

Two fresh uninterrupted 1440p replays of the existing renderer used the same
snowy Two Betrayals combat core, 19 enemies, sun fade and Shared flashlight.
Each reached eight local shadow slots. Hard 2048 averaged 244.3 FPS and spent
18 of 120 seconds below 240, with a longest stretch of two seconds. Soft
2048 averaged 246.7 FPS and also spent 18 seconds below, with a longest
stretch of five seconds. The earlier six-second counts and these repeats use
the same tracked production input hashes in their benchmark manifests.

Separate Hard and Soft launches sampled CPU submission and asynchronous GPU
timestamps every sixteenth frame. Around combat ticks 2066Ã¢â‚¬â€œ2096 (the
38-second dip in the earlier runs), object submission took an additional
0.108 ms per frame in Hard and 0.122 ms in Soft compared with adjacent
above-target windows. Diffuse and specular world-light submission together
rose by 0.173 and 0.204 ms respectively. Late transparent GPU time changed by
+0.045 ms in Hard and Ã¢Ë†â€™0.007 ms in Soft. The earlier ten-second dip varied
between launches. Each pass interval includes its nested work, and CPU and
GPU intervals can overlap.

Across all below-target seconds of the instrumented runs, object submission
increased by 0.081 ms in Hard and 0.053 ms in Soft. World-light submission
increased by 0.058 and 0.106 ms. Local shadow depth GPU time changed by
+0.014 ms in Hard and less than 0.001 ms in Soft. These measurements place
the recurring extra cost in visible-object and world-light work. They do
not identify a single cause for every dip.

The desktop [texture binding](../../port/linux/src/d3d8_gl.c) experiment kept
an existing binding when the generated shader had no sampler on that stage.
A sampled stage requesting texture zero still issued the required unbind.
The paired GPU check replayed the same draw against identical framebuffer
contents and compared RGBA, depth and stencil across the 2560Ãƒâ€”1440 target.
All 336 sampled pairs matched across Hard 2048, Soft 2048 and a stock-effects
control with the lighting enhancements off. Each setting included eight
visibly contributing glow draws and eight smoke draws. All 24 visible glow
draws exercised a retained binding. The sampled smoke draws used the same
bindings on both paths. The linked-program audit found no active sampler
on any stage classified as unread during these runs.

Both paths used the same executable. Five-second blocks alternated in ABBA
order, inverted on the second launch, for 120 measured seconds per path at
each setting. The last column counts five-second combat intervals in which
the candidate beat the control at the same point in the other launch.

| Setting | Average FPS, control Ã¢â€ â€™ candidate | Seconds below 240, control Ã¢â€ â€™ candidate | Faster intervals |
| --- | ---: | ---: | ---: |
| Hard 1024 | 278.2 Ã¢â€ â€™ 277.9 | 0 Ã¢â€ â€™ 0 | 11/24 |
| Soft 1024 | 279.3 Ã¢â€ â€™ 279.5 | 0 Ã¢â€ â€™ 0 | 12/24 |
| Hard 2048 | 248.4 Ã¢â€ â€™ 248.2 | 14 Ã¢â€ â€™ 9 | 13/24 |
| Soft 2048 | 249.5 Ã¢â€ â€™ 248.6 | 15 Ã¢â€ â€™ 11 | 11/24 |

A separate CPU-only sampling pass timed the work inside the object and
world-light functions. On sub-240 seconds, visible model draws rose from
1.153 to 1.201 ms per Hard frame but fell from 1.075 to 1.025 ms in Soft.
Collision ray batches rose from 0.592 to 0.664 ms in Hard and fell from
0.631 to 0.565 ms in Soft. The two runs had 14 and six below-target seconds
respectively. Both modes submitted more BSP light passes during the slow
seconds. Hard went from 64.0 to 70.8 diffuse and specular calls per frame,
and Soft from 63.9 to 68.3. CPU time inside those BSP material draws rose
from 0.330 to 0.357 ms and from 0.331 to 0.351 ms respectively. Surface
selection and triangle-list building were timed separately.

We then held one binding path fixed for the entire 120-second replay. Each
mode had two control and two candidate runs, in control/candidate
order for the first pair and candidate/control order for the second. The
last column gives total seconds below 240 and the longest consecutive stretch.

| Setting | Pair | Average FPS, control Ã¢â€ â€™ candidate | Heavier minute FPS, control Ã¢â€ â€™ candidate | Below 240, total / longest (s), control Ã¢â€ â€™ candidate |
| --- | ---: | ---: | ---: | ---: |
| Hard 2048 | 1 | 248.39 Ã¢â€ â€™ 249.97 | 246.82 Ã¢â€ â€™ 247.91 | 6 / 2 Ã¢â€ â€™ 5 / 1 |
| Hard 2048 | 2 | 253.14 Ã¢â€ â€™ 248.30 | 251.43 Ã¢â€ â€™ 246.71 | 1 / 1 Ã¢â€ â€™ 4 / 2 |
| Soft 2048 | 1 | 245.25 Ã¢â€ â€™ 244.90 | 243.64 Ã¢â€ â€™ 244.37 | 16 / 3 Ã¢â€ â€™ 19 / 2 |
| Soft 2048 | 2 | 255.09 Ã¢â€ â€™ 250.36 | 253.82 Ã¢â€ â€™ 249.22 | 0 / 0 Ã¢â€ â€™ 5 / 2 |

The desktop texture-binding change remains an experiment. Its full-frame
gain did not repeat across the two orders and settings. The existing renderer
still clears the 240 FPS floor at 1024 in these comparisons. At 2048 the
average clears 240, with dips in both Hard and Soft. The 360 FPS target
remains open. We subsequently measured repeated material setup
inside the [diffuse and specular BSP light draws](../../source/rasterizer/xbox/rasterizer_xbox_environment.c)
at the same combat ticks. The comparison below covers exact matches in
shader inputs, texture state and constants with all light contributions and
the original pass and draw order.

The local evidence is in `build/e5-tick-pacing/`. `windows.json` aligns
the earlier and repeated slow seconds to simulation ticks. `same-run.json`
and `components.json` contain the sampled stage and component timings.
`comparison.json` records both alternating orders and the GPU pixel checks.
`fixed-comparison.json` records the uninterrupted replays. The GPU
comparisons cover sampled draws on the Windows RTX 5070 Ti.

### Material setup inside BSP light draws

In the snowy Two Betrayals fight, the renderer revisited the same shader,
bitmap index and vertex-buffer type on 91% of diffuse and 95% of specular BSP
light draws within a frame. Including the sampled texture state, vertex and
pixel shader inputs, UV transform and five light constants reduced those
figures to 25â€“26% and 31â€“32%. These are input matches. Each light still
contributes separately to the image. The profilers sampled material setup time
and state equality on different frames.

Before triangle submission, the material calls averaged about 0.017 ms per
frame for diffuse lighting and 0.025 ms for specular lighting at 2048. Together
they cost about 0.002 ms more in the below-240 seconds than in faster seconds
in both modes. At simulation ticks 2066â€“2096, combined setup took 0.053 ms in
Hard and 0.044 ms in Soft, compared with 0.045 and 0.043 ms in the adjacent
above-target seconds. Both timing runs stayed above 240 FPS in the seconds
overlapping those ticks. These CPU timings include the sampling overhead.

The [shared renderer](../../port/linux/src/d3d8_gl.c) normally expands eighteen
packed pixel-shader colours into float vectors when draw-uniform inputs change.
The comparison reused those exact values per linked program, converting an
active bank only when its input changed. Vertex transforms, textures, passes
and draw order followed the same route. Stock particles and glow used their
original assets, blend state and sampling.

| Mode | Pair | Average FPS, control â†’ candidate | Heavier minute FPS, control â†’ candidate | Seconds below 240, control â†’ candidate | Longest stretch in seconds, control â†’ candidate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 1 | 282.37 â†’ 277.55 | 281.63 â†’ 277.50 | 0 â†’ 0 | 0 â†’ 0 |
| Hard 1024 | 2 | 263.83 â†’ 267.32 | 263.32 â†’ 263.90 | 5 â†’ 0 | 2 â†’ 0 |
| Hard 2048 | 1 | 248.48 â†’ 247.38 | 245.58 â†’ 244.07 | 14 â†’ 21 | 4 â†’ 6 |
| Hard 2048 | 2 | 251.12 â†’ 249.52 | 249.17 â†’ 248.41 | 3 â†’ 6 | 1 â†’ 2 |
| Soft 1024 | 1 | 274.42 â†’ 280.40 | 273.05 â†’ 279.89 | 0 â†’ 0 | 0 â†’ 0 |
| Soft 1024 | 2 | 276.94 â†’ 277.87 | 276.90 â†’ 276.71 | 0 â†’ 0 | 0 â†’ 0 |
| Soft 2048 | 1 | 247.41 â†’ 246.50 | 246.61 â†’ 243.96 | 10 â†’ 14 | 1 â†’ 2 |
| Soft 2048 | 2 | 228.85 â†’ 236.39 | 227.43 â†’ 234.32 | 92 â†’ 67 | 25 â†’ 21 |

Each cell comes from a fresh two-minute replay at 2560Ã—1440 on the Windows RTX
5070 Ti. Each pair used the same checkpoint, nineteen enemies, shared
flashlight and sun fade, reached eight local shadow slots, and reversed launch
order on the repeat. A below-target second contains fewer than 240 rendered
game frames in the harness interval.

The same-draw GPU check found matching colour, depth and stencil across all 579
sampled pairs at Hard and Soft 1024/2048 and with the lighting enhancements
off. These included 40 visibly contributing smoke draws and 40 glow draws. In
these five diagnostic runs the candidate converted 33.4â€“46.4% fewer colours.
GPU readbacks and diagnostic counters were confined to the visual-check build.

The packed-colour cache stays experimental. Soft 1024 gained average FPS in
both orders. Hard 2048 lost average FPS and spent more seconds below 240 in
both orders. Soft 2048 reversed its result between pairs, with both paths
slower in the second pair. During its control run, a three-second sample
recorded about 1.4 logical CPU cores of browser activity. That observation
alone cannot establish the reason for the difference between pairs. Neither
2048 mode met the sustained target across these replays. All 16 matched runs
spent every measured second below 360 FPS. The indexed-draw follow-up appears below.

The local evidence is under `build/e5-bsp-material/`. `survey.json` and
`tick-windows.json` hold the profile counts and timings. `fixed.json` gives
each launch and `pixels-summary.json` records the per-material comparisons.

One additional Soft 1024 candidate replay advanced 3,566 simulation ticks. It
had frame intervals of 739.2 and 519.1 ms. Its trace is in `unmatched.json`.
The table uses its repeat, which met the established 3,600-tick route with a
two-tick tolerance. The cause of the two stalls is unknown.

### Indexed map-light draws in the snowy encounter

The Windows renderer submits separate diffuse and specular draws for illuminated
map surfaces. At Soft 2048 the game requested the same index buffer on almost
every draw. The existing state cache already reduced those requests to about
13 OpenGL element-buffer binds per frame. Most vertex bindings genuinely changed.

In the Soft 2048 upload audit, 92.3% of repeated writes were at most 512
bytes. A 64-entry candidate reused the GPU offset after an exact length and
byte comparison. Reallocating the ring invalidated its offsets. Eight isolated
tests covered adjacent and sparse draws, changed contents, collisions, buffer
rebinding, wrapping and a new context.

The visual probes recorded 3,939,527 successful reuses during their sampled
intervals. All 635 paired draws matched in colour, depth and stencil at
2560x1440, including 40 visible smoke and 40 visible glow draws.
The checker also compared the bytes at each sampled GPU index offset with the
list requested by the game. None of the 42 sampled Hard 1024 diffuse draws
changed a visible pixel. Contributing diffuse draws were verified in the other
four conditions. The stock control recorded zero cache hits.

Timed builds ran the same 19-enemy Two Betrayals checkpoint on the RTX 5070 Ti.
Both builds reached eight local slots with the shared flashlight, fixtures and
sunlight enabled. Each column combines two 120-second runs per implementation,
with the launch order reversed. A below-target second averaged under 240 FPS.
The streak is the longest continuous run of those seconds within one replay.

| Mode | Reference / candidate average FPS | Lowest minute FPS | Seconds below 240 out of 240 | Longest streak (s) |
| --- | ---: | ---: | ---: | ---: |
| Hard 1024 | 272.6 / 268.3 | 270.3 / 265.2 | 0 / 0 | 0 / 0 |
| Soft 1024 | 270.7 / 269.9 | 266.1 / 267.8 | 1 / 0 | 1 / 0 |
| Hard 2048 | 246.0 / 243.2 | 244.0 / 239.4 | 32 / 71 | 4 / 6 |
| Soft 2048 | 247.8 / 245.9 | 241.9 / 240.2 | 39 / 60 | 5 / 5 |

In a separate same-executable check, the implementation alternated in five-second
blocks, then reversed for matching blocks of the replay. The first second of
each block was excluded to keep switches out of the samples.
Soft 2048 gained in 9/24 blocks, with a mean difference of -2.30 FPS.
Hard 2048 gained in 14/24 blocks, with a mean difference of +1.26 FPS.

Separate timer replays measured the two map-light GPU passes per frame.
CPU and GPU intervals overlap. GPU elapsed time can include gaps in submission.
GPU coverage counts samples that passed the depth and stencil tests.

| Mode | GPU ms in below / at-target seconds | Index-upload CPU ms, below / at-target | Vertex / element bind CPU ms per frame |
| --- | ---: | ---: | ---: |
| Hard 2048 | 0.892 / 0.783 | 0.038 / 0.036 | 0.030 / 0.0008 |
| Soft 2048 | 0.839 / 0.771 | 0.036 / 0.034 | 0.032 / 0.0008 |

At combat ticks 2066 through 2096, the two GPU passes took 0.785 ms in
Hard and 0.890 ms in Soft. The nearest above-target seconds on either
side measured 0.778 and 0.812 ms respectively. This fixed interval
showed a larger difference in Soft than in Hard.

The reported bind times include the measuring clocks. After subtracting
back-to-back timer overhead, the vertex estimates were 0.003 to 0.006 ms
and the element estimates were close to the timer floor. The GPU draw
ranking and covered samples are joined by frame, pass and submission time.
Full per-material results, per-launch pacing and visual comparisons are in
the local evidence directory `build/e5-index-submit/`.

The specular pass on the exterior metal floor ranked first in both GPU
profiles. In Hard it accounted for 25.0% of sampled map-light draw time
and 40.2% of passing samples. In Soft the figures were 25.5% and 40.4%.
Those percentages describe sampled map-light draws. The complete GPU frame
also includes models, shadow generation and other passes.

The index-content cache stays experimental. Hard lost average FPS in
both orders at both resolutions. Both 2048 modes spent more time below
240 across the two fixed candidate replays. Every measured second of
the 16 fixed replays was below 360 FPS. These tests used Windows and
the RTX 5070 Ti. Other GPUs and map families were outside this comparison.

A deferred experiment would measure how many expensive map-light
fragments leave the destination colour untouched. If finite light bounds
explain those pixels, test a conservative per-light screen rectangle.
Use the full viewport for lights without a proven bound and verify
identical colour, depth and stencil at the edge before timing it.

### Matched 1024 and 2048 combat dips

Four archived control runs per setting shared the same tracked renderer inputs.
Grouping frames by 30 simulation ticks identified two intervals, 1680 to 1709
and 2340 to 2369, where at least two of four replays fell below 240 FPS in
each 2048 mode. All corresponding 1024 replays stayed above target.
In the archived profile using the adopted opaque world-depth path, local model
depth had the largest repeatable GPU increase among measured stages. The
1024-to-2048 difference averaged 0.117 ms in Hard and 0.115 ms in Soft across
the two intervals. Each estimate had three or four sampled frames per setting
per interval. An earlier independent profile also showed this model cost rising
in all four comparisons. These observations do not identify every combat dip.

One candidate used the distance to the existing posed model bounds to order
local shadow draws from near to far. World casters, unknown bounds and ties
retained their original ordering. The depth test could reject hidden fragments
earlier while every part still used its original mesh, pose and material.
Across 192 reordered cube faces in the snowy Xbox checkpoint, including 83 with cutouts,
all 503,316,480 sampled 32-bit depth values matched the original order.

Fresh Windows RTX 5070 Ti runs used 2560x1440 and the same 19-enemy combat
checkpoint with sunlight, fixtures and the Shared flashlight. Every replay
reached eight local slots. Values below show original / near-first order.
Each 1024 setting has one 120-second run per build. Each 2048 setting has two,
with the launch order reversed. The lowest minute and longest dip are taken
within a single run. A dip is a run of seconds averaging below 240 FPS.

| Mode | Average FPS | Lowest minute FPS | Seconds below 240 | Longest dip (s) |
| --- | ---: | ---: | ---: | ---: |
| Hard 1024 | 267.2 / 264.8 | 265.2 / 261.4 | 0 / 4 | 0 / 1 |
| Soft 1024 | 271.1 / 267.3 | 269.9 / 267.2 | 0 / 1 | 0 / 1 |
| Hard 2048 | 249.4 / 240.8 | 246.5 / 233.2 | 41 / 112 | 7 / 21 |
| Soft 2048 | 242.7 / 248.8 | 234.2 / 241.7 | 107 / 31 | 32 / 7 |

Keep the current draw order. Soft 2048 improved in both launch orders, but
neither of the two preselected slow intervals improved in both Soft pairs.
Hard 2048 lost average FPS in both orders. The current renderer stayed above
240 in every measured second at 1024 in both modes. The candidate introduced
brief dips there. Fixed 1024 is the signed-off baseline for Phase 2.

A separate instrumented run alternated original and near-first order in the
same launch. Local model-depth GPU time (original / near-first) was
0.196 / 0.195 ms in Hard over 115 sample quartets and
0.192 / 0.190 ms in Soft over 114 sample quartets.
These timer runs are separate from the frame-rate measurements.

A concurrent desktop workload started during the first reversed Hard 2048
candidate run (200.5 FPS, 96 seconds below target) and overlapped the next
Hard 1024 control. The table uses fresh comparisons for both. The original
logs were retained, including the 242.3 FPS control from the interrupted pair.

Local evidence is in `build/e5-matched-dips/matched.json`, `depth.json`
and `results.json`. The timing result is specific to this Windows GPU and encounter.

## From a saved setting to a gameplay frame

1. [game_load](../../source/game/game.c) latches the preference after a
   successful scenario load. `game_initialize_for_new_map` already runs
   material selection after object placement. These shared boundaries cover
   Xbox, CE and converted MCC maps. MCC discovery and conversion retain their
   separate lifecycle.
2. [material_prepare_map](../../port/linux/game/material_prepare.c) asks
   [modern_shadows_prepare_loaded_map](../../port/linux/game/modern_shadows.c)
   to consume the pending preparation once. A cached load can bypass the copy
   screen, so this path can present the stock loading frame using
   [progress_bar_present_standalone](../../source/interface/progress_bar.c).
   Reverts and BSP transitions do not re-arm the pending flag.
3. [modern_shadow_preload](../../port/linux/src/modern_shadows_gl.c) prepares
   the selected desktop resources. At 1024, eight six-face 32-bit primary
   local depth maps occupy up to 192 MiB. Private viewmodel cubes and large
   tiers retain on-demand allocation. Through 1024, both environmental model
   depth targets are also allocated during loading. Initial BSP sun ownership and emissive fixture patches are
   prepared when their respective settings are enabled. Sun and spot resources use
   their own paths. Allocation failure retains the established fallback.
4. [prepare_shadow_models](../../port/linux/src/d3d8_gl.c) draws a triangle
   to a private 1Ãƒâ€”1 target for each distinct discovered shadow model program.
   This lets the driver perform first-draw work while the loading frame is on
   screen. Vertex formats, material limits and unusual later combinations can
   still leave some work for gameplay. The general shader and texture
   preparation settings remain independent.
5. Map unload clears the preference, GPU resources and borrowed references.
   Changing BSP clears geometry and cached depths while retaining reusable
   shadow allocations. Desktop hosts enforce their lighting policy on use.
   GLES builds retain their existing exclusions.

## Recurring costs

[Local depth rendering](../../port/linux/src/modern_shadows_gl.c) remembers
which cube faces already contain clear depth. It clears dirty faces before
reuse and copies only the contiguous range required by the world and private
viewmodel layers. [Static sunlight](../../port/linux/game/modern_sun.inl) and
[fixture depth](../../port/linux/game/modern_spots.inl) survive frames with
intact breakable geometry. Changing any breakable bit invalidates all three
static images, including fixtures outside the current view.

For the [sun receiver](../../port/linux/src/modern_environment_gl.inl), the first
posed draw validates the current depth target. Reusing it keeps later uploads
and draws queued on the driver. The first direct scene-depth copy for each
single-sample image pair is checked as well. A rejected copy falls back to
the depth blit on the same frame. Replacing the destination or changing the
source requires validation again. Multisample depth still uses the resolve.

[Model lighting](../../port/linux/src/plasma_occlusion_gl.c) requests a
collision field only for a slot the material actually samples. The
[collision batch](../../port/linux/game/plasma_collision.c) observes the
engine's cluster eligibility and root bounds. The [model node
test](../../port/linux/game/plasma_collision_models.c) uses the stock bounded
segment and closest-node limit. Rays whose entire octant stays in a single
BSP leaf share the validated leaf result while testing each ray against the
objects in that leaf. For a full light query, `trace_batch_starts` walks from
the BSP root towards the light's origin once. It checks the endpoints against
planes close enough to split the light volume. For example, if the first
three nodes contain an entire ray and the fourth separates its endpoints,
ordinary traversal starts at node four with the original depth budget. A
malformed origin path falls back to the existing traversal.

At a leaf boundary the [surface query](../../source/physics/collision_bsp.c)
can reuse the first reference matching that leaf and plane during the same
batch. It still projects the current ray and checks later matching references
when an earlier surface is ineligible. The batch owns this lookup alongside
the BSP node cache. Stock simulation queries pass a null cache.

## Acceptance method

The earlier recorded route and checkpoint were unavailable. The replacement
uses a core save in the first snowy exterior of Xbox Two Betrayals, with a
fixed exterior camera and a deathless player. The setup placed the mapÃ¢â‚¬â„¢s
`e3_a` and `c3_base_tier_2` AI squads before saving the encounter. Each run
restored that same state, with active enemy fire and 19 living Covenant in
the measured group. The enemies continue to simulate. This
provides a repeatable camera and starting state while combat timing can vary.
The older 271.1/277.5 FPS results used another route and cannot establish a
gain or loss here.

Use 2560Ãƒâ€”1440, AA and VSync off, release PGO and LTO, uncapped presentation,
25 seconds of warmup and 120 measured seconds. Enable all lighting categories,
sun, fixtures and the flashlight, use Shared, and fix the shadow resolution at
1024 for the signed-off baseline or 2048 for the higher-quality comparison. The beam
occupies one local slot while active. The indoor control uses both Rat Race fixtures. GPU and collision instrumentation
run separately from throughput measurements.


### Measuring sustained FPS

For capture-free runs the [benchmark runner](../../tools/lighting_performance_run.py)
counts completed frames in each one-second wall-clock window. It reports time
below 240 and 360 FPS, the number of separate dips and the longest consecutive
stretch at each target. A second with zero frames counts. Only a trace spanning
the full measurement interval qualifies. These windows describe sustained
slowdowns to one-second precision. Use the underlying frame times for short
hitches, and keep the two-minute and heavier-minute averages for historical
comparison. Repeat close results on the same workload.

When the trace includes a simulation tick, `sustained.tick_frame_times` also
reports the frame count and mean duration for frames that advanced the game
clock and those between ticks, grouped by whether the second met its target.
It skips the first measured frame and tick resets. Both groups include the
cost of drawing the frame. Use them to check whether a slowdown persists
between game updates.

#### Sun fade playtest comparison

The earlier heavy-run traces at `07cb0ba16` had 42 of 120 seconds below 240
in Hard and 43 in Soft. The longest stretches were 27 and 22 seconds. Lighting
alone stayed at or above 240 in every one-second window. This is why the
full-run average alone cannot qualify a build.

The six runs below compare the preceding playtest (`9f114ec33`) with the
accepted sun fade and first-person detail (`2864bc026`). Both used the same
release build options, exterior save, fixed camera and capture-free settings.
Hard ran previous then accepted, Soft ran accepted then previous, and lighting
alone ran previous then accepted. Each run retained 19 living Covenant at
every squad sample. Both shadow modes used sunlight throughout and reached
eight local shadows including the shared beam.

| Build | Mode | Full FPS | Heavier minute FPS | Seconds below 240 / 120 | Longest stretch (s) | Frames over 16.7 ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Previous | Lighting alone | 259.5 | 243.7 | 22 | 6 | 0 |
| Accepted | Lighting alone | 262.0 | 253.1 | 5 | 2 | 1 |
| Previous | Hard 1024 | 235.3 | 219.7 | 71 | 49 | 1 |
| Accepted | Hard 1024 | 209.3 | 202.2 | 120 | 120 | 2 |
| Previous | Soft 1024 | 236.6 | 229.1 | 69 | 25 | 4 |
| Accepted | Soft 1024 | 210.0 | 202.6 | 120 | 120 | 2 |

The new shadow path cost about 0.53 ms/frame more in both full runs.
The lighting-only control moved by 2.5 FPS in the other direction. These
live-combat comparisons identify a sustained regression with shadows active.
They do not attribute the entire difference to one function. All six traces
are under `build/lighting-performance/sustain-<revision>-<mode>/`. The
accepted fade and first-person detail are the visual reference for subsequent
optimisations.

In a separate Hard profile, the heavier minute averaged three complementary
object projections per frame. Their depth and receiver submission took
0.572 ms/frame, of which 0.566 ms elapsed in driver error queries. This
is time waiting inside the call and can include earlier queued work. BSP
diffuse and specular cluster selection together took 0.004 ms/frame, surface
selection 0.086 ms, triangle preparation 0.047 ms, and draw submission
0.881 ms. World and cluster rays took 0.745 ms, and visibility field
reconstruction took 0.194 ms. These CPU scopes overlap. The local phase trace
is `sustain-phase-hard/profile.csv`.

The [environmental receiver](../../port/linux/src/modern_environment_gl.inl)
checks a new single-sample handoff draw before recording its target, immutable
mesh, alpha texture and bone count. A repeat submits the same depth and colour
draws without waiting for the driver after each object. Changed inputs take
the checked path. Failed setup or first use can still fall back. A resized
target or changed scene-depth source invalidates validation. Ordinary object
projections and multisample resolves keep their per-draw checks.

A separate Hard run alternated the checked and queued paths in two-second
intervals, reversing order halfway through each eight-second block. All 14
complete blocks favoured queuing. Mean frame time fell from 4.555 to 4.189 ms.
This comparison kept the original coverage decisions and every shadow draw.

Separate uninterrupted runs with queuing gave the following results. These are
the same two-minute exterior workload as the accepted fade runs above. Sunlight
was active throughout. Both modes reached eight local shadows including the beam.

| Mode | Full FPS | Heavier minute FPS | Seconds below 240 / 120 | Longest stretch (s) | Worst frame (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard 1024 | 240.1 | 227.7 | 58 | 39 | 9.3 |
| Soft 1024 | 239.7 | 227.4 | 67 | 52 | 10.5 |

Neither run sustained 240 in the heavier combat. The full frame traces are
`sustain-handoff-{hard,soft}-all/frames.csv`. As a separate experiment, a
tighter bound tested whether an object's entire projected shadow lay inside
the full-strength area of the sun tile, where its complementary draw has zero weight.
Across 14 paired Hard intervals this took 4.163 ms/frame against 4.165 ms
with the existing bound, winning five intervals. The renderer retains the
simpler coverage test.

A second instrumented build timed the shared renderer with the retained
optimisation. The heavier minute still submitted three complementary shadow
projections per frame. Their combined CPU submission time was 0.003 ms.
Diffuse and specular BSP light draws took 0.475 and 0.358 ms/frame, with
0.235 ms in draw preparation and 0.153 ms in visibility tracing inside those
two passes. Across the complete frame, world and cluster rays took 0.593 ms
and visibility reconstruction took 0.198 ms. These nested CPU measurements
cannot be summed or interpreted as isolated GPU costs. The trace is
`sustain-final-profile-hard/profile.csv`. The remaining shared-renderer
question is how much of BSP drawing outside preparation comes from walking
materials, applying rasterizer state and issuing the actual draws.

In Mercury Rising 2 the exterior Soft 1024 approach used the same 19 camera
positions and the same 11-by-11-pixel ground patch beside the Covenant cache.
Relative to the sun-off reference, the largest darkening between consecutive
steps was 6.23 levels in the accepted fade and 5.88 after optimisation (8-bit
RGB). At corresponding approach steps the two patch means differed by at most
0.87. On retreat, separate sessions did not always settle at the same time.
Switching the checked and queued paths at each of four fixed camera positions
gave identical patch pixels. The map cinematic interrupted the first near
comparison, so it was repeated after the cinematic ended. In CE Death Island,
switching both ways with the player pose and camera frozen gave identical
1600-by-1000 Soft 1024 frames.

The Windows release build, Android guest and web build passed, along with
160 targeted lighting, collision, shadow and pacing checks, including the
Windows GPU receiver tests. Two local Windows peers with Hard and Soft on
Xbox Blood Gulch completed a 74-second comparison with no mismatched
seconds. The visual and performance runs used NVIDIA hardware. Local evidence
under `build/e5-sustained/` includes `combat-results.json`,
`visual-results.json` and the matched comparison images.

### Snowy exterior results

Release Windows on an i7-14700K and RTX 5070 Ti. Each row covers 120 measured
seconds using the camera and settings above. The Soft reference is the repeat
run. The first Soft reference averaged 178.3 FPS.

| Rendering in the snowy fight | Average FPS | 99th percentile (ms) | Worst frame (ms) | Frames above 16.7 / 33 / 50 / 100 ms |
| --- | ---: | ---: | ---: | --- |
| Stock, reference | 418.4 | 3.87 | 164.62 | 8 / 7 / 5 / 3 |
| Stock, E5 | 423.5 | 3.24 | 9.00 | 0 / 0 / 0 / 0 |
| Dynamic lighting only, reference | 240.9 | 7.90 | 25.80 | 9 / 0 / 0 / 0 |
| Dynamic lighting only, E5 | 278.8 | 5.36 | 7.68 | 0 / 0 / 0 / 0 |
| Hard 1024, reference | 199.2 | 6.78 | 11.54 | 0 / 0 / 0 / 0 |
| Hard 1024, E5 | 241.9 | 6.00 | 8.03 | 0 / 0 / 0 / 0 |
| Soft 1024, reference | 200.7 | 6.52 | 13.03 | 0 / 0 / 0 / 0 |
| Soft 1024, E5 | 240.9 | 6.03 | 8.65 | 0 / 0 / 0 / 0 |

Both final combat runs reached all eight local slots, with sunlight active
throughout. The earlier lighting-only control used the same settings and
camera with Classic shadows. Hard still needs about 1.36 ms per frame to reach 360 FPS here.
The 15-bot Rat Race control with two independently shadowed lamps averaged
381.8 FPS over two minutes on an earlier E5 build. A separate 30-second
profile recorded both lamps on every frame and 405.8 FPS. The indoor scene
measures a different workload. Results and frame series are retained locally under
`build/lighting-performance/`.

One isolated comparison moved the recurring sun driver check to first use of
each target and raised Hard from 205.5 to 218.7 FPS. Checking a newly used
scene-depth image pair once raised Hard to 222.1 FPS. Profiling then located
another 0.46 ms per frame waiting for the sunlight projection. The same
single-sample sun receiver now validates its fixed draw on a new target and
queues subsequent draws, giving the final figures above. In that build all per-object
draws and multisample resolves still checked for fallback. The later
[sun fade comparison](#sun-fade-playtest-comparison) narrows repeated checks
for verified single-sample handoffs. In the preceding CPU capture,
world and cluster rays used 0.64 ms per frame, posed-model rays 0.03 ms and
visibility reconstruction 0.28 ms. The phase timings are inclusive and may
include queued driver work. The
collision reference compared 38,976,000 render rays against the stock
queries with zero mismatches in its combat run. A separate same-pose
experiment partitioned ray batches by their dominant axis as well as their
octant. Across 45,766,656 rays, it matched the stock hits but needed
8.65 seconds of tracing versus 8.08 seconds with the existing octants. The
narrower partition was discarded. Suppressing object tests in that
diagnostic changed 428,454 results.

Environmental model depth occupied about 0.47 ms per frame on the GPU in a
separate profile. In the last 5,000 nonempty windows of a combat capture,
all 84,167 model-shadow submissions included an alpha-tested draw. A cache
restricted to fully opaque submissions would save no draws in that sample.
Reusing depth across frames for these models would also need to establish
that the cutout texture, its coordinates and the posed geometry still match.

At 960Ãƒâ€”540 with Hard shadows still fixed at 1024, a 30-second diagnostic
averaged 286.0 FPS. The same 30-second window of the native-resolution Hard
run averaged 263.6. Pixel workload accounts for part of the gap. Substantial
work remains at the lower display resolution too. Fixed-quality 360 FPS in
the snowy fight remains the open E5 performance gate.


### Collision tracing follow-up

The frozen-input diagnostic rotated the timing order and compared each
returned depth with the stock query before the simulation could advance.
Across 32,379 batches (49,734,144 rays), all four paths returned identical
depths. With both changes bypassed, tracing took 8,638.6 ms. Classifying the first split
took 8,389.5 ms, and adding the leaf-reference cache brought that to
8,303.0 ms. Together the changes saved 3.9% of tracing time in this check.
These timings cover complete batches and include the object tests.

The following uncapped two-minute runs used the same 1440p exterior setup as
above, with a 25-second warmup and fixed quality. The lighting-only control
was repeated on the preceding executable. The preceding Hard and Soft
figures come from the E5 runs in the table above.

| Mode | Earlier average FPS | New average FPS | New 99th percentile (ms) | New worst frame (ms) |
| --- | ---: | ---: | ---: | ---: |
| Dynamic lighting only | 279.4 | 286.9 | 5.28 | 8.29 |
| Hard 1024 | 241.9 | 246.7 | 5.94 | 15.46 |
| Soft 1024 | 240.9 | 242.5 | 6.01 | 9.09 |

Both shadowed runs used sunlight throughout and reached eight local shadow
slots including the shared flashlight. Each mode averaged approximately 29
selected lights. These live AI runs measure the whole frame and can vary
between runs. A lighting-only control alternated the two optimisations in
two-second intervals (on, off, off, on). Across 14 complete eight-second
blocks inside the measured window, the enabled intervals averaged 272.1 FPS
and the bypassed intervals 272.5 FPS. Six blocks favoured the new path.
The matched-ray check establishes a CPU tracing improvement. The alternating
control did not demonstrate a whole-frame FPS gain. The 360 FPS target
remains open.

The before/after NVIDIA image check used 25 pairs from the same frozen
exterior pose at tick 270, with four local shadows and 24 sun casters.
The largest mean absolute error in any RGB channel was 0.295 on a 0Ã¢â‚¬â€œ255
scale, with particles and shields still animating.

On the final source, 140 collision and GPU regression checks passed, and
the Windows, Android and web builds succeeded. A live Soft run crossed
Xbox Two Betrayals, MCC Digsite, CE Death Island and Xbox Blood Gulch.
In a separate two-peer CE Death Island run the Classic host reported zero
active local-shadow samples and the Hard client reported 46. Both respected
the host's stock shield policy. The tests used NVIDIA hardware on Windows.

### Visibility reconstruction and receiver stencil state

Movement triggered 20,050 of 20,142 full ray batches in a 35-second lighting-only
sample of the snowy fight. No stationary field retraced solely because its
blockers changed. Hard recorded 19,934 moving origins, 136 new admissions and
one stationary scene change in 20,071 batches. A moving light can also have
changed blockers. Both tests kept all eight slots available.

An experimental BSP shortcut identified octants entirely inside empty space.
Both paths matched the stock depths for 49,368,576 rays and the shortcut
reduced paired tracing time by 4.9%. The preceding commit and the experiment
were also built into one executable and alternated every two seconds (new,
old, old, new). Across 14 complete eight-second blocks, lighting alone averaged
273.5 FPS on the preceding path and 272.9 with the shortcut. Soft averaged
241.3 and 239.6 FPS. The separate lighting runs had differed by 14.1 FPS
(286.9 to 272.8). That drop did not reproduce under alternation, and neither
matched test established a whole-frame gain. The branch retains the preceding
collision traversal.

The retained [visibility reconstruction](../../port/linux/game/plasma_occlusion.c)
packs the four neighbouring depths for each cubemap texel before projecting
the 16Ã‚Â³ receiver grid. For example, a cell on the edge of a face reads the
same duplicated last texel as the scalar lookup. SSE2 builds use 24 KiB of
scratch during reconstruction. Other CPU targets keep the scalar path.
Across 143,768 live inputs the two layouts produced byte-identical fields.
Including packing, reconstruction took 2,026.0 ms against 3,235.9 ms.

The [renderer](../../port/linux/src/d3d8_gl.c) selects the effective stencil
state once per draw in `apply_stencil_state`. Consecutive world draws can
reuse the receiver bit. An ordinary draw or an external rendering pass
restores or invalidates the cached state through the existing renderer
interface. This belongs in the shared renderer because it owns both the
emulated stencil state and the cache of values sent to the driver.
On the same alternating Soft 1024 workload, stencil function,
operation and write-mask submissions fell from 141.7 to 25.1 per frame.
CPU occlusion fell from 0.925 to 0.844 ms per frame. All 14 blocks favoured
the new occlusion path. Whole-frame throughput was 240.1 versus 241.0 FPS
and eight of 14 blocks favoured the changes. The full-frame difference is
small relative to block-to-block variation.
With lighting alone, a separate comparison in the same fight measured
272.5 FPS on the preceding code and 278.6 on the retained code. All 14
blocks had shorter mean frames. CPU occlusion fell from 0.891 to 0.803 ms
per frame.

In separate uncapped 120-second release runs at 2560Ãƒâ€”1440, lighting alone
averaged 282.5 FPS, Hard 1024 averaged 245.3 and Soft 1024 averaged 247.2.
Both shadow modes reached eight local slots and used sunlight throughout.
The slowest frames were 12.0, 10.8 and 11.8 ms respectively. The second
minute averaged 270.0, 237.0 and 237.4 FPS. All three full-run averages
clear 240 FPS. The 360 FPS goal in this encounter remains open.

A matched NVIDIA image check froze the reference and retained build at tick
271 with seven local maps and 20 sun casters. Across 25 paired captures the
largest per-channel mean absolute difference was 1.021 on a 0Ã¢â‚¬â€œ255 scale.
Snow and effects continue to animate during a simulation freeze. A separate
Soft run crossed Xbox Two Betrayals, MCC Digsite, CE Death Island and Xbox
Blood Gulch. The two-player CE host-policy check recorded zero local shadow
samples on the Classic host and 60 on the Hard client.

### Heavier combat minute

At release commit `07cb0ba16`, the same checkpoint, camera and 2560Ãƒâ€”1440
settings gave the following averages on the i7-14700K and RTX 5070 Ti.
Each row is a separate two-minute Windows run. The stock control used the
ordinary flashlight with the optional lights and shadows disabled.

| Mode | Full run (FPS) | Second minute (FPS) | 99th percentile frame time (ms) |
| --- | ---: | ---: | ---: |
| Stock | 436.1 | 396.3 | 3.04 |
| Dynamic lighting | 285.0 | 272.1 | 5.25 |
| Hard 1024 | 251.6 | 237.6 | 5.97 |
| Soft 1024 | 247.9 | 237.1 | 6.01 |

Hard and Soft both used sunlight throughout and reached eight local slots
including the shared flashlight. At 360 FPS the frame budget is 2.78 ms.
The second-minute stock average used 2.52 ms, lighting used 3.68 ms, and Hard
and Soft each used about 4.21 ms. The live fight can vary between runs.

Two traversal candidates were tested in separate executables that alternated
the original and candidate code over two-second intervals, reversing the order
in each eight-second block. Each comparison covered fourteen complete blocks.
Replacing tail recursion with an explicit BSP descent loop took Hard from
247.9 to 247.4 FPS and Soft from 246.6 to 243.6. Preparing each ray octant
from its first classified ray's split took lighting from 284.8 to 282.3,
Hard from 245.2 to 245.8 and Soft from 244.5 to 242.2 FPS. This second
candidate increased mean occlusion time in all three modes. Both experiments
were removed.

In a separate instrumented Hard run, the heavier minute spent 0.62 ms per
frame tracing world and cluster rays, 0.05 ms tracing posed models and
0.20 ms reconstructing visibility fields. The shared draw preparation
timer included model field work and averaged 1.29 ms. GPU queries sampled
every 32nd frame measured 0.38 ms for environmental model depth and about
0.38 ms for local shadow setup, world and model depth together. CPU and GPU
timers overlap. The local captures and frame series use the prefix
`build/lighting-performance/e5-heavy-`.

A separate pass timed the top-level renderer in the same heavy minute.
Model rendering averaged 0.40 ms stock and 1.17 ms in Hard. The two BSP
lighting passes together averaged 0.09 ms stock and 0.91 ms in Hard.
These are inclusive times, including any on-demand field updates.
[Diffuse and specular light submission](../../source/objects/object_lights.c)
each builds a cluster list, then the [BSP renderer](../../source/structures/structure_render.c)
selects surfaces and builds temporary triangles for each pass. The
[follow-up profile](#sun-fade-playtest-comparison) measures those operations
separately from drawing. Diffuse and specular radii
can differ, so sharing a surface list needs an exact match of its inputs.
In sampled Hard world queries, 12% of rays stayed in the origin leaf and
the mean ray encountered 2.67 tree forks.

The first frozen capture pair had a camera offset. Reapplying the camera
after freezing produced identical saved camera poses in both builds.
At tick 271 with seven local maps and 20 sun casters, 21 paired captures
had a maximum per-channel mean absolute difference of 1.01 on a 0Ã¢â‚¬â€œ255 scale.

### Per-light index uploads in the snowy fight

The shared [OpenGL renderer](../../port/linux/src/d3d8_gl.c) calls
`index_upload` for each non-mirrored indexed draw. The diffuse and specular
passes in the [BSP renderer](../../source/structures/structure_render.c)
already have a complete triangle list for each pass. Materials draw slices of
that list. The experiment uploaded the list on its first eligible draw and
pointed subsequent draws at their original slices. It kept the original draw
order, vertex ranges, material changes and ring-buffer wrap handling.

In the heavier minute of Hard 1024, 1,077 sampled frames averaged 470 indexed
draws for BSP lighting. Walking the material list outside the material draw
callbacks took 0.050 ms per sampled frame. Draw submission inside the callbacks
took 0.719 ms, including 0.273 ms in draw preparation and 0.111 ms in index
uploads. The timer around submission to OpenGL includes the nested upload.
These are instrumented CPU times. Separate GPU queries measured 0.139 ms for
enhanced diffuse geometry lighting and 0.243 ms for specular lighting per
sampled frame. CPU and GPU times can overlap.

The whole-frame CPU timer measured 0.682 ms for world and cluster ray tracing
in Hard and 0.646 ms in Soft. Sampling one in 127 rays put the world BSP
section at 193 ns per sample and the subsequent object/cluster section at
75 ns in Hard. A pair of consecutive timer calls measured 48 ns. These short
timers identify which section is more expensive. Multiplying them by the ray
count overstates the cost of the ordinary path.

The comparisons used the same 1440p encounter, eight local slots (including the beam),
the full sun fade and fixed quality. Both paths were in one executable. The
candidate ran in the first and last two seconds of each eight-second block, and the
original ran in the middle four.
Each comparison used 14 complete blocks and omitted 15 ms at each switch
boundary. Negative frame-time changes favour the candidate.

| Mode | Uploads per frame, original Ã¢â€ â€™ batched | Mean paired change | Faster blocks | Heavier minute FPS, original Ã¢â€ â€™ batchedÃ‚Â¹ |
| --- | ---: | ---: | ---: | ---: |
| Lighting only | 489 Ã¢â€ â€™ 65 | Ã¢Ë†â€™0.121 ms | 11/14 | 242.1 Ã¢â€ â€™ 253.2 |
| Hard 1024 | 476 Ã¢â€ â€™ 63 | Ã¢Ë†â€™0.075 ms | 10/14 | 216.6 Ã¢â€ â€™ 225.7 |
| Soft 1024 | 475 Ã¢â€ â€™ 62 | Ã¢Ë†â€™0.087 ms | 11/14 | 216.5 Ã¢â€ â€™ 221.5 |

Ã‚Â¹ The paired comparison ends at second 144. These averages use seconds
85Ã¢â‚¬â€œ144 and only the intervals assigned to each path. Live AI can change the
work between intervals. In Hard the 95% block-bootstrap interval for the
frame-time change was Ã¢Ë†â€™0.159 to +0.017 ms. The Soft and lighting-only
intervals favoured batching. Uploading whole lists transferred about 12%
more bytes than the material slices actually used during candidate intervals.

On the same day the pinned OpenCE Classic 1024 control averaged 370.0 FPS
over two minutes and 352.4 FPS in the second minute. The pinned GulchCE
Classic control measured 422.4 and 381.2 FPS. Both used the stock flashlight.
The instrumented shadow profiles and the Classic controls have different
workloads and instrumentation.

At simulation tick 271, a fixed Xbox exterior view showed seven local maps,
the shared flashlight and 22 sun casters in both upload paths. Across 11
alternating image pairs per mode, the largest mean absolute difference in
any colour channel was 0.97 in Hard and 0.99 in Soft on a 0Ã¢â‚¬â€œ255 scale.
Snow and shield animation continued at the fixed pose. A separate 32-bit
test passed 123,911 draws through the experimental upload code, including
38,701 storage wraps. The bytes at the returned offsets matched the input
slices for every draw.

The settled Soft 1024 views also exposed a cost that depends on which
materials draw. On MCC Mercury Rising 2, 76 uploads became nine, and bytes
per frame went from 3,870 to 4,296. Five settled image pairs differed by at
most 0.002 in mean absolute error per channel. On CE Death Island, two
uploads of 354 bytes in total became one upload of 1,134 bytes. Both settled
pairs were pixel-identical. The first pair on each map included the camera
transition and was excluded from the image comparison. These are two
individual views. The extra 780 bytes on Death Island establish over-transfer
in a sparse pass. Its impact on frame time was not measured there.

Separate runs held the upload path fixed for the full two minutes, in the
order batched Hard, batched Soft, original Soft, original Hard. A repeat of
original Soft followed the map checks. Every run used 19 Covenant, sunlight
and up to eight local slots including the flashlight.

| Fixed upload path | Full run FPS | Heavier minute FPS | Seconds below 240 | Longest stretch below 240 |
| --- | ---: | ---: | ---: | ---: |
| Hard 1024, original | 225.4 | 211.8 | 89/120 | 66 s |
| Hard 1024, batched | 240.7 | 229.0 | 57/120 | 27 s |
| Soft 1024, original repeat | 225.3 | 216.6 | 104/120 | 88 s |
| Soft 1024, batched | 230.3 | 216.7 | 83/120 | 67 s |

All four spent every measured second below 360 FPS. The first original Soft
run reached only 79.6 FPS, including 75.2 FPS in its heavier minute.
Its executable manifest, settings, saved camera and checkpoint metadata
matched the batched run. Mean occlusion time was 2.10 ms versus 0.91 ms on
the original Soft repeat. GPU clocks and utilisation were recorded only on
the repeat. The cause of the slow run remains unknown. It is retained as an
outlier and excluded from estimates of the batching gain.

The whole-list prototype remained experimental. Hard benefited in the fixed
run. Soft showed a gain under alternation and effectively tied in the fixed
heavier minute. The [follow-up comparison](#world-collision-and-dense-material-draws)
tests a bounded upload for adjacent material draws and profiles BSP nodes
and leaf intersections on matched rays.

The local evidence is in `build/e5-bsp-cost/`. It includes the tested source
patch against `bdb781a7d`, build manifests, per-phase results, image pairs
and the range test. The `e5-bsp-cost-*` and `e5-index-*` runs under
`build/lighting-performance/` retain the frame traces. This was a Windows
NVIDIA investigation. AMD hardware and native Linux remain unverified.

### World collision and dense material draws

The Windows follow-up used the same snowy encounter, camera, 19 Covenant
and 1440p fixed-quality settings as the preceding comparison. The branch
control was `ef94431a4`. With shadows active the full sun fade ran throughout
and the shared flashlight and local shadow maps reached eight slots.

**Collision.** Of 208,896 sampled world rays, 96,698 hit. All hit flags,
distances, hit surfaces and ordered leaf lists matched the stock query.
The rays visited 3,009,451 nodes. Cached lookups served 98.7% of those visits.
The leaf section, including surface tests, accounted for 23% of inclusive
instrumented time. The timers add overhead and the remainder includes query
setup. The tested change stores pending far children on a bounded stack
during a render batch. It preserves the near-first visit order, split-plane
state and original node and depth limits. Gameplay queries keep their own
entry and recursive split handling. Both paths use the same leaf tests.
[Collision traversal](../../source/physics/collision_bsp.c)

In a separate same-input comparison the experimental walk took 9.127 s
over 36,748 complete batches. Two measurements of the preceding cached walk
took 9.642 and 9.431 s. All 56,444,928 depths matched the stock reference.
After integration, a fresh 31,100-batch check compared 47,769,600 depths
with the stock query. All depths matched, including the cache and
unprepared comparisons.

**Geometry uploads.** The Windows BSP diffuse and specular passes expose
their immutable triangle list only for the duration of one eligible light
pass. The OpenGL backend uploads the first two adjacent material slices
exactly. On a third adjacent draw it can read ahead by at most the bytes
already used in that run, bounded by the list's end. Later materials reuse
only a fully covered range. A ring-buffer wrap invalidates the uploaded
range. The geometry owner supplies the list bounds because the backend
receives only the current draw's indices. The renderer's streamed-byte
counter accounts for saved writes and read-ahead.
[Pass lifetime](../../source/structures/structure_render.c) Ã‚Â·
[Triangle storage](../../source/rasterizer/xbox/rasterizer_xbox_draw_primitives.c) Ã‚Â·
[OpenGL uploads](../../port/linux/src/d3d8_gl.c)

| Settled Soft 1024 view | Exact slices, calls / bytes | Whole list, calls / bytes | Adjacent slices, calls / bytes |
| --- | ---: | ---: | ---: |
| CE Death Island | 2 / 354 | 1 / 1,134 | 2 / 354 |
| MCC Mercury Rising 2 | 79 / 4,806 | 8 / 5,124 | 36 / 5,460 |

Both settled views gave pixel-identical comparisons. The MCC view shows that
bounded read-ahead can write more bytes than a whole list. In a Hard combat
diagnostic the respective policies averaged 503, 64 and 194 uploads per
frame and about 83,254, 89,596 and 85,695 bytes. A separate 32-bit storage
test checked the returned data on 123,936 draws through 38,440 ring wraps.

**Individual and combined frame times.** A single executable switched among
the four combinations once per second. Each 16-second block gave each
combination four intervals, with the order varied. The table uses seven
complete blocks and omits 35 ms at interval boundaries. Negative values are
milliseconds saved per frame relative to both changes off.

| Mode | Collision only | Upload only | Combined | Faster blocks, combined |
| --- | ---: | ---: | ---: | ---: |
| Dynamic lighting only | Ã¢Ë†â€™0.036 | Ã¢Ë†â€™0.047 | Ã¢Ë†â€™0.104 | 7/7 |
| Hard 1024 | Ã¢Ë†â€™0.030 | Ã¢Ë†â€™0.039 | Ã¢Ë†â€™0.059 | 6/7 |
| Soft 1024 | +0.042 | Ã¢Ë†â€™0.032 | Ã¢Ë†â€™0.081 | 5/7 |

The Hard combined comparison's 95% block-bootstrap interval spanned
Ã¢Ë†â€™0.098 to +0.004 ms. Live AI and light counts varied within the fight.
As a sensitivity check, a within-block fit adjusted for selected and scene
light counts, and for sun caster and local map counts in shadow modes.
The combined frame-time differences were Ã¢Ë†â€™0.071 ms for lighting alone,
Ã¢Ë†â€™0.060 for Hard and Ã¢Ë†â€™0.041 for Soft. These estimates depend on the workload
model. The fixed release-build runs below measure actual FPS and slow stretches.
In fixed separate executables the first lighting-only pair gave 280.9 FPS
for the control and 235.2 for both changes. Reversing the order gave 278.8
for both changes and 271.0 for the control. The 46 FPS loss did not repeat.
The first fixed Hard candidate also had a 181 ms stall. Its measured
occlusion update took 2.55 ms in that frame. The remaining time has not
been attributed.

At the same frozen Xbox pose (tick 271), both modes had seven local maps,
the shared beam and 22 sun casters. Across 18 Hard and 17 Soft image pairs,
the largest per-channel mean absolute differences were 1.03 and 0.98 on a
0Ã¢â‚¬â€œ255 scale. Snow and shield effects continued to animate. The local logs,
patches and matched capture sheet are in `build/e5-trace-density/`.
The two-minute frame records use `build/lighting-performance/e5-density-*`.

#### Fixed builds with both changes integrated

Each mode ran uncapped for 120 seconds after 25 seconds of warmup, without
screenshots or per-ray instrumentation. Both shadow modes used 1024, the
full sun fade, environmental fixtures and all eight shared local slots.
In this table the first value is the unmodified branch control and the
second is the integrated build. Lighting-only controls repeated in reverse
order are reported above.

| Mode | Full average FPS | Second-minute FPS | Seconds below 240 | Longest below 240 | 99th-percentile frame, ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Dynamic lighting only | 280.9 Ã¢â€ â€™ 293.3 | 268.3 Ã¢â€ â€™ 284.8 | 3 Ã¢â€ â€™ 0 | 1 Ã¢â€ â€™ 0 | 5.48 Ã¢â€ â€™ 5.09 |
| Hard 1024 | 242.5 Ã¢â€ â€™ 252.4 | 231.3 Ã¢â€ â€™ 243.1 | 54 Ã¢â€ â€™ 19 | 35 Ã¢â€ â€™ 8 | 6.23 Ã¢â€ â€™ 5.82 |
| Soft 1024 | 241.7 Ã¢â€ â€™ 248.7 | 233.4 Ã¢â€ â€™ 239.8 | 58 Ã¢â€ â€™ 35 | 30 Ã¢â€ â€™ 8 | 6.37 Ã¢â€ â€™ 5.75 |

All runs used the same combat core. The integrated Hard run had six slow
stretches and Soft had fourteen. Neither contained a frame longer than
10 ms. Lighting alone recorded a single 20 ms frame despite clearing 240
in every one-second window. All three configurations were below 360 in
every measured second. The average and pacing improvements warrant keeping
both changes. The size of the speedup varies between runs.

At Classic 1024 with enhanced lighting and shadows disabled, GulchCE
averaged 429.5 FPS before and 429.3 afterward. The second minute averaged
383.1 and 386.9 FPS respectively. Pinned OpenCE `307a42ee04ba` averaged
384.3 FPS and 371.2 FPS in the second minute. All three stayed above 240
in every measured second. They spent seven, two and seven seconds below
360 respectively. The [stock control](#upstream-stock-control) describes
the difference in Classic shadow allocation between the engines.

The previous Windows build hosted a Blood Gulch LAN game in Soft and the
integrated Windows build joined in Hard. Across 68 compared seconds, the
peers had zero simulation drift in 2,037 ticks. A separate frozen Xbox
combat view at tick 271 had seven local shadow maps, the shared flashlight
and 22 sun casters. Across 18 Hard and 17 Soft image pairs, the largest
per-channel mean absolute differences were 1.025 and 0.973 on the 0Ã¢â‚¬â€œ255
scale. Snow and glow continued animating in the frozen view.

On MCC Mercury Rising 2, the original and integrated Windows builds traced
the same seven outdoor camera positions toward and away from the Covenant
caches in Soft 1024. The largest per-channel mean absolute difference at
960Ãƒâ€”600 was 0.179 on the 0Ã¢â‚¬â€œ255 scale. These settled views check the result
at each distance. Continuous motion between samples remains unmeasured.
The Windows release, Android and web builds passed. The focused collision,
renderer and NVIDIA GPU suites passed all 292 cases. Native Linux gameplay
and AMD hardware remain unverified for these changes.

### Texture setup and sustained combat frame time

On the integrated renderer at `07566a473`, the heavier Hard 1024 minute
averaged 0.298 ms in the BSPÃ¢â‚¬â„¢s lit material callbacks. Inside them, draw
preparation took 0.190 ms, including 0.118 ms in texture setup. Issuing the
indexed draw took about 0.010 ms of CPU time. These clocks rotate between
scopes and subtract a back-to-back clock measurement. They measure CPU time
spent submitting work. The GPU can continue executing after a draw returns.
[Material draws](../../source/rasterizer/xbox/rasterizer_xbox_environment.c) Ã‚Â·
[Texture binding](../../port/linux/src/d3d8_gl.c)

In a separate stage trace, the late part of the Hard run averaged 345
texture lookup misses per frame, 283 of them for palettized textures.
Across 496 sampled combat frames, all 145,695 palette comparisons matched
the previously seen bytes. The Windows experiment allowed the stage cache
to reuse a palettized texture after checking the current palette. A
separate run checked 1,341,046 reused results against the ordinary lookup
with zero mismatches. The existing path still needs to handle palette edits
and recycled texture variants on other workloads.
[Native palette and texture cache](../../port/linux/src/xbox_textures.c)

The same executable alternated the two paths during each fight. Each of
14 eight-second blocks gave both paths four seconds. The comparison omits
frames within 50 ms of each one-second boundary and weights seconds equally.
Positive time means the experimental path took longer.

| Mode | Paired frame-time change | Faster blocks | 95% block-bootstrap interval |
| --- | ---: | ---: | ---: |
| Lighting only | +0.002 ms | 6/14 | Ã¢Ë†â€™0.043 to +0.046 ms |
| Hard 1024 | +0.016 ms | 5/14 | Ã¢Ë†â€™0.017 to +0.050 ms |
| Soft 1024 | +0.011 ms | 7/14 | Ã¢Ë†â€™0.068 to +0.096 ms |

Timing the texture function itself in an independent Hard run gave
301 ns per lit draw on the existing path and 291 ns with reuse. That
amounts to about 0.005 ms across 480 draws. All three whole-frame
intervals span zero. The prototype remains outside the renderer.

A separate trace followed the frame from simulation through presentation.
In the Hard run with detailed timers, frame time grew by 0.416 ms between
the first and second minutes. The decal pass accounted for 0.196 ms of that
increase. On seconds below 240 FPS, decals took 0.386 ms per frame,
compared with 0.206 ms during seconds that met 240. Replaying the ordinary
branch traces also shows the slowdown on frames between simulation ticks.

| Render-thread elapsed time, Hard 1024 | First minute | Second minute |
| --- | ---: | ---: |
| Whole frame | 3.873 ms | 4.288 ms |
| Five decal layers combined | 0.168 ms | 0.364 ms |
| Sending visibility-query results to the GPU buffer | 0.335 ms | 0.385 ms |
| Dynamic-light occlusion | 0.794 ms | 0.867 ms |
| Modern shadow preparation | 0.191 ms | 0.193 ms |
| Simulation clock update, including due ticks | 0.186 ms | 0.197 ms |

The decal timer includes selecting the marks in each visible cluster and
submitting them in blend order. The query-copy timer measures elapsed time
on the NVIDIA rendering thread, including time it may wait for the driver.
Swap and the frame-pacing fence were timed separately. See
[decal ordering and batches](../../source/rasterizer/xbox/rasterizer_xbox_decals.c)
and [visibility submission](../../port/linux/src/d3d8_gl.c).

Two more replays used asynchronous GPU timers on every 32nd frame.
The probe identified decals at the actual batch flush, including the last
batch in each layer. Counts below include every measured frame and GPU
times include the sampled draws. Each second has equal weight within a
minute.

| First Ã¢â€ â€™ second minute | Hard 1024 | Soft 1024 |
| --- | ---: | ---: |
| Queued decal entries per frame | 450 Ã¢â€ â€™ 1,011 | 449 Ã¢â€ â€™ 1,007 |
| Decal batches per frame | 251 Ã¢â€ â€™ 591 | 251 Ã¢â€ â€™ 585 |
| Submitted decal vertices per frame | 3,985 Ã¢â€ â€™ 8,947 | 3,972 Ã¢â€ â€™ 8,989 |
| GPU time in decal draws | 0.219 Ã¢â€ â€™ 0.507 ms | 0.220 Ã¢â€ â€™ 0.503 ms |
| Visibility queries sent per frame | 23.27 Ã¢â€ â€™ 23.33 | 23.27 Ã¢â€ â€™ 23.33 |

Decals contributed the largest measured increase in GPU time in both
replays. Local shadow depth grew by 0.007 ms in Hard and 0.004 ms in Soft.
Render-thread and GPU work can overlap, so adding the two decal increases
would overstate the potential gain. The visibility workload had a nearly
constant query count as its submission time rose. Timing alone does not
identify a driver stall.

#### Skipping off-screen combat decals

A visible BSP cluster can contain bullet marks and scorch marks behind the
camera. In the last seven paired Hard windows, blend changes accounted for
530 of 619 decal batches per frame, and texture-group changes accounted for
87. No batch reached capacity.

At [decal batch submission](../../source/rasterizer/xbox/rasterizer_xbox_decals.c),
`decal_outside_window` can use the final clipped world vertices and the
actual rasterizer camera together. A mark is skipped only if every vertex
lies more than 0.01 world units beyond the same side plane. A crossing
mark still draws. Mirrored views and invalid projections use the original
path. Each mark stays in the cache and can reappear when the camera turns.
Blend order, texture bindings and the trailing vertex attributes are
preserved across Classic, lighting-only, Hard and Soft modes.

With the final implementation toggled in the same fight, the later
sample windows averaged 684 submitted batches with the check disabled and
85 with it enabled. In a separate GPU validation pass, the NVIDIA driver
drew each tested mark individually. All 111,198 proposed rejections produced
zero passing samples. The 10,462 positive controls produced 6,234,640 samples.
The same draw-level test on staged CE Death Island and MCC Mercury Rising 2
marks recorded 947 and 2,628 proposed rejections, with 0 passing samples.
All five CE and seven MCC image pairs were pixel-identical. Capture events
were matched by camera position and direction, simulation tick, mark count
and shadow state before comparing the frames.
A frozen snowy combat view at tick 3151 with 900 decal entries also
produced 35 before/after image pairs. The largest mean channel difference
was 0.90 of 255. Snow and effects continue animating during a simulation freeze.

The table uses separate passive Windows release benchmarks on the RTX 5070 Ti.
Each replay used the same 19-Covenant checkpoint, 25 seconds of warmup
and two measured minutes at 2560Ãƒâ€”1440 with AA and VSync off. Hard and
Soft used 1024 with the full sunlight fade and all eight local shadow slots.
Only the frame-time runs are used for FPS. The GPU query run is for
visibility correctness. The OpenCE comparison uses commit `307a42ee04ba`.

| Renderer | Full FPS | Second-minute FPS | Seconds below 240 | Longest run below 240 (s) | Seconds below 360 |
| --- | ---: | ---: | ---: | ---: | ---: |
| OpenCE Classic | 378.2 | 360.4 | 0 | 0 | 29 |
| GulchCE Classic before | 439.7 | 403.4 | 0 | 0 | 0 |
| GulchCE Classic after | 527.9 | 527.6 | 0 | 0 | 0 |
| Lighting before | 289.5 | 277.5 | 0 | 0 | 120 |
| Lighting after | 302.1 | 297.5 | 0 | 0 | 120 |
| Hard before | 252.5 | 242.7 | 28 | 11 | 120 |
| Hard after, first run | 266.3 | 265.8 | 2 | 1 | 120 |
| Hard after, repeat | 262.8 | 264.5 | 1 | 1 | 120 |
| Soft before | 250.3 | 241.0 | 30 | 11 | 120 |
| Soft after, first run | 265.7 | 266.9 | 1 | 1 | 120 |
| Soft after, repeat | 116.6 | 115.8 | 120 | 120 | 120 |
| Soft after, third run | 264.2 | 266.5 | 1 | 1 | 120 |

The 116.6 FPS Soft launch was slow from its first measured second.
Its executable, settings, GPU, shader-cache startup and peak shadow count
matched the first launch. CPU residency and concurrent GPU load were first
recorded on later runs. The cause of the slow launch is open.

A separate Soft replay alternated the final decal check every few seconds
within one process. Across fourteen eight-second blocks, frame time fell
from 4.002 to 3.805 ms. Twelve blocks improved. The mean paired difference
was Ã¢Ë†â€™0.197 ms (95% block-resampling interval Ã¢Ë†â€™0.272 to Ã¢Ë†â€™0.126 ms).
The rendering thread sampled a performance core on all 30,869 measured frames.

With all 189 submitted marks in view on a stationary CE Death Island floor,
frame time changed from 1.086 to 1.089 ms. The 95% paired interval across
nine eight-second windows was Ã¢Ë†â€™0.006 to +0.013 ms.

On the sampled Hard GPU replay, decal draws in the heavier minute fell
from 0.507 to 0.057 ms per frame. Local shadow depth cost 0.423 ms, and
environmental depth cost 0.259 ms. A separate Hard run measured 0.776 ms
per frame of dynamic-light occlusion on the CPU. GPU and CPU work overlap.
In the two final Hard runs, the slowest twelve one-second windows cost
0.286Ã¢â‚¬â€œ0.325 ms more per frame than the middle twelve. CPU occlusion
accounted for 0.097Ã¢â‚¬â€œ0.103 ms of that increase.

Keep the decal change. Every measured second of the independent Hard and
Soft runs fell below 360 FPS. The slow Soft launch also keeps the sustained
240 FPS target open. The [slow-launch investigation](#measuring-slow-launches-and-local-depth) records GPU load,
render-thread processor samples and separate present timers. AMD uses its own readback path.

Windows, Android and web builds passed with 168 focused regression checks
on Windows. A two-player Windows loopback run compared the pre-change
host and updated client across 70 matched seconds with no mismatched
seconds. They used Soft and Hard sunlight at 1024 with local weapon
lighting off. AMD hardware and native Linux gameplay remain unverified.
Local evidence is in `build/lighting-performance/e5-decal-*` and
`build/e5-decal-batches/`.

#### Measuring slow launches and local depth

At renderer `086578a5b`, fresh launches replayed the 19-Covenant
snowy Two Betrayals checkpoint on Windows at 2560Ãƒâ€”1440. Hard and Soft used
1024, the full sun fade and eight available local slots, including the
shared flashlight. The measured fight lasted 120 seconds after 25 seconds
of warmup. Each second below a target is a complete one-second FPS window.

| Run | Average FPS | Heavier minute FPS | Seconds below 240 | Longest streak below 240 (s) | Seconds below 360 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard | 256.7 | 256.6 | 3 | 2 | 120 |
| Soft | 257.4 | 255.6 | 2 | 1 | 120 |
| Hard with timers | 258.5 | 258.2 | 3 | 1 | 120 |
| Soft with timers A | 246.9 | 244.4 | 26 | 5 | 120 |
| Soft with timers B | 258.9 | 258.1 | 1 | 1 | 120 |
| Soft with timers C | 256.2 | 255.9 | 10 | 2 | 120 |

The preceding 116.6 FPS Soft replay was already down to 118.6 FPS at
15Ã¢â‚¬â€œ20 seconds, before the measured window began. Its two normal repeats
were at 259Ã¢â‚¬â€œ263 FPS by that point. The four new runs with present timers
did not reproduce that launch. Across them, the render thread finished
122,474 of 122,474 measured frames on a performance core. No shader
compiles or links were recorded during those measured frames. The CPU
time spent issuing query copies, swapping, waiting on the pacing fence
and reading visibility results together averaged 0.052Ã¢â‚¬â€œ0.057 ms/frame.
These calls can still take longer on individual frames. The frame-boundary
CPU samples cannot establish the earlier process's scheduling history.
[Windows stall instrumentation](../../tools/fixtures/lighting_stall_diagnostics.inc)

In a separate Soft diagnostic the private game process alternated between
the two CPU classes. Excluding transitions, performance cores delivered
262.3 FPS and efficiency cores 135.7 FPS. Affinity applied
to all threads in this test process. GPU clocks remained above 2.89 GHz
in both groups. GPU use averaged 75% on performance
cores and 40% on efficiency cores. CPU occlusion
rose from 0.74 to 2.04 ms/frame in this control, while the historical
slow replay spent about 1.2 ms there. Core placement can produce a large
slowdown, but attributing that particular replay still requires a natural
recurrence with telemetry.

Two further Soft launches alternated the normal process affinity mask with
a restriction to all available performance cores. The second reversed the
order. Both used the CPU classes reported for this i7-14700K. Results
below use the logged mask and exclude transitions, then give each of
fourteen eight-second blocks equal weight. FPS is the reciprocal of each
policy's paired mean frame time. Restoring a process mask mid-game can
itself move threads. The untouched launches above are the fresh Auto control.

| Order | Restored Auto FPS | P-core-only FPS | P minus Auto (ms/frame) | 95% block-resampling interval (ms) | Blocks favouring P | E-core frame endpoints with Auto |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| First | 211.2 | 232.1 | -0.427 | -0.641 to -0.227 | 13/14 | 340/4,395 |
| Reversed | 222.2 | 255.6 | -0.588 | -0.814 to -0.384 | 14/14 | 389/4,778 |

These are process-wide tests with a hidden window on one Windows machine.
The game uses normal scheduling by default. The [E5 roadmap](modern_dynamic_shadows.md#e5--rendering-performance-and-hitch-removal)
also records the [subsequent fresh foreground launches](#fresh-windows-launches)
that kept the selected policy throughout each run.

A separate GPU diagnostic timed cube setup (including clears and copies),
world geometry, posed models and viewmodels independently. GPU values are
milliseconds per sampled frame in the second minute. Environmental depth
in this table covers the instrumented environmental model pass. CPU timings
come from the same window. CPU and GPU costs can overlap.

| Mode | GPU frames | Cube setup | World depth | Model depth | Viewmodel depth | Env. model depth | CPU world/cluster rays | CPU model rays | CPU field rebuild/fade |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Hard | 488 | 0.084 | 0.111 | 0.178 | 0.000 | 0.273 | 0.472 | 0.035 | 0.194 |
| Soft | 488 | 0.083 | 0.110 | 0.178 | 0.000 | 0.243 | 0.484 | 0.023 | 0.197 |

A separate prototype skipped the 16 KiB visibility history copy when the
cached texture already needed refreshing. An asset-free comparison matched
the original visibility, targets and texture-validity bits on all
1,462 field updates. In fourteen eight-second blocks per shadow mode,
each process alternated the original and prototype. Negative differences
favour the prototype.

| Mode | Mean difference in CPU occlusion (ms/frame) | Mean difference in frame time (ms) | Frame-time 95% block-resampling interval (ms) |
| --- | ---: | ---: | ---: |
| Hard | -0.0178 | -0.0385 | -0.0937 to +0.0174 |
| Soft | +0.0117 | +0.0114 | -0.0730 to +0.0907 |

Defer the history-copy prototype. Frame time improved in six of fourteen blocks in
each mode, and the CPU occlusion result changed direction between Hard and Soft.
The [subsequent exact-ray survey](#exact-ray-reuse-and-receiver-state)
measured reuse across updates and compared batch results against stock.

Local evidence and reproduction scripts are under `build/e5-launch-investigation/`.

#### Fixed-build combat controls for the material investigation

The renderer stayed at `07566a473` for these measurements. The fixed builds
used passive benchmark clocks with the palette prototype and pass timers
absent. All five runs used the same snowy Two Betrayals core, 19 live
Covenant, 25 seconds of warmup, 120 measured seconds, 2560Ãƒâ€”1440, and AA and
VSync off on the RTX 5070 Ti. Hard and Soft used 1024, the full sun fade
and all eight available local slots with the flashlight sharing one.

| Mode | Full FPS | Second-minute FPS | Seconds below 240 | Longest below 240 | Seconds below 360 |
| --- | ---: | ---: | ---: | ---: | ---: |
| OpenCE Classic `307a42ee04ba` | 378.2 | 360.4 | 0 | 0 | 28 |
| GulchCE Classic | 437.0 | 402.1 | 0 | 0 | 0 |
| GulchCE lighting only | 280.9 | 268.9 | 0 | 0 | 120 |
| GulchCE Hard | 246.7 | 234.5 | 50 | 22 | 120 |
| GulchCE Soft | 249.5 | 242.5 | 22 | 5 | 120 |

Each configuration had one independent live fight. Their ordering varies
between repeats. The prototypeÃ¢â‚¬â„¢s [paired frame-time comparison](#texture-setup-and-sustained-combat-frame-time)
is the comparison for the proposed code change. Local fixed reports are
`build/lighting-performance/e5-material-fixed-*`. Timing breakdowns and the
paired analysis are under `build/e5-material-submit/`.

### Exact-ray reuse and receiver state

A diagnostic build of [the lighting benchmark](../../tools/lighting_performance_build.py)
with `--ray-reuse-survey` kept a bounded table of 65,536 world rays. It compared
the origin and vector bit for bit across batches. On a match it checked the
fresh world hit, depth, leaf count and a hash of leaf order. It compared the
final depth again after intersecting moving objects. A BSP change or a change
to the breakable-surface flags invalidated the entries. The table recorded
432 repeats among 82,771,968 rays in the snowy fight (0.00052%). Evictions
make this a lower bound for that table. A separate test exercises a moving
object that changes the final depth under the same world ray. A cache of whole
ray answers has little demonstrated reuse here.

In the heavier measured minute of the original Hard and Soft profiles,
world traversal and object-cluster selection took 0.496 and 0.490 ms per
frame respectively. Rebuilding the visibility field took 0.191 and
0.195 ms. The diffuse and specular BSP light draw calls together took
0.778 and 0.720 ms on the CPU. These are inclusive CPU timings.
On the GPU, setup plus local world and model depth took 0.361 and 0.360 ms.
The environmental model depth pass took 0.271 and 0.249 ms. GPU queries
sampled every 32nd frame. CPU and GPU figures can overlap.

[Ray construction](../../port/linux/game/plasma_occlusion.c) now computes the
texel offsets once for the six faces. The
[collision batch](../../port/linux/game/plasma_collision.c) borrows the current
breakable flags until the synchronous batch ends. A same-input check found
zero bit differences in 83,165,184 final depths versus the preceding batch,
including 2,598,912 also checked against individual stock collision queries.
Alternating which batch ran first gave 13.654 s for the original and 13.603 s
for the candidate across all 54,144 batches.

The [model renderer](../../port/linux/src/d3d8_gl.c) and
[shadow receiver](../../port/linux/src/modern_shadows_gl.c) remember the last
slot mask or filter sent to each linked program. Each link starts with fresh
state. Moving lights, per-receiver texture bindings, first-person cubes and
the environmental fade still use the current draw's inputs. This is a
renderer change because the feature modules do not own linked programs.
Across the same diagnostic run, 137,190 of 12,829,386 instrumented uniform
updates needed a driver call. The other requests repeated the value already
stored by that program. This count applies to those four uniform updates.

#### Fresh Windows launches

These four visible runs used the same snowy exterior checkpoint and camera,
19 live Covenant, 2560x1440, no AA, 25 seconds of warmup and 120 measured
seconds. Both modes used 1024, the complete sun fade, independent fixture
shadows and all eight local slots with the flashlight sharing one. Runs
were uncapped. Each policy took effect before loading the map. Hard ran
Auto first and Soft ran P-cores first.

| Mode and scheduling | Average FPS | Heavier minute FPS | Below 240, total / longest (s) | Below 360, total / longest (s) |
| --- | ---: | ---: | ---: | ---: |
| Hard Auto | 297.6 | 294.9 | 0 / 0 | 120 / 120 |
| Hard P-cores only | 294.6 | 293.6 | 0 / 0 | 120 / 120 |
| Soft Auto | 298.8 | 297.8 | 0 / 0 | 120 / 120 |
| Soft P-cores only | 299.5 | 298.9 | 0 / 0 | 120 / 120 |

| Run | Foreground samples | E-core frame endpoints / all | Process mask |
| --- | ---: | ---: | --- |
| Hard auto | 24/24 | 0/35,710 | `0xfffffff` |
| Hard performance | 24/24 | 0/35,348 | `0xffff` |
| Soft auto | 24/24 | 0/35,855 | `0xfffffff` |
| Soft performance | 24/24 | 0/35,941 | `0xffff` |

Processor samples describe the render thread at frame boundaries. The
process restriction also affects workers. The counters do not track the full
execution history of every thread. One foreground launch per mode and policy
establishes behaviour on this i7-14700K and RTX 5070 Ti. Auto remains the
default.

#### Repeatable and indoor controls

The following hidden-window controls used the same fixed graphics settings.
Two Betrayals used the same checkpoint as the foreground runs. Rat Race
used its two authored lamps and fifteen bots. The CPU policy was Auto.

| Mode and scheduling | Average FPS | Heavier minute FPS | Below 240, total / longest (s) | Below 360, total / longest (s) |
| --- | ---: | ---: | ---: | ---: |
| Upstream OpenCE Classic | 373.8 | 359.0 | 0 / 0 | 36 / 18 |
| GulchCE Classic | 520.9 | 516.5 | 0 / 0 | 0 / 0 |
| Lighting before | 308.1 | 304.6 | 0 / 0 | 120 / 120 |
| Lighting after | 308.6 | 305.1 | 0 / 0 | 120 / 120 |
| Hard before | 260.8 | 259.8 | 3 / 1 | 120 / 120 |
| Hard after | 263.4 | 261.8 | 3 / 1 | 120 / 120 |
| Soft before | 260.5 | 260.3 | 5 / 1 | 120 / 120 |
| Soft after | 259.0 | 258.8 | 2 / 1 | 120 / 120 |
| Rat Race Hard before | 357.7 | 350.2 | 0 / 0 | 62 / 26 |
| Rat Race Hard after | 391.3 | 378.5 | 0 / 0 | 32 / 10 |
| Rat Race Soft before | 372.1 | 368.7 | 0 / 0 | 46 / 7 |
| Rat Race Soft after | 407.2 | 389.0 | 0 / 0 | 22 / 6 |

The paired combat executable alternated the old and new ray setup and uniform
uploads in balanced two-second intervals. The comparison uses seven complete
16-second blocks, excluding the first 100 ms after each switch. Negative
differences are faster. The interval resamples the seven blocks.

| Mode | Difference (ms/frame) | 95% interval (ms) | Blocks faster |
| --- | ---: | ---: | ---: |
| Hard | -0.030 | -0.075 to +0.002 | 5/7 |
| Soft | -0.004 | -0.032 to +0.024 | 3/7 |

#### Sidewinder reveal

These 60 FPS runs used a new private cache for first visits and copied the
map history and shader cache for warm visits. The system driver cache was
left in place. Preparation occurs during loading. Peak private memory was
sampled externally every half second. The frame clock has 1 ms resolution,
so a normally paced 17 ms frame counts in the >16.7 ms column.

| Mode and visit | Preparation (ms) | Worst reveal (ms) | P99 (ms) | >16.7 / 33 / 50 / 100 ms | Peak private (MiB) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard first | 84 | 35 | 18 | 162 / 1 / 0 / 0 | 678 |
| Hard warm | 36 | 32 | 18 | 148 / 0 / 0 / 0 | 668 |
| Soft first | 71 | 35 | 18 | 158 / 1 / 0 / 0 | 678 |
| Soft warm | 43 | 30 | 17 | 165 / 0 / 0 / 0 | 668 |

The [reveal diagnostic](../../tools/plasma_visual_run.py) compares consecutive
rendered frames at the transition to the visible Ghost. Fresh and warm runs
issued 119 draws on the first visible frame in both Hard and Soft. Each
submitted 22 texture uploads totalling 2,043,905 bytes with default settings.
Fresh runs linked 11 programs. Warm runs obtained all 11 from the program
cache. That counter includes programs already loaded at map entry. It does
not measure disk reads or driver execution time.

Separate Hard 1024 controls enabled the existing desktop preloading options
with `--reveal-texture-mib 64` and `--reveal-shader-prepare`. The loading
time below covers the instrumented material pass only. Reveal time is the
longest frame in the first four visible simulation ticks. Counts refer to
the first visible frame.

| Preparation and visit | Material pass (ms) | Worst reveal (ms) | Programs linked / obtained from cache | Texture uploads / bytes |
| --- | ---: | ---: | ---: | ---: |
| Default, first | 78 | 35 | 11 / 0 | 22 / 2,043,905 |
| Default, warm | 44 | 31 | 0 / 11 | 22 / 2,043,905 |
| Textures, 64 MiB, first | 71 | 32 | 11 / 0 | 19 / 1,650,697 |
| Textures, 64 MiB, warm | 42 | 30 | 0 / 11 | 19 / 1,650,697 |
| Shaders, first | 2984 | 33 | 7 / 0 | 22 / 2,043,905 |
| Shaders, warm | 143 | 28 | 0 / 0 | 22 / 2,043,905 |
| Both, first | 156 | 31 | 7 / 0 | 19 / 1,650,697 |
| Both, warm | 135 | 28 | 0 / 0 | 19 / 1,650,697 |

A warm shader-prepared visit eliminated first-frame program acquisition.
Texture preparation avoided three of the 22 uploads. The remaining reveal
cost warrants timing the specific streaming, preparation and draw work for
the newly visible materials. These comparisons do not isolate the cost of
each driver operation. The shipped defaults are unchanged.

The Mercury Rising 2 comparison used the same Soft 1024 approach to the
outdoor Covenant caches and the same retreat in the previous and candidate
builds. The engine saved its actual camera position and basis before and
after each capture. All 57 camera pairs passed the pose checks. On the
960x600 comparisons, the median whole-image RGB mean absolute difference
was 0.16 levels on a 0-255 scale. The largest individual channel mean was
0.85. The distant, intermediate and close views retained the sun handoff
and the nearby cast shadows. Across 20 matched Sidewinder Ghost captures,
the largest channel mean difference was 0.26. These figures include moving
effects and character animation.

The [Windows startup policy](../../port/windows/src/win32_cpu_scheduling.c)
reads the available processor classes and inherited process restrictions.
It applies the optional mask once, before map loading. The Windows playtest
switched from the inherited mask to the P-core mask and back on separate
launches. The [menu runner](../../tools/modern_shadow_menu_run.py) exercises
Save, Cancel and Defaults. A separate Windows compatibility run traversed
Xbox Two Betrayals, MCC Digsite, CE Death Island and Xbox Blood Gulch.

Foreground Hard Auto needs approximately 0.58 ms/frame less to reach 360 FPS,
and Soft Auto needs 0.57 ms less. The repeated polls and fresh launches give
little reason to expect that budget from CPU scheduling on this machine.
Measure CPU BSP collision traversal and light draw submission alongside
local and environmental GPU depth on the foreground critical path. The
paired runs above could not reliably distinguish the smaller end-to-end
difference from combat variation.

### Upstream stock control

Include the following runs in each performance comparison: upstream OpenCE Classic,
GulchCE Classic, GulchCE lighting alone and GulchCE with modern shadows
(measure Hard and Soft separately). Use the same stock flashlight in both
Classic controls. Run them sequentially on the same GPU. Record both the
120-second average and the heavier second minute. Repeat a suspected
regression in reverse engine order. An upstream result from a different map,
camera or graphics setting is a separate workload.

Pin the upstream commit and the GulchCE commit before building. With the
Windows release toolchain available, the following commands build the
[common frame clock](../../tools/lighting_stock_control.inl) into source-only
snapshots. The [stock builder](../../tools/lighting_stock_control_build.py)
uses the normal portable release configuration and records each revision,
probe hash and executable hash in its manifest. The packaged game has no
benchmark hooks.

```powershell
python tools/lighting_stock_control_build.py upstream/main build/upstream-stock
python tools/lighting_stock_control_build.py HEAD build/gulch-stock
```

Pass the same trusted local exterior core, profile copy, map data and display
settings to the [runner](../../tools/lighting_performance_run.py) for both
executables. For example, with paths in PowerShell variables:

```powershell
$common = @('--map', 'c40', '--checkpoint-root', $profileCopy, '--combat-core', $combatCore,
    '--data-root', $mapData, '--settings', $displaySettings, '--invulnerable-player',
    '--fps', '360', '--uncapped', '--warmup', '25', '--seconds', '120',
    '--resolution', '2560x1440', '--no-captures')
python tools/lighting_performance_run.py @common --enabled 0 --modern-shadows off --stock-flashlight --build-dir build/upstream-stock --label upstream-stock-1024
python tools/lighting_performance_run.py @common --enabled 0 --modern-shadows off --stock-flashlight --build-dir build/gulch-stock --label gulch-stock-1024
```

Set `anti_aliasing = "off"` and `shadow_resolution = 1024` in the settings
file's display section for parity with the existing E5 tests. Also run both
Classic controls at 128 to measure the default per-object shadow resolution.
Use a new label for each run. For the lighting/Hard/Soft rows, use the regular
GulchCE performance build that records actual shadow and lighting counters.
The [combat fixture](../../tools/lighting_combat_core.py) queues the checkpoint
load when the simulation reaches tick 62. The E5 core restores tick 480 and its
script threads, so the fixture waits for the engine's load acknowledgement
before queuing the saved camera at tick 540. Earlier runs sent both commands
after wall-clock delays from the first rendered frame. Changing when that frame
appears can change the combat workload even with the same core and 19 Covenant.
The runner checks the starting squad count and the measured camera positions.
Compare the mean scene-light and sun-caster counts alongside frame times.
Keep the map and checkpoint local.

#### First matched Windows controls

OpenCE `307a42ee04ba` and GulchCE `9af0b61c237c` used the same timing
probe, Windows PGO profile, full LTO, SDL binary and NVIDIA 621.24 driver.
The logs confirmed the requested framebuffer size, AA off, the exterior
camera and 19 living Covenant at every squad sample. Each run advanced
about 3,600 simulation ticks during the measured two minutes. Each row is
one run per engine. The 1440p Classic 1024 runs used OpenCE first, and
the 1440p Classic 128 runs used GulchCE first.

| Framebuffer and Classic shadow size | OpenCE average FPS | GulchCE average FPS | OpenCE second minute | GulchCE second minute |
| --- | ---: | ---: | ---: | ---: |
| 2560Ãƒâ€”1440, 128 | 415.8 | 428.3 | 376.3 | 380.4 |
| 2560Ãƒâ€”1440, 1024 | 379.7 | 421.3 | 364.1 | 379.9 |
| 1280Ãƒâ€”720, 1024 | 458.2 | 489.1 | 431.7 | 439.5 |

In pinned OpenCE, `halo_shadow_map_scale` sets a common size for the
Classic object targets. In the [GulchCE renderer](../../port/linux/src/d3d8_gl.c),
`halo_shadow_map_size` selects a size according to the object's projected
diameter, capped by the setting. At 1024 the player setting matches while
the texel workload can differ. The native 128 pair controls for that
difference. The timings alone cannot attribute the full gap to this path.

At 1440p with Classic 1024, the stock GulchCE second minute took 2.63 ms
per frame. Lighting alone took 3.68 ms and Hard took 4.21 ms on the same
fixture. Soft took 4.22 ms. The 360 FPS target allows 2.78 ms. Reducing
framebuffer pixels by 75% improved both stock controls, by 21% in OpenCE
and 16% in GulchCE at 1024. These measurements show resolution sensitivity.
They cannot apportion CPU submission, simulation, driver and GPU time or
establish a historical regression against an older GulchCE release.

The local stock reports are `e5-opence-stock-control` (OpenCE 1440p/1024)
and `e5-common-{gulch1024,gulch128,opence128,gulch720,opence720}` under
`build/lighting-performance/`. The saved map SHA-256 starts `b5fb3db5`,
the encounter core `a9188c28`. The [sun fade comparison](#sun-fade-playtest-comparison)
uses this workload for both playtests. The [Phase 2 roadmap](modern_dynamic_shadows.md#e5--rendering-performance-and-hitch-removal)
gives the comparison order, profiling priorities and fixed-quality targets.

### Loading, compatibility and driver checks

Four menu styles passed Save, Cancel, Defaults and restart checks. The live
map-transition probe kept Soft selected through Xbox Two Betrayals, MCC
Digsite, CE Death Island and Xbox Blood Gulch. It recorded three map tick
resets, 51 samples with active local maps, 84 with sunlight, 26 with a spot
source, and eight captures. Separate Hard and Classic transitions exercised
the same map families. This covers the tested transitions and exposed
materials. In a two-peer CE test the host selected Classic, the client selected
Hard, and the host required stock shield effects. Both peers respected that
shield setting. With scripted weapon fire, the client reported local shadow
maps in 49 samples and the host in zero. Without scripted fire an earlier
run saw no local maps on either peer and could not establish the client's
positive case. AMD hardware and a native Linux playthrough remain unverified.

In paired, warm Sidewinder hidden-Ghost runs, map preparation took 84 ms in
Hard and 82 ms in Soft. The worst reveal frame went from 38 to 36 ms in Hard
and from 38 to 35 ms in Soft. A separate first visit with no map history
measured 3.15 seconds of preparation and a 34 ms reveal. An unprepared
first visit saw a 483 ms reveal. The driver cache was not independently
cleared between trials, so that pair does not establish a cold-driver
speedup. A later warm repeat measured 77/80 ms of preparation and 36/34 ms
worst reveal in Hard/Soft. Five paired Ghost captures per mode differed by
at most 0.46 of 255 in mean absolute error per RGB channel. Some first-use
work and reveal hitches remain. The Windows playtester reported few noticeable
hitches with shadow precaching enabled. The hidden reveal still needs separate
first-visit and warm-reload measurements when the renderer changes.

The final sun receiver was also compared against its preceding implementation
in a frozen exterior pose on NVIDIA. At tick 270 both runs reported 24 sun
casters and four local shadows. Across 24 paired 1280Ãƒâ€”720 images the largest
per-channel mean absolute difference was 0.262 on a 0Ã¢â‚¬â€œ255 scale. The snow
and shield animations continue to draw while simulation is frozen.
