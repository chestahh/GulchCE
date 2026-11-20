# E4 outdoor shadows

Enable Sun Shadows in the Shadows menu and select Modern Hard or Modern Soft.
Modern places Shadows under Advanced Graphics. PC, Xbox and Off place it
under Video Setup. Fixture Shadows, Flashlight and Flashlight Budget are
available alongside Shadow Mode and Shadow Resolution in each style.
Classic and disabled sources are the defaults. The host lighting policy and
desktop platform gates apply before preparing the sun pass.

## Outdoor coverage

Use one stabilized directional map with a 12-world-unit half-width. The sun
uses the active sky and a separate budget. Eligible nearby casters block
the estimated direct portion of the BSP lightmap where static geometry lets
sunlight through. The existing environmental path covers families outside
the modern tile. An actor under a roof should retain the authored shade.
The map's original lightmap and tags remain usable in Sapien and other engines.

At 1280 × 720 on Sidewinder a ten-second camera pullback showed the tradeoff.
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

Hard tests the owning texel. Soft interpolates four comparisons of the same
depth map, correcting receiver depth for each sample. Samples beyond the
tile or behind static geometry contribute zero shadow. The center also has
to belong to the sunlit BSP receiver. The filter costs no additional caster
passes and does not consume a local shadow slot.

A 1024² 32-bit moving depth texture and 32-bit static visibility texture
occupy 8 MiB together. At 4096² they occupy 128 MiB. These figures exclude
other render targets, driver overhead and temporary storage while resizing.
The renderer validates a replacement attachment before releasing the
previous texture. The menu uses the existing 128 through 4096 tiers.

## Checking a build

Run the environment GPU shader tests and the environment and modern shadow
harnesses. Exercise Sun Shadows and Fixture Shadows with the desktop
menu probe in all four styles, including Save, Cancel, Defaults and restart.
Use fixed Hard and Soft 1024 in the snowy Two Betrayals checkpoint and in
the two-lamp room on Rat Race. Enable the fixtures, sun and stock beam, use
the Shared flashlight budget, and leave the eight-slot local capacity intact.
The busy-combat target is a two-minute average of at least 240 FPS.

On the i7-14700K and RTX 5070 Ti, the uncapped 2560 × 1440 runs with AA off
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
