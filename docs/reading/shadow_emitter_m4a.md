# M4A: single-source shadow quality and performance

**Replanned on 17 November 2026. The multi-sample finite-emitter experiment is
retired, not paused. The first single-map Soft experiment has been measured on NVIDIA.**
M4A closed on 19 November 2026 with the Hard ground correction and the existing
four-tap Soft filter. M5 stability and M6 integrated acceptance remain open.

## Decision and scope

Use one emitter position per logical light for shadow-depth generation. An
omnidirectional source still needs its cubemap faces, and existing distinct
world/view receiver ownership must be preserved. This does not mean one draw
call or one texture for every case. Remove the multiplier from rendering each
of 8/32 finite-emitter positions and repeating material passes for each sample.
Evaluate each existing lighting/material pass once per logical light.

Hard mode retains crisp point-source visibility with minimal edge antialiasing.
Soft mode will test bounded percentage-closer soft shadows (PCSS): estimate
blocker distance, then vary the visibility-filter width with source size and
light/blocker/receiver distances. Depth comparisons may be repeated; geometry
and material rendering must not be repeated per filter tap. Use cube-aware
sampling and consistent distance units, including reversed-depth decoding.

PCSS is an approximation, not equivalent to integrating visibility over an
emitting volume. A single viewpoint cannot represent geometry visible only
from other parts of an extended source. Close/stuck grenades are therefore a
required decision case, not a promised fix. Source size controls a documented
shadow approximation; it must not be inferred as physical radiance from a
particle's decorative glow. Keep the illumination origin and shadow origin
consistent. Hard mode remains a point-source interpretation, not an exact
hard-edged finite-area shadow.

Technical reference: [NVIDIA PCSS integration guide](https://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf).
This is an algorithm reference, not authorization to copy third-party code;
implementation must follow repository provenance and licensing requirements.

## M4A execution order and gates

These are steps within M4A, not a new milestone sequence.

1. **Validate the hard-shadow reference.** Retain the close-caster clipping fix
   and prior source-placement, composition and geometry fixes. Recheck the
   reported stuck/ground grenade, player/NPC muzzle, Ghost and shield scenes
   using actual source origins, animated casters and receiver geometry.
   Separate calculation defects from legitimate point-source magnification
   and approximation limits. Do not blur, shrink or move a faulty silhouette
   to conceal its cause. Gate: reproducible captures and an explained cause
   for each comparison case; unresolved calculations block that case.
2. **Build the bounded single-source comparison.** Start with plasma grenades,
   using the existing shadow representation and material path. Add Soft-mode
   blocker search and bounded PCSS visibility filtering in feature-owned code.
   Hard mode remains a control. No temporal accumulation initially: motion,
   noise and shadow stability must be judged without history masking failures.
   Record actual depth submissions, material draws, texture taps, memory and
   CPU/GPU timings. Gate: demonstrate that extra source-position rendering and
   repeated material evaluation are absent, with correct disabled behavior,
   resource failure handling and lifecycle cleanup.
3. **Decide visual suitability before expanding.** Capture matched Hard/Soft
   comparisons at 30/60/120 FPS: attached and floor grenades, close/far walls,
   moving and crouched bipeds, held weapons, contact and self-shadowing. Check
   leaks, seams, noise, bands, unstable bias, disappearing shadows and pulses.
   If the single-view approximation fails the close-source cases, stop and
   review the concrete tradeoff. Do not automatically restore multi-source
   sampling, enlarge exemptions, reduce the shadow budget or accept the defect.
4. **Measure busy combat, then qualify broader integration.** Compare the
   reference and candidate in the actual snowy Two Betrayals encounter at
   2560x1440, 1024 shadow resolution and eight logical shadow lights. Target
   240+ average FPS; separately report effect-active intervals and their light
   populations so a brief expensive effect cannot hide in the whole-run mean.
   Use the same route/settings and distinguish live-AI variation from measured
   renderer cost. Expand to other supported light categories only after the
   bounded quality/performance decision passes. Check supported resolutions,
   representative Xbox/CE/MCC scenes and the full object/receiver inventory.

## Unchanged full-system requirements

- Keep the feature isolated and off by default. Preserve host policy,
  Android/GLES exclusion, AMD/NVIDIA workarounds and Xbox/CE/MCC boundaries.
- Preserve eight shadow lights; 1024 is the performance baseline. Higher
  selectable resolutions remain quality options, not a 240 FPS guarantee.
- Preserve independent per-light contributions and environmental grounding.
  Multiple shadows from distinct lights can be correct; do not globally erase
  stock/environmental shadows merely because a dynamic light exists.
- Keep players, NPCs, held/dropped weapons, grenades, corpses, vehicles, scenery
  and world receivers in scope, including eligible self-shadowing.
- Preserve Jackal/Ghost owner-and-occupant exemptions and vehicle turret
  collision-based chassis blocking. External lights still shadow these objects.
- M5 retains runtime/lifecycle stability, combined switch hitching and eligible
  MCC replacement coverage. M6 retains combined visual, performance, memory,
  platform and release acceptance. CStencil remains separate future work.

No candidate is accepted unless both its quality and measured cost are suitable.
Build success, isolated scenes or a fast interval after shadow fallback are not
acceptance. Report missing platform/device checks explicitly.

## Why the finite-emitter implementation was removed

The retired prototype generated separate depth and lighting work for multiple
source positions. Its 32-sample model retained visible bands and excessive
active cost. The last same-executable, two-minute snowy-combat comparisons were:

| Depth copies | Whole-run average | Finite-active average |
| --- | ---: | ---: |
| Full | 269.7 FPS | 62.7 FPS |
| Conservative rectangles | 266.5 FPS | 64.9 FPS |

Rectangles removed about 71% of copied pixels, but did not establish a meaningful
FPS gain. Different live combat populations prevent interpreting the small FPS
differences as an isolated gain or regression. Exact temporal depth reuse had
zero hits in its combat test. Separate shared/private depth allocations failed
in combat and were rejected; shadowless fallback FPS is invalid evidence.

The finite-emitter builders, sampling/material/depth-copy helpers, dedicated
fixtures/tests, offline extent sampler, launch controls and disposable candidate
binaries are removed. The established clipping, composition and shared-depth
fixes are retained. General benchmark detection of shadow-resource fallback is
also retained; it is useful independently of the retired experiment.

Historical reports, captures, manifests and copied-save archives remain local.
Raw runs: `build/lighting-performance/finite-rect32-on` and `finite-rect32-off`.
The superseded full report is `build/shadow-m4a-retirement/previous-m4a-report.md`;
its commands and candidate paths are historical and may name removed tools.
Removal inventory and validation are recorded in that retirement directory.
This retirement/replan introduces no replacement renderer or new acceptance
claim, and does not commit or push the existing work.

## Retirement validation

The removal passed 114 focused tests with no skips, covering retained near-source
GLSL, shared-depth ownership, composition, stencil-reference diagnostics,
benchmark reporting and disabled-feature isolation. Python compilation, runner
help and reference checks passed. All 2,168 production C/header/include inputs
match their pre-retirement hashes; no ordinary rendering behavior was changed.
Cleanup reclaimed about 643 MiB. The finite-emitter evidence directory retains
about 7.3 MiB of reports/captures, with saves and other investigation evidence
preserved separately. See `build/shadow-m4a-retirement/validation.json`.
No new game build, busy-combat run, platform acceptance, commit or push was needed
or performed for this tooling/documentation retirement.

## Rebased Hard reference, 19 November 2026

Two Sidewinder captures used the player biped, a fixed camera, and one real plasma grenade light at Hard 1024. The first grenade rested on the ground. The second stayed attached while the biped strafed and reversed. The geometry dumps at simulation ticks 115 and 130 contain the submitted pose and source position for that tick. Gameplay captures are 2560 Ã— 1440. Offline visibility comparisons sample 960 Ã— 540 with identical receivers and opaque geometry for the ray and depth-map paths.

| Grenade | Tick | Ray shadow coverage | Hard 1024 coverage | Disagreement at 1024 | Disagreement at 4096 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ground | 115 | 38.26% | 42.27% | 4.11% | 0.67% |
| Ground | 130 | 38.36% | 42.34% | 4.08% | 0.65% |
| Attached | 115 | 99.95% | 99.95% | 1 pixel | 0 pixels |
| Attached | 130 | 99.96% | 99.96% | 3 pixels | 0 pixels |

Percentages describe eligible receiver pixels for this light. The reference excludes 2,715 alpha-cutout triangles and does not reproduce final material or environmental shadow composition. At tick 130, removing the attached biped from the offline caster set reduced ray coverage from 99.96% to 57.68% on the same receivers at 640 Ã— 360. Removing only the grenade left coverage at 99.96%. The moving case therefore warrants checking source placement against the posed biped. For the ground case, inspect the 1024 disagreement pixels before changing Soft filtering.

Local evidence is in `build/shadow-hard-rebase/`. The four per-tick directories contain the replay reports and masks. `hard-and-ray.png` compares the two scenes, and `attribution.json` records the caster-removal checks.

## Hard 1024 matched ground-correction test, 19 November 2026

The research branch retains the exposed-side source for the stock plasma grenade while attached in Hard mode. The projectile root transforms the measured model-space point each render frame. The attached biped stays eligible as a caster. The ground-depth slope correction remains a benchmark candidate.

Two uncapped, two-minute Windows runs used the same snowy Two Betrayals checkpoint, input route, binary and settings at 2560 Ã— 1440 on an RTX 5070 Ti. Both logged an attached grenade. Within each four-second combat interval the correction ran for two seconds and the control ran for two seconds. The second run reversed the schedule. The candidate applies a slope factor of âˆ’0.001953125 to local Hard 1024 shadow-map rendering on this GPU. All sources shared the normal eight-light shadow budget.

| Average combat FPS | Correction off | Correction on |
| --- | ---: | ---: |
| Run A | 263.8 | 264.1 |
| Run B | 259.2 | 259.6 |

The workload-matched estimate used 4,335 frames per condition, about 15% of eligible frames. Frames shared the same four-second interval and selected-light and map counts, with comparable draw and triangle counts. Turning on the correction added an estimated 0.0054 ms/frame. The 95% twelve-second block-bootstrap interval spans âˆ’0.0271 to +0.0357 ms/frame. After each FPS window an additional same-geometry GPU replay alternated which condition drew first. Across 1,176 paired map draws the order-balanced difference was âˆ’0.83 Âµs/map, with opposite signs across the two runs. These tests detected no consistent cost and both conditions met the average FPS target.

The earlier NVIDIA ground comparison still shows a contact tradeoff. At tick 115 the ray disagreement fell from 4.12% to 0.33%, while 143 of 1,212 thin-contact samples were lost against 121 in the control. AMD hardware remains unverified. Soft mode is outside this experiment.

The analysis and build recipe are in `build/ground-hard-matched/`. Raw frames, GPU pairs, captures, copied-checkpoint manifests and reports are in `build/lighting-performance/matched-slope-a/` and `matched-slope-b/`.

## Ground grenade visual acceptance on NVIDIA

Keep the ground-depth correction as accepted for the Hard 1024 ground-grenade case on the RTX 5070 Ti. This acceptance run changed no shipping rendering code. Soft and AMD hardware remain outside the result.

Sidewinder, 1920 Ã— 1080, 30 simulation ticks per second. The real plasma grenade rested on the ground while the player stepped sideways and reversed. Two game captures cover ticks 114â€“160. The exact visibility comparison replays one of those captures twice and traces independent rays through the same opaque triangles. All three use the final world projection read from the renderer. The two textured game captures can differ in interpolation and plasma particles. The reference excludes 2,715 alpha-cutout triangles and shows the grenadeâ€™s direct visibility only.

Across 83,641,529 eligible receiver samples, disagreement with the rays fell from 4.075% to 0.336%. Among 273,483 thin-contact samples, 19,665 were missing with the correction off and 21,078 with it on. The largest connected newly missing near-blocker patch measured 1 Ã— 10 pixels. In the floor-height band, the corresponding maximum was one pixel, and thin misses totaled 624 off and 626 on. On consecutive ticks with a stable receiver and ray-confirmed thin shadow, the visibility changed 1,460 times off and 1,465 times on in 106,459 comparisons. Native-size inspection showed no persistent boot gap or visible new flicker through the reversal. The worst seam remains visible in the enlarged mask.

Local evidence in `build/ground-contact-acceptance/` includes the playable `hard-1024-ground.mp4`, the three-way `hard-1024-comparison.png`, native sole details in `sole-motion.png`, and per-tick `contacts.json`. The earlier matched combat test covers the performance cost.

## First single-map Soft comparison, 19 November 2026

The ordinary research build now applies the accepted Hard 1024 depth correction to the local shadow map of a settled, unparented plasma grenade. The capture build also gives its experimental Soft receiver that same depth map and the same exposed-side placement for an attached grenade. The eight shadow slots and the number of caster and material passes are preserved. The experimental filter is confined to the grenade fixture.

The experiment uses a disk radius of 0.01 world units. It reads the centre and four search taps, then up to eight filter taps, bounded to twelve cubemap texels. A separate 64-ray apparent-disk reference used the same radius and opaque geometry. The same-camera GPU replay used one 1024 depth cubemap for Hard, the existing four-tap Soft control and the candidate. Comparisons cover 640 Ã— 360 visibility at two poses per scene and exclude 2,715 alpha-cutout triangles.

| Ground grenade at tick 130 | Hard | Existing Soft | Experimental Soft |
| --- | ---: | ---: | ---: |
| Mean absolute visibility error over eligible pixels | 0.2033 | 0.2030 | 0.2078 |
| Mean error where rays gave partial shadow | 0.2855 | 0.2834 | 0.2780 |
| Majority-lit pixels where all 64 rays were blocked | 472 | 463 | 1,487 |

The candidate widened some edges in all four poses. Whole-image visibility error increased in all four. The ground scene has distinct steps and light leaks. The attached scene still misses the broad, smooth penumbra in the reference. Keep the candidate available for comparison and defer adoption. The next diagnostic is to attribute the erroneous lit ground pixels to individual blocker and filter taps, particularly at grazing receiver angles, before increasing the search footprint or sample count.

Sidewinder captures at 1920 Ã— 1080 on the RTX 5070 Ti exercised both modes and both grenade placements at 30, 60 and 120 FPS. The candidate's active-frame averages ranged from 30.6 to 30.9, 60.4 to 60.5 and 119.3 to 119.4 FPS respectively. All twelve captures recorded an active grenade shadow and zero shader or incomplete-map failures. The initial cold Hard 30 run averaged 21.9 FPS overall and 30.4 FPS while active. Its warm repeat averaged 30.0 overall. Interleaved GPU timings for the isolated receiver draw ranged from 0.235 to 0.279 ms for the candidate and 0.223 to 0.281 ms for Hard. These are single-light timings. AMD hardware and the 240+ FPS busy-combat target for the candidate remain unverified.

Build the diagnostic with `tools/shadow_grenade_soft_build.py` and replay the opaque visibility with `tools/shadow_grenade_soft_compare.py`. Local evidence is in `build/m4a-single-map-soft/`, including the gameplay comparison and the four replay sheets and measurements under `replay/`.

## Soft offset-tap leak investigation

On the two Sidewinder ground-grenade poses, the original filter introduced 1,014 and 1,015 majority-lit pixels where Hard and all 64 reference rays agreed on shadow. A GPU trace recorded the centre, four blocker-search taps and eight filter taps on those pixels. At tick 130, for pixel (298, 60) the receiver was 1.469 world units from the light. The centre saw a blocker at 0.197. All eight offset filter taps saw geometry at roughly 0.196â€“0.200 but voted lit. Four rays faced away from the receiver plane and four intersected its extension before the blocker.

The diagnostic shader now compares offset blocker depths with the original receiver projected onto the sampled texel ray. It uses the plane only to identify a hit on the receiving surface itself. Measuring this residual along the surface normal retains grazing contacts without the visible floor stripes produced by a radial-only comparison. The centre uses the original Hard comparison. No depth submissions, material passes, texture reads or shadow slots were added.

| Sidewinder pose | Hard 1024 deep leaks | Previous experiment | Corrected experiment |
| --- | ---: | ---: | ---: |
| Ground, tick 115 | 481 | 1,495 | 49 |
| Ground, tick 130 | 472 | 1,487 | 43 |
| Attached, tick 115 | 13 | 16 | 12 |
| Attached, tick 130 | 21 | 21 | 21 |

Each count is over eligible opaque receiver pixels where all 64 disk rays were blocked but the filter reported over half visibility. Of the two traced ground sets, zero and three pixels respectively remained majority lit. No previously correct, fully lit reference pixel crossed into majority shadow in these four poses. Hard was pixel-identical to its baseline. At ground tick 130, mean absolute visibility error was 0.2033 for Hard, 0.2078 for previous Soft and 0.1907 for corrected Soft. The broad area-shadow transition remains narrower than the reference. The captured single-light receiver pass does not establish busy-combat performance. AMD hardware remains unverified.

Keep the correction in the opt-in capture build. Local evidence in `build/m4a-soft-tap-trace/` includes per-tap GPU values and receiver coordinates, the original and corrected visibility sheets, `ground-130-before-after.png` and the per-pose metrics in `verified/comparison.json`.

## Soft edge width on ground and attached plasma grenades

The Sidewinder replay used the original camera, two poses per placement, 640 Ã— 360 opaque receivers, one 1024 cubemap for each caster set and the same 0.01-world-unit disk for the 64-ray reference. Hard and the reference matched the previous replay pixel for pixel. A deterministic subset checked with 256 rays differed from the 64-ray reference by 0.0059â€“0.0064 mean absolute visibility. Alpha-cutout geometry (2,715 triangles) is outside this comparison.

The grenade casing lies close to the assumed disk centre. At tick 130 the nearest grenade surface was 0.00241 units away on the ground and 0.000495 attached. The disk used by the reference faces each receiver and can extend across the casing. Where the reference reports 10â€“90% visibility but corrected Soft returns outside that range, deleting only the grenadeâ€™s 136 opaque triangles from the ray test leaves 97.3% of the ground samples and 95.5% of the attached samples over 90% lit. This identifies near-source occlusion by the grenade. It does not establish that a receiver-facing disk is the intended shape of the luminous surface.

The blocker search and filter have distinct limits. At tick 130 the search found zero blockers at 89.2% of the missed ground samples and 33.8% of the missed attached samples. Of the remainder, the estimated filter width exceeded the 12-texel clamp at 92.0% and 99.9% respectively. Increasing search and filter footprints to 256 texels with the existing tap count raised coverage of reference partial pixels from 5.7% to 32.5% on the ground and from 2.5% to 74.8% attached. The replay shows separated silhouettes and strong bands, with attached deep leaks increasing from 21 to 30.

A second control omitted the grenade mesh from both map and rays. It preserved the biped, armour, world geometry and original receiver set. On a wall scanline at tick 130, the number of pixels with 10â€“90% visibility was:

| Grenade | Hard 1024 | Corrected Soft | Only filter cap raised to 48 | 64 rays |
| --- | ---: | ---: | ---: | ---: |
| Ground | 0 | 4 | 9 | 10 |
| Attached | 0 | 3 | 8 | 22 |

Both measurements used row 145 with 60-pixel windows starting at x=350 and x=429 respectively. These are individual edges. Across all external casters, the unchanged Soft kernel recovered 64â€“65% of reference partial pixels for the ground case and 12â€“19% for the attached case.

Retain the existing experimental defaults and eight-light budget. Next, test a reference anchored to the measured exposed luminous surface, with the casing and attached biped casting. Use that reference to judge a bounded filter for external silhouettes before any runtime change. Wider probes and caster omissions were offline controls. The local GPU evidence and replay scripts are in the main checkout under `build/m4a-shadow-edge-width/`. These single-light RTX 5070 Ti replays do not measure busy-combat FPS or AMD behaviour.

## Reference anchored to the luminous grenade surface

The default offline reference now draws 256 equal-area samples from the 32 posed triangles using the stock grenadeâ€™s `pg light` material. Barycentric positions follow the mesh. The casing centre provides a stable orientation for the alternating strip winding. Samples model uniform outward radiance, weighting visible area by projected angle and inverse squared distance. A two-millionth-world-unit offset avoids intersecting the emitting face at the ray origin. Every sample had a clear ray along its outward normal through the grenade mesh in the four calibration poses. All opaque triangles, including the luminous strip, casing and biped, remain in the actual visibility test. The original receiver-facing disk remains available to historical diagnostics.

Pixel errors measure the fraction of direct grenade light visible, excluding receivers with zero unoccluded outward emission. In the tick-130 ground and attached captures this leaves 195,388 and 173,646 eligible receivers respectively at 640 Ã— 360. The 2,715 alpha-cutout triangles excluded by the opaque replay remain outside the reference. A 1,024-sample subset check differed by 0.0010â€“0.0033 mean absolute visibility across the four captures. Independent CPU segment tests on 94 stratified receivers agreed within 0.00000032 visibility. Both casing-only and armour-only blockers were observed in each pose.

The opt-in grenade shader searches out to eight times the emitterâ€™s angular radius at the receiver, capped at 48 texels, and filters out to 20 texels. The previous experiment capped both stages at 12 texels. The new kernel keeps the existing centre, four search taps and eight filter taps. All variants shared one Hard 1024 cubemap per pose and the eight-light selection budget.

| Four held-out poses per placement | Hard 1024 | Previous experimental Soft | Tuned experimental Soft |
| --- | ---: | ---: | ---: |
| Ground, mean absolute visibility error | 0.30677 | 0.29512 | 0.29121 |
| Attached, mean absolute visibility error | 0.16924 | 0.17003 | 0.16912 |

These eight Sidewinder captures are at ticks 118, 123, 137 and 146. The tuned filter improved upon previous Soft in each. It beat Hard on all four ground poses and one of four attached poses. On the tick-130 calibration frame, Soft recovered 10.0% of the ground pixels and 0.23% of the attached pixels where the surface reference gave 10â€“90% visibility. The attached point source sees a substantially different obstruction pattern from the distributed luminous strip. Wider footprints alone did not resolve it. Retain the tuning in the diagnostic only. No game-wide quality or busy-combat performance acceptance follows from this test; AMD hardware remains unverified.

Local evidence in the primary checkout under `build/m4a-grenade-surface/` includes `surface-comparison.png`, reports under `final/`, `heldout/` and `verified/`, and the parameter sweeps. The existing `tools/shadow_grenade_soft_compare.py` command runs the new reference against the current experiment.

## Stuck plasma grenade caster rule

In Hard and Soft, a plasma grenade that sticks to an object illuminates it and omits that exact object's geometry from the grenade's shadow map. The struck object still receives shadows from other casters in that map and casts normally for other lights. The grenade's own light omits its casing in flight, on the ground and when stuck. External lights still see the casing. Other objects and level geometry remain eligible blockers. A grenade held before throwing has no impact attachment. A grenade resting on the level uses the existing ground path.

A matched Sidewinder replay at tick 130 attributed the broad floor shadow to the grenade casing. In the Hard stuck pose, removing the casing alone restored direct light at 15,427 of 18,726 blocked floor samples. The other blockers remained in the depth pass.

The attached-grenade ray comparisons above include the struck armour as a blocker and document the earlier experiment. A reference for the current rule must omit the casing for the grenade's light and the struck armour while attached.

## M4A filter decision and visual acceptance, 19 November 2026

Retain the existing four-tap Soft filter. The bounded filter gives a better ground edge, but its improvement varies with attachment pose. It also leaves most of the broad reference transition fully lit or fully shadowed. The candidate remains reproducible as a diagnostic.

The reference builder's `--reference-geometry` option records a datum ID beside each posed shadow draw. In this diagnostic capture the camera can see the entire host, its weapon and the casing. Rays start at 256 area-stratified samples on the exposed luminous strip. The depth map and reference rays both omit the casing for this light and the impact host while stuck. Both retain the held weapon, other objects and world geometry. The camera pass retains the struck armour as a receiver. Ordinary playtests use a separate build with the normal caster rules.

| Pose | Hard mean error | Existing Soft | Candidate | Candidate coverage of reference transition | Fully blocked pixels incorrectly majority-lit, existing → candidate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ground, tick 115 | 0.08970 | 0.09029 | 0.07446 | 14.1% | 448 → 30 |
| Ground, tick 130 | 0.08993 | 0.09043 | 0.07478 | 14.0% | 445 → 30 |
| Attached, tick 115 | 0.00740 | 0.00795 | 0.00884 | 33.4% | 22 → 26 |
| Attached, tick 130 | 0.01065 | 0.01069 | 0.01007 | 24.4% | 34 → 42 |

Each pose compares the three receiver filters using the same source, opaque geometry and 1024 depth cubemap. Transition coverage measures the proportion of reference pixels at 10–90% visibility that the filter also puts inside that interval. The reference omits 2,715 alpha-cutout triangles. These measurements describe direct grenade visibility.

Sidewinder ground and moving-host playtests covered Hard and Soft at 30, 60 and 120 FPS caps. The ground grenade projected the player's silhouette onto the wall. The attached grenade followed the moving body, with light continuing across the floor. Continuous recording missed cadence on one 60 FPS and one 120 FPS run. Sparse-capture repeats of those same cases averaged 59.61 and 119.20 FPS. Keep the recording results with the repeats. The scoped cross-checks included player and Marine muzzle flashes, a shield bearer and an occupied Ghost beside a parked Ghost. At tick 130 both Ghost engine maps submitted the three neighbouring vehicle draws and omitted the source hull in both modes.

Local evidence is under `build/m4a-closeout/`. The `reference-complete/` sheets and report include the host receivers. The `motion-30.jpg`, `motion-60.jpg`, `motion-120.jpg` and `representative-scenes.jpg` sheets use gameplay captures. The original Pillar of Autumn Marine fixture failed to find a floor. The successful Marine evidence is from Assault on the Control Room.

## Handheld firing lights and final combat replay

In the matched Sidewinder wall scene, omitting the shooter alone left the tall rifle silhouette. Omitting the firing rifle alone removed it and preserved the shooter's helmet and shoulder. Both modes use the latter rule for players and NPCs. The gun and shooter still receive light. Independent lights still project both. The first-person gun follows the same caster rule while the arms continue casting. Weapon-owned attachments and glows use the weapon's firing or venting state. A lit projectile uses its exact launching gun while it overlaps that gun at launch.

The retained Windows build ran the first snowy exterior encounter on Two Betrayals at 2560×1440, uncapped, with 1024 shadows and eight slots. Each recording measured 120 seconds after 25 seconds of warmup. FPS below is frames divided by elapsed frame time.

| Mode | Average FPS | First / second minute | Grenade-active seconds / FPS | Mean / maximum shadow slots | Frames over 50 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hard | 291.13 | 287.80 / 294.65 | 5.33 / 255.99 | 7.545 / 8 | 0 |
| Soft | 259.84 | 265.14 / 254.72 | 5.27 / 261.83 | 7.984 / 8 | 0 |

Both runs exceeded 240 FPS on average with zero map fallback frames. Stuck-grenade motion was tested separately on Sidewinder. The before-and-after images and per-draw ownership checks are in build/m4a-closeout/. The capture host used an NVIDIA RTX 5070 Ti. AMD hardware remains unverified.
