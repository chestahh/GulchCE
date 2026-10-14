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

This iteration softens **world diffuse lighting**. Model lighting and world
specular lighting retain their existing falloff; model lighting remains vertex
based unless the separate existing per-pixel setting is enabled. No shadow or
occlusion system is added. Custom projectile tag names and projectiles without
light attachments are outside this prototype.

## Isolation and lifetime

Selection, render tracking and texture generation live in the dedicated
`plasma_lighting.c` and `plasma_falloff.c` modules. The small bridges in
`object_lights.c` are necessary because light storage and cluster ownership
are private there. They validate handles and ownership before refreshing
attachments or changing the runtime dynamic flag. No map-family loading,
conversion, gameplay or network protocol changes are intended.

World diffuse begin/end hooks are necessary because the attenuation sampler
belongs to that existing pass. Only selected plasma draws bind the replacement
texture. End restores the stock texture and filter, including if the option
has just been disabled. Allocation failure falls back to stock attenuation.
The texture is released on disable, map initialization/unload, light recovery
and shutdown. Per-view rasterizer indices reset for each split-screen view.

After interpolation ends, the feature restores original dynamic flags and
simulation-time positions and cluster membership. Map boundaries discard old
handles. Excess lights beyond the bounded tracking array retain stock behaviour.
The disabled path performs no light refresh or texture substitution.

## Validation

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

The tests do not establish softer model/specular shading, wall occlusion,
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

Repeat for caps 30, 60 and 120 and enabled values 0/1. Use `--label` for a new
output directory when repeating a combination. The builder temporarily installs
`tools/fixtures/plasma_lighting_capture.c`, restores the production source and
configuration even on failure, and rebuilds the normal executable. The fixture
is only present in `build/plasma-visual/halo-capture.exe`. The runner uses hidden
windows and private data/save folders, removes copied maps and generated map
caches, and retains screenshots and diagnostic logs.
