# E5 map-load preparation and combat performance

Modern Hard and Soft apply when a gameplay map loads. The menu may save a new
mode during play. The current level and checkpoint reverts keep the mode that
loaded with the map. Classic remains the default. This change governs Shadow
Mode in each menu style. The existing host lighting policy still applies.

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
   to a private 1×1 target for each distinct discovered shadow model program.
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
fixed exterior camera and a deathless player. The setup placed the map’s
`e3_a` and `c3_base_tier_2` AI squads before saving the encounter. Each run
restored that same state, with active enemy fire and 19 living Covenant in
the measured group. The enemies continue to simulate. This
provides a repeatable camera and starting state while combat timing can vary.
The older 271.1/277.5 FPS results used another route and cannot establish a
gain or loss here.

Use 2560×1440, AA and VSync off, release PGO and LTO, uncapped presentation,
25 seconds of warmup and 120 measured seconds. Enable all lighting categories,
sun, fixtures and the flashlight, use Shared, and fix the shadow resolution at
1024. The beam occupies one local slot while active. The separate indoor
control uses the two Rat Race fixtures. GPU and collision instrumentation
run separately from throughput measurements.


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
queues subsequent draws, giving the final figures above. Per-object draws and
multisample resolves still check for fallback. In the preceding CPU capture,
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

At 960×540 with Hard shadows still fixed at 1024, a 30-second diagnostic
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
The largest mean absolute error in any RGB channel was 0.295 on a 0–255
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
the 16³ receiver grid. For example, a cell on the edge of a face reads the
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

In separate uncapped 120-second release runs at 2560×1440, lighting alone
averaged 282.5 FPS, Hard 1024 averaged 245.3 and Soft 1024 averaged 247.2.
Both shadow modes reached eight local slots and used sunlight throughout.
The slowest frames were 12.0, 10.8 and 11.8 ms respectively. The second
minute averaged 270.0, 237.0 and 237.4 FPS. All three full-run averages
clear 240 FPS. The 360 FPS goal in this encounter remains open.

A matched NVIDIA image check froze the reference and retained build at tick
271 with seven local maps and 20 sun casters. Across 25 paired captures the
largest per-channel mean absolute difference was 1.021 on a 0–255 scale.
Snow and effects continue to animate during a simulation freeze. A separate
Soft run crossed Xbox Two Betrayals, MCC Digsite, CE Death Island and Xbox
Blood Gulch. The two-player CE host-policy check recorded zero local shadow
samples on the Classic host and 60 on the Hard client.

### Heavier combat minute

At release commit `07cb0ba16`, the same checkpoint, camera and 2560×1440
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
selects surfaces and builds temporary triangles for each pass. Measure the
cost of those operations separately from drawing. Diffuse and specular radii
can differ, so sharing a surface list needs an exact match of its inputs.
In sampled Hard world queries, 12% of rays stayed in the origin leaf and
the mean ray encountered 2.67 tree forks.

The first frozen capture pair had a camera offset. Reapplying the camera
after freezing produced identical saved camera poses in both builds.
At tick 271 with seven local maps and 20 sun casters, 21 paired captures
had a maximum per-channel mean absolute difference of 1.01 on a 0–255 scale.

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
The [combat fixture](../../tools/lighting_combat_core.py) checks the engine's
checkpoint acknowledgement, starting squad count and fixed camera. A rejected
save cannot supply a matched result. Keep the map and checkpoint local.

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
| 2560×1440, 128 | 415.8 | 428.3 | 376.3 | 380.4 |
| 2560×1440, 1024 | 379.7 | 421.3 | 364.1 | 379.9 |
| 1280×720, 1024 | 458.2 | 489.1 | 431.7 | 439.5 |

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
the encounter core `a9188c28`. Further shared-renderer profiling is parked
under E5. Resume with the matched OpenCE and GulchCE controls in this scene
to separate BSP surface selection, temporary lighting triangles and model
drawing before attributing the difference to OpenGL or a renderer change.

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
work and reveal hitches remain.

The final sun receiver was also compared against its preceding implementation
in a frozen exterior pose on NVIDIA. At tick 270 both runs reported 24 sun
casters and four local shadows. Across 24 paired 1280×720 images the largest
per-channel mean absolute difference was 0.262 on a 0–255 scale. The snow
and shield animations continue to draw while simulation is frozen.
