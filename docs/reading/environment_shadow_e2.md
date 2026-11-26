# E2 directional sunlight

On desktop, set `environment_sun = true`, `modern_shadows = "hard"` and
`shadow_resolution = 1024` in the display settings to try the experiment.
The sun is disabled by default. The host's dynamic-lighting policy still
applies. The existing local lights retain all eight shadow slots.

The renderer follows the strongest upward, exterior-flagged directional light in the active
sky. One 24-world-unit orthographic tile follows the view in whole-texel
steps. The caster volume includes the full depth of the BSP along the sun
direction so distant cliffs and roofs can block it. Characters, vehicles,
items and machines transfer their complete attached family together.
A caster outside the camera frustum is eligible if its shadow can enter the
view. Incomplete coverage or a failed draw leaves the previous per-object
path available.

The baked lightmap combines direct and indirect light. For eligible world
surfaces the prototype removes an estimated direct share only where a moving
caster newly blocks visible sun. A nearer roof, a receiver in a no-sky cluster
or a surface shared by different skies keeps its authored lighting. This pass
precedes additive feature lighting. Existing model lighting and model
self-shadowing retain their own path. Static visibility is reused until the
tile, sky, resolution, BSP or a relevant cutout changes. BSPs with breakable
surfaces conservatively refresh it.

## Validation setup

The performance comparison uses the same uncapped 2560×1440 Windows
executable, Hard 1024, the recorded Two Betrayals snowy exterior combat route,
a 25-second warmup and a 120-second measurement. Run the ordinary local
lighting fixture, then repeat with `--environment-sun`. The CSV records both
`shadowed` (local slots used) and `sun_casters` (families submitted to a
successful directional draw). GPU readbacks and screenshots run separately.

The visual checks use a Warthog in Two Betrayals, a biped under the Beaver
Creek bridge, and a camera crossing portal 4 between clusters 16 and 15 in
Two Betrayals. Take a colour readback immediately before and after the sun
draw to distinguish its contribution from the stock shadow and the bake.

On the vehicle crossing, the camera was in exterior cluster 4. Rays along the
authored sun direction from the exposed endpoint cleared the BSP. At the
shaded endpoint the first blocker was the cliff, about 113 world units away.
At 1280×720 the vehicle's sun draw changed 14,557, 12,540, 0, 0 and 0 pixels
at the five successive capture points. The sun kept ownership of the vehicle
in shade. Its mounted gun is a logical attachment without a render model and
does not expand the caster bounds. Visible descendants still contribute.

Under the Beaver Creek bridge, the sun draw changed zero pixels at all five
checkpoints. At the Two Betrayals doorway, the no-sky camera used the fallback
in cluster 16. In sky-enabled cluster 15 the roof still blocked the direct
contribution. The two covered captures there also changed zero pixels.
The GPU harness compares the bounded receiver draw with the full-screen
reference from outside and inside its volume, including reversed depth.

## Busy Two Betrayals result

RTX 5070 Ti and i7-14700K, anti-aliasing off. The two sun runs bracketed the
control using the same binary, settings, checkpoint and controller recording.
All eight local slots were available throughout. The sun rendered in every
measured frame of both enabled runs.

| Run | Average FPS | First / second minute | Sun and eight locals together | 99th percentile frame | Peak process private memory |
| --- | ---: | ---: | ---: | ---: | ---: |
| Sun on A | 245.3 | 249.3 / 241.3 | 99.4% | 5.67 ms | 688.7 MiB |
| Sun off | 273.8 | 269.1 / 278.4 | — | 4.88 ms | 691.6 MiB |
| Sun on B | 247.6 | 248.4 / 246.9 | 98.6% | 5.62 ms | 688.3 MiB |

The sun cost about 0.39–0.42 ms of total frame time in this comparison. All
three runs had zero frames over 50 ms. These process memory figures include
the game and driver and do not measure dedicated GPU memory. At 1024 the
sun's independent R32F static-visibility texture occupies 4 MiB, alongside
the reused environmental depth target. The private scene-depth copy is shared
with existing environmental projections within the view.

Earlier development comparisons missed the target, including 237.4 FPS for
the full-screen receiver with surface ownership and 217.6 FPS for an
intermediate bounded receiver. A separate sun-off control also varied down
to 127.9 FPS. The final bracket above uses the corrected Warthog bounds,
bounded receiver rasterization and the single-sample packed-depth copy.
Those changes were tested together. Their individual FPS gains were not
isolated.

A separate timing build sampled the sun pass during the same fight. Over 16
two-second windows after warmup, 673 asynchronous GPU queries averaged
0.092 ms. The CPU spent 1.46 ms of wall time inside the sun call, including
driver waits. That figure overlaps other GPU work and cannot be added to the
GPU time or interpreted as the incremental frame cost. The static tile was
reused throughout those windows. Moving-camera cache misses are a separate
E4 profiling case.

Local benchmark reports, frame data, memory samples and binary/source hashes
live under `build/lighting-performance/e2-owned-{sun-a,control,sun-b}`.
The original E1 route hash is
`eed04f1790f1a73e55df8677ad357960cc32f2ed81cc5dc01f6ea82e95756376`.
Soft sunlight, distant cascades and adaptive scaling belong to later
milestones. AMD hardware was unavailable for E2.
