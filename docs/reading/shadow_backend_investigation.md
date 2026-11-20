# Shadow backend investigation and bounded stencil comparison

Status: investigation and staged follow-up implementation; not 1.0 acceptance.
The entire dynamic-shadow project remains in scope. The three diagnostic
scenes isolate failure classes; they do not restrict which objects or lights
the finished system must support.

Production baseline: `d9b0cc859aae334dab51d0e8b1dfa419074cb50d`.
The investigation preserved all production files. M1 corrects the three
feature-owned marker selectors. M2 adds the guarded environmental object-shadow
replacement described below. Experimental hooks are inserted only by diagnostic
builders, restored byte-for-byte in `finally`, and followed by an ordinary Windows
rebuild. There is no new setting, host-policy change, Android/GLES execution,
map conversion or AMD/NVIDIA workaround change.
Existing eight-light selection and default-off behavior remain intact.

## Current implementation roadmap (20 November 2026)

Build 739 ships dynamic lighting with stock shadows. The modern renderer and
local follow-up work live in this research worktree. The prototype milestone
tables in earlier documents record previous development cycles. Use the
following sequence for remaining implementation and acceptance.

| Milestone | Accepted research foundation and outstanding evidence |
| --- | --- |
| M1. Source placement | Keep world/viewmodel marker corrections. Check the actual source against the visible held weapon when diagnosing a silhouette. |
| M2. Environmental object shadows | Keep replacement of eligible stock runtime projections with fallback. Baked world lightmaps retain their own contribution. M5 owns a positive MCC replacement case. |
| M3. Per-light composition | Keep the tested diffuse/specular composition. Independent lights can cast distinct shadows, including both Ghost running lights. M6 owns positive specular evidence from gameplay. |
| M4. Caster geometry and cost | Keep posed render geometry, cutout materials, family bounds and shared world/viewmodel depth with private casters added separately. The later 254.87 FPS run supersedes the 236.2 FPS result as the retained reference. |
| M4A. Near-source quality | Closed for the retained single-map implementation. Kept the Hard ground correction and four-tap Soft after comparison with the luminous-surface reference. The firing weapon is omitted from its own firing light. The final Hard / Soft 1024 combat averages were 291.1 / 259.8 FPS with all eight slots available. |
| M5. Stability and compatibility | Settings, recovery, map transitions and a naturally eligible MCC replacement have been exercised. Cold first-use hitches are tracked as a non-blocking cost. |
| M5A. Warthog turret muzzleflash | Closed. Retain collision-based chassis blocking for the firing Warthog. The chassis-and-wheel mesh still projected large wedges during a turret sweep. |
| M6. Integrated acceptance | Open. Qualify current GulchCE integration, visual coverage, performance, memory, multiplayer and platforms before release. |

### M4A implementation order

1. Reproduce floor and stuck armed grenades, player and NPC muzzles near a wall,
   a shield bearer and two neighbouring Ghosts. Capture the light origin, posed
   caster and receiver. Check Hard against the independent ray reference where
   its opaque-triangle assumptions apply. Resolve unexplained placement,
   clipping and visibility errors before judging the Soft approximation.
2. Start with plasma grenades. Compare the existing four-tap Soft shader with
   one bounded blocker search and a distance-dependent filter (PCSS). Keep one
   emitter position per light, existing cube faces and receiver ownership.
   Decode depth consistently across cube seams. Count taps, depth submissions
   and material draws. Extra filter samples must cost texture reads only,
   without repeating geometry or material passes per sample.
3. Review matched Hard, existing Soft and candidate clips at 30/60/120 FPS.
   Require attached contacts and plausible widening with separation. Look for
   bands, leaks, shimmer and lost thin blockers in motion. A central view can
   miss blockers visible elsewhere on an extended emitter. If the stuck case
   still fails, retain the reference and review the failing clip before any
   expansion. The finite-emitter implementation is retired.
4. Compare the reference and candidate on the snowy Two Betrayals route at
   2560×1440, 1024 shadow resolution and eight available shadow slots. Use the
   established 25-second warmup and two-minute recording with matched settings.
   Report raw average FPS, emitter-active FPS and duration, occupied slots,
   fallback and recurring stalls. Require 240+ average with functioning shadows
   and review active intervals for cost hidden by the average. Expand only to
   suitable emitter classes after visual and performance review.

See the [M4A experiment record](shadow_emitter_m4a.md) for the detailed
protocol, retired candidate timings and retained evidence.

### M5 implementation order

1. Reproduce the recorded 584 ms combined switch gap. Time cold and warm
   lighting-only, environment-only and combined switches. Separate shader
   creation, resource allocation and first draw. Fix the measured cause and
   compare worst switch time and repeatability against the controls.
2. Exercise Off/Hard/Soft, all resolutions, save/cancel/defaults/persistence,
   failed allocation, unload/reload and shutdown. Verify stock shader selection,
   release of optional resources, recovery and host-policy transitions.
3. Capture an MCC caster naturally eligible for the stock runtime projection
   using Classic, Modern replacement and forced fallback in the same scene.
   Require exactly one environmental grounding contribution in each case.
   Prior MCC c40 gameplay exercised local reception below the stock replacement
   threshold. Check Xbox/CE/MCC transitions and cleanup using normal loaders.

### M5 stability and compatibility results

The initial M5 investigation reproduced a hitch when first enabling both environmental and local
shadows. At 1024 on the RTX 5070 Ti, a cold 200 ms frame spent 166.5 ms linking
one model program and 4.2 ms generating two shadow maps. Warm combined switches
took 22-24 ms. Preparing compiled programs in a separate process reduced the
initial switch to 23-25 ms, but deferred 74-104 ms frames into play. In a
diagnostic that held four completed binaries and timed their first real draws,
each draw took 54-63 ms and the frame took 261 ms. This candidate was withdrawn.
The Save/Apply comparison below tests when to prepare the visible materials.

Both menu implementations passed Save, Cancel, Defaults and restart checks.
Live Off/Hard/Soft switching covered 128 through 4096, including runs at 30,
60 and 120 FPS. A forced first allocation failure preserved ordinary lighting;
turning shadows off and back on recovered. Map transitions through Xbox Two
Betrayals, MCC Workshop Digsite, CE Death Island and Xbox Blood Gulch exercised
three map resets without a shadow exception. Host-policy and mixed-settings
checks passed on CE and late join. One Xbox co-op run showed a brief position
divergence during a scripted pickup and firing. The 80-second repeat with a
later pickup and no scripted firing passed.

On MCC Workshop campaign a30 (item 3014096841), seven naturally eligible
casters produced 1,701 environmental contributions across 243 matched frames
per mode. Each used exactly one path: stock in Classic, Modern in Hard, and
stock when environmental replacement was deliberately rejected. Local shadows
remained active during forced fallback.

Matched two-minute, uncapped snowy Two Betrayals runs on the compiler candidate
at 2560x1440 and shadow resolution 1024 averaged 282.6 FPS in Hard and 255.8
in Soft. Both reached eight active slots. These measurements cover steady
combat; the switch and first-use measurements above determine hitch acceptance.
A repeat on the retained renderer averaged 269.1 FPS in Hard and 258.7 in Soft;
both minute averages passed 240 in each mode and both runs reached eight slots.
AMD hardware remains unverified. The captures, timing breakdowns and per-mode
counts are in the worktree under build/shadow-stability-m5.

### M5 Save/Apply and first appearance

The desktop in-game settings handler applies changes before the ordinary world
draw behind the menu. An experimental one-time GPU wait after that draw was
compared with the existing behaviour on an RTX 5070 Ti at 1280x720, capped at
60 FPS, with 1024 shadow maps. On Sidewinder the same Ghost and external plasma
light were present throughout. The camera either saw the Ghost during Save or
faced away until the reveal. The fixture closed the menu two rendered frames
after Save. The Save-to-playable measurement includes those frames. The last
column measures the first rendered frame following the camera turn.

| Mode and material state | Ghost visible at Save | Save to playable | First reveal |
| --- | --- | ---: | ---: |
| Classic control | No | 47 ms | 32 ms |
| Hard, cold, ordinary Save | Yes | 261 ms | 18 ms |
| Hard, cold, experimental wait | Yes | 265 ms | 18 ms |
| Hard, cold, ordinary Save | No | 47 ms | 267 ms |
| Hard, cold, experimental wait bounded to the Save scene | No | 47 ms | 258 ms |
| Soft, cold, experimental wait | Yes | 268 ms | 18 ms |
| Soft, cold, experimental wait | No | 47 ms | 275 ms |
| Hard, warm replay with Ghost visible | Yes | 47 ms | 16 ms |

Two shadow programs first appeared in the menu draw for the visible case or in
the reveal frame for the hidden case. On the cold Soft reveal the model program
took 180 ms to link and compile, the world-lighting program took 55 ms, and the
local shadow map took 3.5 ms. A separate Blood Gulch scene required five visible
shadow programs and took 697 ms from Save to playable with the wait, or 46 ms
on warm replay. Cold probes used distinct shader sources with a zero-valued
uniform to exercise cold driver compilation without deleting the normal cache.
All captures exited normally. The cold hitches count against the harness's
whole-run 60 FPS check.

The explicit wait was dropped. The ordinary menu already prepares visible
receivers. The hidden Ghost exposed cold receiver programs after Save. Subsequent comparisons use
that reproduction and check Classic alongside both shadow modes. The matched logs and machine-readable results are under
build/shadow-apply/comparison.json.


### Model shader first use and the Shadows menu

Video Setup now opens Shadows alongside Dynamic Lighting in Stock, Classic PC,
Xbox and Modern. The shared resolution and shadow mode have one entry each.
All four styles passed both setting transactions and restart persistence. Classic
is the default. The six existing resolution values and saved preferences carry over.

The model receiver uses one local depth-comparison loop and one environmental
comparison loop. Hard evaluates one sample and Soft evaluates the same four
weighted samples. The world-light receiver retains independent texture lookups.
On Sidewinder the Ghost stayed hidden during Save and first appeared on the
scripted camera turn. At 1280×720, capped at 60 FPS with 1024 shadow maps, two
cold runs per mode and renderer gave these first-reveal frame times.

| Shadow mode | Baseline | Model optimization |
| --- | ---: | ---: |
| Hard | 259, 269 ms | 224, 236 ms |
| Soft | 257, 258 ms | 220, 222 ms |

The model program's link/compile time fell from 169–174 ms to 131–137 ms.
Save to playable remained 47–48 ms. Side-by-side Ghost captures showed the
same shadow placement. Across eight comparisons fewer than 1% of pixels varied
by more than three 8-bit levels on any channel. The live captures include moving
particle effects. The 73 synthetic NVIDIA GPU checks also passed.

Putting the comparison loop in the world pass reduced cold reveal time further,
but Soft averaged 191.5 FPS in two-minute snowy Two Betrayals combat. That
version was discarded. On the same route at 2560×1440, Hard 1024 averaged
266.4 FPS and Soft 1024 averaged 277.6 FPS with the model-only version.
The original renderer averaged 255.3 FPS in the same-session Soft control.
All three runs reached eight active shadow slots. These single combat runs
qualify the average-FPS target on the tested RTX 5070 Ti. AMD hardware remains
unverified. The residual cold hitch is a non-blocking cost. Evidence is in
`build/shadow-submenu/`, including the comparison images and raw timings.

### M5A. Warthog turret muzzleflash

Retain [collision-based chassis blocking](modern_shadow_refinement.md#collision-based-turret-chassis-blocking)
for the Warthog’s own muzzleflash. External lights continue to use the complete
vehicle geometry. The mesh candidate was confined to the test build.

On the Xbox Sidewinder Warthog, the rotating assembly spans two model regions.
The experiment separated the gun by its animated nodes, including its mount,
and kept the chassis and wheels. In two captured poses the whole vehicle
blocked 37.6% and 38.8% of the same ground grid. The gun alone blocked 30.1%
and 31.2%. The chassis and wheels alone blocked 15.6% in both poses. These
areas overlap. Removing the gun substantially reduced the footprint, but its
remaining shape still failed the visual comparison.

Matched Hard and Soft 1024 sweeps at 30, 60 and 120 FPS caps covered fire over
the hood and out to the side while the gun changed elevation. With the mesh
candidate a large angular edge crossed the exposed ground, including in Soft.
The retained collision mask gave the cleaner spill in both positions. The
nearby-caster control used a second vehicle, with the gunner still seated.
The external-light control exercised the complete Warthog independently of its
own flash. Evidence is in `build/m5a/`, including `sidewinder-comparison.jpg`,
`turret-sweep.webp` and `ray-comparison.png`.

On the RTX 5070 Ti at 2560×1440, the 30-second uncapped Sidewinder runs
averaged 699.7 FPS with collision blocking and 747.4 with the mesh during
active Hard muzzleflash frames. In Soft the corresponding figures were 653.1
and 700.4 FPS. The candidate lost on visual quality. In separate two-minute
snowy Two Betrayals runs, the retained renderer averaged 262.9 FPS at Hard
1024 and 253.7 FPS at Soft 1024. Both reached eight active slots. These are
Xbox-map measurements on NVIDIA hardware. M6 owns broader qualification.

### M6 integration and release

Preserve a recoverable snapshot of the research worktree, including uncommitted
files, before assembling a candidate against then-current testing. Testing
already contains the old shadow commits and their withdrawal. Reapply the
reviewed feature delta and audit intervening renderer changes. Compare against
current stock and lighting-only controls. Preserve host policy, off defaults,
AMD/NVIDIA fixes, Android/GLES exclusion and MCC ownership. Document necessary
shared hooks and verify the stock path incurs no modern GPU setup.

Review first- and third-person players, NPCs, corpses, held and dropped weapons,
live grenades, vehicles, cutouts and world receivers. Include weapon-on-body
and helmet self-shadowing and a positive gameplay specular case. Cover projectile,
muzzle, prong/vent, held, shield, vehicle and world lights. Check Jackal colour
spill, Ghost owner/rider exceptions, external shadows on both and vehicle
turret chassis blocking. Use matched clips at 30/60/120 FPS and representative
Xbox, CE and MCC gameplay. Judge hard silhouettes against the pstencil visual
references. CStencil remains a separate future project.

Repeat the busy-combat target on the integrated candidate. Qualify 2048/4096
for quality, memory and fallback. Scope the 240+ average target to 1024.
Compare settled memory across repeated map/mode cycles with the lighting-only
control. Exercise multiplayer join, respawn and weapon changes with mixed
client settings and host denial. Record platform CI, runtime coverage and
actual AMD/NVIDIA hardware results separately. Deliver a Windows playtest,
matched comparison clips and measured results for visual sign-off before
promoting to testing and main.

## Findings

1. **Third-person manual muzzle lights can use first-person marker poses.**
   `muzzle_lighting.c:muzzle_position` calls
   `first_person_weapon_adjust_light` without the third-person guard used by
   `source/objects/object_lights.c`. First-person poses remain live during
   shoulder-camera operation. The controlled player-wall fixture measures
   approximately 0.954 world units between the selected source and the actual
   held weapon's world marker. A capture-only world-marker control reduces
   that distance to zero. It changes the resulting silhouette substantially,
   but does **not** remove near-source magnification. `ammo_counter_lighting.c`
   and `needler_lighting.c` have the same unguarded call pattern; their runtime
   effects need separate confirmation before a shared correction is shipped.
2. **The overlapping stock character shadow is a separate runtime projection,
   not simply a baked sun shadow.**
   `source/render/render_objects.c:render_object_shadow_begin` uses the
   object's `shadow_vector` and `shadow_color`. Suppressing only this pass in
   the capture removes a separate contribution; the large modern wall
   silhouette remains. Neither shadowmaps nor stencil volumes automatically
   reconcile these two lighting contributions. Globally disabling the stock
   pass would remove unrelated ambient/world-light grounding and is not a
   demonstrated fix. Baked world lightmaps are a separate system again.
3. **The sampled player caster pose matches the visible pose.**
   The 19-node player capture reports maximum matrix error zero at ticks
   100/130/160/190/220. This rules out a pose mismatch for that specific
   reproduction, not for every asset, attachment, map family or camera mode.
4. **The Ghost genuinely supplies two point sources.**
   Its ambient-emitter profile selects both authored `running lights`
   markers. Both/left/right controls isolate their contributions. Two point
   lights can produce two shadows of a nearby object; a different visibility
   backend preserves that property. The two-Ghost fixture places the second
   Ghost on uneven terrain, so it is useful for source separation, not a
   faithful reconstruction of the user's exact vehicle placement.
5. **Changing the visibility algorithm does not change point-light geometry.**
   A low or close source magnifies and stretches blockers. The bounded
   stencil renderer and independent ray reference use the same captured
   posed opaque triangles and point positions as the map comparison. Their
   matching large silhouettes distinguish this behavior from map resolution
   artifacts. This does not establish that point emitters are a satisfactory
   visual approximation for every muzzleflash, shield, engine or grenade.

Evidence lives beneath `build/shadow-comparison/` and the explicitly named
`build/plasma-visual/*-compare-*` runs. `marker-comparison.png` shows the
original source, world marker, and world marker with stock projection removed.
The muzzle fixture invokes the real light event repeatedly at a fixed pose;
it does not simulate bullet firing. The low-source player fixture uses a
charged plasma projectile, not a plasma grenade. These controls must not be
labelled as ordinary gameplay or substituted for armed-grenade validation.

## What the stencil experiment measures

`tools/shadow_stencil_compare.py` builds closed two-sided triangle prisms,
cancels exactly coincident opposite side edges, and uses infinite extrusion
with depth-clamped z-fail stencil counting. It does not require a watertight
mesh or approximately weld distinct vertices. Analytic tests cover open
geometry, reversed winding, a camera inside the volume and shared-edge ray
roundoff. A small native GL command replay removes Python dispatch gaps
from GPU timings, and readback equality verifies that native submission
produces the same output.

The comparison uses opaque captured triangles, including alpha-tested
materials whose captured texture is fully opaque. Actual alpha cutouts are
omitted and counted in every report. It does not implement receiver-owner
exceptions, transparency, materials, full coloured illumination, stock-shadow
composition, an integrated stencil cache, native CPU silhouette generation,
skinning cost or multiplayer lifecycle. It is a backend feasibility probe,
not a complete CStencil implementation or a stencil game-FPS result.

The map comparator uses the production GLSL depth comparison, hard filtering,
1024/4096 cubemaps and conservative per-triangle cube-face culling. Both
methods have their receiver depth preparation included in the replay timer;
both exclude geometry preparation/upload and allocation. The production
renderer may batch or reuse these differently. The reference is an
independent GPU ray/triangle test with a small shared-edge numeric tolerance.
Red pixels in the masks mark disagreement, not necessarily visible artifacts
in the final coloured game image.

## Whole-project acceptance remains open

| Area | Required next evidence |
| --- | --- |
| Light sources | Correct world/viewmodel source ownership and marker space across every supported emitter, including transient muzzle/vent/projectile effects |
| Composition | Dynamic visibility removes only that light's contribution; stock projection and baked lighting are identified separately |
| Player/NPC geometry | Held weapon, helmet, limbs, attachments, corpses, dropped weapons and grenades cast/receive from applicable lights in both views |
| Vehicles | External-light silhouettes plus the agreed Jackal/Ghost owner exceptions and Warthog/Scorpion own-light chassis blocking |
| Materials | Alpha-tested cutouts, transparent effects, render permutations and deformed geometry |
| Runtime | Eight-light selection, motion/animation at 30/60/120 FPS, map/session changes, host policy, off/default stock rendering |
| Platforms | Xbox/CE/MCC asset paths, AMD/NVIDIA behavior and Android exclusion; this Windows Xbox-map investigation is not renewed cross-platform acceptance |
| Performance | Matched busy-combat 240+ average FPS with the actual chosen integrated implementation, not a mask replay extrapolation |

## Recommendation

Preserve the current implementation, correct source-space ownership, and
resolve the intended stock/dynamic shadow relationship before choosing a
replacement backend. Overlapping shadows from separate light contributions
are not by themselves proof of incorrect compositing; a per-light pixel
decomposition should establish any actual defect before that code changes. Stencil is a
credible hard-edge alternative for opaque geometry, but it does not resolve
the source/model/composition failures reproduced here. Do not add broader
owner exemptions, move sources arbitrarily, blur oversized shadows or reduce
the eight-light budget to disguise them. An integrated stencil decision
needs bounded material, lifecycle and CPU/GPU cost evidence after those
shared inputs are corrected.

## Public references and provenance

The experiment is original diagnostic code; no Doom 3 code or asset is
imported. [NVIDIA GPU Gems 3, chapter 11](https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-11-efficient-and-robust-shadow-volumes-using)
explains robust z-fail volumes, depth clamping, non-closed meshes and the
fill-rate tradeoff. The public
[dhewm3 stencil-shadow source](https://github.com/dhewm/dhewm3/blob/master/neo/renderer/tr_stencilshadow.cpp)
is a research reference for silhouette edges, caps and clipping. Neither
reference establishes performance or correctness for GulchCE; local evidence
and the stated exclusions above determine that conclusion.

## Measured results (Windows, NVIDIA RTX 5070 Ti)

The valid uncapped Two Betrayals route uses the copied pre-exterior checkpoint,
ordinary controller movement, a final `--turn-seconds 0.8`, a test-only
invulnerable player, 2560x1440, 1024 shadowmaps, 25 seconds of warmup and
120 measured seconds. All dynamic-light categories are enabled in both runs.
Screenshots at approximately 35/95/155 seconds verify the active exterior
fight is in view. No capture/replay workload or compiler ran alongside these
two measurements. Periodic screenshot frames are excluded from the pacing
column; raw averages are also retained.

| Modern shadows | Raw average FPS | Pacing average FPS | Mean / maximum shadowed lights |
| --- | ---: | ---: | ---: |
| soft | 252.94 | 253.01 | 7.93 / 8 |
| off | 308.81 | 308.92 | 0.00 / 0 |

Soft's individual minute averages were 256.31 and 249.70 FPS. This meets the
agreed average target for this encounter/sample on this machine. It is not
a minimum-FPS claim, all-map acceptance, a stencil performance result or a
new optimization. AI activity is not deterministic: selected-light counts
differ, so the on/off delta is not an isolated per-pass cost.
`production-verification.json` records that all 32 production inputs in the
retained performance-build manifest match this baseline, allowing Git LF /
checkout CRLF normalization. Its older commit label alone is not the evidence.

The initial `shadow-compare-baseline-*` and `shadow-compare-combat-*` attempts
are rejected for combat acceptance: the route faced a pillar or away from
the fight. One rejected off run was interrupted. Their logs remain diagnostic
evidence, but their higher FPS must not replace the valid exterior results.
The separate geometry-capture executable performs readbacks and its FPS is
not used for performance claims.

### GPU visibility replay

Every row below uses identical opaque inputs at 2560x1440 for all three
algorithms. Values are median GPU milliseconds over 20 native-submission
samples after three warmups. The combat rows are five exported light slots
from one captured prepare call; this is not a complete eight-light frame
replay or a sum that can be subtracted from game frame time. In particular,
slots 0/1 are small-radius sources near the camera, not a representative full-screen world
light. Reports retain source positions, geometry/camera/code hashes, omitted
cutouts, masks, sample timings and nearest caster distances.

| Captured input | Stencil ms | Map 1024 ms | Map 4096 ms | Stencil disagreement % | 1024 disagreement % | 4096 disagreement % |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| ghost-left | 0.1945 | 0.1156 | 0.1941 | 0.5614 | 0.2807 | 0.0852 |
| ghost-right | 0.1955 | 0.1152 | 0.1965 | 0.0043 | 0.1591 | 0.0372 |
| ar-world | 0.2276 | 0.1393 | 0.2592 | 0.0009 | 0.1637 | 0.0455 |
| pistol-world | 0.2826 | 0.1463 | 0.3271 | 0.0029 | 0.0812 | 0.0271 |
| low-source | 0.2446 | 0.1391 | 0.2686 | 0.0067 | 0.1495 | 0.0294 |
| grenade | 0.3618 | 0.1851 | 0.3288 | 0.0097 | 1.4863 | 0.2789 |
| combat-0 | 0.3018 | 0.1970 | 0.3147 | 0.0024 | 1.8708 | 0.5880 |
| combat-1 | 0.3143 | 0.1985 | 0.3311 | 0.0017 | 5.4706 | 2.0645 |
| combat-2 | 0.4024 | 0.2133 | 0.3864 | 0.0458 | 0.3788 | 0.0791 |
| combat-3 | 0.4322 | 0.2152 | 0.3931 | 1.2590 | 0.1849 | 0.0480 |
| combat-4 | 0.5693 | 0.2205 | 0.3869 | 6.8343 | 0.0991 | 0.0135 |

Disagreement is measured against the independent ray reference over valid
receiver pixels, not against a desired artistic silhouette. The stencil
prototype initially disagreed badly on receivers viewed from the side away
from the light. An analytic two-sided receiver test reproduced that boundary
problem. Radially moving the near caps by the same 0.0005-world-unit endpoint
tolerance used by the reference resolves that case without changing the
silhouette rays. A camera-depth polygon-offset experiment did not resolve it
and was removed. This is a numerical comparison tolerance, not an effect
softening pass or a production fix.

Residual stencil errors remain: notably the ghost-left and combat-3/4 rows
show extra surface shadowing/precision patterns. These fail prototype quality
acceptance and must not be attributed to the production renderer. The
comparison therefore supports a hard-edge feasibility result, not a claim
that this stencil implementation is universally more accurate. The correctly
matched AR/pistol/low-source/grenade silhouettes still demonstrate that
stencil does not inherently remove near-light magnification.

The current prototype is slower than the 1024 map comparator in every sampled
row. At 4096 the relative cost varies. Native CPU silhouette generation,
mesh adjacency/caching, skinning/upload and integrated engine costs remain
unmeasured. No stencil 240+ FPS claim follows from these numbers.

### Capped controls and limits

The corrected world-marker player controls pass the existing capture checks
at 30/60/120 FPS, with five tick-labelled images each. The real plasma-grenade
projectile probe was captured at the same three caps: selection and shadow
rendering are present, with no shadow shader errors. The 30/60 grenade runs
failed the overall pacing gate (27.01 / 53.92 average FPS); active-light
intervals averaged 30.28 / 60.28 FPS. The 120 run passed. Their captures
perform frequent readbacks and include cold/transient work. The cause of the
overall misses was not separately profiled; they are not clean timing passes.
This probe moves real grenade objects near a receiver, but does not qualify
the full thrown/stuck/fuse/detonation lifecycle.

18 focused diagnostic tests passed, including GPU checks on both sides of a
receiver, camera-inside volumes and shared-edge handling. The native helper
compiled with warnings treated as errors. All temporary production hooks were
restored and the ordinary Windows build completed. No production source diff
remains. These checks do not renew Linux/Android/AMD or CE/MCC runtime acceptance.

## Reproduction

Run from this worktree using its configured Windows toolchain. Capture builders
require a fresh directory and restore sources/rebuild the ordinary executable
on exit; do not run another build concurrently with them. They never modify
original maps/saves. Diagnostic runs use explicit opt-in settings only.

```text
python tools/shadow_comparison_build.py --output build/shadow-comparison/new-capture
python tools/shadow_busy_capture_build.py --output build/shadow-comparison/new-busy-capture
clang --target=x86_64-pc-windows-msvc -shared -nostdlib -fuse-ld=lld -O2 -Wall -Wextra -Werror -Wl,/NOENTRY,/NODEFAULTLIB tools/fixtures/shadow_replay_commands.c -o build/shadow-comparison/replay.dll
python tools/shadow_stencil_compare.py --capture build/plasma-visual/60-1-compare-marker-world --frame 180 --slot 0 --output build/shadow-comparison/repeat-ar --size 2560x1440 --native build/shadow-comparison/replay.dll
python -m pytest -q tools/test_shadow_stencil_compare.py tools/test_modern_shadow_geometry_gpu.py
```

Set `SHADOW_COMPARE_GPU_TEST=1` for the GPU tests. Comparison controls and
commands are retained in `build/shadow-comparison/run-marker.py`,
`run-final-controls.py`, `run-exterior.py`, `run-final-replays.py` and each
run's manifests. `SHADOW_COMPARE_WORLD_MARKER`, `SHADOW_COMPARE_NO_STOCK` and
`SHADOW_COMPARE_ENGINE` exist only in the explicit comparison binary.
The ordinary executable cannot activate them.

## Retained evidence and cleanup

Useful reports, pose/mesh/texture inputs, comparison masks and screenshots are
retained locally. BMPs were converted to PNG with pixel-hash verification;
loose test saves were archived with verified contents. Only this assignment's
verified test maps, caches and redundant builds were removed: approximately
3.43 GiB reclaimed. Original game maps/checkpoints and the ordinary
build remain intact. The comparison directory is approximately 98 MiB, with
approximately 957 MiB of supporting run evidence in the separate
visual/performance folders. These artifacts are not staged as source assets.

The deliverable adds diagnostic tools and this report only. No production fix
is included and nothing is pushed to testing. The whole dynamic-shadow 1.0
acceptance gate remains open.

## Follow-up M1: source placement and pose ownership

The original six-milestone refinement, now extended by M4A above, remains in scope: source ownership,
environmental object-shadow replacement, per-light composition/emitter modelling,
complete geometry/material coverage, runtime stability, and performance/release
acceptance. M1 does not replace the stock environmental projection, change the
point-source approximation, or constitute whole-system 1.0 acceptance.

The correction follows the stock `lights_preprocess_scene` contract:
`muzzle_position`, `ammo_counter_lighting_position` and
`needler_lighting_position` consult `third_person_active(render.local_player_index)`
before using `first_person_weapon_adjust_light`. Shoulder cameras retain
first-person poses for other engine services; their availability is not proof
that those poses describe the visible weapon. The feature selectors use the
world marker in shoulder view and retain the first-person marker in first person.
The counter also retains its correct world/viewmodel-specific gain and offset.

The three small guards remain in their existing feature modules. No shared
engine changes, new state, new settings or extra GPU work are needed. There is
no change to authored offsets, spill strength, host policy, defaults, eight-light
selection, Android exclusion or shader/driver workarounds.

### Source and geometry audit

| Path | Ownership and pose route |
| --- | --- |
| Manual muzzle pulses | Valid current weapon and parent; viewmodel only outside shoulder view; invisible/model-less turret weapon falls back to the vehicle marker |
| AR counter / Needler needles | Valid visible current weapon and holder; corrected camera selection; dropped/holstered weapons cannot borrow the holder's pose |
| Authored sword, prong, charge and grenade lights | Existing guarded stock light-preprocessing path, including the separate guarded flare placement; feature modules change profiles, not camera ownership |
| Rocket and additional Covenant projectiles | Object-owned flight lights; rendered attachment/object transforms, not first-person weapon markers |
| Jackal/stationary shields and fuel rods | World markers, including the Hunter's forearm marker rather than its model-less weapon |
| Vehicle/world emitters | Vehicle markers or interpolated world origins; particle-only flame centroids retain their separate smoothing |
| Shield glow / overshield | World object nodes, independent of weapon viewmodel markers |
| Visible world models / modern casters | Both use `object_get_node_matrices`, which provides the frame's interpolated matrices; existing CE/MCC model conversion and palettes are preserved |
| First-person shadow geometry | Existing first-person remap accessor uses the drawn animation pose; `viewmodel_is_visible` excludes shoulder view |

These are source-audit findings, not a claim that every asset/animation has been
visually accepted. Geometry/material completeness remains a later milestone.
The previously measured 19-node player pose agreement applies only to that
controlled reproduction.

### Regression method

The muzzle and emitter harnesses reproduce the retained-viewmodel problem before
the correction. Their camera checks cover first/third/first-person selection,
world gain for the AR, missing markers and dropped/remote-style weapon handling.
Removing each new camera guard must fail its corresponding negative control.
Existing lifecycle, defaults, vehicle/chassis and shadow-owner checks are retained.

`tools/lighting_source_pose_build.py` adds capture-only probes to the actual three
selectors, builds through `plasma_visual_build.py`, restores all original bytes
in `finally`, and rebuilds the ordinary Windows executable. The logs compare the
selected marker with both the world marker and retained first-person marker,
before the existing authored display/turret offsets. Diagnostic code is absent
from ordinary builds. Runtime results and remaining platform limits follow below.

### M1 results (2026-11-17)

Source correction and focused Windows validation are complete locally on
`modern-dynamic-shadows`; this is not final shadow-system acceptance or a new
240+ FPS performance qualification. No branch was pushed for this milestone.

- The final focused suite passes **84 tests**, including negative controls for
  each camera guard, actual engine first-person player ownership, eight-light
  selection, caster/viewmodel ownership, chassis and owner exceptions,
  lifecycle/defaults and Android exclusion.
- Xbox Sidewinder: AR counter and Needler first/third/first-person captures pass
  at 30, 60 and 120 FPS, using Hard shadows at 1024. The corresponding averages
  are 29.39/60.00/119.99 and 30.00/60.00/119.99 FPS. Selected marker error is zero
  against the appropriate pose in every recorded sample, before authored
  offsets. First-person shadow geometry is excluded during shoulder view and
  restored on return.
- AR firing with a persistent counter light passes the separate source/geometry
  checks at all three caps with Soft shadows. The original runner rejected the
  combination because the emitter fixture equips the AR instead of the combat
  fixture; its validation now accepts only this matching pair and still requires
  successful creation and real muzzle submissions. The existing recordings were
  checked by `lighting_source_pose_check.py --switching`; their initial runner
  failures remain in the logs. An earlier muzzle-only run had insufficient sparse
  shadow-selection samples within the short third-person interval and is not
  counted as a complete camera-geometry pass.
- Warthog and Scorpion firing controls pass at 60 FPS with collision occlusion
  active and zero raw marker error. No chassis policy or authored offset changed.
- Unowned cyborg holder captures pass crouch/turn/drop controls at 60 FPS for
  both weapons. Needler telemetry confirms world-marker selection. The AR probe
  samples the local AR first each tick, so those logs do not independently
  measure the second holder; the actual engine-helper harness covers NPC and
  remote ownership. These are unowned actors, not a live remote-client test.
- CE Death Island passes the combined AR firing/counter camera-switch control
  at 60 FPS (58.57 average), with zero recorded marker error.
- The off control selects no enhanced lights, shadow maps or occlusion updates.
- Screenshots were inspected across all three caps. Captures use a 1280x720
  output window and inherited internal render scale; these are placement/motion
  checks, not native-resolution busy-combat benchmarks. The contact sheet is
  `build/shadow-source-m1/camera-transitions.png`; original-resolution images,
  logs, manifests and source reports remain in the explicitly named run folders.

Validation gaps remain explicit. Two attempted C40 Marine fixtures lacked the
requested actor tag and are rejected. MCC Digsite failed to enter gameplay after
its loopback host connection failed, so it provides no MCC visual evidence.
The Windows release build succeeds and its ordinary executable contains no
source-probe strings. Linux runtime/build validation was unavailable (WSL is not
installed). `ninja android` and `ninja web` report unavailable targets in the
current configuration, which has no configured NDK/Emscripten toolchain. Android
exclusion tests pass, but no new Android/web full build or device run is claimed.
The GPU here is NVIDIA; no AMD device validation or new sustained-combat result
is claimed. Shader and AMD/NVIDIA workaround code is unchanged.

Self-review retained the three direct guards instead of adding a new pose API,
cache, shared renderer hook or settings state. The existing emitter lifecycles,
first-person engine animation/sound service and map-family boundaries remain
unchanged. The remaining environmental composition, near-source light modelling,
geometry/material coverage, stability and performance milestones stay open.

M1 cleanup reclaimed approximately 1.47 GiB: repeated capture executables and
private shader caches were removed, screenshots converted losslessly to PNG
with pixel equality verified, and 111 test-save files archived and hash-verified
in `build/shadow-source-m1/test-saves.zip` with their original relative paths.
The disposable diagnostic executable/symbols were removed after validation;
its manifest and rebuild tool remain. About 266 MiB of per-run evidence/saves
remain, plus the contact sheets, small manifests and ordinary Windows build.
Original maps/saves and earlier investigation artifacts were preserved.


## Follow-up M2: environmental object-shadow ownership

This milestone concerns runtime object shadows projected onto the world. It
neither removes baked world lightmaps nor invents a single global sun. The engine
already samples an environmental direction and colour for each object. Modern
Hard/Soft use those same inputs, eligibility, distance/darkness fade and
camouflage adjustment to render a directional depth map of the object and its
held/attached children. Classic/Off still execute the original projection.

The replacement is attempted immediately before the original shadow begins.
Only a complete caster and receiver pass suppresses that object's stock pass.
Invalid/unsupported geometry, exhausted capacity, missing scene targets and
resource failures leave stock responsible. Thus environmental grounding does
not depend on a local light winning one of the eight shadow slots. Separate
physical lights can still legitimately cast distinct shadows; this ownership
rule prevents duplicate *environmental* projection, not every multiple shadow.

The feature owns one reusable 2D depth texture at the shared selected shadow
resolution and one depth snapshot per rendered window. Environmental work is
bounded to 32 object attempts per window, with stock fallback beyond that bound;
attachment traversal is bounded to 128 objects. No local-light slot is removed.
Immutable mesh conversion and current interpolated node/material evaluation are
reused from Modern's caster path. The scene depth texture is copied rather than
sampled while attached for writing. A conservative screen rectangle bounds the
receiver draw. Hard uses one comparison; Soft uses four comparisons over the
same geometry. Empty depth texels never occlude.

### Necessary integration and composition boundary

- `render_objects.c`: a guarded success/fallback hook receives the already
  computed stock environmental frame, colour and radius. Recomputing these in a
  separate feature module would duplicate private engine eligibility/lighting.
- `render.c`: brackets structure lightmap rendering to identify world receivers.
- `d3d8_gl.c`: exposes the active target/depth format, scaled viewport and engine
  pixel centre; records world coverage in reserved stencil bit 7 during that
  bracket. Existing stencil comparison and bits 0/1 remain intact. The next
  ordinary draw restores cached D3D state. This metadata is unavailable through
  the feature's existing object/light interfaces.
- The feature's GL module performs the actual depth rendering and composition;
  existing shader translation, reversed-depth driver fixes and map-family
  conversion are unchanged. GLES hooks return stock/failure without allocation.
- When local lighting groups are all off but Modern remains on, frame-end
  cleanup retains the environmental resources. Turning Modern off or resetting
  the map releases them through the existing feature lifecycle.

The receiver currently follows the original BSP/world boundary. Multiplying
model colour here would also darken unrelated local lights already accumulated
in the model pass. Environmental self-shadowing on models belongs to the later
per-light composition/geometry milestones, alongside the remaining near-source,
material and silhouette issues. They remain in scope; M2 is not their sign-off.

### Validation method and investigation record

`environment_shadow_build.py` provides capture-only ownership/eligibility logs,
forced-stock fallback and opt-in GPU depth dumps. Ordinary builds contain none
of these controls. The initial Sidewinder scene did not exceed the stock
shadow-darkness threshold and is not evidence of replacement coverage. Blood
Gulch provided eligible casters. The early prototype's depth reconstruction used
the wrong clip-depth convention; it now uses Halo's 0..1 projection and the
renderer pixel centre. Explicit world coverage also prevents model colour from
being multiplied by the environmental pass.

A GPU readback established that the final receiver changes the intended target;
independent replay of captured scene/map depths produces the same silhouette.
Enlarged captures resolve grounding that was ambiguous in the wide screenshots.
One-off solid-colour and unmasked receiver probes were removed from the builder.
Their logs remain investigation evidence, not production functionality.

The focused native harness covers ownership, complete descendant poses,
short/malformed attachment data, cycles, missing maps, allocation/budget and
receiver failures, Off and non-primary targets. Actual GLES implementations link
and run without renderer/config/network dependencies. Eight hidden-GL shader
checks cover Hard/Soft, forward/reversed depth and colour versus packed
float-depth/stencil textures. These synthetic tests do not establish gameplay
performance or other GPU-vendor support.
### M2 results and remaining acceptance

M2's environmental ownership implementation is complete locally on
`modern-dynamic-shadows`. It remains off by default. This is not Modern 1.0
sign-off, and no publication is part of this milestone run.

- Windows portable Release builds succeeded. The final passive performance
  executable's 34 production-file hashes match the working source. Capture-only
  environment probes are absent from the ordinary source/build. The focused
  native, GPU, emitter-policy and Android-exclusion selection passed **79 tests**
  (`build/environment-shadow-m2/focused-tests.txt`).
- Controlled Xbox captures at 30/60/120 FPS produced 30.004/60.004/119.992 FPS
  respectively. These cover a biped, a combined vehicle/projectile fixture and
  a Ghost with local lights off. The combined vehicle fixture is elevated, so
  it is not evidence of physical chassis/ground contact. The 4096 Hard Ghost
  check also averaged 119.992 FPS at a 120 cap. Screenshots were inspected.
- Classic/Off and forced-stock fallback retain the original grounding. In the
  Ghost comparison region, only 30 pixels differed and no channel differed by
  more than one 8-bit value; this is close agreement, not byte-identical output.
- Custom Edition Death Island at 2048 Soft averaged 60.015 FPS and completed
  environmental replacements alongside local projectile shadows. MCC campaign
  gameplay (Workshop Halo: Brutality Evolved c40) averaged 59.992 FPS with local
  shadows and no incomplete-map samples. Its sampled environmental darkness
  stayed below the stock eligibility threshold, so this is MCC integration/
  fallback evidence, **not successful MCC environmental-replacement coverage**.
  Digsite's loopback connection failed before gameplay. An initial MCC campaign
  run exceeded the old 45-second runner deadline; the campaign runner now allows
  its actual startup/test duration, and the subsequent run completed.
- Three Off/Hard/Soft cycles with local lighting disabled averaged 59.710 FPS.
  Each Off interval has no completed environmental GPU pass; each Hard/Soft
  interval has completed passes, and no local cubes were allocated. The runner
  initially incorrectly required local cubes in this test. Its assertion was
  corrected and replayed against the recorded logs without another game run.
- The combined local-light plus Modern switching run exercised the transitions
  and resource release, but averaged **54.140 FPS at a 60 cap**, including a
  **584 ms gap**. It fails timing acceptance. The environmental-only control
  narrows the issue but does not prove its cause. Keep the combined switching
  hitch open in M5 runtime stability; do not hide it with a favourable rerun.

The fresh busy Two Betrayals snowy-exterior test measured 120 seconds after
25 seconds of warmup, at 2560x1440, 1024 Soft, AA off and all lighting groups on.
A native capture confirms the camera faces the active Covenant encounter.
The uncapped **raw average was 252.112 FPS**, including capture frames; minute
averages were **255.134 and 249.242 FPS**. Local shadow-map usage reached all
**eight** slots and averaged 7.867. This meets the user's 240+ average-FPS target
for this run, not a claim that every frame stays below 4.167 ms or that this
change improves performance over an independently paired baseline. Raw p99 was
5.432 ms, with one 74.753 ms stall. Peak private memory was about 724.6 MiB;
this short run cannot establish long-session memory stability. Evidence is in
`build/lighting-performance/env-m2-busy-soft/` and
`build/environment-shadow-m2/performance/manifest.json`.

M3 still owns per-light composition and emitter modelling: muzzle/grenade
near-source magnification, Ghost multi-source overlap and correct diffuse/
specular contribution. M4 still owns remaining caster silhouette/material
coverage and environmental self-shadow integration on models. M5 owns runtime
switching/lifecycle stability; M6 owns broader busy-combat, memory and platform
acceptance. Existing Jackal/Ghost owner exceptions and turret chassis blocking
were not broadened or replaced by this work.

Host-policy gates, default settings, Android/GLES exclusion, eight local slots,
map conversion ownership and AMD/NVIDIA workaround code remain intact. Testing
used an NVIDIA RTX 5070 Ti; AMD hardware, full Linux/Android/web builds and live
multiplayer/device acceptance were unavailable here. Native GLES exclusion
checks are not a substitute for those platform runs.

Final evidence includes `caps.png`, `grounding-v6.png`, `fallback-and-ce.png`,
`mcc-preview.png`, `busy-preview.png` and `run-index.json` under
`build/environment-shadow-m2/`. Exploratory failures remain in their logs;
old probes and favourable samples are not substitutes for the final checks.

M2 cleanup reclaimed approximately 2.94 GiB from its own disposable copied maps,
converted-map/shader caches and duplicate exploratory executables/symbols.
All 129 test-save files were archived with original relative paths and SHA-256
verification before removing loose copies; 117 BMP captures were converted to
PNG with pixel equality checked. Original maps/saves and earlier investigation
artifacts were untouched. Final capture/performance executables, the ordinary
Windows build, logs, manifests and diagnostic evidence remain available.
The containment-checked inventory is `build/environment-shadow-m2/cleanup-report.json`.


## Follow-up M3: per-light composition and emitter model

M3 verifies the agreed physical composition: an unblocked dynamic light adds
illumination inside an environmental shadow; blocking that light removes only
its own contribution. When it leaves range or fades, environmental grounding
remains. It does not force all lights into one rotating silhouette or erase
shadows from other physical sources.

### Implementation decision and evidence

The audit found this ordering already implemented by the combined renderer:

1. `render_window` draws the world lightmaps, then the M2 environmental object
   replacement (or stock fallback), then `lights_render_diffuse`.
2. `nv2a_pixel_shader_to_glsl` applies `modern_world` to the current light's
   attenuation texture: stage 2 for diffuse, stage 1 for specular. The original
   additive light passes remain intact. World material modulation follows the
   diffuse accumulation; specular is added afterward.
3. `model_lighting` starts with the environmental/ambient terms and adds the two
   point-light slots independently. `modern_model` gates each slot separately.
   Receiver-owner exemptions disable the corresponding visibility lookup, not
   the contribution of unrelated lights. Model environmental self-shadowing
   remains M4; multiplying finished model colour would be incorrect here.

No defect in this composition equation was demonstrated, so M3 does **not**
change production blending, light gains, source positions, shadow budgets or
owner policies. All nine audited production files match the M2 snapshot. This
is a validation and regression-coverage result, not a new visual algorithm or a
claim that the wider shadow implementation has no defects.

`test_shadow_composition_gpu.py` compiles the actual emitted model-lighting and
visibility GLSL. Eight GPU cases independently block neither/either/both coloured
point lights in Hard/Soft, check that environmental and unrelated-light terms
survive, and check receiver exemptions. Both slots and the base contribution are
measured numerically. These are synthetic pixel checks, not gameplay FPS tests.

`shadow_composition_build.py` temporarily adds readbacks around the real world
passes and delegates to the existing capture builder. Its nested restoration
returns all production bytes and rebuilds the ordinary executable. The capture
fixture moves a real charged projectile into range and back out using its normal
attenuation; it does not modify shadow direction or intensity. The recorder
preserves the captured wall-clock timing, including readback stalls, and adds a
caption bar outside the rendered image.

Actual framebuffer results at the closest-light keyframe:

| Scene | Environmentally darkened pixels | Those pixels receiving dynamic fill | Pixels darkened by dynamic diffuse |
| --- | ---: | ---: | ---: |
| Player, Soft 1024 | 3,092 | 2,395 | 0 |
| Ghost, Hard 1024 | 5,459 | 2,431 | 0 |

The readback separates these contributions before later texture/fog/effect
passes. It does not infer lighting solely from final screenshot brightness.
The remaining environmentally shadowed pixels need not all receive fill:
facing, source range and dynamic occlusion still apply. Both clips return to
environmental lighting as the source leaves range. Captures and their encoded
keyframes were inspected; the MP4 was decoded in full and its frame count
verified. The video is a controlled in-engine demonstration, not normal projectile
flight or an unedited combat recording.

### Emitter modelling and scope

Source-marker ownership remains the M1 correction. The existing light radius,
attenuation and shadow origin are shared; M3 adds no separate shadow position.
A nearby point source can physically magnify a silhouette. The prior independent
ray/stencil comparison supports that explanation; filtering, arbitrary offsets
or broad owner exclusions are not introduced to disguise it. This is still a
point-source approximation, not a newly implemented finite-area emitter.

The Ghost's two authored engine markers remain two sources. They can cast two
shadows on another object, with the agreed Ghost/occupant exception applying only
to their own emitter. External lights continue to shadow vehicles. Muzzle pulses
retain their time-based fade and Warthog/Scorpion collision-based chassis policy;
grenade and other authored effects retain their own lifetime and no-specular
flags. No stock explosion, particle or weapon behaviour is changed.

The charged-projectile video has no specular contribution. A separate rocket
specular-only control also reports template flags 3 (including no-specular) and
zero specular draws. That is eligibility evidence, **not positive gameplay
coverage of a shadowed specular highlight**. The source audit and existing
stage/extent harness cover its conditional wiring; broader material acceptance
remains open for M4/M6. The rocket recording averaged 51.83 FPS at a 60 cap while
writing 178 images and fails timing acceptance; it is not used for performance.

### Capped checks, preservation and remaining milestones

The player approach/departure fixture at 30/60/120 FPS with five keyframes
averaged **28.933 / 59.992 / 120.015 FPS**, passing the existing 5% cap tolerance.
No shadow shader failures or incomplete maps were recorded. The continuous
player video/readback run averaged 27.97 FPS and fails that timing gate; the
Ghost recording averaged 29.10 FPS. These readback-heavy videos are visual
evidence, not the busy-combat performance result.

M4 retains silhouette/material completeness and environmental model self-shadow
integration. As agreed after M2, **M5** explicitly includes a naturally eligible
MCC environmental-replacement scene and the combined Off/Hard/Soft switching
hitch with local dynamic lighting enabled. M6 rechecks the resulting compatibility,
performance and release candidate. None is relabelled complete by this pass.
The full dynamic-shadow project remains in scope, including players/NPCs,
held/dropped objects, corpses, vehicles and every supported emitter category.

Default-off behaviour, host policy, Android/GLES exclusion, AMD/NVIDIA fixes and
Xbox/CE/MCC asset ownership are unchanged because production code is unchanged.
This pass uses Windows/NVIDIA and Xbox-map gameplay. It does not renew AMD,
Linux/Android/web device or live multiplayer acceptance, nor close the known MCC
environmental-eligibility gap. The existing M2 platform exclusions remain explicit.

### M3 busy-combat result and deliverables

The fresh 120-second Two Betrayals snowy-combat run, after 25 seconds of warmup,
measured **247.023 FPS raw average** at 2560x1440, 1024 Soft, AA off, all lighting
groups on and the unchanged eight-shadow budget. Minute averages were
253.002/241.188 FPS. All eight local maps were active throughout sampled frames.
Raw p99 was 5.503 ms, worst frame 22.622 ms, and there were no frames over 50 ms.
This meets the agreed average-FPS target for this run; it is not a minimum-FPS
or long-session memory guarantee. Peak private memory was 725.9 MiB and the
minute-average private memory rose from 714.6 to 720.5 MiB; M6 stability work
remains necessary.

The passive executable from M2 was reused only after verifying all 34 production
source hashes against the current tree. No production rendering changes were
made during M3. The lower average than M2's 252.1 FPS is not evidence of a code
regression or an optimisation result: these are separate combat runs with
unchanged code, not a deterministic paired performance comparison. Evidence:
`build/lighting-performance/m3-composition-busy-soft/`.

The deliverable is `build/shadow-composition-m3/modern-shadow-composition.mp4`
(11.77 seconds, 353 decoded frames), accompanied by its JSON pass measurements.
`shadow_composition_report.py` reproduces the report and video from preserved
screenshots/logs. The video encoder was installed only into the task's build
folder; no game dependency was added. Two unused capture-only helpers were
removed during final review. Ordinary source contains no M3 readback hook, and
its Windows Release rebuild succeeded. No branch was committed or pushed.

### M3 closeout

The composition audit and regression-coverage work is complete. The final
focused suite passed **107 tests**, including eight actual emitted-GLSL GPU
composition cases. The restored ordinary Windows build reports no pending work.
A native frame from the fresh performance run was inspected and confirms active
Covenant combat in the Two Betrayals exterior. The existing blend equation was
retained because no contrary defect was demonstrated; this is not acceptance of
all reported silhouette defects, a finite-area-light implementation, or 1.0
signoff. Positive gameplay specular coverage remains explicitly open above.

No M3 readback symbols remain in production source. All 34 recorded production
source hashes match the passive M2 performance build. Host policy, default-off,
Android exclusion, driver workarounds and map-family behavior were not changed.

M3 cleanup converted 430 BMPs to PNG with pixel equality verified, archived the six runs' non-cache saves with SHA-256 verification, and removed their disposable shader caches. This reclaimed approximately 1.16 GiB. Original maps/saves, earlier milestone evidence, raw framebuffer measurements, the capture executable, logs and video are retained. Inventory: build/shadow-composition-m3/cleanup.json. The 30/60/120 keyframe contact sheet was inspected after conversion.

## Follow-up M4: caster geometry and environmental model self-shadowing

The implementation is present locally, but **M4 acceptance remains open**:
the final two-minute combat run averaged 236.2 FPS, below the agreed 240+ target.
This section supersedes earlier descriptions of M4 as unimplemented. It does
not close M4A's near-source emitter work or whole-system visual acceptance.

### Implementation and review

`modern_shadows_environment_model` in
[modern_shadows.c](../../port/linux/game/modern_shadows.c) uses the actual model
draw packet's two distant-light directions. The model-begin/end hooks in
[rasterizer_xbox_models.c](../../source/rasterizer/xbox/rasterizer_xbox_models.c)
are necessary because this packet owns the lighting for the current material
pass, including queued passes. Feature logic stays in the modern-shadow module.
The feature is gated by the existing mode/host policy, primary render target,
environment-shadow setting and valid BSP. First-person viewmodels retain their
separate projection and do not enter this environmental model path.

The root object and its bounded attachment family cast into two reusable 2D
depth maps. These are separate from the unchanged eight local-light slots.
The existing caster path supplies interpolated node transforms, inverse bind
poses, selected regions, CE/MCC node palettes, opaque surfaces and animated
alpha-tested materials. Each corresponding distant diffuse contribution is
multiplied by its own visibility; ambient and local-light contributions are
untouched. The emitted-shader GPU tests check this separation explicitly.
This does not add environmental self-shadowing to unsupported shader families
or establish positive gameplay coverage of every specular material.

`environmental_family_bounds` uses the existing render-bound service for each
family member. The previous collision sphere clipped seven of 1,082 sampled
Ghost hull vertices, reaching 1.04389 times its extent. Render-family bounds
remove that clipping: all sampled meshes in the 13 successful final geometry
captures fit their frames. Sampling is periodic, so this is evidence for the
captured meshes, not a proof that every mesh in every asset was sampled.
Held weapons can expand the family frame without arbitrary light offsets.

[modern_environment_gl.inl](../../port/linux/src/modern_environment_gl.inl)
owns allocation, bounded per-window generation, reuse, failure fallback and
cleanup. Each map retains its own framebuffer attachment, avoiding repeated
attachment changes and completeness checks. Allocation still checks errors;
failed allocation is latched until resolution change/reset. Ordinary depth
submission no longer polls `glGetError` for every map, following the existing
local-shadow submission contract. GL debug diagnostics remain available and
the environmental world receiver retains its completion/fallback check.

Short profiled runs measured CPU time attributed to environmental model work
falling from 1.84 ms to 0.028 ms after removing that synchronous poll, while
total render time changed only from about 3.89 to 3.77 ms. Much of the apparent
CPU reduction is driver waiting moving elsewhere; it is **not** a demonstrated
1.8 ms frame-time saving. Fixed framebuffers alone did not demonstrate an FPS
gain. No visual filtering, shadow-slot reduction or owner-exemption expansion
was used as an optimisation.

The final self-review covered shared hooks, emitted shaders, material eligibility,
failure paths, resource lifetime and map-family boundaries. Environment-only
model reception does not activate plasma falloff GPU resources. With the feature
off, the inspected emitted stock shader is byte-identical to the pre-M4 output
(4,436 bytes; SHA-256
`91e4b1b19aa0e7c1211239d467e0f5851d5d1ef4a2dd18bb2c8f75c0021220c7`).
No M4 capture/profiling hooks remain in production source. Host policy, defaults,
driver workarounds, Android/GLES exclusion and map conversion ownership were
not changed. This was a self-review, not an independent review.

### Validation and limits

The final focused suite passed **95 tests**. Coverage includes real feature
functions for root/attachment bounds, cycle limits, first-person/off/failure
guards and GPU-resource lifecycle; shader activation and GLES exclusion;
existing material/pose/palette checks; nine environmental GPU tests including
weighted two-bone skinning and animated alpha-cutout thresholds; and 40 actual
emitted-shader GPU composition cases across Hard/Soft and local/distant masks.

Windows/NVIDIA captures at 30/60/120 caps cover Xbox Blood Gulch players, AR and
rocket attachments, first-person preservation, Sidewinder Warthog/Scorpion/Ghost,
an Xbox c40 Elite, and CE Death Island player, corpse, dropped AR and frag pickup.
The frag pickup is not an armed-grenade lifecycle test. A CE held Needler at
2048 Hard averaged 119.71 FPS under a 120 cap. A 4096 Soft environmental-only
player capture averaged 30.00 FPS under a 30 cap with GL diagnostics enabled
and no reported GL errors. These short captures do not establish busy-combat
performance at 2048/4096. The matched 1024 Soft player control disables only
environmental visibility while retaining the same shader path; its comparison
shows the helmet/shoulder/weapon self-shadow contribution.

CE files came from `C:/Games/GulchCE/custom_maps`. MCC Workshop content came from
`E:/SteamLibrary/steamapps/workshop/content/976730`; normal game autodetection
was not changed. Isolated runners use selected package copies. The Brutality
Evolved c40 package rendered a visible Elite and held Plasma Rifle with modern
model reception active at a 60 cap. This is limited model-rendering evidence,
not naturally eligible MCC environmental ground-shadow acceptance or campaign
completion. The earlier MCC player fixture fell off its staging walkway and
is excluded from visual acceptance despite a successful process exit. Digsite's
loopback-host fixture failed to start before gameplay; it is not a rendering
pass or a demonstrated shadow defect. M5 retains eligible MCC replacement and
combined mode-switch hitch coverage.

Windows Release and the full web build passed. The web retry fixed the test
shell's Python PATH, without repository changes. A full Android build was
attempted but stopped in the existing Windows build tooling: its generated
command executes the Unix NDK launcher directly and its shell/CMake command
does not run correctly on this host. No Android device acceptance is claimed.
Linux/WSL is not installed; no Linux build or runtime pass is claimed. AMD
hardware and live multiplayer were not tested in M4. GLES harness checks and
the web build do not substitute for those unavailable runs. Original configure
arguments (`--portable --release`) were restored.

### Final busy-combat result

`build/lighting-performance/m4-final/` records a fresh 120-second Two Betrayals
snowy-combat run after 25 seconds of warmup: 2560x1440, 1024 Soft, AA off, all
lighting groups enabled, invulnerable test player and the unchanged eight-slot
budget. All 35 production source hashes matched the benchmark manifest.
The engine's frame limiter and VSync were disabled.

Raw average was **236.191 FPS**; minute averages were **235.687 / 236.839 FPS**.
All eight local shadow maps were active throughout sampled frames. Raw p99 was
5.658 ms, with one 316.777 ms frame. Excluding capture-associated frames gives
236.263 FPS and does not change the failed average-FPS gate. Peak private memory
was 722.34 MiB; first/last minute averages were 714.78/718.09 MiB. This short run
does not establish memory stability.

The initial M4 two-minute run measured 227.34 FPS. The later short profiled run
measured 246.98 FPS, but neither the improvement over the first run nor the
short run establishes a controlled end-to-end performance gain. Combat evolves
between runs. The final result is retained rather than rerunning until a pass.
Further measured performance work is required to close M4's 240+ gate. M4A,
M5 and M6 remain separate, and the full dynamic-shadow project remains in scope.

M4 cleanup covered only the 35 runs identified by this pass's manifests and
explicit follow-up captures. It converted 165 BMPs to PNG with pixel equality,
archived non-cache saves and raw geometry with SHA-256 verification before
removing loose copies, and removed obsolete diagnostic binaries and disposable
shader caches. It reclaimed 1,368.42 MiB. Original maps/saves, previous milestone
outputs, logs, reports, final capture/performance binaries and the ordinary
Windows/web builds remain. Inventory: `build/shadow-geometry-m4/cleanup.json`.
The final comparison is
`build/shadow-geometry-m4/environment-player-comparison-final.png`; particle
animation differs between the two captures and is not a shadow comparison.
No commit or push was made in this pass.


## M4 performance follow-up: controlled comparisons

The follow-up preserves the complete M4 production implementation. Both tested
optimisations were rejected because neither demonstrated an end-to-end combat
rendering improvement. The previous 236.191 FPS passive two-minute result remains
the acceptance result; M4's 240+ average-FPS gate is **not closed**. A faster short
or alternating diagnostic run is not substituted for that result. M4A, M5 and M6
remain in scope as described above.

### Environmental model cost

The diagnostic performance builder now supports `--environment-model-control`.
Its runtime controls disable only environmental model self-shadowing, retaining
the local-light shadow budget, environmental ground shadows and all lighting.
`--alternate-environment-model` alternates two-second ABBA windows within one
live encounter. The flag is rejected without a matching diagnostic manifest;
ordinary production builds contain neither the control nor its CSV logging.

At 2560x1440, 1024 Soft and eight local slots, separate 120-second runs measured
255.316 FPS with environmental model reception disabled and 179.089 FPS enabled.
These are **not a causal 76 FPS feature-cost measurement**: mean selected lights
were 10.99 and 22.00 respectively, and occlusion time rose from 0.218 to 0.403 ms.
The same checkpoint and input route did not produce the same evolving combat.

A subsequent 60-second within-session comparison measured mean renderer time
of 3.775 ms enabled versus 3.721 ms disabled, with environmental-model CPU work
of 0.030 versus 0.015 ms. These broad window means still include encounter and
switch-boundary variation; they indicate a much smaller environmental-model
cost than the separate-run FPS gap. They do not prove an exact universal cost.
Evidence: `build/lighting-performance/m4-pair-env-{off,on}/` and
`m4-alternating-profile/`, including frame/profile CSVs and source manifests.

### Rejected optimisation experiments

Two further 60-second comparisons alternated the candidate and original paths
in two-second ABBA windows in the same live combat. Analysis excluded the first
and last 120 ms of each window. GPU timings sum all queries of each kind per
sampled frame before averaging. These are diagnostic samples, not acceptance
FPS runs or identical-frame replays.

- **Opaque depth shader:** six GPU tests produced bit-identical depth to the
  existing coverage shader with weighted animated geometry; alpha-cutout checks
  also passed. The sampled local depth interval fell from 1.372 to 1.219 ms,
  but mean renderer time changed from 3.803 to 3.851 ms, and other GPU intervals
  increased. There was no demonstrated total rendering gain. The shader variant,
  batching, optional resource and experiment-specific test/control were removed.
- **Per-bone influenced-vertex bounds:** an offline audit removed 4.4�10.1% of
  triangle-face submissions in sampled animated Xbox/MCC scenes, and none in the
  sampled vehicles. Surface/seam tests remained conservative. In live combat,
  mean renderer time changed from 3.903 to 3.921 ms and the sampled local depth
  interval from 1.433 to 1.544 ms. No combat speedup was established, so the
  approximately 3 MiB maximum extra metadata and implementation were removed.

Experiment source snapshots, comparison JSON and logs remain under
`build/shadow-m4-performance/`; run data is under
`build/lighting-performance/m4-{opaque,bounds}-profile/`. No shader-quality,
light-position, shadow-budget or ownership concession was made to obtain these
measurements. The unchanged local depth passes remain the largest measured
shadow GPU interval. Further work should attribute that interval to actual
world/model submissions and fill cost before choosing another optimisation;
these results do not justify disabling environmental self-shadowing.

### Final restoration and validation

All production hashes in the initial control-build manifest match after
restoration, and the local backend matches its pre-experiment bytes exactly.
The focused final suite passes **101 tests**, including environmental ownership,
resource lifecycle, off/GLES guards, shadow geometry, actual emitted GLSL and
composition readback. The ordinary Windows Release build passes. Earlier
30/60/120 visual evidence applies to the identical production sources; no fresh
live visual or sustained-performance pass is claimed from these diagnostic runs.
Host policy, off defaults, AMD/NVIDIA workarounds and Xbox/CE/MCC ownership are
unchanged. This review is a self-review; AMD hardware remains unavailable.


Platform refresh also completed against the restored sources:

- **Android:** guest ELF, host library, SDL and staged assets compile/link checks
  passed using a local Windows adapter for the generated Unix commands. This
  adapter is confined to ignored test output; repository build tooling was not
  changed. No APK, device runtime or Android visual acceptance is claimed.
- **Linux:** all 1,022 object compilations and a cross-link/weak-and-wide-symbol
  check passed using the repository-pinned Debian i386 dependencies and an X11
  SDL cross-build on Windows. The full release SDL backend gate remains open:
  this environment lacks the required Wayland/libdecor, PipeWire/PulseAudio and
  DBus backend coverage. This is not a full Linux release-build/runtime pass.
- The prior web build covers the unchanged production source. No AMD runtime or
  live multiplayer result was added. No commit, push or CI run was requested or
  performed in this follow-up.

Logs and local platform adapters are in `build/shadow-m4-performance/`.


Follow-up cleanup verified all five test-save archives against their recorded
SHA-256 entries and removed 145.44 MiB of obsolete diagnostic executables and
the temporary Linux cross-linked binary. Manifests, measurements, captures,
experiment source snapshots, original maps/saves and current Windows/Android
builds remain. The pinned Linux dependency tree is retained (121.74 MiB) for
continuing platform checks. Inventory: `build/shadow-m4-performance/cleanup.json`;
save verification: `build/shadow-m4-performance/save-verification.json`.


## M4 local-depth attribution and shared receiver depth

The next focused pass split local depth queries into setup/clears, world
geometry, world-space models and viewmodel geometry. The diagnostic-only
`--depth-profile` builder preserves draw order, replaces the outer local query
with non-overlapping phase queries, and records submitted draws/triangles.
A 60-second c40 run after 25 seconds of warmup sampled 466 frames:

| Phase | Mean GPU interval per sampled frame | Mean draws | Mean triangles |
| --- | ---: | ---: | ---: |
| Setup/clears | 0.270 ms | � | � |
| World geometry | 0.371 ms | 588 | 271,833 |
| World-space models | 0.536 ms | 836 | 449,259 |
| Viewmodel | 0.119 ms | 81 | 69,015 |

These are instrumented GPU intervals, including submission effects, not CPU
costs or acceptance FPS. Queries sample every 32nd frame and never wait for
results. Evidence: `build/shadow-depth-profile/attribution.json` and
`build/lighting-performance/m4-depth-attribution/`.

### Retained optimisation

Nearby lights previously rendered the geometry common to world and first-person
receivers independently into both depth cubes. `modern_shadow_end` now renders
`MODERN_SHADOW_BOTH` casters once, copies the six depth faces exactly, then adds
world-only bodies/weapons and viewmodel-only geometry to their respective cubes.
Private passes skip faces with no private caster. Lights without a viewmodel
cube keep their single pass. Existing textures, mesh cache, eight-light budget,
resolution, skinning, animated cutouts, depth convention and receiver shaders
are unchanged. No new texture allocation or frame-to-frame shadow reuse occurs.
The hidden player body cannot enter the viewmodel cube through this copy.

The copy follows the existing desktop GL entry point and the Khronos
[copy-image contract](https://wikis.khronos.org/opengl/GLAPI/glCopyImageSubData)
for identically formatted cube faces. GLES remains excluded. The queue harness
checks private-caster separation and single submission of common geometry;
four real-GL checks compare complete depth arrays in all six directions, with
weighted skinning and animated cutout UVs. Both receiver results are bit-identical
to independently rendered cubes in those checks.

A 60-second same-session ABBA comparison alternated shared and independent depth
in two-second windows, excluding 120 ms at each boundary:

| Measurement | Independent depth | Shared depth |
| --- | ---: | ---: |
| Mean local-depth GPU interval | 1.425 ms | 1.183 ms |
| Mean CPU depth submission | 0.163 ms | 0.105 ms |
| Mean renderer time | 3.674 ms | 3.458 ms |
| Whole-frame average within selected windows | 252.41 FPS | 266.85 FPS |
| Mean selected lights | 10.96 | 10.93 |
| Mean occlusion time | 0.201 ms | 0.203 ms |
| Active local shadow maps | 8 | 8 |

The approximately 0.24 ms local GPU and 0.058 ms CPU-submission reductions support
retaining this optimisation. These alternating live windows are not identical
frame replays or a universal FPS guarantee. The two-minute passive acceptance
run is recorded separately. Evidence: `build/lighting-performance/m4-depth-reuse-comparison/`.
The original path is accessible only through an explicitly built diagnostic
control (`--depth-reuse-control`, runner `--depth-reuse legacy|shared|alternate`).

Review also found that unused environmental-model cleanup called two GL deletion
functions with entirely zero resource arrays. It now calls them only when it
owns names. A regression check verifies repeated unused cleanup makes no driver
calls, while allocated-resource cleanup and failure recovery remain covered.
This changes no lighting or shadow calculation.


### Passive acceptance and qualification

The fresh normal timing build (no GPU queries or depth-reuse control) completed
120 seconds after 25 seconds of warmup on the archived c40 exterior-door route:
2560x1440, Soft 1024, environmental model shadows enabled, AA off, stock model
detail, per-pixel lighting off. NVIDIA RTX 5070 Ti / driver 617.42 was the tested
hardware. The copied checkpoint and invulnerable test player preserve the active
encounter; this is a repeatable stationary combat exposure after the input route,
not a complete level playthrough.

- Raw average: **254.87 FPS** across 30,584 frames / 119.996 seconds.
- Excluding the six declared screenshot/readback frames: **254.96 FPS**.
- First/second measured minutes: **254.21 / 255.72 FPS**.
- Eight local shadow lights were active in every measured frame; selected lights
  averaged 12.92 and scene lights peaked at 25. The budget was not reduced.
- P95/P99 frame time: **4.70 / 5.14 ms**. One **313.55 ms** frame remains in both
  raw and capture-filtered results; it was not discarded as an outlier. This run
  meets the agreed average-FPS criterion, not a minimum-frame-rate guarantee.
- Process private memory averaged 713.60 MiB in the first minute and 716.83 MiB
  in the last, peaking at 726.25 MiB. This short run cannot establish long-term
  memory stability or resolve the previously tracked memory-growth question.

Evidence: `build/lighting-performance/m4-depth-reuse-acceptance/report.json`,
`frames.csv`, screenshots, settings, source-hash manifest and archived test saves.
The earlier 236.19 FPS result was a separate session, so the same-session ABBA
comparison above is the stronger evidence for attributing the gain to this change.

Final focused tests passed: **105**, including actual-GL depth equality,
composition/environment/stencil comparison, queue/lifecycle/failure cases,
shader isolation, shadow centres and surface references. Windows Release,
Android guest and web builds passed. Linux's 1,022-object compilation and
cross-link/symbol checks passed; its full release SDL backend qualification
(Wayland/libdecor, PipeWire/PulseAudio and DBus) and runtime remain untested here.
No AMD hardware, Android device, browser runtime or new live multiplayer test
was available for this pass. Host policy, default settings, Android/GLES exclusion,
map-family loading, driver workarounds and network code were not changed.

Self-review covered the complete attribution/control tooling, production queue,
GPU copy/shader contract, allocation failures, resize/reset, receiver separation,
and regression fixtures. Production changes are confined to feature-owned GL
modules: shared local depth and the unused-cleanup guard. Diagnostic variants
restore original source in `finally`; the ordinary Windows build was rebuilt
and manifests checked after restoration. No commit, push or CI was requested
or performed in this focused follow-up. M4A near-source/silhouette work, M5 MCC
environmental replacement and mode-switch lifecycle acceptance, and M6 final
visual sign-off remain separate open milestones.


### Focused visual checks and cleanup

Seven 2560x1440 first-person moving-light capture sequences completed. Actual
images were inspected, alongside active-map and shader-failure diagnostics:

| Map / setting | Cap | Measured capture FPS | Result |
| --- | ---: | ---: | --- |
| Xbox Blood Gulch, Soft 1024 | 30 | 30.00 | Pass |
| Xbox Blood Gulch, Hard 1024 | 60 | 59.99 | Pass |
| Xbox Blood Gulch, Soft 1024 | 120 | 118.63 | Pass |
| Xbox Blood Gulch, lighting/shadows off | 60 | 59.99 | No active modern maps |
| CE Death Island, Hard 2048 | 60 | 60.00 | Pass |
| MCC Brutality Evolved c40, Soft 1024 | 60 | 59.73 | Pass |
| Xbox Blood Gulch, Hard 4096 | 30 | 30.00 | Pass |

Weapon shading follows the passing light, the visible world remains intact,
and no new opaque body mask appeared across the viewmodel in these sequences.
Each sequence retained five images, with no incomplete-map or shader failures.
MCC's light was staged 0.5 units away to avoid the previously observed walkway
placement failure; it entered gameplay through the existing package loader.
These short captures exercise local shadow receiver separation, not full map
acceptance, the unresolved MCC environmental replacement cases, or a guarantee
of 240 FPS at 2048/4096. Their timing includes capture overhead.
Evidence: `build/plasma-visual/*-depth-reuse-*/`, summary and review contact
sheets in `build/shadow-depth-profile/`.

Cleanup removed 760.36 MiB of disposable test outputs: losslessly converted BMP
copies, archived loose test saves, test shader caches, obsolete attribution/A-B
executables, the capture PDB and temporary Linux cross-linked binary. All ten
save archives were SHA-256 verified, as were PNG pixel data before BMP removal.
Original maps and saves were untouched. Logs, measurements, manifests, PNGs,
regression fixtures and current normal/platform builds remain; the focused
profile folder is 59.11 MiB, including the current timing/capture executables.
`cleanup.json` records individual removals. All 35 production source hashes in
the acceptance manifest still match the final source; its profiling and A/B
control flags are false. Final `git diff --check` passed.
