# Modern dynamic shadow prototype

This branch adds optional rendered shadow maps to GulchCE's dynamic lighting.
It leaves the original object shadow projection in place. Set
`display.modern_shadows` to `"hard"` or `"soft"` (environment override
`HALO_MODERN_SHADOWS`); the default is `"off"`. This initial desktop prototype
has no menu entry. Android and web compile no-op implementations.

## Scope and light eligibility

All enabled lights selected by the dynamic lighting system are eligible:
projectiles, muzzle flashes, world and vehicle emitters, shield emitters,
Shield Glow and held-weapon glows. The historical `plasma_lighting_selected`
name does not restrict selection to plasma or projectiles. Existing local
toggles and host policy remain authoritative; shadows do not create lights
or enable a disabled lighting category.

The first milestone renders **one selected light per primary view**. This is
a bounded performance prototype, not the final light budget or a promise
that every eligible light is shadowed simultaneously. The score favours
bright, nearby lights with useful radii; the previous identity stays selected
while its score is at least 80% of the best candidate. Recycled rasterizer
indices cannot inherit another light's map.

For example, with projectile lighting disabled and Covenant shield lighting
enabled, a stationary shield can supply the selected light. Opaque world and
model triangles block its illumination. The translucent shield surface is
not turned into an opaque wall. Other lighting and baked illumination remain
unchanged. A muzzle flash participates through the same selection interface.

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
   Hard and soft modes share the same map. Soft mode uses four visibility
   samples to soften the edge; it does not blur baked lighting.
4. `modern_shadow_emit` and `modern_shadow_bind` feed only the selected
   light's world diffuse/specular attenuation or model point-light slot.
   Receiver-plane depth comparison reduces grazing-surface acne without a
   large constant offset that would detach shadows from feet.
5. [plasma_occlusion.c](../../port/linux/game/plasma_occlusion.c): a complete
   rendered map replaces the chosen light's coarse collision visibility.
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

Each depth cube is 512 by 512 pixels per face. A visible first-person model
adds a second cube, allocated lazily and rendered only while those casters
are present. World receivers use the complete world body and weapon; the
first-person receiver uses the arms/weapon pose and excludes their hidden
world counterparts. Other casters participate in both maps. This prevents
double silhouettes without removing the player's body shadow from the world.
Both cubes share converted meshes and the frame's copied bone matrices;
the extra depth storage is 6 MiB. Mesh storage is bounded to
2,048 parts and 32 MiB; a frame holds at most 256 caster draws with 64 bones
per draw. Incomplete caster collection falls back instead of displaying a
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
Broad material batches, map/mesh capacity, selection changes and grazing
surfaces still need quality acceptance before expanding the light budget.

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
