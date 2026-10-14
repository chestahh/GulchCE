# Plasma lighting prototype

The `plasma-lighting-prototype` branch starts from `main`. The local
`chesters-cool-stuff` branch remains a reference. CStencil, Modern shadows,
rockets and muzzle flashes are outside this prototype.

Set `plasma_lighting = true` in config.toml's `[display]` section
(or `HALO_PLASMA_LIGHTING=1`). The default is false. Keep
`display.interpolation` enabled, its default, to interpolate between ticks.

This first experiment addresses temporal stepping. Persistent light
attachments were refreshed by simulation, while their projectiles were drawn
from interpolated node matrices. The prototype refreshes the existing lights
on projectile tags named `weapons\plasma rifle\bolt` before light visibility
and receiver selection. It uses the existing marker pose, not an estimated
velocity or an extra smoothing filter. New objects without interpolation
history use the engine's existing fallback pose.

It adds no lights, changes no tags, and preserves light colour, radius,
falloff and impact effects. It therefore does **not** yet address sharp spatial
falloff or vertex lighting on models. It provides a baseline for comparing
movement before introducing a new per-pixel contribution. It cannot produce
lighting for maps whose projectile tags lack existing light attachments.

## Isolation and lifetime

Feature selection and frame tracking live in `port/linux/game/plasma_lighting.c`.
The light owner and spatial partition are private to `object_lights.c`, so a
small validated bridge there refreshes an attachment through the existing
disconnect/reconnect interfaces. A preprocess hook is necessary to refresh
before culling; merely overriding the GPU submission would leave receiver
selection and bounding spheres at the old position.

After rendering, `render_interpolation_frame_end` disables interpolation before
restoring the same attachments and their partition membership. Restoration
runs even if the setting has been disabled. Map initialization, unloading and
the existing light-array recovery path discard tracked handles. The bridge
checks the salted light handle, owner and persistent attachment type before
touching it. The feature creates no independent lights to survive impact.
Split-screen views share one refresh. Tracking is bounded to 128 lights;
overflow leaves additional lights stock.

The disabled path does not refresh lights. No simulation, protocol, map
discovery, conversion or MCC compatibility logic changes. Xbox, Custom Edition
and MCC continue to use their established loading paths; the same narrow
runtime projectile-tag check applies after loading.

## Validation and next experiment

The asset-free harness exercises disabled behavior, tag/type selection,
render-to-simulation restoration (including disabling while pending), repeated
views, bounded capacity, unload, missing objects and changed light ownership.
Negative controls verify
that breaking restoration order, reset or the off switch fails the tests.

Local validation on Windows (2026-10-14): release build passed; all 11 harness
cases passed; the existing light-storage checks passed ordinary and full-LTO
links, including their negative control. Hidden isolated sessions with the
option off at a 30 FPS cap and on at a 120 FPS cap exited normally with no
logged exceptions. Both started Xbox Blood Gulch and received console commands
for Custom Edition Death Island, MCC Mercury Falling and a return to Blood
Gulch. Logs confirm CE conversion and MCC loading; this is a smoke check, not
proof of complete compatibility or smooth projectile illumination. Temporary
map copies were removed and diagnostic logs retained under `build/plasma-smoke`.
Android and web builds could not run: this local configuration has neither
target/toolchain. Linux, browser networking and live projectile footage were
not validated. No gameplay or network protocol changes are intended.

Before considering this visually accepted, compare a plasma rifle fired past
a wall and nearby model at 30, 60 and 120+ FPS, enabled and disabled. Check
birth, impact, deletion, pause, split screen and map changes. Confirm the light
follows the bolt and leaves no residue. Automated lifecycle checks and map
loads alone do not establish visual quality.

The next rendering experiment should provide a feature-owned, smoothly
attenuated per-pixel contribution on world geometry and models, suppressing
the corresponding stock contribution only while enabled. Preserve the stock
path when disabled. Shadows and occlusion remain separate work.
