# Modern dynamic shadows

## Phase 2 roadmap to 1.0

Phase 2 adds shadows from authored sunlight, identifiable map fixtures and
the flashlight. The effects are opt-in on desktop and follow host visual
policy. Classic remains the default. Android and web retain their existing
exclusions. The sun and fixtures have their own shadow budgets. Shared gives
an active flashlight one of the eight local slots. Extra Slot allows eight
local shadows plus the flashlight.

### Established on modern-dynamic-shadows

| Milestone | Result |
| --- | --- |
| E1 — Map sources and baselines | Verified active sky directions, fixture attachments and emissive BSP surfaces on representative Xbox, CE and MCC maps. Recorded the original 240 FPS baselines in the snowy Two Betrayals encounter and Rat Race lamp room. [Sources and measurements](environment_shadow_e1.md) |
| E2 — Directional sunlight | Added a stabilized Hard sun map with its own budget. Off-camera casters qualify when their shadows can reach visible receivers. Covered objects hand off from their existing environmental shadow path. [Sunlight and ownership](environment_shadow_e2.md) |
| E3 — Fixtures and flashlight | Added two independent fixture slots using map data, and a stock flashlight that shadows eligible objects and world geometry. Rat Race is the two-lamp reference scene. [Fixtures and flashlight](environment_shadow_e3.md) |
| E4 — Outdoor geometry, Soft and menus | Eligible placed models and BSP geometry cast and receive near-camera sunlight. Hard and Soft share the selected resolution. Environmental controls are available in every desktop menu style. [Coverage, limits and measurements](environment_shadow_e4.md) |
| E5 — Rendering performance | Signed off on 24 November 2026 at sustained 240+ FPS in busy Two Betrayals on the reference Windows PC. [Acceptance results](environment_shadow_e5.md#fresh-windows-launches) |

The reference Windows release at `e4022bf04` averaged 271.1 FPS in Hard
and 277.5 FPS in Soft in separate two-minute busy combat runs. Soft had
three frames above 100 ms. Those runs established the starting point for E5.
Sustained 360 FPS is deferred to a future performance milestone.

### E5 — Rendering performance and hitch removal

The Windows playtest at `2864bc026` is the accepted visual reference for
the Mercury Rising II outdoor sun fade and the first-person player silhouette
at Soft 1024. Keep the approach and retreat past the Covenant caches and the
matched Death Island pose as regression checks. The [matched combat comparison](environment_shadow_e5.md#sun-fade-playtest-comparison)
measured a sustained slowdown with Hard and Soft active. [Sun coverage and visual checks](environment_shadow_e4.md)

Map-load preparation and next-map shadow-mode changes are implemented. On
Xbox, CE and MCC maps, selection takes effect behind the existing gameplay-map
loading screen. A checkpoint revert or mid-level BSP change keeps the active
mode. Some first-use stalls remain. [Loading and driver measurements](environment_shadow_e5.md#loading-compatibility-and-driver-checks)

1. **Keep the accepted comparison reproducible.** Alternate the previous playtest
   (`9f114ec33`) and accepted playtest (`2864bc026`) in the same snowy
   Two Betrayals encounter. Use the same exterior checkpoint, camera and
   combat workload on the i7-14700K and RTX 5070 Ti. Run release PGO/LTO at
   2560×1440, AA and VSync off, 25 seconds of warmup and 120 seconds uncapped.
   Enable all dynamic lighting categories. Compare lighting alone, Hard 1024
   and Soft 1024. For both shadow runs, enable the sun, fixtures and Shared
   flashlight with all eight local slots available and adaptive scaling off.
   Record actual light, beam and combat activity. Keep busy Rat Race with both lamps as the indoor control.
2. **Windows CPU scheduling.** Four [fresh foreground launches](environment_shadow_e5.md#fresh-windows-launches)
   completed on the i7-14700K. Each of the 480 measured seconds in Hard
   and Soft exceeded 240 FPS. Both policies averaged 295 to 300 FPS when
   rounded to whole frames. Auto used performance cores at every sampled
   render-frame boundary on this workload. Keep Auto as the default.
   **CPU scheduling** is beside **Frame Rate Limit** in all four
   menu styles on supported Windows hybrid CPUs. **P-cores only** applies once at game startup to the client process
   and its workers, independently of the shadow settings. Save/Apply takes
   effect on the next game launch. Respect the process's existing CPU
   restrictions and keep normal scheduling if a usable set cannot be identified.
   Save, Cancel and Defaults passed in all four menu styles. Relaunching the packaged build restored Auto after selecting it.
3. **Find where the frame time goes.** Retain matched stock OpenCE and
   GulchCE controls. In the earlier heavier minute, stock GulchCE took
   2.63 ms/frame, lighting alone 3.68 ms, Hard 4.21 ms and Soft 4.22 ms.
   Start with the 1.05 ms stock-to-lighting gap. Time collision tracing and
   visibility reconstruction separately from BSP surface selection, temporary
   lighting triangles and model drawing. The [bounded exact-ray survey](environment_shadow_e5.md#exact-ray-reuse-and-receiver-state)
   found 432 repeats across 82.8 million rays. Retain exact moving-object
   intersections. The next comparison should profile foreground CPU world
   traversal and BSP diffuse/specular draw submission alongside GPU local
   and environmental depth. Continue timing local and environmental
   shadow depth generation and reuse, receiver shading, draw submission and
   driver waits. CPU and GPU measurements can overlap. Investigate the largest
   repeatable costs on the critical path. Pursue larger savings alongside
   smaller, measurable wins. Verify each with release timings and matched
   images, then measure the combined changes on the same workload. [Stock controls and benchmark procedure](environment_shadow_e5.md#upstream-stock-control)
4. **Recheck hitches independently.** Use Sidewinder's hidden Ghost reveal
   on first visit and warm reload. Record map preparation time, peak memory,
   worst reveal frame, 99th-percentile frame time and counts over 16.7, 33,
   50 and 100 ms. The [Sidewinder reveal checks](environment_shadow_e5.md#sidewinder-reveal) still measured a 30 to 35 ms reveal with the default preparation settings. A warm shader-prepared visit eliminated first-frame program acquisition while texture uploads continued. Keep
   preparation behind map loading and check Classic, host policy, failed
   preparation, cleanup and map transitions after changes.
5. **Qualify sustained FPS at fixed quality.** Use sustained 240+ FPS
   (4.17 ms/frame) in busy combat as the regression target for Hard and Soft
   independently. Record how many seconds fall below target, how often it
   happens and the longest continuous stretch.
   Track the full two-minute and heavier-minute averages alongside these
   sustained measurements. A repeatable gain in average FPS counts as progress
   while the sustained target is open. Check that the gain preserves visual
   quality and identify any increase in the frequency or length of slow stretches.
   Investigate recurring slow stretches and report isolated hitches separately.
   Occasional 1% lows are acceptable. Retain all eight local
   slots and independent environmental budgets. The continuous distance fade
   to stock shadows and the nearby player silhouette are required at every
   performance tier. Keep the Mercury Rising 2 outdoor camera move in the
   visual comparison so a faster result cannot hide the return of pop-in.
   Check thin contacts, cutouts, self-shadowing and light spill against matched
   captures.

### E6 — Distant sunlight and smooth transitions

The near sun map has a 12-world-unit half-width. Its visible strength already
fades continuously between 7.2 and 11.1 units in the sun's projection plane,
including the handoff to existing object shadows and baked BSP lighting.
E6 extends the distance at which geometry can cast and receive sunlight.
Compare a cached outer tile starting at 36 units half-width against the
accepted near-only build. The earlier prototype showed extra vehicle shadow
on Sidewinder and added submission time. Use those captures and timings as
the starting comparison. [Near coverage and outer-tile experiment](environment_shadow_e4.md#outdoor-coverage)

Keep eligible BSP and object geometry, including off-camera casters whose
shadows reach the view. Reuse valid static depth, refresh moving casters and
invalidate on relevant sky, BSP, geometry or map changes. Blend overlapping
tiles once per sun contribution and fade the outer coverage back to the
authored scene. Check Sidewinder drives, turns and strafes in both directions
and repeat the Mercury Rising II approach and retreat. Accept extended reach
when it adds visible distant shadows, preserves near detail and continuous
transitions, and meets the fixed-quality E5 performance target.

### E7 — Optional map authoring

Design a versioned companion file keyed to the compiled map and BSP. Allow
authors to refine emitter position, extent and direction and optionally
export a source's static direct-light contribution in existing lightmap UVs.
Occlude only that contribution and preserve indirect light. Verify automatic
lighting without the file, higher fidelity with a matching file, and safe
fallback for stale or malformed data. Preserve Sapien, stock tags, original
maps and multiplayer map identity.

### E8 — Optional adaptive quality

Offer independent adaptive resolution and shadow-count controls, both off
by default. Keep the existing planned targets of 30, 60, 120 and 240 FPS and
include 360 FPS for the new target hardware. On sustained shadow-related
pressure, lower resolution first and reduce counts only when enabled. On
recovery, restore counts first, then raise resolution up to the selected
ceiling (4096 maximum). Use separate thresholds and slower recovery to
avoid oscillation. Keep environmental budgets independent. Exercise sudden
combat, camera cuts and quiet cutscenes, check quality recovery and measure
allocation hitches. The fixed-quality E5 gate applies with scaling disabled.

### E9 — Compatibility and Phase 2 1.0 sign-off

Verify stock/off isolation, host allow/deny, mixed client settings, joining,
respawns and map transitions across representative Xbox, CE and MCC maps.
Check all desktop menu styles and the pending shadow-mode lifecycle. Exercise
AMD and NVIDIA renderer paths and record any unavailable hardware. Refresh
platform checks, including Android exclusion. Check visuals at 30/60/120 FPS
and repeat the fixed Hard/Soft 1024 combat, Rat Race lamp and Sidewinder
transition tests. Review combined changes and cache lifetimes, deliver matched
captures, frame-time and loading results, and a Windows playtest for sign-off.
Promote through testing and main with CI verification when approved.

CStencil remains a separate project after Phase 2.

## Current investigation

Visual acceptance remains open after further player, Ghost and grenade
reports. The [backend investigation and bounded stencil comparison](shadow_backend_investigation.md)
separates emitter placement, stock-shadow composition and visibility quality.
The existing shadowmap implementation is preserved; the stencil probe is an
offline diagnostic, not a shipped CStencil mode.

The follow-up M2 environmental replacement is implemented locally; its results,
stock fallback and open acceptance gaps are recorded in the investigation above.
M3 verifies per-light composition with GPU/readback evidence and an in-engine video.
M4 adds environmental model self-shadowing and render-family bounds, with
geometry/material checks across Xbox, CE and MCC captures. The initial 236.2 FPS
busy-combat result missed the target. A subsequent comparison found a small
environmental-model cost; opaque-depth and tighter-bounds experiments did not
demonstrate an overall combat gain and were discarded. Local-depth attribution
then identified duplicated common geometry between world and first-person cubes.
Sharing that depth produced a measured gain and a fresh two-minute result of
254.9 FPS with all eight shadow lights active at 2560x1440 / Soft 1024. This
meets the average-FPS gate for that run, with one 314 ms hitch still recorded;
see the investigation for the controlled comparison and qualification limits.
The [M4A single-source shadow experiment](shadow_emitter_m4a.md)
follows M4 and precedes M5 stability and M6 final acceptance. Its multi-sample
finite-emitter prototype was retired; the replacement PCSS comparison is planned,
not implemented or accepted. M5 includes MCC
replacement coverage and combined mode-switch hitching. Wider visual acceptance
remains open.

## Refined shadow behaviour

The [refinement milestones and reproducible baseline](modern_shadow_refinement.md)
track the renewed acceptance work, including held-weapon shadows on the player
body and helmet/visor self-shadowing.

Sidewinder playtesting in both Hard and Soft exposed incorrect Warthog/Ghost
silhouettes, one-sided Jackal shield illumination, missing corpse shadows,
missing player self-shadowing and overlapping stock character
shadows. The earlier controlled checks and passing CI do not establish visual
acceptance for these cases. The refinement retains the off default and now uses
an eight-light budget with the shared 128–4096 shadow-resolution setting.
The current quality, performance and delivery evidence is in the refinement
guide; older milestone measurements below describe the original two-light
implementation. Self-shadowing acceptance covers
all applicable objects, players and NPCs under every supported dynamic-light
category, including their own continuous weapon glows and firing effects.
The revised Jackal/Ghost agreement is an emitter-local exception: shield and engine
lights illuminate their own owner without casting or receiving modern shadows
from that light. Other objects still cast and receive from those lights, and
external lights still shadow the Jackal and Ghost normally. This supersedes the
earlier requirement for shield-on-Jackal and engine-on-hull self-shadowing.
Ghost engine lights also exempt seated occupants and their attached gear;
dismounting immediately restores their ordinary shadow participation.

### Visual reference and acceptance criteria

The user-provided Halo 2 pstencil-era images and the
[Halopedia gallery](https://www.halopedia.org/Pstencil) are the visual reference
for this refinement. They supplement the cases above; they do not change the
implementation to stencil volumes or bring CStencil into scope. The aim is
recognisable silhouettes, convincing self-shadowing and a clear relationship
between each emitter, blocker and receiving surface. The screenshots are visual
targets, not evidence of how this implementation should calculate shadows.

- Under an external light, the Warthog's wheels, chassis and cabin must produce
  recognisable, correctly oriented silhouettes on the ground and on the vehicle.
  The Ghost must pass the same geometry and motion checks. A nearby onboard
  emitter need not project the complete vehicle silhouette.
- Moving weapon and projectile lights must move character and vehicle shadows
  coherently across floors, walls and other models. Living and dead bipeds,
  player bodies, NPCs and applicable first-person geometry need explicit checks.
- Jackal shields retain radial spill and damage colours. The bearer and its
  currently equipped weapon are lit without casting or receiving shadows from
  that shield. Other lights can still shadow both. Switching or dropping the
  weapon immediately restores its participation in the shield's shadow map.
- Ghost engines retain their existing local illumination without projecting
  their own hull's strips and wedges. This applies to campaign and multiplayer
  engine emitters only; external lights still cast the Ghost's silhouette.
- The Ghost engine exception includes its seated occupants and their attached
  gear. Neither exemption extends to nearby
  characters, dropped weapons, other vehicles, stationary shields or other lights.
- Hard mode must retain crisp, stable edges; Soft mode must preserve the same
  silhouette and contact while softening edges. Detached shadows, surface acne,
  conspicuous cube seams and popping fail acceptance in either mode.
- Shadowing must attenuate the relevant dynamic light, preserving unrelated
  lights and baked illumination. The reference's darkest areas are not a reason
  to force black shadows everywhere or suppress the level's ambient lighting.

Use matched before/after video with identical cameras, light placement and
settings, including motion at 30, 60 and 120 FPS. Consult the refinement guide
for the tested cases and remaining device/capture limitations. Busy Two
Betrayals validation must retain the agreed 240+ average FPS target with the
refined effects and bounded eight-light budget. Off-default behavior, host
policy, AMD/NVIDIA workarounds, Android exclusion and Xbox/CE/MCC ownership
remain requirements throughout.

In the Modern menu, open **Video > Advanced Graphics > Shadows**. Other menu
styles use **Video Setup > Shadows**. Choose **Classic**, **Modern Hard** or
**Modern Soft** and set the shared shadow resolution.
Classic is the default and uses the original object shadows. The mode setting is
`display.modern_shadows` (`"off"`, `"hard"`, `"soft"`); `HALO_MODERN_SHADOWS`
remains its environment override. Android and web omit this control and compile
no-op implementations.

Changing the spinner stages a preference; Save applies it and Cancel discards
the edit. Defaults stages Classic and the default shadow resolution.
The saved choice survives a restart. Selecting Hard or Soft does not enable
any lighting category: shadows need an independently enabled, eligible light.
Host lighting policy remains authoritative.

## Scope and light eligibility

All enabled lights selected by the dynamic lighting system are eligible:
projectiles, muzzle flashes, world and vehicle emitters, shield emitters,
Shield Glow and held-weapon glows. The historical `plasma_lighting_selected`
name does not restrict selection to plasma or projectiles. Existing local
toggles and host policy remain authoritative; shadows do not create lights
or enable a disabled lighting category.

The refinement budget renders **eight selected lights per primary view**. This
is a bounded implementation, not a promise that every eligible light is shadowed
simultaneously. Vacancies and weaker incumbents are considered first;
a surviving light keeps its map slot when scores reorder. The score favours
bright, nearby lights with useful radii; an incumbent identity stays selected
while its score is at least 80% of its best available challenger. Recycled rasterizer
indices cannot inherit another light's map. A challenger between 1.25 and
2 times the current score must remain stronger for 100 ms of rendered game
time. A light at least twice as strong, or replacement of an expired or
ineligible light, is immediate. This suppresses frame-to-frame competition
without delaying strong flashes or retaining host-rejected lighting.

For example, with projectile lighting disabled and Covenant shield lighting
enabled, a stationary shield can supply the selected light. Opaque world and
model triangles block its illumination. The translucent shield surface is
not turned into an opaque wall. Other lighting and baked illumination remain
unchanged. A muzzle flash participates through the same selection interface.
For a handheld muzzle flash, Hard and Soft omit the firing weapon from its own
light's caster list in first and third person. The shooter and first-person arms
still cast. For weapon-mounted lights, the exclusion also covers charge buildup,
a held full charge, overload, automatic fire between rounds, shot recovery and overheating.
Either trigger can activate it, even with muzzle flashes disabled. A newly fired
projectile also identifies its launching weapon while their render bounds overlap.
After it travels clear, its light casts the weapon normally. A different projectile or another
shooter's light can cast the entire weapon throughout.
For the Warthog turret and both Scorpion triggers, the firing vehicle's own
collision hull blocks its muzzle light. A selected modern map omits only that
vehicle's rendered caster and still shadows other objects, including occupants.
The vehicle remains a receiver, and external lights still cast its full rendered
shadow. This removes the flash contribution behind blockers; it does not darken
baked illumination or replace the original vehicle shadow. Collision blocking
uses the existing coarse visibility field, so it is not a detailed silhouette of
the turret or barrel. The Ghost engine-light occupant exemption is separate.

## Reading route and ownership

1. [modern_shadows.c](../../port/linux/game/modern_shadows.c):
   `modern_shadows_submitted` records stable light identities;
   `modern_shadows_prepare` selects from existing feature lighting.
2. `world_casters`, `object_casters`, `viewmodel_casters` and `mesh_get` collect rendered triangles,
   material alpha tests and interpolated world-model poses. The map owns its
   original buffers. The feature owns only its converted copies. Existing
   CE/MCC palette accessors retain ownership of their family-specific data.
   `model_caster` shares the skinning and material path between world models
   and first-person models. The latter read the current interpolated pose
   through [modern_shadows_first_person.inl](../../port/linux/game/modern_shadows_first_person.inl),
   included by the owner of the private weapon/hand remapping tables. It
   copies poses without advancing animation or modifying game state.
3. [modern_shadows_gl.c](../../port/linux/src/modern_shadows_gl.c):
   `modern_shadow_begin`, `modern_shadow_draw` and `modern_shadow_end` build a
   six-face depth cube: a depth image looking in each direction from the light.
   Casters use reversed floating-point perspective depth and zero-to-one
   clipping, without writing fragment depth. Receivers decode it to radial
   distance for the existing plane comparison. The 0.005-unit near plane
   must agree between the caster projection and receiver decoding.
   Each selected light has its own maps; hard and soft modes share their format. Soft mode uses four visibility
   samples with bilinear comparison weights to soften the edge; it does not
   blur baked lighting. Samples reproject across cube faces explicitly.
4. `modern_shadow_emit` and `modern_shadow_bind` feed only the selected
   light's world diffuse/specular attenuation or model point-light slot.
   `modern_shadows_map` resolves the two model slots independently; their
   order need not match the shadow slots. The second receiver cube uses
   texture unit 10; unit 9 remains reserved for caster alpha textures.
   Receiver-plane depth comparison reduces grazing-surface acne without a
   large constant offset that would detach shadows from feet.
5. [plasma_occlusion.c](../../port/linux/game/plasma_occlusion.c): a complete
   rendered map replaces the chosen light's coarse collision visibility.
   Vehicle muzzle lights are the exception: `modern_shadows_collision_owner`
   requests an owner-only collision field alongside the rendered map.
   `plasma_collision_object_depths` traces the current chassis pose without
   tracing world geometry or unrelated objects a second time. A changed owner
   or failed map invalidates that field before restoring full-scene visibility.
   Existing directional/height masks still come from
   [ammo_counter_falloff.c](../../port/linux/game/ammo_counter_falloff.c).
   Other lights retain their existing occlusion. Failed/incomplete maps use
   the established path instead.

The additive hook in [render.c](../../source/render/render.c), after light
preprocessing and before object drawing, is necessary because a feature
module alone cannot schedule depth rendering at that engine boundary. The
hook is marked `port:`. Shader selection and GL state integration live in
[d3d8_gl.c](../../port/linux/src/d3d8_gl.c) and
[nv2a_psh.c](../../port/linux/src/nv2a_psh.c); feature calculations remain in
the shadow modules. No gameplay, networking protocol or map loading rules
are changed.

## Lifecycle and bounds

Each depth cube uses `display.shadow_resolution` pixels per face (128 through
4096). The unchanged default is 128; 1024 is the recommended modern-shadow
quality/performance balance. Hard and Soft use the same selected resolution.
Higher resolution improves silhouette detail but does not reproduce the
resolution-independent edges of shadow volumes. A visible first-person model
intersecting the light's range adds a second cube, allocated lazily and rendered
only while those casters are present. World receivers use the complete world body and weapon; the
first-person receiver uses the arms/weapon pose and excludes their hidden
world counterparts. Other casters participate in both maps. This prevents
double silhouettes without removing the player's body shadow from the world.
Each light's two cubes share the frame's copied bone matrices. Converted
meshes and the draw queue are shared between lights. At 1024, each cube takes 24 MiB: eight world maps use up to 192 MiB,
or 384 MiB with all first-person maps allocated. Each doubling quadruples
this depth storage: the equivalent maximum is 1.5 GiB at 2048 and 6 GiB at
4096, excluding other renderer resources. These are explicit quality choices,
not a promise of 240 FPS at every resolution. Changing resolution releases
old cubes before lazily creating replacements; converted meshes are retained.
Only selected lights allocate cubes. Conservative posed mesh bounds reject
geometry outside the light and skip cube faces it cannot intersect; unsupported
bounds retain the original submission. The local eight-light Soft/1024 combat
run now clears 240 average FPS. See the [refinement measurements and limits](modern_shadow_refinement.md#conservative-caster-submission);
later emitter-placement and edge-stability acceptance remain separate gates.
Mesh storage is bounded to
2,048 parts and 32 MiB; each selected light holds at most 256 caster draws with 64 bones
per draw in the reused queue. Incomplete caster collection falls back instead of displaying a
partially populated shadow map. Initialization is lazy. Disabling shadows,
changing BSP or disposing lighting releases feature resources. Each render
window clears transient slot associations and validity.

The shadow pass restores the engine's clip convention and uses
`xgpu_gl_end_external_pass` to release its vertex array and invalidate GL
state, matching the existing anti-alias/HDR handoff. Subsequent draws and
clears bind their own targets; draws reapply the unchanged D3D viewport and
depth range. This avoids synchronous driver-state queries before each depth
pass. The helper is needed because the device's scratch vertex array is
private to the renderer. It does not change reversed-depth conventions. Shadow shader
failures have separate failure state, restore existing collision attenuation
(including an already-bound world texture), and stop further depth passes
until the feature or map resets. Disabled draws use the stock/lighting
shader keys without shadow uniform lookup, binding or depth passes.

Desktop model-mask uploads also avoid querying the active texture unit.
They use reserved units 6/7, then leave uploads on unit zero. Desktop
material draws use named multi-bind, so their bindings remain unchanged;
GLES retains its previous active-unit save/restore. In a matched, fixed
emitter profile this reduced model-light binding from 0.925 to 0.035 ms
and raised the diagnostic average from 446 to 564 FPS with the same soft
filter. This identifies a driver synchronization cost, not a reduction in
shadow quality. The fixed scene is not busy-combat acceptance.

## Prototype limits and validation

World models and first-person arms/weapons cast using their interpolated
poses and original inverse bind matrices. Hidden viewmodels, zoom, mirrors
and third-person views do not add first-person casters. Missing or invalid
remapping tables cannot borrow a previous weapon's pose. A map which omits
hands still excludes the hidden body from its weapon's receiver map.

Model/environment cutouts use the original bitmap permutation, alpha
threshold and texture animation evaluation, including object function
values and render time. Opaque materials skip the alpha evaluation.
Transparent materials and alpha-blended decals do not cast solid shadows.
Removed model regions retain the engine's signed `NONE` interpretation.
Broader material and map/mesh capacity stress coverage remains necessary as
the light budget expands. The one-light quality coverage is recorded below.

The two small additions in `source/` are required because private first-person
remapping tables and the actual receiver draw boundary are not accessible
from a standalone game module. The included pose accessor owns feature
logic; `_rasterizer_model_begin`/`_rasterizer_model_end` only select and clear
the receiver kind. Off/GLES paths perform no shadow GPU work. The world/first-
person distinction does not alter the AMD/NVIDIA workarounds or map adapters.

Use the real shader selection/failure harness and renderer translation tests
for disabled-path and GLES checks. Use `tools/plasma_visual_build.py` and
`tools/plasma_visual_run.py --modern-shadows off|hard|soft` for controlled
images, including non-projectile sources. Capture timing includes screenshot
readback and is not sustained performance evidence. Use the passive timing
build and `tools/lighting_performance_run.py` for matched busy-combat runs
from the same compatible checkpoint, resolution and quality settings.

Acceptance uses sustained busy-combat **average FPS**; 240+ is the target.
Recurring stalls still need reporting, while an isolated 1% low is not by
itself a failure. Completed coverage is listed below. CStencil and a
production multi-light budget are separate work.

## Initial prototype validation (2026-11-15)

The focused suite passes 82 checks, including actual selection and shader
dispatch, host/local eligibility, GLES exclusion, failure fallback, state
handoff and mask-binding ownership. Windows and web release builds pass.
The full Android build could not run on this Windows toolchain because its
generated build graph requires POSIX utilities; no Android device result is
claimed. The existing AMD/NVIDIA workarounds were not changed; runtime
captures here use an NVIDIA RTX 5070 Ti.

Representative final captures exercise Xbox Blood Gulch soft model shadows
at 120 FPS, Custom Edition Death Island hard model shadows at 60 FPS, and
MCC Digsite soft world/model receiving at 30 FPS. Earlier captures also
exercise stationary shields, Warthog flashes, world emitters and disabled
modern shadows. These are bounded functional checks, not exhaustive material,
map, multiplayer or hardware acceptance. Captures and their source manifests
are under `build/plasma-visual`; production sources contain no capture hooks.

Busy-combat measurements use a verified copy of the Xbox c40 checkpoint
before exterior door A, ordinary controller input into the snowy encounter,
native 2560x1440, all dynamic-light categories enabled, and an uncapped frame
rate. A test-only invulnerable player prevents death/reload contamination.
Each run has 25 seconds of warmup and 120 measured seconds, without image
readback or concurrent compilation. AI and camera displacement can differ
between runs, so averages and light counts must be reported together; these
are not deterministic frame replays. The passive fixture records active
shadow maps to distinguish rendering from fallback. Reports and build/source
hashes are under `build/lighting-performance/modern-final-combat-*`.

| Mode | Average FPS | Minute averages | Mean / maximum selected lights | Frames with a shadow map |
| --- | ---: | --- | --- | ---: |
| Modern shadows off | 317.7 | 332.1 / 303.2 | 13.04 / 23 | 0% |
| Hard | 375.9 | 394.5 / 357.4 | 11.18 / 20 | 100% |
| Soft | 327.7 | 356.0 / 299.3 | 11.64 / 20 | 100% |

All three runs exceed the average-FPS target, with no frame above 50 ms.
Different combat loads mean this table does not establish that shadows are
faster than lighting alone; the matched fixed-emitter profile above provides
the evidence for removing binding queries. First-use mesh/shader construction
is outside these warmed measurements. Two-minute runs do not establish
long-term memory stability. This is a one-light prototype, not sign-off of
the final multi-light shadow system or all material and multiplayer cases.

## Milestone 1: caster coverage (2026-11-15)

First-person casting now uses the same interpolated remapping as the visible
weapon and hands. Regression fixtures exercise missing models, zoom/hidden
views, mirrors, respawn ownership, invalid remaps, inverse bind matrices,
removed regions, material permutations, animated cutouts, receiver separation,
lazy second-map allocation, failure fallback and disposal of both cubes.
The final focused suite passes 137 checks, including the existing host/local
eligibility, stock-shader isolation and Android exclusion tests.
Windows and web release builds pass. The configured Android build is blocked
by the existing Windows build environment: its generated command launches a
non-Windows compiler wrapper, and SDL configuration also fails. No APK or
Android device validation is claimed; the GLES no-op path and Android
exclusion checks remain green. The original configure options were restored.

Representative NVIDIA captures cover Xbox first-person hard/soft shadows at
30/60/120 FPS, a crouching/standing player at 120 FPS, Warthog firing at 60 FPS,
CE Death Island muzzle flashes at 60 FPS and MCC Digsite model receiving at
30 FPS. The capture-only diagnostics confirm two maps for visible first-person
casters, one for the exterior vehicle view, and no incomplete-map fallback in
these successful captures. Animated cutout transforms and transparent-material
rejection have asset-free regression coverage; this is not exhaustive visual
acceptance of every material, map or render effect. One initial MCC run failed
its sandboxed loopback connection before loading; the same local-match check
passed with local networking available.

The following fresh combat runs use the same checkpoint, route, native
2560x1440 resolution and passive measurement method described above: all dynamic
lighting categories enabled, 25 seconds warmup and 120 measured seconds, with
no screenshots or concurrent build. Each mode uses the same Windows release
executable (`--pgo=off --lto=off`).

| Mode | Average FPS | Minute averages | Mean / maximum selected lights | Frames with a shadow map |
| --- | ---: | --- | --- | ---: |
| Modern shadows off | 296.2 | 320.9 / 271.6 | 18.20 / 31 | 0% |
| Hard | 303.1 | 312.0 / 294.1 | 8.66 / 16 | 100% |
| Soft | 291.0 | 300.6 / 281.5 | 11.88 / 22 | 100% |

Every measured minute exceeds the 240 FPS average target. No measured frame
exceeds 50 ms. The live fight's differing light counts preclude a claim that
Hard is faster than Off, or a percentage comparison with earlier combat runs.
These are bounded, warmed runs, not long-term memory or first-use acceptance.
Reports and source manifests are in `build/lighting-performance/modern-m1-combat-*`;
visual evidence is in `build/plasma-visual/*-m1-*`, and check/build logs are in
`build/modern-m1`. AMD hardware was not available; vendor workarounds are unchanged.

The final review shared the world/viewmodel collector rather than duplicating
skinning or material handling. Converted meshes and bone submissions are reused
between receiver maps. Further caching or depth-pass optimisations need measured
evidence; the multi-light budget, menu integration and broader quality acceptance
remain later milestones.

## Milestone 2: shadow quality (2026-11-15)

The receiver and selection fixes stay inside the two shadow modules. They
add no shared renderer hooks, depth passes, texture allocation or sample
count. Hard remains a single binary comparison. Soft uses four weighted
comparisons on the cube face's texel grid instead of four equal-weight
samples in a tangent basis that changed abruptly with direction.

Receiver-plane comparison now uses the actual forward intersection instead
of clamping it to within 5% of the receiver distance. Empty texels remain
unblocked when that intersection lies outside the light's range. Small
screen-space derivatives are normalized rather than mistaken for a missing
normal: the old cutoff disabled plane correction on close-up viewmodels,
producing repeating stripes on the rifle. The offset remains small; this
does not hide acne by lifting the entire shadow away from its caster.

`tools/modern_shadow_quality.py` runs the actual emitted receiver GLSL in a
hidden Windows OpenGL context against independently constructed analytic
depth cubes, and rasterizes the production caster into all six faces to
check the projection/receiver agreement. It requires NumPy and Pillow, but no game assets or production
instrumentation. Run it with `--output build/shadow-quality`; use `--source`
and `--record` to retain a pre-fix comparison. It checks front-facing and
grazing planes, positive/negative cube seams, a cube corner, close receivers,
small derivatives and the map-range boundary. Separate blockers check that
Hard stays binary, Soft preserves fully lit/shadowed interiors, and an edge
crossing a cube seam remains continuous.

On the RTX 5070 Ti, the original grazing fixture incorrectly shadowed 55.9%
of pixels in Hard and 100% in Soft; the corrected fixtures have zero false
shadow pixels in either mode. The close-up derivative fixture likewise goes
from 100% false shadow coverage to zero. Soft's edge traversal improves from
three distinct levels and a maximum 128/255 step to 52 levels and a maximum
6/255 step, with the same four texture reads. The blocker crossing the cube
seam has a maximum 1/255 step along the seam. These are controlled shader
measurements, not a claim of alias-free shadows at every distance.

The selection fixture reproduces repeated shadow-slot swaps on the original
code and verifies the new 100 ms admission rule at 30, 60 and 120 FPS. It also
checks immediate dominant flashes, host rejection, identity reuse, clock
rewind and existing map/disable/failure cleanup. The one-light prototype can
still make a discrete transfer when a selected light expires or a dominant
challenger wins. It does not blend through an unoccluded state; other lights
keep their established collision visibility. A larger simultaneous-light
budget remains a separate milestone.

The final focused suite passes 137 checks. Windows and web release builds
pass; the Android build attempt still fails before producing an APK because
the Windows graph launches the NDK's non-Windows compiler wrapper, with a
separate SDL/CMake configuration failure. GLES exclusion tests pass. No AMD
device or Android runtime result is claimed; vendor workarounds and platform
guards are unchanged. The shader cache keys binaries by emitted GLSL, so
these receiver changes do not require users to delete their cache.

Final captures exercise Hard and Soft first-person receiving at each of
30/60/120 FPS, MCC Digsite at 120, Warthog chassis receiving at 60, a crouching
holder at 120, and a moving door at 30. All pass the runner's existing
frame-rate and completeness checks, with no incomplete-map samples. CE
Death Island at 30 completes its visual/muzzle sequence, but its overall
rate check fails because of one 1.83-second gap while no feature light is
selected; active-light sections average 30.18 FPS. That run is retained as a
pacing failure, not counted as a clean cap pass. Its cause is not established
by this capture. The full Xbox Hard/Soft matrix supplies the cap validation.

One earlier Soft/120 capture overlapped web linking and missed its rate
check; another exhausted the short wall-clock deadline during cold startup.
`plasma_visual_run.py --startup-grace 15` allows additional startup time
without changing captured simulation ticks, expected actions or acceptance
thresholds. The clean Soft/120 run completes all three shots and 16 images,
averaging 119.73 FPS. The retained before/after close-up comparison is
`build/modern-m2/fp-quality-comparison.png`. GPU reports, source manifests,
build/check logs and final `*-m2-quality-*` captures retain the evidence.

### Combat performance investigation

The first passive combat matrix used the fast validation build
(`--pgo=off --lto=off`), native 2560x1440, the same archived snowy-route
checkpoint, all dynamic lighting categories, 25 seconds warmup and two
measured minutes. Off averaged 317.6 FPS, Hard 331.7, but Soft only 215.5
(244.4 / 186.6 in its two minutes). All shadow-enabled frames had valid maps,
and no measured frame exceeded 50 ms. The failed Soft result is retained in
`build/lighting-performance/modern-m2-acceptance-soft`; it is not a passing
240 FPS result. Live AI/light loads differ between runs.

A controlled receiver comparison then used identical selection/collection
code, a fixed shield, the same three lights and profiling instrumentation,
changing only `modern_shadow_emit`. With 15 seconds warmup and 30 measured
seconds, the old/new receivers averaged 383.6 / 392.0 FPS. CPU render time
was 1.198 / 1.190 ms; sampled model-light GPU time was 0.0359 / 0.0361 ms,
diffuse 0.0784 / 0.0832 ms, and specular 0.1281 / 0.0985 ms. This provides
no evidence of a substantial receiver-cost regression; it does not establish
a general speedup. Reports are `modern-m2-fixed-{old-receiver,quality}`.

The 90-second busy Soft profile averaged 226.8 FPS in the same fast build.
Its inclusive CPU render time was 3.882 ms. Model-field resolution took
0.873 ms and collision tracing 0.622 ms; these scopes overlap and must not
be added. Shadow preparation itself was 0.052 ms CPU and the depth passes
0.079 ms GPU. The evidence points to the broader CPU workload rather than
the new four-tap filter. The profile is diagnostic, not acceptance timing.
The profiling builder's selection timer now follows the new selection helper.

Final passive validation uses the **normal Windows release configuration**,
`--portable --release`: full LTO and `pgo/halo_windows.profdata`, with no
profiling instrumentation beyond the passive timing fixture. The scene,
resolution, lighting categories, route, warmup and 120-second measured
duration remain unchanged; no builds, GPU diagnostics or captures run
alongside it.

| Mode | Average FPS | Minute averages | Mean / maximum selected lights | Frames with a shadow map |
| --- | ---: | --- | --- | ---: |
| Hard | 325.1 | 350.5 / 299.7 | 10.38 / 20 | 100% |
| Soft | 306.7 | 338.9 / 274.6 | 12.31 / 22 | 100% |

Both normal release runs meet the 240+ average target in each measured
minute. Neither has a frame above 50 ms (worst: 15.59 / 17.13 ms). Reports
and exact source/build manifests are
`build/lighting-performance/modern-m2-release-{hard,soft}`. The live encounter
still varies: these runs validate the release configuration, but do not
isolate a percentage gain from LTO/PGO or establish that Hard is intrinsically
faster than Soft. The slower fast-build result remains relevant to developer
builds. Future light-budget increases should focus on the measured CPU
visibility workload before changing receiver quality.

The final review retains the same 512-pixel maps, caster coverage, four-tap
Soft budget, authored light shapes and stock shadows. No shared renderer,
map-family, vendor workaround or host-policy implementation changed for M2.
The original configure options and normal Windows executable are restored.
This closes the bounded one-light quality milestone on the tested NVIDIA
system; first-use stalls, the separate CE pacing failure, broader hardware
acceptance and multi-light scaling are not silently signed off by it.


## Milestone 3: two simultaneous shadow lights

The feature now maintains two stable identities and independent world/view
depth-map pairs. It visits the weaker incumbent first, excludes the other
slot's live identity from admission, and retains the existing score hysteresis
and 100 ms challenger interval. Strong flashes remain immediate. Model slots
resolve by rasterizer identity, so reordering lights cannot swap their depth
cubes. A failed or incomplete map cannot borrow another light's depth; an
independently complete map remains usable. Window, BSP, disable and shader
failure cleanup covers both slots.

This change stays in the feature's game and GL modules. It adds no shared
renderer, vendor, host-policy, map-loading or networking changes. It retains
512-pixel faces, the Hard comparison and four-sample Soft filter, animated
casters and separate first-person receiving. The geometry pool, submission
queue and depth program are shared; only depth images and light coordinates
are per selected light. The budget remains two rather than speculatively
raising it to four.

The optional capture-builder flag `--shadow-reuse` measures conservative
cross-frame reuse candidates. It compares the source and full submitted
poses/material data, and rejects alpha-tested draws because their texture
contents may change independently. The moving-projectile check found zero
matches in 120 samples per map/receiver; the stationary c40 shield found zero
in 240 samples per receiver. Full-record comparison can reject unused-field
differences too; this is not proof that every alternative cache would miss.
It provides no justification for adding a persistent cache here. A stationary
light alone is insufficient: animated nearby casters still change its map.
The probe's snapshots and logging exist only in explicitly requested capture
builds, never normal or passive performance builds. The initial M3 diagnostic
capture executable included this probe before it became an opt-in builder
flag; its results are diagnostic, not release performance measurements.


### Visual and platform checks

The GPU quality fixture still checks Hard/Soft interiors, edges and cube
seams. It additionally binds two independent cubes through the emitted model
shader: the first blocks a receiver while the second remains lit, and disabling
the first model slot restores full visibility. Runtime diagnostics identify
both selected map slots independently.

The Xbox three-projectile overlap sequence passes Hard and Soft at 30, 60
and 120 FPS, with two complete maps and the third light retaining collision
occlusion. The initial Hard/120 capture averages 108.8 FPS and fails its cap
check (worst recorded gap 128 ms); an unchanged-binary repeat averages 119.8.
Its cause is not established, and the failed capture is retained. CE Death
Island passes its AR muzzle sequence at 30 FPS; MCC Digsite passes the
three-projectile/model/specular sequence at 120 FPS (119.3 average). These
are representative map checks, not exhaustive family or hardware acceptance.

The Warthog exterior capture passes at 60 FPS. The Soft/Off comparison shows
the chassis blocking muzzle illumination under and behind the vehicle;
stock flash effects and original shadows remain. A separate 120 FPS capture
records a 468 ms gap and fails its overall rate check at 110.0 FPS despite a
9 ms active-frame 95th percentile. That failure remains part of the evidence. The unchanged-binary repeat
passes at 120.0 FPS; neither run establishes the cause of the intermittent
gap. The stationary-shield reuse probe uses c40; an initial Blood Gulch attempt
failed because that map lacks the shield tag and is not counted as coverage.

Web builds successfully. Android's full build attempt still fails at the
Windows launch of the NDK's extensionless non-Windows compiler wrapper, with
the separate SDL configuration issue; no APK or device run is claimed. AMD
hardware is unavailable. Existing vendor workarounds and GLES exclusion stay
unchanged. The normal Windows build uses the original `--portable --release`
configuration, including LTO and the repository's Windows PGO profile.

The final combined review checks slot ownership, fallback, lifetime, shader
binding and platform isolation; all 137 focused checks pass. The tests use
production selection and GL-owner functions, including failed-one/valid-other
handling, reordered model slots, host rejection, identity reuse and cleanup.
The review introduces no persistent depth cache or shared-engine changes:
there is no demonstrated benefit sufficient to justify either for this
bounded two-light milestone. Broader CPU visibility work remains a separate
optimisation opportunity, not a claimed outcome of these shadow changes.


### Normal release combat acceptance

Both runs use the archived pre-snowy-fight c40 checkpoint, the same controlled
route, all dynamic lighting categories, native 2560x1440 resolution, 25 seconds
warmup and 120 measured seconds. The passive release executable uses normal
LTO/PGO, without profiling or the reuse probe. No build or capture runs
concurrently.

| Mode | Average FPS | Minute averages | Mean / maximum selected lights | Worst frame (ms) |
| --- | ---: | --- | --- | ---: |
| Hard | 298.2 | 314.1 / 282.3 | 13.96 / 24 | 13.02 |
| Soft | 290.7 | 307.9 / 273.4 | 8.68 / 15 | 13.77 |

Both shadow maps are active in 100% of measured frames in both runs. Every
measured minute exceeds the 240 FPS average target; neither run records a
frame over 50 ms. Live AI and light loads differ from the one-light M2 runs,
so these numbers do not isolate a percentage cost or speedup. They establish
the bounded two-light release budget on the tested NVIDIA system, not
long-term memory stability, first-use pacing or acceptance of four lights.

Reports and source manifests are in
`build/lighting-performance/modern-m3-release-{hard,soft}`; the validation
index, GPU checks, build logs and cleanup report are in `build/modern-m3`.
The `*-m3-*` visual folders retain screenshots and failures. Saves are
archived and verified before disposable test maps and executables are removed;
the normal Windows executable and original configuration remain available.

## Milestone 4: settings and integration acceptance

The shared settings inventory now registers the desktop-only Modern Shadows
spinner. Both generated frontends use the existing setting transaction
handlers; no separate shadow preference store or new menu event implementation
is necessary. The classic page fits its existing layout, and the modern page
uses its existing scrolling list. The configuration comment now describes the
two-light budget. No production renderer, vendor workaround, Android, host-policy
or networking code changes in this milestone; the wire protocol is unchanged.

The isolated menu executable drives the real classic menu handlers through
staged edits, Cancel/reopen, Save, Defaults/Cancel, Defaults/Save and a process
restart. Hard and Soft persist without enabling the lighting categories. Native
2560x1440 captures also verify the modern frontend's scrolled row, three values
and help panel. The small menu regression harness exercises the production
setting functions. Diagnostic hooks are inserted only by the explicit test
builders and restored before the normal build.

Runtime Off/Hard/Soft cycles at 30, 60 and 120 FPS average 30.0, 60.0 and 119.7
FPS in the three-projectile capture. Each Off interval stops shadow rendering;
the GL-owner probe observes depth images and mesh storage released, and later
Hard/Soft intervals render complete maps again. The shader-isolation checks cover
non-shadow receiver variants, and stock variants when lighting is also disabled. Retained program
cache entries are not active shadow resources and do not require recompiling
every shader after each toggle.

The compatibility harness now marks its copied settings as deliberate choices
so the one-time lighting migration cannot reset them. It checks the preferences
actually used at runtime, the received host policy and active shadow maps. The
original scripted damage probe does not fire a weapon; an optional test-only
controller pulse now supplies real muzzle flashes. Enabled CE client coverage
requires observed shadow maps, not merely a saved Soft preference.

Initial Xbox/MCC comparisons flagged brief position differences around the
forced pickup teleport at tick 360 and the following regroup. These test actions
can straddle the peers' sampled state: the probe teleports on both machines and
the host then gathers the players. Their original logs are retained. The final
firing scenarios place the artificial pickup in the initial setup period before
the unchanged synchronization comparison window. Inventory changes and respawns
must still be observed by both peers; comparison tolerances are not relaxed.
The separate MCC shadows-Off control passes. These checks are local two-peer
multiplayer coverage, not public-internet transport or full-campaign acceptance.

| Scenario | Host / client shadow choice | Result |
| --- | --- | --- |
| Xbox Blood Gulch | Hard / Off | 81 seconds of paired snapshots; respawns, inventory changes and forced Shield Glow policy agree; Off client has no maps |
| CE Death Island, real firing | Off / Soft | 80 seconds paired; client renders shadows while respecting the host's stock Shield Glow policy |
| MCC Digsite, real firing | Hard / Soft | 76 seconds paired; host renders shadows, client with lighting categories off renders none; player-choice policy preserved |
| MCC control | Off / Off | 72 seconds paired; no shadow maps on either peer |
| Xbox c40 → MCC Digsite → CE Death Island → Xbox Blood Gulch | Soft, one process | Three simulation resets, both family loaders observed, shadow preference and lighting choices retained, no exceptions |

The representative visual captures also pass: Warthog exterior at 30 FPS,
Scorpion exterior at 60 FPS, stationary shield damage/recovery at 120 FPS,
crouched/standing Needler holder and world powerup at 60 FPS, and MCC overlapping
projectiles/model/specular receivers at 120 FPS. The camera sequence switches
first → third → first person at 60 FPS and observes the viewmodel caster owners
disappear and return appropriately. All ten M4 capture scenarios average within
5% of their cap, with complete active maps. Captures show retained chassis
blocking, shield damage colours, held glow and stock effects. This is sampled
coverage, not every emitter on every map at every cap.

### Pacing diagnosis and release performance

The bounded 120 FPS diagnostic pair uses 30 seconds from the same copied c40
checkpoint, Soft mode and no captures. Only application shader binaries are
carried into the warm run; the NVIDIA driver cache is not cleared. Both average
119.8 FPS. The first measured nonzero frame is 55.44 ms cold and 56.57 ms warm;
explicit compile/link work accounts for 2.16 ms and 0.19 ms respectively. Their
coarse thread-CPU samples both report 15.625 ms. After the first second, the
worst frames are 13.00 / 13.03 ms. Warming the application cache does not remove
this entry delay. It does not justify a shader-cache rewrite or establish the
cause of the earlier capture hitches. Diagnostic startup overhead remains part
of these observations; they are not normal-release performance measurements.

Release acceptance uses the normal `--portable --release` LTO/PGO executable,
RTX 5070 Ti, native 2560x1440, the copied pre-snowy-fight checkpoint, all lighting
categories, the controlled route and invulnerable test player, 25 seconds warmup
and 120 measured seconds. No build, capture or second game runs alongside it.
Both maps are active in 100% of measured frames in every run below.

| Run | Average FPS | Minute averages | Mean / max selected lights | Worst frame (ms) |
| --- | ---: | --- | --- | ---: |
| Initial Hard — target missed | 117.3 | 120.6 / 113.9 | 14.55 / 24 | 272.18 |
| Soft | 312.4 | 317.7 / 307.1 | 7.91 / 16 | 12.72 |
| Unchanged-binary Hard repeat | 290.9 | 300.8 / 281.0 | 14.85 / 26 | 9.59 |

The initial Hard result is retained as an unresolved performance outlier, not
silently replaced by the repeat. No competing game/build or GPU thermal
throttling was observed. Discord had background CPU activity; the user reported
no streaming/recording and closed it before the Hard repeat. Soft was already
fast before that reply, so Discord is not an established cause. The repeat has
a similar mean light load to the slow Hard run. The same executable and unchanged
renderer/shadow source hashes rule out a code change between these runs, but
do not identify the environmental or runtime cause. The passing Hard and Soft
runs exceed 240 FPS in each measured minute and have no frames over 50 ms;
this is not a claim that the intermittent slowdown or long-term memory behavior
is solved. No speculative renderer optimisation was made to explain it away.

### Review, platform checks and evidence

The combined review keeps the production change limited to the settings
inventory, generated menus and corrected configuration comment. The existing
transaction, resource lifetime and host-policy paths suffice. Test improvements
verify actual runtime settings, real firing and active maps, preserve failed
attempts, and record the cache source for warm comparisons.

The final focused suite has **169 passes** and one pre-existing failure:
`test_generated_assets_routes_and_platform_filter` finds a regenerated
`title_0.png` pixel mismatch. The same mismatch was reproduced using the
pre-M4 settings inventory from `dee3dd0c6`; unrelated artwork is preserved.
Windows and web build successfully. Android again fails before completion at
the Windows NDK wrapper launch and SDL configuration; no APK/device validation
is claimed. Android exclusion tests pass. AMD hardware is unavailable, and
vendor workarounds remain unchanged.

`build/modern-m4/validation.json` indexes results, including the failed Hard
target. Build/test logs and menu captures are alongside it; runtime captures are
in `build/plasma-visual/*-m4-*`, multiplayer records in
`build/lighting-compatibility/m4-*`, and passive measurements in
`build/lighting-performance/modern-m4-*`. Original maps and saves remain intact.
The cleanup records document verified save archives, lossless screenshot
conversion and removal of disposable test maps and executables. The normal
Windows executable and original release configuration are retained.

## Milestone 5: integration and final acceptance

The five feature commits were rebased onto testing `a6c42bfad` without
conflicts. Testing already contains the model-mask upload optimisation, so
the rebase does not reintroduce that patch. The combined review covers the
selection and caster modules, GL ownership and shader fallback, shared render
hooks, settings, generated menus, test fixtures and this guide. The two-light
budget, filtering, material semantics and host eligibility remain unchanged.
No new runtime diagnostics, map adapters or network messages were added.

One concrete disabled-path improvement removes empty `glDeleteTextures`
calls when no shadow maps were allocated, including repeated cleanup. The
real GL-owner harness rejects the old implementation and accepts the guard.
This avoids unnecessary driver calls; it is not claimed as a gameplay FPS gain.
Capture-only fault injection now exercises resource-creation failure: existing
lighting continues, setup is not retried each frame, and an Off/On cycle restores
shadow rendering. The menu tools accept independent build directories instead
of overwriting the M4 evidence folder.

The M4 menu-art failure was an environment mismatch: Pillow 13.0 was loaded
instead of the repository's `tools/requirements-frontend.txt` pin, 12.3.0.
Using that pinned dependency passes the pixel comparison without replacing
artwork or weakening the test. The final focused isolation/render/menu suite
passes all 219 checks, and the analytic desktop GL receiver tests pass. The visual
cycle captures run at 30, 60 and 120 FPS; a separate 60 FPS capture verifies
failure recovery. Representative vehicle, shield, CE and MCC captures retain
complete maps. Native 2560 by 1440 menu captures verify readable controls;
Save, Cancel, Defaults and portable restart persistence pass.

The late-join probe previously treated global game tick 600 as the end of a
client's startup period. A late client can already be past that tick before
the reliable host-policy message arrives. The engine correctly uses stock
shield effects while waiting. The test-only probe now records receipt after
the existing message validator accepts the host's policy: every waiting sample
must remain stock, and every received sample must match the host. It no longer
uses a timing grace period. The production policy implementation is unchanged.
A reconnect scenario preserves both client-process CSV histories and requires
the host to observe two players, then one, then two again; restarting a client
without an observed return cannot pass. It follows the normal postgame/lobby
flow when the departing opponent ends the two-player round. This passes with
62 seconds of paired snapshots in the new round, observed respawns and weapon
changes. Xbox, CE and MCC peer checks and late joining also pass with different
local settings and the existing host policy. A single-process Xbox-to-MCC-to-CE-
to-Xbox run passes with three observed map resets, retained preferences and
shadow rendering after transitions. The web release build passes.

The first CI run exposed a standalone HUD-test link dependency: its production
shader translator now references `modern_shadow_emit`. The hidden-GL builder
includes that real generator, without stubbing it or changing the shader.
All 36 MCC meter renders and 15,360 ammunition-meter renders then pass locally;
the same fix is included in the replacement CI run.

The broader C harness also needed its attenuation fixture wired to the new
shadow begin/end/reset dependencies. Counted hooks now check diffuse/specular
scope and cleanup alongside the original falloff assertions. Its shader-source
cache fixture now uses the production mandatory-record allocator: the optional
source-sharing allocation can fail, while device records retain their existing
calloc-or-abort contract. The previous fake incorrectly returned null for both.
The 42 affected falloff, source-cache, framebuffer and visibility checks pass
locally without any production renderer change. The full harness also found
the older shield menu test still expecting ten controls; it now checks all
eleven, explicitly including the desktop-only Off/Hard/Soft control while
retaining the original ownership, help-layout and generated-file assertions.
The expanded focused suite passes 219 checks after these corrections. A full
Windows C-harness attempt recorded 2,563 passes, 353 failures and two skips:
besides the corrected menu expectation, it encounters POSIX-only headers/APIs,
MSVC warning and linker differences, and Windows text-encoding/newline assumptions.
Those unrelated portability cases are retained unchanged; Linux CI is the gate
for the full Linux-oriented suite. The failed local attempt is retained in
`build/modern-m5/full-harness.log`, not represented as a passing full run.

### Performance evidence and remaining limits

The initial rebased release comparison uses the same copied pre-exterior c40
checkpoint and snowy route, with all feature-light categories enabled, an
explicitly invulnerable test player, native 2560 by 1440 rendering, normal
release LTO/PGO, 25 seconds of warmup and 120 measured seconds. Screenshots,
phase profiling and competing builds are disabled during these measurements.

| Shadows | Average FPS | First / second minute | Worst frame (ms) |
| --- | ---: | ---: | ---: |
| Off | 320.4 | 351.5 / 289.4 | 10.16 |
| Hard | 312.6 | 326.3 / 299.0 | 16.09 |
| Soft | 281.7 | 296.1 / 267.4 | 13.29 |

Both enabled modes maintain two active maps throughout; Off has none. Every
measured minute exceeds the agreed 240 FPS average target, with no frames over
50 ms. Combat light populations vary between runs, so these results establish
acceptance on this machine, not an exact shadow overhead or a new optimisation
gain. These initial measurements precede the cleanup-only guard; the final
release confirmations are retained with their build manifests under
`build/lighting-performance/modern-m5-*`.

The final runtime revision `72f0ac46a` confirms Hard at **301.4 FPS**
(316.7 / 286.1 per minute) and Soft at **306.1 FPS** (316.9 / 295.3).
Both maps remain active throughout; the worst frames are 11.68 and 8.96 ms,
respectively, with no frames over 50 ms. These are separate dynamic combat
runs, so the ordering does not establish that Soft is cheaper than Hard.
Private memory rises by 4.4 MiB in Hard and 4.0 MiB in Soft over the sampled
windows; the longer-term qualification below still applies.

The earlier 117 FPS Hard outlier did not recur in this comparison. Its cause
remains unestablished; neither Discord nor shader compilation has been proved
responsible, and no speculative renderer workaround was added. Private memory
rose by roughly 5 MiB between the first and last sampled windows in all three
modes, including Off. Feature cleanup is verified, but these short runs do not
prove a long-term engine/driver memory plateau or identify that growth's owner.

Testing uses an RTX 5070 Ti and i7-14700K. AMD hardware is unavailable; the
existing vendor workarounds are preserved and the relevant shader/state tests
remain enabled. Android is excluded from shadow execution. The local native
Windows Android build graph still relies on POSIX shell commands and cannot
serve as a full Android build check; the Linux-hosted Android CI is the platform
build gate. This limitation concerns the local build environment, not a change
to Android rendering. Public-internet and full-campaign coverage are not implied
by local multiplayer and representative-map checks.

The M5 evidence is under `build/modern-m5`, `build/modern-shadow-menu/m5-*`,
`build/plasma-visual/*-m5-*` and `build/lighting-compatibility/m5-*`. Original maps
and saves are preserved; disposable copies are removed and retained captures
and test saves are verified before lossless archival. The Windows playtest
ships a normal release executable, with no capture/failure-injection hooks and
with shadows and dynamic lighting still off by default.
