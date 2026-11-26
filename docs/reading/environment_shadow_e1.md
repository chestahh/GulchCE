# Environmental shadows E1

E1 established source ownership and measured the desktop renderer at
`b335a4570` before introducing environmental shadows. The test build used
the release configuration with a passive frame recorder. A separate build
logged the sources that reached the renderer after map loading and conversion.
Both the stock executable and the map files keep their original behaviour.

## Sun and sky ownership

Resolve the camera leaf, its cluster's sky index and the corresponding scenario
sky reference. A valid sky index identifies a candidate. Sunlight can still be
blocked by the roof of a room in that cluster. A missing sky supplies no sun.
The direction in the table points **from the receiver toward the light**.
Light travels along the negative of this vector.

| Map | Direction toward the source in world XYZ | Evidence |
| --- | --- | --- |
| Xbox Two Betrayals | (−0.17918, +0.31036, +0.93358) | The live snowy route selected the exterior-only sky light, power 0.85, in clusters 15 and 2 of `c40_c`. This BSP has eight clusters using sky 0 and nine using no sky. Portal 4 connects clusters 16 and 15 across that boundary. |
| Xbox Blood Gulch | (+0.75441, +0.13302, +0.64279) | The baked ground boundary on surface 1086 agrees with a ray cast toward the authored source. |
| CE Death Island | (+0.52654, +0.62751, +0.57358) | The live converted map supplied the angled light at power 1.7 and an overhead fill at power 0.9. Surface 17763 resolves an edge with the angled source. The overhead fill predicts uniform visibility there. Portal 198 connects a no-sky cluster to a sky-0 cluster. |
| MCC Workshop Blood Gulch | (+0.75441, +0.13302, +0.64279) | Digsite package 3477809506, map 20. The live converted map retained the vector and power 1.8. Its baked ground edge on surface 1078 agrees with the independent ray test. |
| MCC Workshop Albatross | (0, 0, +1) | The live map exposed the overhead source at power 0.5. Its bake gave weak evidence for that light dominating the room. |
| Xbox Rat Race | None | The live renderer reported zero scenario skies, five placed fixtures and 16 clusters. Every cluster has sky index −1. |

The bake check samples original RGB565 lightmap texels and predicts visibility
on the **same receiver triangle** using opaque BSP geometry. For Blood Gulch
the control reverses the horizontal direction while retaining elevation. For
Death Island it uses the actual second sky light. Among sampled triangles
with both lit and blocked points, the predicted lit side was brighter on
21/23 Xbox Blood Gulch, 22/23 CE Death Island and 23/25 MCC Blood Gulch
triangles. The illustrations show a high-contrast example in each family.

Snow in Two Betrayals is a poor direction reference on its own. On six mixed
snow triangles the median lit-minus-blocked brightness was −1.1 on a 0–255
scale. Albatross also had a weak median (−1.2). The cross-family check verifies
the coordinate convention. These two bakes do not establish a recoverable
amount of direct sunlight per texel. E2 should keep ambiguous BSP lighting
baked, including where bright texels may come from indirect light.

[Matched baked texels and predicted visibility](environment_e1/sun-reference.png)

## Indoor fixture ownership

**Rat Race ceiling pair.** The two cyan lights in the reference are scenario
light fixtures 2 and 3. Each uses `car_spotlight`, a `light` model marker and
one attached `ligh` tag. The engine returned these world-space marker positions
after applying the model node and placement transforms:

| Fixture | Emitter position | Forward |
| --- | --- | --- |
| 2 | (7.6693, −10.9724, −0.7990) | (0, +0.0003, −1.0000) |
| 3 | (9.9964, −11.0341, −0.8103) | (0, −0.0003, −1.0000) |

Each lamp also has a separate `car_conelight` BSP mesh beneath it, with
positive authored radiosity power. Counting both representations would
duplicate each source. Use the attachment as the source and preserve the
visible cone and bake. The scenario placement's colour and intensity
overrides are zero on this map. Treating them as the effective output
would miss both lamps.

**Rat Race ramp.** The three-lobed cyan panel is four connected BSP triangles
starting at surface 2280, centred at (−8.537, −12.395, −2.255). The visible
face opens toward +X. It uses `ratlwhite`, the `lights small strips` texture,
authored radiosity power 90 and RGB (0.651, 0.867, 0.800). The panel belongs
to a single emitting patch. The same material also appears elsewhere on the
map, so the material name alone cannot locate the source.

**CE and MCC.** Death Island's 20 placed light fixtures include a wall fixture
at (−17.681, −6.205, 11.013) with a `generic light` attachment. The live
renderer also retained 14 emissive BSP material records. MCC Albatross has
zero placed light fixtures and retains emissive BSP materials, including red
and blue strips. Closed emitting meshes can have a vanishing summed normal.
They need independent evidence for an emission direction before admission as
a directional source.

[In-engine fixture and combat views](environment_e1/baseline-scenes.jpg)

## Performance baseline

Windows, i7-14700K and RTX 5070 Ti. Native 2560×1440, release LTO/PGO,
uncapped, VSync off, all dynamic light categories enabled, shadow resolution
1024 and eight available local shadow slots. Each run had 25 seconds of
warmup and 120 measured seconds. The timed game sessions had screenshot
readback and source logging disabled. Each campaign run used the same copied checkpoint,
invulnerable test player and recorded route into the snowy fight. Each indoor
run used the same fixed camera with 15 Heroic bots playing local Slayer.

| Scene | Mode | Frames | Average FPS | First / second minute FPS | Active slots, mean / peak |
| --- | --- | ---: | ---: | ---: | ---: |
| Two Betrayals | Hard | 32,173 | 268.1 | 271.1 / 265.1 | 7.99 / 8 |
| Two Betrayals | Soft | 30,120 | 251.0 | 255.5 / 246.5 | 8.00 / 8 |
| Rat Race | Hard | 42,420 | 353.5 | 406.1 / 300.9 | 3.08 / 8 |
| Rat Race | Soft | 47,292 | 394.1 | 404.7 / 383.5 | 3.39 / 7 |

All four runs passed the 240 FPS average gate, recorded zero frames above
50 ms and reported no runtime errors. Bot fights varied between the indoor
runs. The indoor results measure headroom and do not rank the cost of Hard
and Soft. Outdoor Soft averaged 3.98 ms per frame against a 4.17 ms budget.
Additional sun or fixture passes still need a matched measurement.

For a repeat, use `tools/lighting_performance_run.py` with the release
benchmark build, the archived `fresh-checkpoint` and `finite-rect32-on`
controller route and display settings. The Rat Race camera was at
(8.100, −15.050, −2.280), aimed at (8.830, −11.030, −1.720). The visual
control shows bots crossing beneath the two fixtures. The local E1 evidence
package in `build/environment-e1/E1-evidence.zip` includes the camera
harness, commands, map and build fingerprints, frame timings, source surveys,
and matched reference images. E1 did not measure new environmental passes or
AMD hardware.
