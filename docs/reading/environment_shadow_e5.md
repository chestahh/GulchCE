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
selects surfaces and builds temporary triangles for each pass. The
[follow-up profile](#sun-fade-playtest-comparison) measures those operations
separately from drawing. Diffuse and specular radii
can differ, so sharing a surface list needs an exact match of its inputs.
In sampled Hard world queries, 12% of rays stayed in the origin leaf and
the mean ray encountered 2.67 tree forks.

The first frozen capture pair had a camera offset. Reapplying the camera
after freezing produced identical saved camera poses in both builds.
At tick 271 with seven local maps and 20 sun casters, 21 paired captures
had a maximum per-channel mean absolute difference of 1.01 on a 0–255 scale.

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

| Mode | Uploads per frame, original → batched | Mean paired change | Faster blocks | Heavier minute FPS, original → batched¹ |
| --- | ---: | ---: | ---: | ---: |
| Lighting only | 489 → 65 | −0.121 ms | 11/14 | 242.1 → 253.2 |
| Hard 1024 | 476 → 63 | −0.075 ms | 10/14 | 216.6 → 225.7 |
| Soft 1024 | 475 → 62 | −0.087 ms | 11/14 | 216.5 → 221.5 |

¹ The paired comparison ends at second 144. These averages use seconds
85–144 and only the intervals assigned to each path. Live AI can change the
work between intervals. In Hard the 95% block-bootstrap interval for the
frame-time change was −0.159 to +0.017 ms. The Soft and lighting-only
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
any colour channel was 0.97 in Hard and 0.99 in Soft on a 0–255 scale.
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
[Pass lifetime](../../source/structures/structure_render.c) ·
[Triangle storage](../../source/rasterizer/xbox/rasterizer_xbox_draw_primitives.c) ·
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
| Dynamic lighting only | −0.036 | −0.047 | −0.104 | 7/7 |
| Hard 1024 | −0.030 | −0.039 | −0.059 | 6/7 |
| Soft 1024 | +0.042 | −0.032 | −0.081 | 5/7 |

The Hard combined comparison's 95% block-bootstrap interval spanned
−0.098 to +0.004 ms. Live AI and light counts varied within the fight.
As a sensitivity check, a within-block fit adjusted for selected and scene
light counts, and for sun caster and local map counts in shadow modes.
The combined frame-time differences were −0.071 ms for lighting alone,
−0.060 for Hard and −0.041 for Soft. These estimates depend on the workload
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
0–255 scale. Snow and shield effects continued to animate. The local logs,
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
| Dynamic lighting only | 280.9 → 293.3 | 268.3 → 284.8 | 3 → 0 | 1 → 0 | 5.48 → 5.09 |
| Hard 1024 | 242.5 → 252.4 | 231.3 → 243.1 | 54 → 19 | 35 → 8 | 6.23 → 5.82 |
| Soft 1024 | 241.7 → 248.7 | 233.4 → 239.8 | 58 → 35 | 30 → 8 | 6.37 → 5.75 |

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
per-channel mean absolute differences were 1.025 and 0.973 on the 0–255
scale. Snow and glow continued animating in the frozen view.

On MCC Mercury Rising 2, the original and integrated Windows builds traced
the same seven outdoor camera positions toward and away from the Covenant
caches in Soft 1024. The largest per-channel mean absolute difference at
960×600 was 0.179 on the 0–255 scale. These settled views check the result
at each distance. Continuous motion between samples remains unmeasured.
The Windows release, Android and web builds passed. The focused collision,
renderer and NVIDIA GPU suites passed all 292 cases. Native Linux gameplay
and AMD hardware remain unverified for these changes.

### Texture setup and sustained combat frame time

On the integrated renderer at `07566a473`, the heavier Hard 1024 minute
averaged 0.298 ms in the BSP’s lit material callbacks. Inside them, draw
preparation took 0.190 ms, including 0.118 ms in texture setup. Issuing the
indexed draw took about 0.010 ms of CPU time. These clocks rotate between
scopes and subtract a back-to-back clock measurement. They measure CPU time
spent submitting work. The GPU can continue executing after a draw returns.
[Material draws](../../source/rasterizer/xbox/rasterizer_xbox_environment.c) ·
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
| Lighting only | +0.002 ms | 6/14 | −0.043 to +0.046 ms |
| Hard 1024 | +0.016 ms | 5/14 | −0.017 to +0.050 ms |
| Soft 1024 | +0.011 ms | 7/14 | −0.068 to +0.096 ms |

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

| First → second minute | Hard 1024 | Soft 1024 |
| --- | ---: | ---: |
| Queued decal entries per frame | 450 → 1,011 | 449 → 1,007 |
| Decal batches per frame | 251 → 591 | 251 → 585 |
| Submitted decal vertices per frame | 3,985 → 8,947 | 3,972 → 8,989 |
| GPU time in decal draws | 0.219 → 0.507 ms | 0.220 → 0.503 ms |
| Visibility queries sent per frame | 23.27 → 23.33 | 23.27 → 23.33 |

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
and two measured minutes at 2560×1440 with AA and VSync off. Hard and
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
was −0.197 ms (95% block-resampling interval −0.272 to −0.126 ms).
The rendering thread sampled a performance core on all 30,869 measured frames.

With all 189 submitted marks in view on a stationary CE Death Island floor,
frame time changed from 1.086 to 1.089 ms. The 95% paired interval across
nine eight-second windows was −0.006 to +0.013 ms.

On the sampled Hard GPU replay, decal draws in the heavier minute fell
from 0.507 to 0.057 ms per frame. Local shadow depth cost 0.423 ms, and
environmental depth cost 0.259 ms. A separate Hard run measured 0.776 ms
per frame of dynamic-light occlusion on the CPU. GPU and CPU work overlap.
In the two final Hard runs, the slowest twelve one-second windows cost
0.286–0.325 ms more per frame than the middle twelve. CPU occlusion
accounted for 0.097–0.103 ms of that increase.

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
snowy Two Betrayals checkpoint on Windows at 2560×1440. Hard and Soft used
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
15–20 seconds, before the measured window began. Its two normal repeats
were at 259–263 FPS by that point. The four new runs with present timers
did not reproduce that launch. Across them, the render thread finished
122,474 of 122,474 measured frames on a performance core. No shader
compiles or links were recorded during those measured frames. The CPU
time spent issuing query copies, swapping, waiting on the pacing fence
and reading visibility results together averaged 0.052–0.057 ms/frame.
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
Covenant, 25 seconds of warmup, 120 measured seconds, 2560×1440, and AA and
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
between repeats. The prototype’s [paired frame-time comparison](#texture-setup-and-sustained-combat-frame-time)
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
casters and four local shadows. Across 24 paired 1280×720 images the largest
per-channel mean absolute difference was 0.262 on a 0–255 scale. The snow
and shield animations continue to draw while simulation is frozen.
