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
   samples with bilinear comparison weights to soften the edge; it does not
   blur baked lighting. Samples reproject across cube faces explicitly.
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
depth cubes. It requires NumPy and Pillow, but no game assets or production
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
