# Plasma lighting prototype

Work lives on `plasma-lighting-prototype`, based on `main`.
`chesters-cool-stuff` remains an unchanged local reference. CStencil, Modern,
rockets and muzzle flashes are separate milestones.

Set `plasma_lighting = true` in config.toml's `[display]` section, or
`HALO_PLASMA_LIGHTING=1`. The default is false. Keep `display.interpolation`
enabled for motion between simulation ticks.

## Rendering

The feature selects existing light attachments on the projectile tag
`weapons\plasma rifle\bolt`, up to 128 per frame. It refreshes their marker
poses before visibility and receiver selection. New objects without a previous
snapshot use the engine's existing fallback pose.

Live captures revealed that Xbox Blood Gulch's bolt light has flags zero:
its attachment supplies a lens flare but is not marked dynamic. The first
temporal prototype therefore did not illuminate world surfaces on that map.
Selected attachments now become dynamic during rendering, with their original
flags restored afterward. Tags are never edited and no extra light is allocated.
Already-dynamic attachments retain their authored flag after rendering.

World diffuse illumination uses a feature-owned 64³ attenuation texture:
`max(1 - distance_squared / radius_squared, 0)²`. Both value and slope approach
zero at the outer boundary. Texels are sampled at their centres and filtered
linearly. The authored colour and radius remain unchanged. The existing
additive pass still applies material colour and surface facing.

Model point lights now use the same squared falloff. The model shader's
existing contribution is multiplied by the extra distance factor only for
selected plasma slots. Ambient, distant and other point lights keep their
existing arithmetic. Models remain vertex lit unless the separate existing
per-pixel setting is enabled; that path applies the same curve per pixel.
No setting is silently enabled. Coarse model meshes can still reveal vertex
interpolation in the lighting.

Omnidirectional world specular passes use the same texture on their own
attenuation stage, preserving the authored specular radius multiplier,
material mask, exponent and highlight colour. Spotlight gel passes and model
cubemap reflections remain stock; this does not add a new model specular BRDF.
No shadow or occlusion system is added. Custom projectile tag names and projectiles without
light attachments are outside this prototype.

## Isolation and lifetime

Selection, render tracking and texture generation live in the dedicated
`plasma_lighting.c`, `plasma_falloff.c` and `plasma_model_shader.c` modules. The
small bridges in `object_lights.c` are necessary because light storage and cluster ownership
are private there. They validate handles and ownership before refreshing
attachments or changing the runtime dynamic flag. No map-family loading,
conversion, gameplay or network protocol changes are intended.

World diffuse and omnidirectional specular begin/end hooks are necessary
because the attenuation sampler belongs to that existing pass. Only selected plasma draws bind the replacement
texture (stage 2 for diffuse, stage 1 for specular). End restores the stock
texture and filter, including if the option has just been disabled. Allocation failure falls back to stock attenuation.
The texture is released on disable, map initialization/unload, light recovery
and shutdown. Per-view rasterizer indices reset for each split-screen view.

The model slot hooks in `rasterizer_xbox.c` are necessary because only that
submission path maps receiver light slots to rasterizer light indices. A
feature-owned two-bit mask is cleared before each model, each view and at
frame/map cleanup, and gated again by the option at draw time. The renderer
uploads it as a separate uniform, without borrowing stock constants or flags.
Minimal translator hooks call the feature emitter for recognized two-point-light
model shaders and the existing per-pixel model shader. The vertex path changes
only the point light colour multiply-add's RGB operand; the parallel scalar
instruction and alpha are untouched. These shared paths serve Xbox, CE and MCC
receivers, with no changes to map admission, loading or conversion.

After interpolation ends, the feature restores original dynamic flags and
simulation-time positions and cluster membership. Map boundaries discard old
handles. Excess lights beyond the bounded tracking array retain stock behaviour.
The disabled path performs no light refresh or texture substitution.

## Model and specular validation

Windows portable release, 2026-10-14: 20 harness tests passed. Coverage adds
model-slot selection, disable/view/frame cleanup, specular stage/filter
restoration, and translation of the actual stock shader payloads (both model
point-light slots reached; non-point programs untouched). Light-storage ordinary
and full-LTO checks and their negative control passed. The normal release
executable was rebuilt after removing all capture instrumentation.

Twelve hidden runs captured three real projectiles apiece, with plasma lighting
off/on at each cap. Blood Gulch used a spawned Spartan receiver; Carousel used
world diffuse lights disabled via its existing debug setting, isolating the
world specular contribution. Counts below are actual selected model draws or
specular receiver draws, not just light submissions.

| Cap | Model off/on FPS | Model draws on | Specular off/on FPS | Specular draws on | Most poses per tick |
| --- | --- | --- | --- | --- | --- |
| 30 | 30.00 / 29.57 | 88 | 30.00 / 29.72 | 56 | 1 |
| 60 | 59.99 / 58.89 | 165 | 60.00 / 59.56 | 114 | 2 |
| 120 | 119.99 / 119.99 | 348 | 119.61 / 119.99 | 270 | 4 |

All runs exited normally; disabled runs had zero selected draws. Median frame
intervals were 33/17/8 ms. A further enabled 60 FPS model run with per-pixel
lighting passed (59.17 FPS, 150 selected draws). Measurements include capture
and initial shader work and are not a comparative performance benchmark.

Reviewed off/on images and consecutive frames show cyan illumination traversing
the armour and highlights crossing the indoor floor. The 60/120 FPS sequences
and telemetry contain intermediate poses within each simulation tick. Off/on
frames are matched to the nearest projectile position, not pixel-identical
simulation timestamps. Short-lived bolts still disappear at normal impacts;
no artificial lifetime or impact fade was added. Coarse vertex-lit meshes and
stock material highlight sharpness remain visible limitations.

Evidence: `build/plasma-visual/model-comparison.png`, `model-sequence.png`,
`specular-comparison.png`, `specular-sequence.png`, `model-specular-results.json`,
and each `*-model-final` / `*-specular-final` directory. The test-only binary
and build manifest are retained; copied test maps and caches were removed.
The initial short-trajectory probe runs are diagnostic evidence only.

Production Xbox Blood Gulch -> CE Death Island -> MCC Mercury Falling -> Xbox
transition smoke checks passed off/on: three console commands each, confirmed
CE conversion/MCC loading, exit zero and no logged exceptions. Logs are in
`build/plasma-model-transitions`. This is not visual certification of CE/MCC
plasma shaders or split-screen.

A two-instance Windows loopback Slayer smoke test also passed with the host's
option on and the client's off: both logged two players through tick 870 and
exited normally. This checks session compatibility, not networked plasma combat.
Logs remain in `build/plasma-model-network-complete`. The first attempt lacked
the private profile's lobby map and never entered play; it is not counted.

Android and web builds were attempted but those targets are unavailable in the
local toolchain. Linux/mobile/web runtime and browser-network interoperability
remain untested. The new code uses existing GL entry points and both new modules
are included by the native, Android guest and web source globs; no new Android
host imports or network-version change is needed.

## Earlier world diffuse validation

Windows portable release, 2026-10-14: 17 harness checks passed, including
negative controls, disabled selection, view boundaries, stale ownership,
restoration of both dynamic and lens-flare-only flags, allocation failure,
texture/filter restoration, and monotonic falloff with a zero boundary slope.
Existing light-storage checks passed ordinary and full-LTO links and their
negative control.

Hidden Xbox Blood Gulch captures launched three real plasma-rifle projectiles
along a repeatable trajectory across visible ground. The fixture creates real
projectile objects using normal physics and impacts; it does not simulate
controller firing. Screenshots and per-render-frame telemetry were collected
with the option both off and on:

| Cap | Off average | On average | On median frame interval | Most distinct rendered poses within one tick |
| --- | --- | --- | --- | --- |
| 30 FPS | 30.01 | 29.67 | 33 ms | 1 |
| 60 FPS | 60.00 | 59.56 | 17 ms | 2 |
| 120 FPS | 119.99 | 120.04 | 8 ms | 4 |

The measurements include capture overhead and initial shader/texture work.
Images show a feathered pool following the bolt across the ground. Successive
60/120 FPS frames and telemetry show intermediate poses rather than a light
held at each 30 Hz tick. Selected lights return to zero after projectile removal.
No crashes occurred in these six runs. Captures are under `build/plasma-visual`;
`sequence-contact.png` shows consecutive enabled frames at each cap.

Separate hidden runs with the production executable exercised Xbox Blood Gulch,
Custom Edition Death Island and MCC Mercury Falling with the option off and on.
Both runs sent all three transition commands and exited normally without logged
exceptions; the logs confirm CE conversion and MCC loading. These were loading
and transition smoke checks, not projectile-lighting visual tests on CE/MCC.
Their logs remain in `build/plasma-falloff-transitions`; copied test maps were
removed. An earlier sandboxed attempt could not connect to the local console
and is not counted as a successful transition test.

Those earlier tests did not establish softer model/specular shading, wall occlusion,
split-screen visual quality, every plasma tag variant or full map compatibility.
Android/web targets are unavailable in the local build configuration. Linux,
Android, web, browser networking and multiplayer runtime were not tested.
The change introduces no wire-format or simulation contract changes, so the
network version is unchanged.

## Reproducing the captures

With the repository's Windows compiler environment configured:

```
python tools/plasma_visual_build.py
python tools/plasma_visual_run.py --data-root C:/Games/GulchCE --fps 30 --enabled 1
```

For a visible model receiver, add `--model --distance 1.3
--receiver-distance 1.8 --height -0.3`. For an indoor specular isolation run,
add `--map carousel --distance 1.5 --height -0.25 --specular-only`; this uses
an existing debug setting to disable world diffuse lights in the private test
session. Add `--per-pixel` to exercise the existing model lighting option.
The scene flags affect only the capture executable and private configuration.

Repeat for caps 30, 60 and 120 and enabled values 0/1. Use `--label` for a new
output directory when repeating a combination. The builder temporarily installs
`tools/fixtures/plasma_lighting_capture.c`, restores the production source and
configuration even on failure, and rebuilds the normal executable. The fixture
is only present in `build/plasma-visual/halo-capture.exe`. The runner uses hidden
windows and private data/save folders, removes copied maps and generated map
caches, and retains screenshots and diagnostic logs.
