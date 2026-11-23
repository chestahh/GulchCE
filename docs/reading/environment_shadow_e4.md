# E4 outdoor shadows

Enable Sun Shadows in the Shadows menu and select Modern Hard or Modern Soft.
Modern places Shadows under Advanced Graphics. PC, Xbox and Off place it
under Video Setup. Fixture Shadows, Flashlight and Flashlight Budget are
available alongside Shadow Mode and Shadow Resolution in each style.
Classic and disabled sources are the defaults. The host lighting policy and
desktop platform gates apply before preparing the sun pass.

## Outdoor coverage

The wide sun tile has a 12-world-unit half-width. The sun
uses the active sky and a separate budget. BSP triangles and placed models
use the same material eligibility as local shadows, including alpha cutouts.
Casters can lie outside the camera frustum or cross the edge of the tile.
Models and lightmapped world surfaces sample both depth maps. The renderer
adds directional sunlight on exposed surfaces. When a model blocks sunlight
on otherwise exposed lightmapped ground, its silhouette also removes a
bounded estimate of the direct share of the bake, based on the sky colour
and surface angle. A roof or other BSP blocker keeps its authored shade.
Both adjustments fade at the edge of the tile. The existing environmental path covers families outside
the tile.
The map's original lightmap and tags remain usable in Sapien and other engines.

The original mobile-caster prototype compared camera ranges at 1280 × 720 on Sidewinder.
At the far end of the move the six-unit tile contributed zero shadow pixels.
The 12-unit tile still changed 143 pixels. A second stabilized tile with a
36-unit half-width contributed 24 additional pixels from a distant vehicle.
It changed 103 additional pixels at the start. The 12-unit tile kept the
near vehicle throughout the move and avoids a second static map and draw pass.
In the Two Betrayals cliff crossing, the single map changed 14,125 receiver
pixels in direct sun and zero at the shaded endpoint. The far cascade added
zero pixels at both endpoints. Sampled GPU time for the directional pass
on the snowy combat route was 0.077 ms for single, 0.122 ms for two cascades
and 0.074 ms for near-only. CPU submission including driver waits averaged
0.638, 1.470 and 1.388 ms respectively in the separate diagnostic runs.
Those short runs have different frame timings from the release measurements.

## Filtering and memory

Hard tests the owning texel. Soft interpolates four comparisons using the same
two depth maps, correcting receiver depth for each sample. Hard and Soft
share their comparison and interpolation code with local-light shadows.
The sun uses an orthographic projection. A blocker in either map removes
the added directional light. The filter adds no caster passes or local slots.

Nearby posed objects also render into a six-world-unit-wide region of the
object depth atlas. Both regions use the same sun direction and depth range.
BSP depth stays in the wide map. Hard samples the finer object depth and
Soft filters it at its own texel spacing. Visibility blends into the wide
object depth at the rim. An allocation failure falls back to wide coverage.

The dedicated 32-bit sun textures occupy 12 MiB at 1024² and 192 MiB at
4096² with the detail region. BSP generation also uses the shared environmental scratch depth
(4 MiB at 1024² or 64 MiB at 4096²). These figures exclude the scene depth
copy, other render targets, driver overhead and temporary storage while resizing.
The renderer validates a replacement attachment before releasing the
previous texture. The menu uses the existing 128 through 4096 tiers.

## Geometry checks

On Sidewinder, compare the same camera with a placed pine present in the sun
depth map, omitted from that map, and then restored. Keep the tree visible
in all three frames. Its silhouette should disappear from the snow only
during the omission. Check the alpha-tested branches and the lower trunk at
native resolution.

On the i7-14700K and RTX 5070 Ti, the full-geometry build averaged 271.1 FPS
in Hard and 277.5 FPS in Soft in the first snowy Two Betrayals encounter.
Each uncapped run measured 120 seconds at 2560 × 1440, AA off, shadow
resolution 1024 and the Shared flashlight setting. Hard had zero frames
over 100 ms and Soft had three. The AI encounters vary between runs.

A full-screen 1440p Soft filter comparison on that GPU measured 0.0594 ms
for individual depth reads and 0.0599 ms for gathered reads. No pixel
differed by more than 0.00001. This check establishes equivalent output.
The renderer also binds the sun textures once per view and reuses each
model shader's sun uniforms across consecutive draws.

The GPU receiver tests also put a BSP blocker across one half of a model
and a posed-model blocker across the other. Removing either blocker should
restore only that side's sun contribution. A roof should preserve the baked
base even in a scene with no model casters.

## Checking a build

Run the environment GPU shader tests and the environment and modern shadow
harnesses. Exercise Sun Shadows and Fixture Shadows with the desktop
menu probe in all four styles, including Save, Cancel, Defaults and restart.
Use fixed Hard and Soft 1024 in the snowy Two Betrayals checkpoint and in
the two-lamp room on Rat Race. Enable the fixtures, sun and stock beam, use
the Shared flashlight budget, and leave the eight-slot local capacity intact.
The busy-combat target is a two-minute average of at least 240 FPS.

Before extending sun coverage to placed scenery and model receivers, the
uncapped 2560 × 1440 runs with AA off on the i7-14700K and RTX 5070 Ti
gave the following results. Each run used 25 seconds of warmup, followed
by 120 measured seconds.

| Scene | Hard 1024 | Soft 1024 |
| --- | ---: | ---: |
| Two Betrayals, first snowy exterior combat | 247.4 FPS | 242.3 FPS |
| Rat Race, two-lamp room with 15 bots | 361.1 FPS | 410.8 FPS |

Both combat minutes cleared 240 in each mode. Both modes reached eight local
slots including the beam. Sunlight was active throughout the combat measurements.
The two Rat Race fixtures were simultaneously active on 81% of Hard frames and
77% of Soft frames. Separate runs have different combat activity, so these room
averages do not measure the isolated cost of filtering. On an independent
Sidewinder capture the fixture pass also shaded the receiver beneath the
authored wall strip at (−5.952, −19.585, −0.295).

A live Soft sunlight run cycled 128, 256, 512, 1024, 2048 and 4096 before
returning to 1024. Both sun textures followed each selection. The peak
process-private sizes in the two-minute combat runs were 696 MiB in Hard and
739 MiB in Soft. These are process measurements, not GPU allocation totals.
