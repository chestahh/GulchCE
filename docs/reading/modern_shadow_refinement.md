# Modern shadow refinement

This is the refinement plan following the reopened
[modern-shadow visual acceptance](modern_dynamic_shadows.md#visual-acceptance-reopened).
The Halo 2 pstencil references define the visual target. The implementation
remains shadowmaps; CStencil is deferred.

The user has approved owner-specific shadow exceptions for
Jackal shield lights and Ghost engine lights. Their owners remain illuminated;
other objects still cast and receive shadows from those lights. Earlier area
light experiments below remain rejected research, not production behaviour.
Milestone 4's quality and budget evidence is recorded at the end of this guide.

## Milestones

| Milestone | Work | Acceptance gate |
| --- | --- | --- |
| 1. Reproducible baseline | Fixed vehicles, shield, living/dead character and first-person scenes; matched camera/light paths and diagnostic evidence | Retained baseline captures and reproduction status for each reported problem; distinguish observations from unresolved causes |
| 2. Silhouettes and self-shadowing | Animated geometry, vehicle parts, material cutouts, corpses, body and weapon receivers | Recognisable silhouettes and geometry-correct self-shadowing under external and owned dynamic lights in Hard and Soft |
| 3. Emitter behaviour | Two-sided Jackal shield spill, Ghost engines, audit of other attached emitters | Jackal/Ghost own-light exceptions only; other receivers and external-light self-shadowing remain intact; no excessive blur hiding defects |
| 4. Quality and budget | Edge stability, bias, cube seams, transitions, stock-shadow interaction, eight-light cost and fallback | Stable 30/60/120 FPS motion checks and 240+ average FPS in busy Two Betrayals combat with the finished effects |
| 5. Acceptance and delivery | Xbox/CE/MCC, settings and lifecycle checks, combined review, matched video, testing CI and Windows playtest | Required cases and CI pass; remaining uncertainty is stated, and the reviewed playtest is delivered |

Performance checks accompany substantive changes rather than waiting until
Milestone 4. Off-default behavior, host policy, AMD/NVIDIA workarounds, Android
exclusion and map-family ownership remain requirements in every milestone.

Self-shadowing includes the actual player character and its carried geometry:
an AR can cast onto the holder's chest, arms or face when the light and weapon
positions make that possible. Helmet/visor relief must block incident light
where the geometry warrants it. Ordinary normal-facing shading on a curved
surface is distinct from a cast shadow; no artificial dark outline is required.
The same rules apply to NPCs, attached objects and eligible vehicle geometry.
Visible dropped weapons and grenade bodies also belong in the caster and
receiver scope. Pickups and thrown/armed grenade objects are separate lifecycle
cases. Transparent trails, particles and explosion sprites must not become
opaque blockers simply because their parent object emits light.
First-person arms/weapons need their own receiving/casting checks; a hidden
third-person body must not cast an unexplained silhouette onto the viewmodel.

## Milestone 1 method

The existing `modern-dynamic-shadows` branch and fetched `origin/testing` both
pointed to `bc5ca01ceafa8c0c0fc6f5fde74866cfb3974e5e` at the start of this work.
The worktree also contained the earlier, uncommitted eight-light selection
draft. The baseline retains that draft and records actual source hashes;
it is not represented as an untouched binary of the published two-light build.
Milestone 1 adds fixtures, diagnostics and documentation, not a production
shadow-quality fix.

The capture build uses normal game objects and the actual GPU renderer:

| Scene | Xbox map | Subject and illumination |
| --- | --- | --- |
| `warthog` | Sidewinder | Grounded Warthog and moving external charged plasma light |
| `ghost` | Sidewinder | Ghost and the same external-light path |
| `ghost-engine` | Sidewinder | Ghost's two authored running-light positions, without an external light |
| `jackal-wall` | Two Betrayals (`c40`) | Living Jackal facing away from a traced wall, with a verified floor; shield light only |
| `player` | Sidewinder | Actual local player with an attached AR; fixed exterior close-up and moving light at upper-body/helmet height |
| `marine` | Assault on the Control Room (`b40`) | Marine with attached AR and moving external light |
| `corpse` | Sidewinder | Biped killed through the unit death API before sampling, including the dropped weapon |
| `first-person` | Sidewinder | Actual local player's AR/arms under the moving external light |
| `dropped-weapon` | Sidewinder | Unattached AR under the moving external light |
| `frag-grenade` | Sidewinder | Frag pickup from the map's game-globals grenade definition |
| `plasma-grenade` | Sidewinder | Plasma pickup from the same authored definition table |

The moving source is a real charged plasma projectile. Only the fixture holds
its simulation lifetime and sets its repeatable positions; ordinary lighting
selection, interpolation and shadow rendering remain active. The camera is
fixed after ordinary shoulder-camera processing so the actual player body can
be inspected. Campaign AI is disabled in these controlled captures. They are
not busy combat sessions or performance qualification.

The runner records five keyframes at simulation ticks 100, 130, 160, 190 and
220. Off/Hard/Soft share subject placement and the tick-indexed light path.
Particle animation and render interpolation can differ slightly between runs;
pixel-identical screenshots are not assumed. Each run records its command,
environment, map hashes, executable hash and source manifest. Source/draw
diagnostics distinguish entry into the caster path, successful map completion
and the actual model receiver shader variant. Their presence alone does not
prove correct pixels.

### Replaying the baseline

Use the configured Windows toolchain from the repository root:

```text
python tools/plasma_visual_build.py --shadow-geometry --output-dir build/modern-refinement/m1-capture
python tools/modern_shadow_baseline.py --data-root <owned-game-data> --capture-dir build/modern-refinement/m1-capture --per-pixel --label <unique-label>
```

The second command runs the eleven scenes in Off/Hard/Soft at 2560x1440 and a
60 FPS cap. `--scenes`, `--modes` and `--fps` narrow a repeat; omit `--per-pixel`
to check the ordinary per-pixel setting disabled. Every run uses a fresh
process, isolated settings/data/save folders and a hidden window. The original
maps and saves are never edited. BMP captures are replaced by PNGs only after
lossless pixel verification. Temporary map copies and the per-run executable
copy are removed; useful logs, keyframes, diagnostics and saves are retained.

The fixture lives in [modern_shadow_capture.inc](../../tools/fixtures/modern_shadow_capture.inc)
and is inserted only by [plasma_visual_build.py](../../tools/plasma_visual_build.py).
The builder restores every instrumented source in `finally` and rebuilds the
ordinary Windows executable. The regular build never links these scene controls
or diagnostics. [modern_shadow_baseline.py](../../tools/modern_shadow_baseline.py)
drives the existing [visual runner](../../tools/plasma_visual_run.py) and retains
an index of commands, outcomes and image hashes.

### Evidence and findings

Milestone 1's baseline and triage are complete. Renderer visual acceptance
remains open: none of the findings below is represented as a production fix.

The retained baseline contains 33 successful scene/mode runs: all eleven
scenes in Off/Hard/Soft at a 60 FPS cap. Six additional runs cover the player
and Jackal scenes in Hard at 30/120 FPS, a repeated player Hard/60 run, and
player Hard/60 with ordinary per-pixel lighting disabled. Together these
produce 195 losslessly verified 2560x1440 keyframes. These are five sampled
frames per run, not continuous motion video or sustained performance tests.

All three modes have identical logged initial camera placement and
tick-indexed external-light positions for each scene. The selected 30/120 FPS
and repeat runs also match those inputs. The 49 production source hashes
recorded by the three baseline capture builds match; fixture-only fixes and
new item cases explain the different executable hashes. This is a useful
controlled comparison, not a claim that all rendered animation/particles are
pixel-deterministic.

| Case | Observation | Next investigation |
| --- | --- | --- |
| Warthog, external light | A vehicle silhouette is present, but coarse edges and poorly resolved detail remain. Extracted caster geometry includes wheels, chassis and turret. | Compare projected depth against the visible mesh and authored material cutouts before changing transforms. Repeat under its own muzzleflash. |
| Ghost, external light | External illumination produces a hull shadow; this is distinct from the engine-light failure. | Check silhouette detail, hovering pose and receiver bias under moving lights. |
| Ghost, own engine lights | Narrow, harsh blue strips and abrupt blocking under the hull reproduce in Hard and Soft. | Inspect emitter origins and finite emitting regions against nearby hull geometry. Do not exclude the entire Ghost as a caster. |
| Jackal beside wall | Hard and Soft produce a broad abrupt light/dark division instead of a convincing body silhouette against the wall. | Examine shield source placement and local blocking; the existing shield falloff is already radial, so merely removing a directional cone cannot fix this case. |
| Actual player and held AR | Both objects enter the caster path; the main opaque model shader receives shadows. Body illumination changes, but the required weapon-on-body and helmet/visor detail is not signed off. | Separate sampling/bias and material-pass coverage from caster geometry. Retain native-resolution upper-body crops. |
| Marine and held AR | Living, attached-weapon case produces a moving silhouette and uses the model receiver path. | Apply the same self-shadowing criteria as the player, including held-weapon overlap and different poses. |
| Corpse and its dropped AR | The dead biped casts a visible shadow in this controlled scene. | The blanket missing-corpse report is not reproduced here. Investigate the failing gameplay conditions rather than adding an assumed dead-object exclusion fix. |
| First-person AR/arms | The main weapon model selects the shadow receiver variant and the separate view caster map is active. | Validate fine weapon/hand self-shadowing and view/world separation; shader activation alone is not pixel acceptance. |
| Dropped AR, frag and plasma pickups | All three enter the caster path and use a shadow-aware opaque model shader. Cast shadows are visible, particularly at the side-lit keyframe. | Refine small-object quality with the general bias/resolution work. Thrown, armed, stuck and disappearing grenades still need lifecycle/motion coverage. |
| Stock character shadows | The stock projected character shadow remains alongside the dynamic contribution. | Assess their interaction separately from baked world lighting; do not remove ambient/baked illumination to force black shadows. |

The main player/AR/item receiver logged shader 11 with model shadow variant 3.
An additional AR pass logged shader 32 without a registered lighting receiver;
its material role still needs inspection. This does not establish that the
main model receiver is missing. Likewise, the caster submission diagnostic
proves entry into that path, not correctness of every triangle in the map.
Some early logs contain receiver object `-1` after a model draw when no held
weapon exists; these are not subject receiver evidence. The final fixture
filters that sentinel explicitly.

CPU ray probes of captured caster triangles found Ghost hull intersections
about 0.0054 and 0.0082 world units from its two engine-light origins, and a
Jackal arm intersection about 0.0027 units from the shield-light origin. These
are evidence of very close owner geometry, not a complete diagnosis: the
probes do not evaluate texture alpha cutouts. The first follow-up should test
placement, bias and projected depth against these captures rather than assume
all model skinning is broken. The existing object-node accessor already
selects interpolated render matrices during drawing.

### Retained local evidence

Artifacts are ignored build outputs, not repository assets. Paths below are
relative to the worktree root:

- `build/modern-refinement/m1-review/accepted-index.json`: 39 accepted runs,
  commands, source index, keyframe paths and SHA-256 hashes.
- `build/modern-refinement/m1-review/*-overview.png`: Off/Hard/Soft comparisons
  at ticks 100/160/220 for every scene. Native keyframes remain at indexed paths.
- `build/modern-refinement/m1-review/player-160.png`, `player-190.png`,
  `ghost-engine-detail.png`, `warthog-detail.png`, `frag-grenade-detail.png` and
  `dropped-weapon-detail.png`: native-pixel detail comparisons, without exposure
  adjustment or generated imagery.
- `build/modern-refinement/m1-capture`, `m1-marine-capture`, `m1-items-capture`:
  frozen capture binaries, symbols and source manifests. The final diagnostic
  cleanup build is `m1-final-capture`; it does not replace baseline binaries.
- `build/modern-refinement/m1-ghost-rays.txt`, `m1-jackal-rays.txt` and
  `ray_geometry.py`: proximity evidence and the diagnostic reader.
- `build/plasma-visual/<fps>-1-<label>-<scene>-<mode>`: logs, settings,
  executable/map/source manifests, keyframes and isolated saves. Geometry
  dumps may be stored in `shadow-geometry.zip`; extract beside the logs before
  using the reader.

The accepted baseline combines `m1-baseline` (15 runs), `m1-vehicles` (6),
`m1-marine` (3) and `m1-items` (9). Earlier failed fixture runs remain labelled
as failures: the original Marine fell because the fixture's floor ray was too
short, and the initial vehicle validator incorrectly treated zero vitality
as death for invulnerable vehicles. Their replacements verify the actual
death flag and grounded subject. An earlier pilot also moved a removed
fixture projectile; its crash was in the capture controller, not established
as a production renderer fault. The fixture now checks that handle first.

### Validation limits and handoff

The focused modern-shadow and shader-isolation suite passes 19 tests,
including two/eight-light selection and desktop/Android isolation cases.
Windows capture builds and restoration of the normal Windows executable
pass. New diagnostics and scene controls are capture-only; no production
receiver, caster, shader, map-family, host-policy or vendor-workaround changes
were made for this baseline milestone. The earlier eight-light draft remains
part of the recorded baseline and still requires final quality/performance
acceptance.

This pass uses owned Xbox maps. It is not CE/MCC runtime qualification,
multiplayer testing, a new Android runtime result, or a 240+ FPS busy-combat
result. Five keyframes at different frame caps cannot establish absence of
shimmer or popping throughout motion. Continuous motion, weapon/grenade
lifecycle cases, additional dynamic source types, full compatibility and
performance remain explicit gates in the later milestones.

Milestone 2 starts with actual geometry/depth agreement and receiver material
coverage, then weapon-on-holder and helmet self-shadowing. Keep the failing
Jackal and Ghost captures as controls for Milestone 3 emitter changes rather
than masking them with indiscriminate blur. The later user-approved Jackal/Ghost
owner-only exception supersedes the original prohibition on owner exclusions.

## Milestone 2 implementation and evidence

Modern shadows now use the existing `display.shadow_resolution` setting.
Both menu frontends offer 128, 256, 512, 1024, 2048 and 4096, with 1024
recommended for modern detail/performance. The stock default remains 128;
modern shadows and dynamic lighting remain off by default. Hard and Soft
use the selected face size, including their texel snapping and filter spacing.
Changing size releases old cubes before allocating replacements and retains
immutable mesh buffers. Allocation failure releases the partial map set and
uses existing lighting until reset or a resolution change; it does not silently
lower the selected resolution. The memory bounds are in the main shadow document.

The material review found a specific receiving gap, rather than evidence that
all model transforms need replacing. The AR's additional lit transparent pass
(shader 32 in the owned Xbox maps) was instruction-recognised as a two-light
model shader but absent from the stock per-pixel registration table. Modern
receivers now use that verified lighting metadata when necessary. The captured
pass selects pixel lighting 2 / modern shadow 3; its old registration remains 0.
Stock shader registration, opacity, emissive terms and the lighting-only path
are preserved. No vendor workaround or map-family loader was changed.

Held objects are collected through the shared object/model path, without a
weapon-name or holder-type allowlist. Each weapon uses its own interpolated
pose, material and region state. Player and NPC weapons can cast onto their
holder and receive from other geometry. The local world body/weapon pair is
excluded only from the separate first-person map, which uses the actual arms
and weapon pose; both remain world casters. Hidden models and transparent
particles are not turned into opaque blockers. The new object-owner regression
covers local and remote/player-style owners, NPC children, dropped objects,
visibility, range and malformed pose fallback.

This coverage includes all valid held weapon models, rather than a list of
AR/Needler/plasma exceptions. Weapon-to-holder, holder-to-weapon and weapon
self-shadowing use the same selected light's depth map. First-person arms
and weapons use their visible pose and remapping tables; a weapon-switch
regression checks that the model, owner and pose change together. Authored
emissive terms remain emissive, and transparent effects remain transparent.
This is a general implementation contract, not a claim that every custom
weapon and holder combination has been visually tested.

Current retained checks (Windows, NVIDIA RTX 5070 Ti):

- 24 focused renderer/selection/ownership tests pass. These include desktop and
  Android shader isolation, two/eight-light selection, actual resolution bounds,
  live cube replacement, allocation failure cleanup and mesh-cache retention.
- `m2-accept-30`, `m2-accept-60`, `m2-accept-120`: 18 successful Hard/Soft runs
  covering the actual player, Warthog and first-person AR; five native 2560x1440
  keyframes each, with explicit 1024 shadow resolution. These are capped visual
  captures, not busy-combat performance or continuous-motion acceptance.
- `m2/held-index.json`: 12 successful runs, including caster-removal controls,
  player Needler/rocket launcher, Marine sniper, Elite plasma rifle, Grunt plasma
  pistol/Needler, first-person Needler/plasma rifle, and 2048/4096 smoke captures.
  Each reported zero incomplete shadow-map samples.
- `build/modern-shadow-menu/m2-resolution-{ce,modern}`: all six values, Save,
  Cancel, Defaults and restart persistence pass through real widgets. Modern
  shadows stay off. The modern menu's native-resolution 4096 capture and 1024
  recommendation are retained in `m2-resolution-menu-visual`.
- The production receiver's analytic GPU test passes at 1024, including grazing
  planes, cube seams and independent model-light slots.

The wider frontend suite reported 45 passes and one failure in unchanged title
artwork (`title_0.png` pixel comparison), before adding the two new focused
resolution/ownership tests. The renderer and menu XML checks are reported
separately; the artwork failure was not hidden by regenerating unrelated art.
AMD workarounds are retained and reviewed, but this machine is not an AMD GPU.
Jackal and Ghost near-emitter failures remain Milestone 3 controls, not claimed
fixed by higher resolution or additional receiver coverage. Continuous edge
stability and the stock projected-shadow interaction remain Milestone 4 work.

### Performance gate (still open)

The normal release build (PGO/LTO defaults, no capture readback) at 2560x1440,
1024 shadow faces, all dynamic-light categories enabled, the archived exterior
checkpoint and the snowy route produced these 120-second measurements after
25 seconds of warmup:

| Mode | Average FPS | Mean shadowed lights | Peak shadowed lights |
| --- | ---: | ---: | ---: |
| Hard | 296.78 | 4.76 | 8 |
| Soft | 180.04 | 7.77 | 8 |

Artifacts: `build/lighting-performance/m2-final-{hard,soft}-1024`. Hard clears
the agreed 240+ average target; Soft does not. These runs carried different
combat/light workloads, so their difference cannot isolate filtering cost.
Milestone 2 is not signed off while this gate remains open. The failed case
is being profiled rather than accepted as a limitation or hidden by reducing
resolution, caster coverage or the eight-light budget.

The owned-Needler capture produced 169 native frames but missed the cap
(56.89 FPS). The same scene without readback passed at 60.01 FPS; retain the
images as visual evidence, not as a sustained-performance result. The actual
resource-failure run was revalidated against the eight-light capacity after
its old two-light cleanup assertion rejected ten correctly freed cubes.
`m2/resource-revalidation.json` records that distinction without rewriting
the original failed test log.

### Depth-pass experiment

The short diagnostic profile identified depth rendering as its largest
measured GPU category (1.294 ms per sampled frame at 4.43 mean shadowed lights).
It was a lighter encounter than the failing eight-light run, so it does not
explain that run's complete cost. The feature now renders reversed 32-bit
floating-point perspective depth with zero-to-one clipping. This removes
the fragment-depth override and radial-distance interpolation from casting.
The receiver converts sampled depth back to the original radial comparison;
receiver-plane bias, manual four-tap filtering and the selected resolution
are retained. This does not change the shared renderer's depth convention,
the stock shaders, host policy, or GLES exclusion.

The GPU quality tool now also rasterizes the production caster shaders into
all six faces and checks self-receiving planes and a receiver behind a real
occluder. Analytic depth uploads alone would not detect a caster/receiver
encoding mismatch. The original radial implementation is retained as an
ignored comparison source at `m2/radial-depth-before.c`.

`build/lighting-performance/m2-hardware-soft-1024` averaged 199.15 FPS over
120 seconds, with 7.91 mean shadowed lights and 13.28 mean selected lights.
The earlier Soft result was 180.04 FPS with 7.77 shadowed and 9.51 selected
lights. The new run is promising under a heavier lighting workload, but is
not a controlled isolated speedup measurement and still fails the 240 FPS
acceptance target. The milestone remains open; resolution, coverage and
the eight-light budget were not reduced to improve this result.

Final checks for this held-weapon/depth pass:

- All 24 focused tests pass, including first-person weapon switching and
  exact stock shader selection after disabling optional lighting/shadows.
- `m2-depth-accept-{30,60,120}` contains 12 successful Hard/Soft player and
  first-person runs at 1024, with five native keyframes each. The matched
  overview is `m2/depth-visual-comparison.png`; full-resolution images remain
  indexed. This is bounded visual coverage, not continuous-motion signoff.
- `60-1-m2-final-ce-player`, `60-1-m2-final-grunt-needler`,
  `60-1-m2-final-warthog` and `60-1-m2-mcc-needler` pass at 60 FPS, with zero
  incomplete-map samples. The MCC Digsite fixture uses its available
  `weapons\needler\mp_needler` tag and the player's spawn location. Its tag
  table contains AR-named resources but no AR weapon tag; earlier AR setup
  failures are retained as fixture failures, not renderer failures. The
  initial restricted runs also could not connect to their own localhost
  server; the successful private host run used local network access.
- `m2/quality-final-1024` and `m2/quality-before-caster` both pass the expanded
  GPU checks. Their reports identify both caster and receiver shader hashes.
- Windows capture and restored normal builds pass. `ninja android` and
  `ninja web` were attempted; their Windows launch rules fail before relevant
  compilation (`sed`/`cp`/`echo` and `emcc.bat` CreateProcess failures).
  Invoking Emscripten's Python driver with the generated compiler arguments
  successfully compiles `d3d8_gl.c`, `modern_shadows_gl.c` and
  `modern_shadows.c`. This is a Web translation-unit check, not a complete
  Web build or Android runtime result. Logs/results are under `m2`.

These changes preserve the off defaults and existing host/vendor/platform
guards. They do not close the performance gate or the later emitter-placement
and continuous-edge-stability milestones. No testing-branch publication or
final refinement playtest is claimed here.

Visual review initially flagged the Grunt fixture's striped floor. Matched
`m2-grunt-depth-control` and `m2-grunt-shadow-off` runs retain those bands with
the previous depth implementation and with modern shadows disabled. The
three-way `m2/grunt-floor-off-control.png` therefore does not establish a new
shadow-depth artifact; it also preserves a visible comparison of the holder's
self-shadowing. A separate analytic stress experiment with a 120-unit-wide
plane only 0.02 units from the light showed grazing acne in both depth
implementations. That extreme geometry remains an edge-stability case, not
a claimed fix or part of the passing finite-plane checks.

Owned BMP captures were replaced only after byte-for-byte pixel verification
of their PNG versions (`m2/png-cleanup.json`). Approximately 4.8 GiB of redundant
BMP data was removed, along with the temporary 683 MiB Emscripten cache copy.
Original maps, saves, the installed SDK and comparison binaries were retained.

### Conservative caster submission

The next optimization retains the eight-light budget and the selected map
resolution. Each immutable shadow mesh records a local bounding box and the
bones actually used by its vertices. Their current affine transforms produce
a conservative posed box: two-bone blended positions lie inside the union of
the transformed boxes. Geometry wholly outside the light sphere is not queued;
queued geometry is submitted only to intersecting cube faces. Unknown bounds,
unsupported weights or a missing/non-affine pose retain all six faces.

This also avoids the second, first-person cube for lights whose sphere cannot
reach the posed viewmodel. Nearby lights retain the separate first-person map
and its own arms/weapon self-shadowing. No temporal reuse, caster exclusions,
resolution reduction or light-budget reduction is involved. Shader filtering,
depth precision, material cutouts and stock render-state restoration remain
unchanged. The work stays in the desktop shadow backend.

The release comparison binaries have identical recorded production hashes
except for `modern_shadows_gl.c`, with the same `--portable --release` build
options. At 2560×1440, Soft/1024, all lighting enabled, AA off, default
anisotropic/per-pixel settings, the copied Two Betrayals
checkpoint and the existing invulnerable-player snowy combat route:

| Two-minute run | Average FPS | Minute averages | Mean active shadow lights | Mean selected lights |
| --- | ---: | --- | ---: | ---: |
| Optimized, `m2-bounds-soft-1024` | 254.10 | 263.99 / 244.21 | 8.00 | 13.91 |
| Previous build, `m2-bounds-control-soft-1024` | 203.32 | 202.54 / 204.09 | 8.00 | 15.73 |

Both measured windows retain eight active shadow lights throughout. Combat
evolves differently between runs; the selected-light counts prevent treating
the observed difference as an exact isolated percentage speedup. The optimized
run clears the requested 240 average FPS gate in this busy encounter, including
each measured minute. It does not establish a 240 FPS floor on every frame or
on other hardware. Neither run records a frame longer than 50 ms.

The harness samples skinned triangle interiors across differing affine poses
and mesh scales, checks cube seams and blended-bone bounds, and verifies that
far first-person geometry skips the second cube while nearby geometry retains
it. Capture-only diagnostics count submitted versus unculled cube faces; they
are absent from normal builds. A representative first-person capture submits
121 mesh-face draws instead of 264 for the same retained queue, while keeping
both world and first-person cubes for the nearby light.

Final local validation for this optimization:

- 24 focused tests pass on the restored production sources. An initial test
  invocation overlapped the capture builder's temporary instrumentation and
  failed on its diagnostic declarations; the serialized rerun passes. No
  production change or weakened assertion was needed for that harness error.
- The real GPU caster/receiver quality checks pass at 1024, including all six
  cube faces, seams, finite grazing planes, cut-off/empty rays, two model light
  slots and Soft filtering. Shader hashes match the pre-culling implementation.
- `m2-bounds-accept-{30,60,120}` contains 18 successful Hard/Soft player,
  first-person and Warthog runs: 90 native 2560×1440 keyframes and zero
  incomplete-map samples. Matched before/after and frame-rate overview images
  are retained in `build/modern-refinement/m2-opt`. These are sampled visual
  checks, not a claim of continuous-motion edge-stability acceptance.
- Nine additional 60 FPS cases cover Ghost Hard/Soft, corpse, dropped weapon,
  both grenade pickups, a Grunt holding a Needler, CE Death Island and MCC
  Digsite. All complete with five keyframes and no incomplete-map samples.
  MCC uses the existing private localhost host fixture and its available
  `weapons\needler\mp_needler` speargun tag. Original maps and saves are untouched.
- The optimized repeat (`m2-bounds-repeat-soft-1024`) averages 345.83 FPS but
  develops into a quieter encounter: only 4.71 mean shadow lights. It is retained
  as workload-variation evidence, not another busy eight-light acceptance pass.
- Windows capture and normal builds pass. Configured `ninja android` and
  `ninja web` again fail at the existing Windows process-launch rules for
  POSIX tools and `emcc.bat`, before relevant compilation. Full platform builds
  and AMD hardware execution are not established by these local checks.

The Milestone 2 implementation and bounded local Windows acceptance are
complete, including the requested busy-combat average-FPS gate at Soft/1024.
Off defaults, host eligibility, Android exclusion and vendor workaround paths
are unchanged. This does not close Milestone 3's emitter placement, Milestone
4's continuous edge/bias/transition checks, or Milestone 5's delivery/platform
acceptance. No testing-branch publication is part of this local milestone.

## Milestone 3: emitter placement

The source-placement review found that the Jackal profile already emits
radially; it has no AR-style cone. Its authored `shield light` marker is very
close to the forearm/hub, however. A marker-relative offset of 0.06 up and 0.08
forward places the representative point in front of/above that hub. Both blue
and gold shields use it. The existing radius, intensity, damage colour,
collapse fade and selection policy are unchanged. The body and weapon remain
casters, and the transparent shield surface remains transparent to shadows.

The Ghost's running-light markers are similarly close to the hull. A 0.02
outward offset follows each marker's forward axis and scale. Multiplayer
Ghosts use the existing ambient emitter profile; campaign Ghosts use their
existing authored light attachments. Both retain their previous source count,
reach, colours and engine-function timing. No whole-Ghost caster exclusion,
extra shadow light, temporal reuse or enlarged filter was introduced.

### Evidence and integration

`build/modern-refinement/m3` retains the original sources, before/after capture
builds, native images, geometry probes and test logs. The 128-direction fixed
sphere probe of captured owner triangles gives these minimum intersections:

| Source | Before | After | Rays hitting owner geometry within 0.02, before / after |
| --- | ---: | ---: | ---: |
| Jackal shield | 0.00253 | 0.08885 | 73 / 0 |
| Ghost left engine | 0.00813 | 0.02817 | 38 / 0 |
| Ghost right engine | 0.00536 | 0.02552 | 47 / 0 |

Distances are world units. The probes use actual converted, posed triangles,
but do not evaluate texture alpha; they demonstrate clearance rather than
prove final pixels. The retained Ghost probes still hit hull geometry in
74 and 81 directions within 0.3 units. Chassis blocking was not removed.
The first Jackal trial moved only upward by 0.06; it cleared immediate
intersections but retained excessive forearm magnification. A separate 0.16
upward trial was not retained. The provisional placement is compared with the
original in `placement-comparison.png` and the native captures.

Campaign attachments need one additive hook in `light_reconnect_to_map`, after
the normal marker pose is read and before bounds and cluster reconnection.
A GPU-only offset would disagree with CPU light selection and cluster bounds;
the existing refresh API does not accept a replacement origin. All placement
policy and state therefore live in `authored_emitter_lighting` and
`plasma_lighting`; the shared function only calls that hook. It affects only
registered render attachments with matching light and owner identities.
Frame cleanup clears the offset before restoring the stock pose/partition,
even after disabling the option. Repeated refreshes do not accumulate offsets.
Android supplies an empty hook and still excludes the feature modules.

### Remaining emitter audit

The review also follows these existing placement paths; it does not retune
their brightness or replace authored effects:

- Muzzle pulses use the rendered weapon marker, or the vehicle marker for
  model-less turret weapons. Scorpion cannon/coaxial sources retain their
  existing distinct flash positions and pulses.
- Plasma prongs, charge/vent effects, sword and vehicle headlights reuse
  authored attachment poses. The Ghost-specific offset is zero for these
  profiles. Prong reach and its separate toggle remain unchanged.
- AR displays keep their screen-relative, rear/up-facing source; Needlers
  retain the needle-cluster smoke marker and broad spill. First-person and
  world poses remain separately evaluated.
- Hunter rods use the animated forearm `rodflare`; the Grunt fuel-rod weapon
  uses `lens flare`. Neither invents a source on the model-less Hunter gun.
- Stationary shields retain their `shield light` marker, shader-matched damage
  colour and collapse handling. Dropship thrusters retain their separate
  markers. World pickups/teleporters and flame-stream centroids retain their
  existing interpolated origins.
- Body shield/overshield glow still approximates its luminous volume with one
  centre point. Its interaction with opaque body geometry requires visual
  comparison; a centre-point approximation is not an area-light solution.

These are point-source shadowmaps. A nearby arm can legitimately project a
large shadow on a distant wall; surface emission over the entire shield would
produce a penumbra that this representation does not reproduce. Hard mode
retains crisp geometry edges. This work does not claim area-light shadows or
Milestone 4's continuous edge/bias/transition acceptance.

### Reopened placement acceptance

The corrected point-source Jackal capture still has an oversized wall shadow
and does **not** pass the intended visual gate. Clearance alone is insufficient:
the broad luminous shield is being approximated by a point beside its holder.
Emitter-sized shadow experiments remain in separate capture binaries; they
are not installed in the production backend. The first 16-sample experiment
softens the projection but introduces unacceptable visible artefacts.

Review also invalidated the initial campaign Ghost captures: the actor falls
off the B40 platform. Zero incomplete-map messages and a created object are not
visual acceptance. The fixture now requires a traced floor for vehicles, and
the campaign cases must be repeated at a safe anchor. The initial runs remain
as failure evidence and are excluded from acceptance totals.

Further inspection of the posed luminous shield mesh measured marker-local
forward bounds of -0.08474 to +0.01518, lateral bounds of about +/-0.225 and
vertical bounds of about +/-0.233. Its area-weighted forward centroid is
-0.02603. Consequently the provisional +0.08 forward offset is a clearance
approximation, not a position on the luminous surface. Do not describe it as
physically correct emission or as an accepted final placement.

The bounded four-position experiment (`m3-multi-source-fixed`, Soft/60) rendered
four shadow views of the same scene from positions spread across that surface.
Its visible overlapping silhouettes also fail acceptance. This experiment
multiplies depth storage and rendering work and is not in the production
renderer; it has not been performance-qualified. The earlier `m3-multi-source`
run failed resource setup and rendered no active shadow maps, so it supplies no
visual evidence for the approach. The backend was restored after both trials.

An extended emitter may still produce a large partial shadow when it is close
to a blocker and the receiving wall is farther away. The goal is plausible
partial visibility, not an arbitrary cap on silhouette size. Neither the
point-placement candidate nor the area-filter and four-position experiments
close this visual issue. Milestone 3 remains open.

The Ghost fixture now seats a real driver so authored engine functions are
evaluated naturally, rather than forcing only outgoing function zero. It logs
the seat relationship and all four outgoing functions at capture keyframes.
The Jackal fixture also keeps the relocated player's forward/up axes orthogonal;
its downward-looking capture camera is independent of that simulation pose.

The corrected campaign Ghost fixture (`m3-campaign-ghost-driver`, Soft/60)
remains on the platform, confirms the driver parent and engine function rising
to 1, and renders complete shadow maps. Image inspection still shows broad,
angular blocking on the nearby floor; this is useful reproduction evidence,
not visual sign-off. Repeating that unchanged appearance at other caps would
not resolve it. The representative MCC speargun-holder capture was inspected:
the weapon/body silhouette is visible on the ground, but this does not qualify
all MCC content or continuous motion. The focused eight-module harness suite
passes 79 tests after the attachment-partition restoration assertion was added.
No new busy-combat performance result is claimed for these provisional M3
changes; M2's measurements must not be relabelled as M3 acceptance.

### Rejected filtering and coordinate audit

The subsequent emitter-sized filtering prototypes are rejected. The user
requires correction of the light/shadow calculations, not softening or
filtering to conceal an oversized projection. The production backend again
uses the point-source Hard/Soft implementation. The provisional Jackal and
Ghost placement offsets remain under investigation, not accepted fixes.

Two valid full-release cost probes also failed the performance target:
`m3-area128-cost-probe` averaged 80.10 FPS and
`m3-area32-cost-probe` averaged 185.79 FPS over 120 seconds. These are rejected
experiments, not measurements of the restored point-source implementation.
Sources and hashes for the final rejected filter are retained in
`build/modern-refinement/m3-next/rejected-area32-sources.zip` and its manifest.

The earlier twelve `m3-compact-area`, `m3-compact-30` and `m3-compact-120`
captures are invalid: a GLSL 450 reserved identifier caused receiver compilation
to fail and the renderer fell back to existing lighting. Their indices now
record that failure. Active-map collection messages alone did not establish
that the receiver sampled the maps. The visual runner now rejects receiver
shader failures, and the standalone receiver test uses GLSL 450, matching the
game. The failed captures must not be used as evidence of corrected shadows.

The coordinate audit has established the following, without changing light
strength, radius, filtering or caster eligibility:

- The visible model and modern caster use the same inverse-bind composition,
  node weight normalization and render-interpolated node source. A capture-only
  comparison observes the actual relative matrices passed by both paths.
- The C40 Jackal Hard/60 capture (`m3-coordinate-jackal`) recorded zero
  coefficient difference across all 27 bones at ticks 100, 130, 160, 190 and
  220. Its five native 2560x1440 images show that the large silhouette remains;
  the shader did not fail. Matching poses do not independently qualify every
  material, LOD, palette or map family.
- The Sidewinder Ghost Hard/60 capture (`m3-coordinate-ghost`) likewise
  recorded zero coefficient difference across all five bones at those five
  ticks, with two selected lights and no receiver shader failures. Native
  image inspection still shows angular floor shadows; matching poses do not
  resolve their appearance.
- Captured world-light vertex programs form attenuation coordinates as
  `0.5 + (surface - light_position) / (2 * radius)`. The modern world receiver
  inverts that mapping; specular uses its existing radius multiplier. No
  reversed vector or factor-of-two error was found in those captured paths.
- The actual GPU caster and receiver passed 84 additional analytic rectangle
  projection cases: six cube directions plus a corner, three source/receiver
  distance configurations, world/model receivers, and Hard/Soft. They include
  two weighted nonidentity bones, positions around `(370, -693, 65)`, and a
  close source. All tested silhouette interiors agree with similar-triangle
  projection; edge-area disagreement must remain below 1.5% of the image.
  Evidence: `m3-next/projection-quality/report.json`.

These checks narrow the investigation; they do not establish acceptable Jackal
emission. The single point near the shield hub and its provisional offset must
still be reconciled with the visible emitting surface and owner geometry.
Changing the projection scale or suppressing the owner would hide that issue.
The filtering experiments are not a proposed solution. Milestone 3 remains
open, and no new busy-combat acceptance or 240+ FPS claim follows from these
coordinate checks.

The focused modern-shadow, Covenant-emitter and shader-isolation harness run
passes 32 tests (`m3-next/coordinate-tests.log`). The Windows capture build and
restored normal release build succeeded. Python syntax checks passed. These
diagnostic changes add no production GPU work. This pass did not repeat the
30/120 FPS, busy-combat or platform-runtime acceptance gates while the visual
defect remains unresolved; the earlier Android/Web launcher failures and lack
of AMD hardware remain unqualified.

### Source-model decision and restored performance baseline

The next geometric probe uses the captured Sidewinder Ghost triangles and
running-light marker axes (`m3-next/ghost-emission-axis.json`). Along each
marker's outward axis, the nearest hull intersection lies behind the marker,
approximately 0.0051 and 0.0083 world units respectively. No self-illuminated
part intersects either axis. This does not support moving the source farther
outward as an established geometric correction. The captured marker and mesh
samples are from nearby idle frames, so these distances are diagnostic rather
than exact same-frame placement tolerances.

The remaining emitter audit confirms that Shield Glow and Overshield use a
point inside the biped: `shield_glow_sample` chooses the root X/Y and the
mid-height of the rendered nodes (adjusted for overshield coverage). Opaque
owner geometry can therefore trap this source. Excluding the owner would
violate the current acceptance criterion and has not been implemented.

The Jackal requires a source-model decision before further visual changes:
retain a crisp point source and its physically large nearby-blocker projections,
or represent emission across the shield surface, with actual partial visibility
and additional rendering cost. At this point the user had been asked to choose; the subsequent surface-emission
authorization and experiments are recorded below. No area filter or caster
exclusion was added in this earlier coordinate pass.

The restored full-release point-source build completed a new 120-second C40
snowy combat-route measurement after 25 seconds of warm-up, at 2560x1440,
Soft/1024, uncapped, with the eight-light budget unchanged:

- Average: **367.43 FPS**; minute averages **383.48 / 351.38 FPS**.
- Shadowed lights: **4.37 mean, 8 peak**, active on every measured frame.
- No shader failures or runtime errors; normal exit. This run had no image
  readback and used the isolated checkpoint copy and invulnerable route fixture.
- The lower average light load is not equivalent to the earlier eight-light
  busy control. This result neither establishes an optimisation gain nor closes
  final busy-combat acceptance for a future source-model implementation.
- Private-memory window averages were **717.80 / 724.62 MiB**. This short run
  does not establish memory stability.

Evidence is retained in
`build/lighting-performance/m3-point-coordinate-baseline/report.json`, its
frame log, settings, checkpoint provenance and build manifest. The executable
matches the audited production-source manifest except for removal of a stale
comment in `covenant_emitter_lighting.c`; executable behavior is unchanged.

Cleanup removed 17 rejected prototype executables/PDBs (476.22 MiB), with paths,
sizes and SHA-256 hashes recorded in `m3-next/rejected-binaries-removed.json`.
Their build manifests, source archive, logs, images and geometry evidence remain.
The current coordinate-capture, restored-performance and normal Windows builds
remain available; original maps and saves were not removed.


### Surface-emission feasibility pass — 16 November 2026

The user authorized the surface-emission approach. This pass implemented and
measured it, but **does not close Milestone 3**. The multi-view implementation
was rejected and removed from the active production source after failing the
combined quality/performance requirements. The earlier M1/M2/M3 work remains.

The prototype sampled the actual transparent Jackal shield region, using the
current permutation, model-part node palettes and interpolated bone pose.
Lighting used the same surface centroid as the shadow views. It did not move
the source an arbitrary distance away, exclude the owner, shrink projections,
or add an area blur. Each logical area light rendered several independent
full-resolution cube views, with instanced layered drawing on supported desktop
GL and sequential drawing as a fallback. Point sources retained their cube
path. The logical shadow-light budget remained eight.

Evidence and outcomes:

- Sixteen surface samples: the real-game C40 Jackal captures at 60 FPS in Hard
  and Soft completed without shader failures and confirmed sixteen active
  source positions. The distant-wall shadow became much fainter. That alone
  is not a visual acceptance pass.
- The actual GPU caster/receiver test matched an independently projected
  rectangle from sixteen origins: maximum interior visibility error was
  0.001961 in both modes (8-bit readback quantization). The existing 84 point
  projection cases still passed. This establishes projection correctness,
  not sufficient in-game quality.
- Sixteen samples, 60 measured seconds after 25 warm-up seconds in the isolated
  C40 snowy combat route: **166.10 FPS** at 2560x1440, Soft/1024, uncapped;
  shadowed-light mean **6.40**, maximum **8**, active throughout. Normal exit,
  no recorded shader/runtime errors.
- Eight samples with CPU/GPU profiling: **221.92 FPS** under the same resolution,
  mode, checkpoint and duration; shadowed-light mean **6.69**, maximum **8**.
  Profiling overhead and encounter variation mean these are not a strict A/B
  estimate of the sample-count saving. Both measured results miss 240 FPS.
- The eight-sample profile contained 416 sampled GPU frames. Mean summed
  shadow-pass GPU time was **1.465 ms**; model lighting **0.793 ms**, world
  diffuse/specular **0.213/0.247 ms**. CPU shadow preparation averaged
  **0.325 ms**, including **0.179 ms** of draw submission. These measurements
  identify the new shadow-view cost; they do not establish a renewed CPU
  collision bottleneck.
- Four samples failed the nearby-wall visual test: clearly separated,
  overlapping silhouettes. No combat benchmark was run for that rejected
  quality setting. `--shadow-wall-gap .2` exercises the nearby receiver;
  the original fixture gap of `.7` remains the default. Captures are at
  2560x1440, Hard/1024, 60 FPS.

The allocation cost also makes the brute-force design unsuitable for the full
resolution range: sixteen 1024-square, six-face 32F depth views require 384 MiB
per logical light and receiver representation before driver overhead. Eight
such lights require 3 GiB; at 4096 this becomes 48 GiB, before separate first-
person maps. These are allocation calculations, not measured GPU-memory usage.
Reducing the selected resolution or the eight-light budget was not used as a
workaround.

Evidence paths (relative to the worktree):

- `build/modern-refinement/m3-surface/rejected-surface-prototype.zip` and
  `rejected-source-manifest.json`: verified archive of 50 files at the final
  four-sample prototype state, including source and regression probes. Earlier
  8/16-sample build manifests identify their own inputs; this final archive
  must not be represented as byte-identical to every earlier prototype.
- `build/modern-refinement/m3-surface/area-regression/report.json`: analytic
  point and finite-source GPU checks.
- `build/modern-refinement/m3-surface-first/index.json`: sixteen-source visual
  captures, Hard/Soft, 60 FPS.
- `build/modern-refinement/m3-surface/four-source-visual.json`: the rejected
  nearby-wall capture and verified lossless PNGs.
- `build/lighting-performance/m3-surface-cost/report.json` and
  `build/lighting-performance/m3-surface8-profile/report.json`: combat results,
  settings, manifests, checkpoint provenance and raw frame logs.
- `build/modern-refinement/m3-surface/eight-source-profile-summary.json`:
  measured CPU/GPU breakdown, derived from the retained CSVs.

All 45 pre-experiment snapshot hashes were verified on restoration. The only
retained changes from this feasibility pass are this record and the diagnostic
wall-distance fixture/runner option. The restored shadow, emitter, renderer,
Android and policy code therefore retains the earlier implementation and its
known visual defects; restoration is not a claim that those defects are fixed.
The focused restored suite passes 65 tests. The restored Windows portable Release/LTO build completed successfully.
Final verification is recorded in `m3-surface/final-verification.json`.

The next implementation must represent finite-source visibility without
multiplying full-resolution depth storage and rendering work by 8–16 for each
logical light. It must pass the near/far wall cases before extension to Ghost
engine openings and body Shield Glow/Overshield. Their source-representation
issues remain open; no whole-owner exemption has been introduced. The eight-
light budget, selected resolution, current source geometry and visual fidelity
must be explicit constraints in that design review.

No new 30/120 FPS, Xbox/CE/MCC runtime, multiplayer or AMD-hardware acceptance
is claimed from this rejected experiment. The requested full 30/60/120 and busy-
combat acceptance matrix remains pending a viable implementation. Android
exclusion, off defaults and host policy were not changed in the retained code.
Nothing from this pass was committed, pushed or packaged as an accepted build.

Cleanup removed four rejected executable/PDB artifacts (104.71 MiB), with
paths and hashes in `m3-surface/removed-rejected-binaries.json`. The sixteen-
sample capture executable and eight-sample profile executable remain for
comparison (about 50.4 MiB combined); original maps and saves were preserved.

### Backprojection information test

16 November 2026. The approved investigation proceeded through the reference
and capture-information gates. **M3 remains open.** No runtime renderer,
emitter, shader, setting or platform code changed during this pass.

The investigated approach is motivated by Guennebaud, Barthe and Paulin,
[Real-time soft shadow mapping by backprojection (EGSR 2006)](https://www.labri.fr/perso/guenneba/docs/SoftShadowMapping-egsr06.pdf).
This diagnostic is an original geometry-information test, not an implementation
or performance evaluation of that paper's GPU algorithm. Before reconstructing
visible emitting area from a depth map, it asks whether the capture contains
the necessary occluders at all.

`tools/modern_shadow_surface_probe.py` reads the retained posed Jackal captures.
It re-poses the emitting shield mesh with exactly the body's node matrices at
the selected frame. It samples 512 positions proportional to triangle area,
with a 1024-position convergence check. The standalone BVH ray/triangle oracle
`tools/fixtures/shadow_surface_reference.c` computes the unblocked source
fraction at each virtual receiver position.

The candidate uses either the surface centroid or two furthest-apart surface
vertices as capture positions. Six 1024-square direction grids per position
identify nearest triangles. The reconstruction is granted the WHOLE original
triangle for every hit, then gets exact visibility through that reduced
geometry. This supplies more information than those sampled depths alone.
Missing coverage therefore identifies occluders that reconstruction from only
those samples cannot recover without additional geometry or assumptions. It
does not prove every possible placement or layered representation must fail.

The reference holds emission uniform and two-sided, using the captured opaque
Jackal parts with no owner exemption. It excludes the rest of the game scene,
held weapon, texture alpha, emissive weighting, distance attenuation and BRDF.
The images are offline visibility diagrams, not game screenshots or a final
artistic target. Two poses check pose dependence, not temporal acceptance.

Final results, in percentage points of visibility:

| Captures / receiver | Mean lost occlusion in reference shadow | Maximum lost occlusion |
| --- | ---: | ---: |
| Centre only, nearby wall | 32.76 | 83.01 |
| Two endpoints, nearby wall, 512 samples | 3.71 | 29.88 |
| Two endpoints, nearby wall, 1024 samples | 3.71 | 30.37 |
| Two endpoints, farther wall | 3.54 | 13.67 |
| Two endpoints, second pose | 3.22 | 28.32 |
| Two endpoints, side A | No reference pixels below 95% visibility | 0.59 |
| Two endpoints, side B | 4.05 | 18.36 |

The 512-to-1024 reference difference averages 0.67 points, with a 2.05-point
99th percentile and 3.13-point maximum. In the 1024-sample nearby case, 3.33%
of all receiver pixels still lose more than ten points of occlusion. The
missing coverage survives convergence and pose checks. Two views improve
substantially over the centre capture but are not accepted as the emitter fix.

Review corrected an oracle tolerance issue: a fraction-of-ray-length near
cutoff could discard nearby triangles on long capture rays. The final oracle
uses a fixed one-millionth world-unit endpoint tolerance. All seven cases were
rerun; the reported conclusions did not change. Seven focused tests compile
the actual oracle and cover BVH hit identity, finite segments, nearby blockers
on long rays, partial visibility, random rays against brute-force intersections,
invalid inputs, and area-weighted sampling under translation.

Two full-resolution 32F cubes for each of eight logical lights would allocate
384 MiB at 1024, 1.5 GiB at 2048, and 6 GiB at 4096 for depth alone. These are
arithmetic estimates, before reconstruction hierarchies, separate viewmodel
maps and driver overhead, not measured VRAM. No resolution or budget was reduced.

The tested reconstruction failed the geometry-information gate. This pass did
not integrate its GPU shader, extend it to Ghost/Shield Glow/Overshield, or
repeat combat and 30/60/120 tests on unchanged production code. Concurrent CPU
oracle durations are not game performance data. No new 240 FPS, AMD-hardware,
platform or multiplayer acceptance is claimed. Off defaults, host policy,
Android exclusion and AMD/NVIDIA workarounds remain unchanged by this work.

The next design needs geometry hidden from these capture positions. A bounded
candidate is to provide nearby posed caster geometry directly to finite-source
visibility reconstruction while retaining existing maps for ordinary point
lights. This is an investigation direction, not an implemented solution or a
proven saving. It needs these reference comparisons and a timed GPU prototype
before integration. Arbitrary source offsets, owner exclusions, blending a
few hard silhouettes and blurring away discrepancies remain rejected workarounds.

Evidence under `build/modern-refinement/m3-backprojection/`:

- `final-report.json`: metrics, convergence, allocations and source verification.
- `final-*/report.json`, `comparison.png`, `reference.npz`: seven reviewed cases,
  capture/tool hashes, complete/reduced visibility and geometry. Earlier
  directories are exploratory and superseded by these results.
- `final-oracle-tests.log`: focused results after final review.
- `before-source.zip` / `before-source.json`: 42 pre-existing changed files;
  only this document differs after the diagnostic pass.
- `run_reviewed.py` / `summarize_reviewed.py`: reproduction and aggregation.
  Captures, original maps and saves were not modified.

Standalone Windows build using the configured Clang environment:

```text
clang -m32 -std=gnu99 -O3 -D_CRT_SECURE_NO_WARNINGS tools/fixtures/shadow_surface_reference.c -o build/shadow-reference.exe
python tools/modern_shadow_surface_probe.py --capture build/plasma-visual/60-1-m3-surface4-near --output build/shadow-reference-near --oracle build/shadow-reference.exe --gap .2 --view-origins diameter
python -m pytest -q tools/harness/tests/test_shadow_surface_reference.py
```

The probe requires NumPy/Pillow and an unused output directory. Link the
standalone oracle with `-lm` on Linux. It is never linked into the game. No
commit, push or accepted playtest package was produced from this investigation.

### Geometry-assisted GPU feasibility

16 November 2026. The user authorized the bounded geometry-assisted prototype
after discussing its distinction from stencil volumes. This pass implements
and measures it in a standalone hidden Windows OpenGL context. **It passes the
tested geometry-reference checks, fails the cost gate, and is not integrated
into the game. M3 remains open.**

The dedicated diagnostic consists of `tools/modern_shadow_geometry_gpu.py`,
`tools/fixtures/shadow_geometry_visibility.comp` and the asset-free packer tests
in `tools/test_modern_shadow_geometry_gpu.py`. No production source or build
wiring changed. The existing uncommitted renderer work was preserved.

The input is the previous pass's actual posed opaque Jackal triangles and
area-weighted source positions. A depth-first BVH groups triangles in leaves
of at most eight; escape indices permit stack-free traversal. The GPU traces
finite receiver-to-source segments through that complete geometry and averages
visibility. This is software ray visibility in a GL 4.3 compute shader, not
hardware ray tracing, a stencil-volume implementation, or the previously
discussed depth-map backprojection algorithm. It establishes whether direct
geometry fixes the information loss and what this particular method costs.

The spatial tree plus triangles occupy 254,208 bytes (about 248 KiB) for this
4,155-triangle posed caster. The one-copy probe's complete SSBO allocation is
1,245,440 bytes, including receiver/source positions and output. There is no
per-source cube allocation. Small storage alone does not make traversal fast.
Python BVH construction and upload are outside the GPU timing; the measured
pass is not a complete per-frame integration cost.

The retained optimization gives each receiver a 64-thread group, distributes
the emitting positions across its lanes, then reduces their visibility counts
in shared memory. The scalar mode remains only as a diagnostic comparison.
GL entry-point/uniform lookup was also moved out of timed submission. An
explicit inverse-direction variant showed no benefit in the controlled
comparison and was removed; its exact source is archived with the evidence.

Final measurements on NVIDIA RTX 5070 Ti, driver 617.42, Windows GL 4.6:

| Case | Sources | GPU elapsed median (ms) | Maximum reference difference |
| --- | ---: | ---: | ---: |
| Nearby wall, scalar comparison | 512 | 310.48 | 1/512 |
| Nearby wall, grouped rays | 512 | 52.75 | 1/512 |
| Nearby wall, convergence reference | 1024 | 115.20 | 1/1024 |
| Farther wall | 512 | 49.84 | 1/512 |
| Receiver side A | 512 | 16.36 | 1/512 |
| Receiver side B | 512 | 88.10 | 1/512 |
| Second pose | 512 | 55.79 | 0 |
| Eight repeated near-wall copies | 512 each | 430.69 | 1/512 |

Each copy evaluates 49,152 virtual receivers (256 by 192). The eight-copy
case executes eight passes with separate output regions and shared identical
geometry. It is a synthetic stress test, not eight distinct combat lights or
a game FPS prediction. Measurements use three warmups and three retained
samples, sequentially, without concurrent builds or reference jobs. Raw samples,
driver identity and source/reference hashes accompany every result. This
bounded measurement is sufficient to reject a pass exceeding the entire
4.17 ms target frame budget by this margin; it is not sustained acceptance.

The paired final scalar/grouped comparison is about 5.9x faster with unchanged
source and receiver counts. Earlier exploratory results (including the initial
2.4-second scalar pass) used different submission/shader revisions and must
not be used to attribute a 30x or 44x gain to one retained optimization.
An elapsed GPU timestamp interval can include command starvation; these are
pass timings, not an isolated count of shader ALU work. Queries bracket the
dispatches and a buffer-update memory barrier before CPU readback, following
the [Khronos timestamp contract](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/glQueryCounter.xml)
and [memory-barrier contract](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Refpages/main/gl4/glMemoryBarrier.xml).

All six distinct reference cases agree within one source sample; the second
pose agrees exactly. Mean absolute errors are at most 1.20e-7. The observed
one-sample differences are consistent with isolated float-precision boundary
differences, not the broad missing coverage of the earlier reduced geometry.
Known rectangle/finite-segment cases run on the actual GPU before each case.
The deliberately incomplete centre-capture geometry fails the same acceptance
check with maximum error 0.830078 and mean error 0.199990, reproducing the
earlier failure. That negative-control exit is expected, not a failed final
implementation test hidden from the report.

Sixteen focused CPU tests pass: nine BVH/packing checks and seven existing
oracle checks. Final self-review covered buffer layout, bounds, finite inputs,
escape links, uniform barriers, GL resource cleanup and diagnostic isolation.
The extra inverse-direction variant was trimmed after its unsupported benefit.

Decision: retain this as a reproducible reference/cost probe, reject direct
hundreds-of-rays-per-receiver tracing for production. Do not turn it on in the
game, reduce the eight-light budget, lower selected shadow resolution, exclude
owners, or mask errors with blur. A future practical design must amortize or
analytically integrate visibility over the emitting surface, rather than repeat
this much geometry work at every receiver. This experiment establishes the
need to preserve occluder information; it does not establish that a cheaper
geometry-assisted algorithm is impossible or already available.

The figures are visibility diagrams through opaque local geometry, with the
previous reference's uniform two-sided emission. They omit full-scene geometry,
held weapons, texture alpha, radiometry and motion acceptance. No new in-game
30/60/120 or 240+ FPS, AMD, Xbox/CE/MCC or multiplayer acceptance is claimed.
Off defaults, host policy, Android exclusion and existing AMD/NVIDIA workarounds
are unchanged. No Ghost/Shield Glow/Overshield integration or accepted playtest
build, commit or push was produced from this rejected runtime approach.

Evidence under `build/modern-refinement/m3-geometry-gpu/`:

- `final-*/report.json`, `result.npz`, `comparison.png`: final GPU measurements
  and reference agreement; `final-negative-control` is the deliberate failure.
- `final-summary.json`: combined metrics, source hashes and preservation check.
- `run_final.py`: exact sequential commands, including expected failure status.
- `final-tests.log`: focused suite after review.
- `before-source.zip/json`: 48 pre-existing changed files; only this document
  differs at completion. Original maps, saves and captures were not modified.
- `reviewed-variants.zip`: the pre-trim inverse-direction comparison source.
- `final-prototype.zip`: retained diagnostic source and tests, hash-verified.

Reproduction requires Windows desktop GL 4.3+, NumPy/Pillow and the retained
reference, with a new output directory:

```text
python tools/modern_shadow_geometry_gpu.py --reference build/modern-refinement/m3-backprojection/final-two-near/reference.npz --output build/geometry-near --packet
python -m pytest -q tools/test_modern_shadow_geometry_gpu.py tools/harness/tests/test_shadow_surface_reference.py
```

Omit `--packet` for the scalar control; use `--copies 8` for the synthetic
budget stress. `--geometry captured` deliberately drops occluders and should
fail the quality gate on the centre-capture reference. No game data is embedded
in the diagnostic source; the local reference archives remain local.

### Projected-area integration feasibility

16 November 2026. The user authorized the recommended sequence: simple
coverage proofs, the captured Jackal, GPU/eight-light cost, then integration
only if the earlier gates passed. The projected-area prototype was implemented
and tested in that order. **It fails the performance gate and is not integrated.
M3 remains open; production shadows still have their earlier emitter defects.**

The new diagnostic is `tools/modern_shadow_projected_area.py`, its shader is
`tools/fixtures/shadow_projected_area.comp`, and the analytic tests are in
`tools/test_modern_shadow_projected_area.py`. It reuses the preceding probe's
hidden WGL context and BVH packer without modifying those helpers or the game.
No stencil buffer or rendered extruded volumes are used.

The captured shield contains 76 nondegenerate emitting triangles and 60 unique
posed vertices. Its smallest principal-axis extent is about 0.069 world units,
so this experiment does not replace it with a flat disk. Each actual facet is
integrated using equal-area quadrature strips, weighted by its true area. For
each strip, the receiver and each occluding triangle define three projection
half-spaces; a fourth requires the emitter to lie beyond the blocker. Clipping
these inequalities yields the blocked interval along that source strip.
The union of those intervals handles overlap without double counting. Area
integration is approximate across strips, while interval union is geometric
within floating-point precision. This is a different algorithm from testing
hundreds of independent source rays.

The inspiration remains the [contour-based backprojection research](https://www.labri.fr/perso/guenneba/SoftShadowMapping_eg07.php).
This is an original strip-integration experiment using the full nearby mesh,
not a reproduction of the paper's contour extraction, shadow-map hierarchy,
adaptive screen sampling, performance or quality claims.

A conservative triangular-beam/AABB test prunes the spatial tree. A group of
64 GPU threads shares one receiver and distributes its strips across lanes.
Geometry storage is 320,688 bytes for the 4,155-triangle caster, including
precomputed triangle planes. Per-strip disjoint interval storage is bounded
at 32. Overflow reports an explicit failure; it never returns missing shadows
as a successful image. The observed maximum is 18 intervals in these cases.

Validation established:

- Analytic partial coverage, overlapping/duplicate blockers, disjoint gaps,
  both sides, crossing the emitter plane, finite segment bounds, winding,
  curved-facet area weights and pose translation.
- CPU projected intervals agree with independent dense ray tests. Actual GPU
  overlap/both-side cases and randomized projection/BVH cases agree with CPU
  interval integration. Eight sampled Jackal receivers agree with CPU interval
  integration within 0.0001 visibility (actual maximum recorded in the report).
- Near/far walls, both receiver sides and a second pose were compared with the
  retained ray references. These are offline opaque-geometry visibility tests,
  not in-game lighting or motion acceptance.
- At matching sampled receivers, 8 versus 32 strips per facet differs by
  0.0634 percentage points on average and 0.583 points at maximum; 2 versus 32
  differs by 0.378 points on average and 3.142 at maximum. The ray reference is
  itself sampled (1024 positions for the near case, 512 for other cases), so
  its difference is not exact ground-truth error.
- The eight-slot overflow control produces the expected `OverflowError` and
  no accepted output. Final review also made zero-orientation projections and
  degenerate planes explicitly nonblocking, consistent with the CPU model.

Final GPU elapsed medians on RTX 5070 Ti / driver 617.42, Windows GL 4.6:

| Case | Strips/facet | Receiver count | GPU ms | Mean / maximum difference from ray reference (percentage points) |
| --- | ---: | ---: | ---: | ---: |
| Near wall | 1 | 49,152 | 94.39 | 0.902 / 7.434 |
| Near wall | 2 | 49,152 | 188.28 | 0.613 / 4.113 |
| Near wall | 8 | 49,152 | 548.12 | 0.389 / 2.505 |
| Near wall convergence subset | 32 | 3,072 | 104.45 | 0.383 / 1.752 |
| Far wall | 2 | 49,152 | 188.47 | 0.627 / 4.899 |
| Side A | 2 | 49,152 | 30.43 | 0.137 / 0.458 |
| Side B | 2 | 49,152 | 248.08 | 0.944 / 4.646 |
| Second pose | 2 | 49,152 | 205.42 | 0.571 / 4.453 |
| Eight repeated near-wall copies | 2 | 49,152 each | 1546.56 | 0.613 / 4.113 |

Each measurement has two warmups and three retained samples, run sequentially
without competing test/build jobs. GPU timestamps bracket dispatches and the
readback barrier; they exclude shader compilation, CPU preparation and upload,
and can include submission starvation. The 256x192 virtual receiver grid is
an isolated diagnostic workload, not a lowered game shadow-map setting. The
32-strip convergence subset is explicitly smaller and cannot be compared as
a same-workload speedup. Eight copies share identical geometry with distinct
outputs; they are a synthetic cost check, not eight actual combat lights.

The initial 16-slot storage trial did not offer a decisive saving, and full
cases require 17-18 slots. It was not adopted as a quality/performance setting.
All final nonnegative-control cases retain 32 slots. More strips improve
integration convergence while increasing work; none of the measured settings
approaches the entire 4.17 ms frame budget for 240 FPS, before the rest of the
game. This implementation is therefore rejected without running unchanged
production code through another combat benchmark.

The remaining cost is geometric projection/traversal and interval union for
many emitter facets at every receiver. This result does not prove that all
contour-based backprojection methods are unsuitable. It does show that this
per-receiver, full-mesh strip implementation is not a viable replacement for
the previous ray method. No faster algorithm or production fix is claimed.

Thirty-three focused tests pass after final review (17 projected-area tests
plus 16 existing BVH/oracle tests). Review covered clipping, overlap, conservative
pruning, degenerate inputs, bounded storage, failure reporting, resource cleanup
and isolation. The existing 51-file snapshot is unchanged except this document.
Source defaults, host policy, AMD/NVIDIA fixes, Android exclusion and Xbox/CE/MCC
ownership are preserved by leaving production code untouched.

No new in-game 30/60/120, 240+ FPS, platform, multiplayer or AMD-hardware
acceptance is claimed. Source emission remains the reference's uniform
two-sided surface model, without texture alpha/radiometry or full-scene/held-
weapon integration. No production build, commit or push was made.

Evidence is in `build/modern-refinement/m3-projected-area/`:

- `final-*/report.json`, `result.npz`, `comparison.png`: final source-hashed
  results; `final-overflow-control/failure.json` records the expected failure.
- `final-summary.json`, `final-cpu-gpu.json`, `final-tests.log`: combined
  metrics, convergence, preservation checks and final focused tests.
- `run_final.py`: exact sequential reproduction commands.
- `before-source.zip/json`: pre-pass files; originals/maps/saves preserved.
- `before-review-prototype.zip`: source for earlier exploratory measurements.
- `final-prototype.zip`: hash-verified retained source and tests.

Example, using Windows GL 4.3+, NumPy/Pillow and an unused output directory:

```text
python tools/modern_shadow_projected_area.py --reference build/modern-refinement/m3-backprojection/final-two-near-1024/reference.npz --output build/projected-near --strips 2
python -m pytest -q tools/test_modern_shadow_projected_area.py tools/test_modern_shadow_geometry_gpu.py tools/harness/tests/test_shadow_surface_reference.py
```

`--copies 8` runs the synthetic budget test. `--stride` selects a documented
receiver subset for investigation, not a game option. `--capacity 8` on the
near-wall case deliberately exercises storage exhaustion.


## Jackal and Ghost owner-only shadow exceptions

The user explicitly narrowed this pass to Jackal shields and Ghost engine
lights, and approved excluding their own owner from those lights' shadows.
This supersedes the earlier requirement to retain their point-source owner
silhouettes. The rejected area-light prototypes remain diagnostic research.

The existing emitter profiles now carry this policy. Jackal shield samples,
multiplayer Ghost running-light samples and campaign Ghost running-light
attachments identify their full owner datum. Each selected shadow map resolves
that owner once. Caster collection omits only that exact object; model receivers
skip that map only when drawing the same owner. Selection remains active for
other receivers, so the old collision mask cannot reintroduce an own-light
shadow. External lights still collect and shadow the Jackal/Ghost normally.
The Jackal's shield/hub is part of its biped model, not a separate opaque object.
Passengers, held weapons, other vehicles and stationary shield generators do
not inherit the exception. No new lights, GPU resources or collision traces
are introduced; the eight-light budget and selected resolution are unchanged.

The existing model-begin hook now passes its object identifier, and model-end
clears it. This minimal shared hook is necessary because the feature module
otherwise lacks the current receiver's identity. Model selection, skinning,
CE/MCC palettes and material handling remain unchanged. Android has no-op
accessors; the GLES receiver hook retains its disabled implementation.

Validation on Windows Release, RTX 5070 Ti, 2560x1440, 1024 shadow resolution:

- 53 focused tests pass, including exact-owner caster/receiver behaviour,
  external lights, other/held objects, stale identities, frame/map cleanup,
  disable/host eligibility, shield damage colours and Android exclusion.
  Two mutation controls prove the caster and receiver exemptions are required.
- Xbox c40 Jackal and Sidewinder Ghost captures pass at 30/60/120 FPS in Hard,
  with Soft checked at 60. The oversized Jackal silhouette and Ghost hull
  wedges disappear while their existing illumination remains.
- Before/after external-projectile Ghost captures retain both its caster
  submissions and shadowed model receiver. Other-object coverage is also
  exercised by the real caster collector in the regression harness.
- Shadows-Off before/after controls retain ordinary lighting and submit no
  modern maps. These are keyframe comparisons, not bit-identical captures:
  animation/interpolation produces small pixel differences between runs.
- CE Death Island Ghost engine-light capture passes. This is bounded visual
  coverage, not full map-family or multiplayer qualification.
- The campaign Ghost fixture selects no engine lights in both the original
  and updated build. Its authored-profile path is covered by regression tests,
  but campaign Ghost engine-shadow visuals are not qualified by this fixture.
- MCC Digsite hosting does not reach gameplay, including a longer startup
  allowance. No MCC visual pass is claimed. Its loading path was not changed.
- Normal Windows Release builds successfully. Full Android/Web builds were
  attempted with explicit toolchains but fail at local Windows command launch
  (POSIX Android generation commands / Emscripten batch invocation), before
  validating these translation units. Android exclusion/stub tests pass.
- No sustained busy-combat or new AMD-device result is claimed. These short
  keyframe scenes do not establish continuous-motion quality or 240+ average
  FPS. No shadow budget/resolution reduction or performance claim is made.

Self-review covered profile selection, lifecycle, receiver binding, fallback,
shared hook, platform stubs and tests. Source comparison against this pass's
snapshot confirms the GPU renderer/shaders, AMD/NVIDIA handling, stock light
implementation and default settings are unchanged. Existing host-policy gates
remain authoritative. No map assets, networking code or user saves were edited.

Evidence: `build/modern-refinement/m3-owner-exception/` contains the source
snapshot/diff, build logs, `final-tests.log`, `visual-summary.json`, protected
source checks, and `before-after.png`. Full keyframes/logs are in the associated
`build/plasma-visual/*-owner-*` folders. This completes the focused production
exception; the unqualified campaign/MCC checks and later refinement milestones
are not signed off. Nothing from this pass has been pushed to testing.

## Milestone 4: quality and budget validation

This pass retains the M3 owner exceptions and eight-light shadow budget. No
production renderer, shader, emitter, setting or host-policy change was needed
to pass the measured Windows quality/budget checks. There is no new optimisation
speedup to claim: the implementation already meets the agreed average-FPS gate
in the verified encounter. Remaining headroom is modest; additional rendering
cost still needs measurement.

### Reliable fixtures and campaign coverage

The earlier campaign Ghost fixture put its camera behind level geometry while
the vehicle slid away. Replaying at `--distance 0 --shadow-camera-angle 180`
inside the control room produces active authored engine lighting and complete
shadow maps. The Ghost retains illumination without its own engine light
shadowing its hull. Its stock ambient/distant-light projection is distinct.

MCC Digsite now reaches gameplay with local loopback networking available.
It contains neither requested Ghost tag. Its qualified case is the actual
player holding `weapons\needler\mp_needler`, under a moving external projectile
light, with Soft shadows at 60 FPS. Both player and weapon cast and receive
shadows. This closes the missing MCC renderer check, not MCC Ghost or full
multiplayer qualification.

The opt-in `--attachment-diagnostics` capture-build option records attachment
eligibility and local host admission. It is never linked into ordinary builds.
The builder restores original source bytes and records their identities; the
extra files are restored only when that diagnostic was enabled.

The first combat replay averaged 366.5 FPS, but screenshots showed the player
facing a pillar. **That result is rejected for acceptance.** The benchmark now
owns its complete controller state, preventing unrelated input leaking into
its timed steps or stopped state. A C regression test failed before this fix
and passes afterward. A signed `--turn-seconds` permits a final ordinary
controller turn after the snowy route. A short inspection established that
`--turn-seconds -0.5` faces the active Covenant encounter. No actors, mission
scripts or production controls were changed.

### Quality and stability

- GPU checks using the actual receiver/caster shaders pass at 1024: grazing
  receivers, cube faces/seams/corners, empty rays, small derivatives, Soft-edge
  continuity, independent model slots and projected caster coverage. They do
  not justify additional blur or bias.
- Sidewinder player/held-AR Hard and Warthog Soft motion sequences pass at
  30/60 FPS. Native 2560x1440 keyframe runs at a 120 FPS cap pass for player,
  Warthog and first-person AR/arms (118.8–119.2 measured FPS). Inspected output
  retains moving silhouettes, contact and weapon/body illumination.
- Recording every frame at 120 FPS introduces synchronous readback/write
  stalls, including at 720p. Those recordings failed their timing gate and
  are not full-rate 120 FPS video or performance evidence. Native keyframes,
  selection timestep tests and passive combat timing provide separate evidence.
- Eight-light mode-switch and first-/third-person checks pass their functional
  assertions: no active maps while Off, resource release, fallback after an
  injected allocation failure, reset recovery and separate view maps. Their
  cold-switch averages failed the FPS gate; those stalls remain in the reports.

Stock-shadow interaction was reviewed through `build_distant_lights`,
`render_object_shadow_end` and the render-pass order. Stock projection comes
from map radiosity/distant lighting; world rendering applies it after lightmaps
and before additive dynamic diffuse lighting. Modern shadows mask the added
light. Removing stock projection whenever a dynamic source appears would
incorrectly remove the other lighting contribution. Neither baked illumination
nor stock projected shadows are suppressed by this pass.

### Sustained combat result

Windows Release, RTX 5070 Ti, 2560x1440, AA/per-pixel lighting disabled, all
optional lighting enabled, **Soft shadows at 1024 with eight shadow lights**.
The archived Xbox c40 exterior-door checkpoint is copied into an isolated
profile. Test-only invulnerability keeps the encounter active; original saves
remain untouched. After 25 seconds of warmup, measurement lasts 120 seconds.
Screenshots confirm the Covenant remain visible and firing across the run.

| Measurement | Result |
| --- | --- |
| Raw average, including screenshot overhead | **246.6 FPS** |
| Average excluding six marked capture frames | **246.7 FPS** |
| First / second minute | **249.9 / 243.5 FPS** |
| Mean / maximum active shadow maps | **7.9997 / 8** |
| Frames with active shadow maps | **100%** |
| P99 frame time / frames over 50 ms | **5.106 ms / 0** |
| Peak process private memory | **724.4 MiB** |

This passes the user's sustained **average** 240+ FPS definition, not a 240 FPS
minimum-frame-rate guarantee. The 1% low is 169.9 FPS after capture exclusion.
Two minutes do not establish long-term memory stability: first/last-minute
private averages are 714.7/719.6 MiB.

Replay with `tools/lighting_performance_build.py` and
`tools/lighting_performance_run.py --map c40 --checkpoint-root
build/modern-shadows/fresh-checkpoint --snowy-route --turn-seconds -0.5
--invulnerable-player --fps 240 --uncapped --enabled 1 --modern-shadows soft
--warmup 25 --seconds 120 --resolution 2560x1440`, passing the retained
`build/modern-refinement/m4/performance-settings.toml`, data root, build directory
and a fresh label. Each run retains checkpoint and executable/source identities.

### Review, platforms and evidence

Self-review covered selection/hysteresis, conservative caster bounds, model
and view ownership, resource lifetimes/fallback, stock-pass interaction and
test-fixture changes. Production files match the pre-M4 snapshot: off defaults,
host gates, AMD/NVIDIA handling, Android exclusion and Xbox/CE/MCC code paths
are unchanged. AMD hardware and Android runtime were not tested here. Existing
CE Ghost evidence remains applicable to the unchanged code.

All 84 focused regression tests pass, including the benchmark input regression.
Normal Windows Release builds pass. Android exclusion/stock-value hook tests
pass; the full local Android build stops at its POSIX `sed` generation command
on Windows, before compiling the renderer. It is not an Android build pass.
The complete Web build passes with the installed Emscripten toolchain and a
private writable cache. The initial default-cache link stalled; rerunning with
that cache completed successfully. This is compilation evidence, not browser
runtime qualification.

Evidence: `build/modern-refinement/m4/` contains GPU/test reports, source
snapshot/diff, platform logs, fixture failures and cleanup records. Accepted
combat CSV, settings, screenshots and provenance are under
`build/lighting-performance/m4-busy-visible-soft/`; the pillar-facing run and
short route inspection are separate. Successful motion sequences and native
keyframes remain in the indexed `build/plasma-visual/` runs. Redundant full-frame
dumps from failed recordings were reduced to nine representative frames while
retaining complete logs. Test saves were archived and hash-verified before
removing loose copies. Milestone 5's broader acceptance, combined review and
delivery remain.

## Milestone 5: acceptance and delivery

The combined refinement was reviewed across emitter eligibility, stable light
identities, eight-slot selection, caster conversion and skinning, material
cutouts, receiver ownership, GL state restoration, allocation failure, settings,
platform stubs and diagnostics. The exact-owner Jackal/Ghost exceptions remain
limited to their own emitters. No additional renderer rewrite or speculative
optimisation was justified. This pass corrects one stale emitter comment and
removes leftover whitespace; production behaviour remains the M4 implementation.
Abandoned area-source experiments remain offline diagnostic tools, never linked
into the game. Their rejection evidence and distinct regression tests are kept.

### Refreshed acceptance

- Classic and Modern menu frontends pass Off/Hard/Soft, Save, Cancel, Defaults
  and process-restart persistence through the real widgets/handlers. The shared
  resolution control passes all six values, defaults to 128, and does not enable
  modern shadows or lighting categories. Native 2560x1440 captures were inspected.
- Xbox Blood Gulch, CE Death Island and MCC Digsite two-peer sessions pass with
  different local Hard/Soft and lighting preferences, firing, weapon pickup and
  respawns. Host-policy observations all match; the peer comparison covers
  80/75/76 seconds respectively without reported divergence. This is local
  Windows loopback coverage, not new public-internet or cross-device qualification.
- A single process completes Xbox c40 -> MCC Digsite -> CE Death Island -> Xbox
  Blood Gulch with three tick resets, active shadows, retained local preferences,
  normal exit and no exception. Original maps and saves remain unchanged.
- The focused suite covers 137 passing cases. One migration test initially hit
  Windows' CP1252 output encoding and passed when rerun with `PYTHONUTF8=1`;
  assertions were not changed. Selection, owner exceptions, held objects,
  resource fallback, stock shader isolation and Android no-op hooks are covered.
- Generated settings screens reproduce the intended resolution change.
  Unrelated generator output was restored to its pre-pass bytes.

The comparison video contains real 30 FPS Off/On moving-light sequences for the
Warthog and player/held AR, plus matched before/after keyframes for Jackal/Ghost
owner exceptions. Its captions distinguish continuous motion from held
keyframes. Input frame hashes and source reports accompany the video; the
encoded file was decoded successfully and representative frames inspected.
M4's 60 FPS motion and native 120 FPS keyframe evidence remains applicable to
the unchanged rendering code. Continuous 120 FPS recording remains unqualified.

### Release gates and retained limits

The final two-minute busy c40 repeat averages **270.5 FPS raw**, including
capture overhead, at 2560x1440 / 1024 Soft / eight-light budget. Minute averages
are 275.8 and 265.4 FPS after excluding six capture frames; mean active maps are
7.94, maximum eight, with shadows active throughout. The raw 1% low is 185.6 FPS;
one frame exceeds 50 ms. Peak private memory is 726.6 MiB, with first/last-minute
averages of 716.9/720.3 MiB. These are the same settings and copied checkpoint
as M4, with ordinary test-route input and test-only invulnerability.

An earlier run averaged 71.8 FPS while 7-Zip extraction was consuming substantial
CPU. It is retained as a failed timing result. The user confirmed the extraction
finished before the repeat. Both benchmark executables' `.text` sections are
byte-identical; their read-only data differs only in compile time and debug
identity. The recovery supports background contention, not a new optimisation
gain. Reports are `build/lighting-performance/refinement-m5-busy-soft/` and
`refinement-m5-busy-quiet/` respectively.

The Windows release build has no test input, capture instrumentation or
invulnerability. Its package includes `PLAYTEST.txt`, runtime libraries,
licences, comparison video, source revision and verified SHA-256 hashes; no
retail maps or personal saves are included. CI must finish successfully on the
published testing revision before delivery. Exact run links and artifact
identities are retained with the delivery evidence.

Modern shadows and dynamic lighting remain off by default. Host policy,
AMD/NVIDIA workarounds and Android exclusion are unchanged. AMD hardware,
Android gameplay, full campaigns and every third-party map are not qualified
by this pass. The local Android build still fails to launch its POSIX `sed`
generation command on Windows; platform CI provides the full-build gate.
The unchanged Web code passed the M4 full build and is checked again by CI.

Cold shader/mode-switch stalls and long-term memory growth remain explicit
limitations. The average-FPS gate applies to the documented hardware and 1024
configuration; it is not a guarantee for 2048/4096 or every graphics option.
The stock ambient/distant-light projection remains intentional, as explained
under M4. CStencil is outside this release.

Evidence: `build/modern-refinement/m5/` contains source snapshots/audit, focused
tests, menu captures, build logs, video provenance, CI results and package
identities. Runtime reports are in `build/lighting-compatibility/refinement-m5-*`
and `build/modern-shadow-menu/m5-*`. Disposable map copies are removed and test
saves are archived with content hashes verified before loose copies are removed.

The first publication's Linux CI compiled the game, then rejected the Android
shader-isolation harness under Clang 22: its recorded translation metadata was
set but never checked in that build. The harness now checks that an unregistered
stock shader receives no lighting translation metadata on either platform.
This strengthens stock-path coverage without changing production code or
disabling compiler warnings; all 17 isolation and negative-control cases pass
locally. The corrected publication reruns the platform CI gates.

## Sidewinder vehicle acceptance follow-up

The player's post-M5 captures reopen visual acceptance for occupied vehicles.
Both shadow modes and every resolution show large angular ground silhouettes
from Warthog/Scorpion muzzle flashes, and smaller wedges under a stationary,
non-firing Ghost. Earlier acceptance views did not catch these presentation
problems; passing CI alone does not close them.

The capture runner now accepts `--shadow-omit vehicle` or `occupants` for vehicle
muzzle fixtures and `ghost-engine`. These diagnostic controls remove only
casters, retaining visible geometry and lighting; normal builds exclude them.
Sidewinder comparisons establish that:

- Omitting the Ghost occupant and attachments removes the residual wedges.
  The existing exact-Ghost exclusion was already working.
- Omitting the Warthog/Scorpion hull removes the large silhouettes but allows
  muzzle light under the chassis. That is not an accepted turret fix.
- An earlier retained Warthog mesh/pose capture places its light just beyond
  the barrel, approximately 0.009 world units from the nearest triangle.
  Near-source magnification is plausible; this does not establish a matrix
  defect or justify arbitrary source offsets.

With the player's explicit agreement, `unshadowed_object` extends a vehicle's
existing engine-light exception to seated occupants and their attached gear.
Caster collection and receiver selection use the same predicate. It follows
live datum/parent identities and requires a seated unit under a vehicle owner,
with a bounded walk for malformed cycles. Dismounting restores ordinary shadow
participation immediately. Jackal exceptions remain exact-owner only. External
lights, nearby characters and unrelated attachments retain their behaviour.
No muzzle-light exception was added.

The focused suite passes 34 cases, including a negative control that reverses
the seat predicate. Sidewinder captures at 30/60/120 FPS exercise the stationary
Ghost in Hard/Soft modes; a moving external-light control retains the Ghost
silhouette. CE Death Island also reaches the live Ghost fixture. MCC Digsite
reaches gameplay, but the fixture reports `SHADOW SCENE failed no floor`;
that run does not validate Ghost rendering on MCC.
These bounded visual checks do not establish sustained-combat performance.
Windows Release and Web builds pass. The local Android build remains blocked
before compilation by its missing POSIX `sed` command; Android exclusion tests
pass. Initial MCC setup attempts failed their loopback join under restricted
network access; the permitted rerun reaches gameplay. Failed attempts are retained.

The runtime change remains inside `HALO_GLES` exclusion. Host policy, off
defaults, resolution, eight-light budget and vendor shader paths are unchanged.
Turret source placement/extent and own-hull projections remain open acceptance
work; this follow-up is not modern shadows 1.0 sign-off. Evidence and source
identities are under `build/modern-refinement/vehicle-review/`; comparisons are
the `build/plasma-visual/*-vehicle-review-*` runs.

## Finite-source turret experiment

The player's request to compare two and four sources is implemented only by
`tools/turret_shadow_probe.py`, installed temporarily by
`plasma_visual_build.py --turret-prototype` or
`lighting_performance_build.py --turret-prototype`. Both builders restore the
original sources in `finally` and rebuild the normal Windows executable.
No multi-source code or setting is added to a production build. The preceding
Ghost occupant fix is independent and remains in the worktree.

The probe retains eight logical shadow lights, all chassis geometry and a
single unchanged lighting contribution. For a vehicle muzzle light it renders
independent cube-array layers and averages their visibility. Two positions are
above/below the muzzle axis; four add left/right positions. The experimental
disk radius and forward distance are each 0.06 world units, explicitly supplied
rather than inferred from tags. The one-source control retains the original
position. This approximates finite-emitter visibility; it does not integrate
near-field radiance or the material response over the disk. A conservative
caster bound and union of per-source face masks avoid dropping blockers.

The initial prototype allocates the requested layer capacity for every active
map, including non-turret maps that use only one layer. At 1024, 32-bit depth
storage is 24 MiB per cube, or 48/96 MiB for two/four sources per world map;
viewmodel-specific copies are additional. These are allocation calculations,
not measured VRAM residency. Do not extrapolate this prototype's memory layout
to an eventual production implementation or qualify 2048/4096 from this test.

Sidewinder's Warthog and both Scorpion triggers were captured at 2560x1440 and
1024 shadow resolution for one, two and four sources at 30/60/120 FPS. Hard was
used at 60 and Soft at 30/120. All 18 runs met the existing 5% cap tolerance,
with complete shadow maps and no reported shader failures. Native images and
comparison sheets show retained chassis blocking but doubled/stepped edges,
especially behind the Warthog. The broad central silhouette remains. This is
not an accepted visual fix, and no production adoption has been made.

The timing fixture suppresses per-frame capture logs and screenshots, buffers
whole-frame times, excludes ten seconds of warmup, and records actual selected
turret maps. Capped visual checks still validate muzzle profiles and fades.
The first two uncapped runs retained valid frame data but failed an obsolete
verbose-log assertion; timing mode now relies on its active-map CSV instead.
The game binaries did not change for that runner correction.

Evidence is under `build/modern-refinement/turret-area/`, with individual
captures in `build/plasma-visual/*-turret-area-*`. These are bounded Windows
experiments on the local NVIDIA GPU, not AMD hardware, Android runtime,
multiplayer or full map-family acceptance. The ordinary renderer and vendor
paths are restored; no turret collision-based alternative is implemented by
this experiment.

Uncapped measurements use 30-second vehicle windows after ten seconds warmup,
with capture readback disabled. The initial averages on the RTX 5070 Ti are:

| Fixture | One source | Two sources | Four sources |
| --- | ---: | ---: | ---: |
| Warthog, whole window | 867.7 FPS | 727.9 FPS | 581.5 FPS |
| Warthog, active-map frames | 832.6 FPS | 681.3 FPS | 518.2 FPS |
| Scorpion, whole window (maps active throughout) | 707.3 FPS | 625.8 FPS | 473.7 FPS |

Active Warthog frame means are 1.20/1.47/1.93 ms; Scorpion means are
1.41/1.60/2.11 ms. These are whole-frame costs, not isolated GPU timestamps.
The Warthog fixture cycles firing, so its active-frame statistic is more useful
than mixing lit frames with idle frames. All stay above 240 FPS in this bounded
vehicle scene; that is not busy-combat acceptance or an eight-turret stress test.

Three sequential Two Betrayals snowy-fight runs used the copied fresh checkpoint,
25 seconds warmup, 60 seconds measured, 1440p/1024 Soft, AA and per-pixel lighting
off, all enhanced light groups on and controlled player invulnerability. Raw
averages were 284.6/202.6/243.3 FPS for one/two/four-source capacity respectively;
mean selected shadow maps were 8.00/7.98/7.88. The two-source run fails the average
240 FPS target. Their non-monotonic ordering and live encounter variation do not
establish a causal per-source cost. These passive runs also do not record active
vehicle-muzzle counts: do not treat them as evidence of sustained multi-source
vehicle firing. Use the dedicated vehicle fixture for that comparison. No
consistent busy-combat 240+ qualification is claimed.

The visual result already fails acceptance, so further sampling counts or broad
optimisation are not justified by this experiment. The recommendation remains
to evaluate collision-based blocking for the firing vehicle's own chassis while
retaining modern maps for other casters and external-light shadows. That is a
separate implementation decision, not a completed change or a blanket vehicle
shadow exemption.

A reverse-order Warthog control repeat gives two/one-source whole-window
averages of 722.5/834.0 FPS and active-frame averages of 677.5/801.6 FPS
(1.48/1.25 ms). It supports a roughly 0.23-0.27 ms two-source increment in
this fixture; it is not a precise universal GPU cost. Both repeats pass the
corrected timing validator. Final focused shadow, shader-isolation and lighting
configuration checks pass 28 cases; the five affected Python tools compile.
All 33 production-source hashes in the performance manifest match after
restoration, and the normal Windows rebuild passes. No additional production
renderer change or platform-build claim is made for these Windows-only probes.

## Collision-based turret chassis blocking

The approved follow-up replaces only the firing Warthog/Scorpion's rendered
caster for its own enhanced muzzle pulse with collision-based blocking. It
does not exempt the vehicle from external light shadows. Both Scorpion
triggers use their existing profile and independent pulse lifetimes. Occupants
and other objects remain rendered casters and receivers; the earlier Ghost
engine-light occupant exemption is separate.

`muzzle_lighting_vehicle_owner` resolves the live full datum identity using
the existing vehicle-occlusion profile flag and host-aware feature setting.
`modern_shadows_prepare` records that owner for the selected map and omits
only its rendered caster. `plasma_occlusion` retains its visibility field for
that light, tracing only the current owner's collision model through
`plasma_collision_object_depths`. Other casters stay in the modern map, so the
collision batch does not trace world geometry or unrelated objects again.
Changing owner or losing a complete map invalidates the field history before
returning to the established full-scene fallback. No collision pose is retained
beyond the synchronous batch.

This preserves the existing coarse field resolution, fading, update limits,
light positions, colours and falloff. Collision blocking is deliberately not a
pixel-accurate turret/barrel silhouette. No additional light, shadow layer or
resolution setting is introduced. The eight-light budget, off defaults, host
policy, GLES/Android exclusion and AMD/NVIDIA shader code remain unchanged.
All runtime changes are within the lighting feature modules and platform stubs;
the shared engine and renderer backend are unchanged.

Sidewinder checks at 2560x1440 and 1024 shadows cover both vehicles in Hard and
Soft at 30/60/120 FPS: all twelve complete within the existing 5% cap tolerance,
with no incomplete maps or reported shader failures. Native captures show the
oversized hard chassis projections removed while muzzle spill remains. External
projectile-light controls retain rendered vehicle shadows. Both shadow-off
controls pass, and lighting-off controls produce no enhanced light submissions,
shadow maps or occlusion updates on serial retry. The first concurrent off
attempts stopped with a Windows invalid-handle exception before gameplay;
their logs are retained rather than counted as passes.

The CE Scorpion control passes at 60 FPS. The initial CE Warthog fixture failed
to find its vehicle and captured rifle fire instead; the fixture now also
accepts the existing multiplayer Warthog tag. The corrected Warthog capture
passes at 60 FPS, with active chassis collision and complete modern maps.
MCC Digsite reaches gameplay but
does not equip the requested vehicles in these captures, so those attempts do
not qualify MCC turret rendering. Initial CE/MCC captures overlapped a platform
build and are not used for performance acceptance. Evidence is retained under
`build/modern-refinement/turret-chassis/` and
`build/plasma-visual/*-turret-chassis-*`.

The normal Windows release and Web builds pass. The Android refresh is blocked
by the local Windows invocation of NDK/CMake and Unix shell prerequisites;
even the guest-only check stops in prerequisite generation before compiling
the changed stub. No Android runtime, AMD hardware or multiplayer acceptance
is claimed from these bounded local checks.

Uncapped measurements use the normal renderer, buffered whole-frame timing,
no screenshots or per-frame capture logs, and no concurrent builds/game tests.
At 2560x1440, AA off, 1024 Soft shadows, the 30-second Warthog firing window
averages 802.9 FPS (763.8 on frames with an active turret map, 85.1% of frames).
The Scorpion averages 720.2 FPS with a turret map active throughout. These are
controlled vehicle fixtures, not busy combat measurements or universal speedup
claims against the earlier finite-source prototypes.

The fresh copied Two Betrayals exterior checkpoint, with the established snowy
route and explicitly invulnerable test player, averages 263.7 FPS over 120 seconds
after 25 seconds of warmup. Its two minute averages are 268.8 and 258.6 FPS;
eight modern maps are active throughout, with up to 27 scene lights. This meets
the requested average-FPS target in this run. P99 is 5.863 ms and one frame takes
84.135 ms: it does not establish a hitch-free 4.167 ms frame budget. Private
memory window averages rise from 714.4 to 719.3 MiB; this short check does not
settle the previously documented long-session memory question. The unchanged
production renderer hashes match the passive performance build manifest.

Final review covers owner identity/lifetime, caster/receiver separation,
map-failure fallback, bounded trace reuse, platform stubs and restoration of
the normal build. All 65 focused shadow, muzzle, collision, model-batch,
occlusion, shader-isolation and host-configuration checks pass; five Python
tools compile and `git diff --check` passes. One earlier harness invocation
overlapped temporary capture-source instrumentation and is retained separately
as invalid validation; the final run follows production-hash verification.
No further production refactoring was warranted by this focused change.

Disposable map copies and duplicate test executables/PDBs were removed.
Test saves were archived and hash-verified; original maps and saves were not
modified. Native captures, timing CSVs, reports and the current test builds
remain as local evidence. This change is local to `modern-dynamic-shadows`;
no testing/main push or CI claim is made by this acceptance record.
