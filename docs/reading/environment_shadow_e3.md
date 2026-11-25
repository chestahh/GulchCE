# E3 indoor fixtures and the flashlight

On desktop, select Modern Hard or Soft and opt in with
`display.environment_fixtures` or `display.flashlight_shadows`. Both default
to false. The host's dynamic-lighting policy applies. Android and other GLES
builds take the stock path. No changes to maps or Sapien are required.

## Sources, shadows and cost

The fixture pass can select two lights per view. It first considers nearby
placed light fixtures and their attachment markers. A bounded search of
planar, connected BSP triangles with positive authored radiosity also finds
emitting surfaces such as the three-lobed wall panel. The search runs once
per BSP. Visible receiver coverage and the presence of a potential moving
caster determine whether a lamp draws a shadow map.

Each lamp has an independent moving-caster depth map and a static BSP
visibility layer. The renderer reuses static depth for unchanged opaque
geometry and redraws it for cutouts or breakable surfaces. A moving blocker
can remove a bounded estimate of that lamp's direct share from a lightmap
only where the lamp has a clear path through the static BSP. A retail bake
combines direct and indirect light, so that share is approximate. The other
lamp and later additive lights can still illuminate the receiver.

The flashlight has its own perspective depth map. It follows the nearest
submitted stock flashlight in each view, including the beam's live position,
direction and range. The renderer rejects BSP batches and object bounds
outside the beam. It uses the existing world triangles, posed meshes and
alpha-cutout materials inside it. The visibility result masks the stock beam
on world geometry and the corresponding stock light on eligible models.
The beam omits its wearer and their equipped weapon from its caster map,
whether the light is attached to the biped or the weapon. Other bipeds can cross and interrupt the beam,
and other lights can still cast the wearer's silhouette.
With multiple players' flashlights in one view, only one receives this extra
map. In Shared mode, a successfully prepared beam uses one of the eight local shadow slots.
When all eight would be occupied, the lowest-scoring local light keeps its
illumination and gives up its shadow. A faded, disabled or unavailable beam
releases the slot immediately. The two fixture slots and E2 sunlight slot
are independent. The projector still uses one 2D map. It never needs a
six-face point-light shadow map. In the Shadows menu, Flashlight Budget → Extra Slot (`display.flashlight_extra_slot`) allows eight other local shadows alongside the beam. This setting defaults to Shared and does not enable the flashlight or modern shadows.

## Rat Race evidence

On the original Xbox map, the test logged the two lamp markers at
(7.6693, −10.9724, −0.7990) and (9.9964, −11.0341, −0.8103). With the same
biped pose and camera, enabling each lamp separately produced opposing floor
silhouettes. Both appeared when enabled together. The test also compares
against a return-to-stock frame to reject incidental differences. The ramp
panel supplied one outward-facing emitter at (−8.5266, −12.3946, −2.2547).

A second frozen comparison placed an armed multiplayer biped and a Warthog
in the stock beam. The flashlight toggle changed lighting on the models and
the geometry behind them. The shoulder-camera check also recorded the active stock
beam on the world weapon. These captures use the Xbox map. They do not
individually exercise every eligible object tag or a live MCC map.

## Measurements

The reference desktop used an RTX 5070 Ti and i7-14700K at 2560×1440 with
AA off and 1024 shadow maps. In the same snowy Two Betrayals fight, alternating
three-second windows tested the old and cached mesh bindings at both budgets.
With eight other lights and the beam, retaining the mesh layout and pose state
raised the average from 231.4 to 248.3 FPS. With seven other lights and the
beam the corresponding averages were 241.4 and 242.7 FPS. Sharing bounds the
maximum number of shadowed lights. The measured savings from sharing alone
varied with the fight.

Before adding the menu choice, fixed Shared averaged 250.0 FPS (Hard) and
264.5 FPS (Soft) in the snowy fight. In the 15-bot Rat Race room it averaged
300.4 FPS (Hard) and 232.9 FPS (Soft). The selectable Shared build repeated the
Soft room run at 301.2 FPS, with both lamps active throughout. In separate
two-minute snowy Hard runs the selectable build recorded 112.9 and 228.2 FPS
on Shared, and 241.2 FPS on Extra Slot. Shared peaked at eight including
the beam. Extra Slot reached nine. In the slowest run, frames with the beam
absent also took about twice as long as in the fixed Shared baseline.
The cause of that run is unconfirmed. These results describe the reference
PC and route. They do not establish a repeatable 240 FPS combat average.

A diagnostic build alternated between consulting the setting and forcing Shared
within a single 120-second snowy Hard fight. Each two-second interval used
four half-second windows in symmetric order. Both paths rendered identical
budgets. Selectable Shared averaged 255.2 FPS and fixed Shared 252.4 FPS.
Across 57 paired intervals with the beam and seven other lights active,
selectable minus fixed was −0.036 ms per frame (standard error 0.024 ms).
This comparison found no measurable penalty from offering the choice.

The menu-enabled build measured 36–38 ms for 100,000 budget setting reads
in the isolated Windows menu acceptance run, or 0.36–0.38 microseconds per
lookup. The renderer reads it once per view with a published beam. Shared adds
no draw, depth allocation or shader sample over the fixed Shared renderer.
Live menu checks covered Save, discarded edits, Defaults and persistence
after restarting under the Modern, PC, Xbox and stock frontend selections.

## Reproduce

Build the Windows measurement executable with
`python tools/lighting_performance_build.py --fixture-room`. With the same
data root and baseline display settings as E1, run
`tools/lighting_performance_run.py` with `--environment-fixtures
--flashlight-shadows`, `--modern-shadows hard` or `soft` and the 1024 shadow
setting. For Rat Race use `--fixture-room timing --map ratrace --bots 15`.
The `comparison`, `panel` and `beam` room modes are visual checks. Frozen
comparison frames are excluded from performance measurements.

For the campaign use the E1 snowy checkpoint and controller route, with
the player protected and the stock flashlight enabled. E2's opt-in sun
runs in Hard. Soft sunlight is part of E4. Report uncapped average FPS over
120 seconds after 25 seconds of warmup, with frame capture disabled. Use
`--extra-flashlight-slot` to exercise the optional ninth shadow.
