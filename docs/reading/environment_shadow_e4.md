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
