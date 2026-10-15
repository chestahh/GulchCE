# Covenant projectile lighting

The existing `display.plasma_lighting` option now includes supported Covenant
projectiles. It remains false by default. `display.plasma_occlusion` adds the
existing approximate collision visibility; it also remains false by default.
Muzzle lighting is independent. Shield illumination and persistent fuel-rod
weapon glow are not part of this change.

## Coverage and map evidence

Read-only inspection of Xbox map tag tables and weapon trigger references
established the following mappings. The campaign inventory covered a30, a50,
b30, b40, c10, c20, c40, d20 and d40; Blood Gulch and CE Death Island supplied
multiplayer comparisons. The inspected weapon references show Shade and
dropship guns using the same turret projectile, and Grunt/Hunter fuel-rod
weapons using the same fuel-rod projectile. No firing, damage, effects or map
tags are modified by this feature.

| Source | Projectile tag | Lighting |
| --- | --- | --- |
| Needler | `weapons\needler\needle`, `weapons\needler\mp_needle` | Missing-light fallback: pink, radius 1.0 |
| Ghost | `vehicles\ghost\ghost bolt` | Authored radius x1.75 |
| Banshee plasma | `vehicles\banshee\banshee bolt` | Authored radius x1.75 |
| Banshee fuel rod | `vehicles\banshee\banshee fuel rod`, `vehicles\banshee\mp_banshee fuel rod` | Authored radius x2.75 |
| Shade/dropship | `vehicles\c gun turret\c gun turret` | Authored radius x2.5 |
| CE multiplayer Shade | `vehicles\c gun turret\mp gun turret` | Authored radius x2.0 |
| Grunt/Hunter fuel rod | `weapons\fuel rod gun\fuel rod` | Authored radius x2.75 |
| CE fuel-rod cannon | `weapons\plasma_cannon\plasma_cannon` | Authored radius x2.75 |
| Wraith | `vehicles\wraith\wraith mortar` | Missing-light fallback: blue-violet, radius 3.5 |

Stock Needler needles and Wraith mortars have no attached `ligh` in the
inspected maps. If a custom map supplies one for those exact tags, it takes
precedence: its colour/effects are preserved, with radius scales 1.5 and 1.75
respectively, and no duplicate fallback is created. Unknown custom projectile
names remain stock. Grenades and non-Covenant projectiles are outside this
extension; the previously supported rockets are unchanged.

## Implementation and isolation

`port/linux/game/covenant_lighting.c` owns exact-tag policy and eight bounded
fallback slots. Existing authored attachments use `plasma_lighting.c` and its
soft diffuse/model/specular falloff. Authored colours, light modulation and
no-specular flags remain authoritative. Needler and Wraith fallbacks use the
existing glow bridge and rendered attachment pose, with no simulation updates.
Keep render interpolation enabled for smooth movement between ticks.

Fallback lights stay dark and disconnected between rendered frames. They are
released on disable, projectile deletion, modeled-object invisibility or leaving the 25-unit
camera range. Map unload, failed-load recovery and shutdown forget their
identities before the engine destroys light storage. Salted light handles and
owners are checked before reuse or deletion. Owned fallbacks bypass authored
radius save/restore so that cleanup cannot reconnect them into simulation.
Model-less projectiles remain eligible: the engine's `object_set_visibility`
cannot clear their invisible flag, including the stock particle-only Wraith mortar.

Minimal `port:` additions in `source/objects/object_lights.c` wire preparation,
cleanup and visibility bounds. The feature module cannot independently run at
these private light-array and partition lifecycle points. All policy remains
in feature-owned modules. No Xbox, CE or MCC loading paths are replaced, and
there are no network payload or gameplay-rule changes requiring a network
version change. Stock trails, impacts, explosions and supercombine effects
remain under the original engine's control.

The existing limits still apply: 128 authored attachments, eight rocket glows,
eight Covenant fallback glows and eight muzzle pulses. Only eight submitted
lights can receive approximate occlusion at once. Excess fallbacks keep stock
lighting; excess occlusion candidates keep unoccluded soft illumination.
Occlusion can leak at coarse edges and is not a shadow-map or stencil system.

## Validation

The 39-case Windows OpenGL capture matrix passed: all eleven projectile tag
variants above at 30, 60 and 120 FPS, plus disabled Needler controls and
32-needle capacity probes at each rate. Captures used hidden 640x480 windows
and were run serially on the local RTX 5070 Ti. Measured average rates across
those cases were:

| Requested FPS | Measured range |
| --- | --- |
| 30 | 29.46-30.03 |
| 60 | 59.98-60.01 |
| 120 | 119.41-120.02 |

Captured sequences were visually reviewed for every variant at every rate.
Enabled native-flight cases selected the expected tag and reached model lighting and
occlusion updates; disabled cases selected no feature lights and performed
no occlusion updates. Capacity probes admitted eight lights from 32 needles.
The 60/120 FPS traces show changing rendered projectile positions within
simulation ticks for every variant. Flight, homing, illumination and stock
impact effects remained consistent across the reviewed rates. This is bounded
automated visual coverage, not a full-resolution performance guarantee or a
complete campaign/vehicle combat acceptance pass.

Additional 60 FPS stock controls for Ghost and Wraith shots selected no
feature lights or occlusion updates; paired captures retained the stock
projectile and impact effects. A real-firing Needler capture with both muzzle
and projectile lighting enabled sustained 119.99 FPS, submitted both light
types and recorded 132 intermediate muzzle fade samples. These three extra
captures also passed and were visually reviewed.

The focused ownership, radius restoration, falloff, model shader, occlusion,
rocket and muzzle harness suite passes 46 tests. Negative controls verify
that missing reconnection and rejecting invisible particle-only projectiles
are detected. The light-storage bridge checks pass ordinary COFF and full
LTO builds, including the deliberate pointer-corruption negative control.

A hidden two-instance Windows loopback test ran real Needler firing with
projectile lighting/occlusion enabled on the host and disabled on the client.
Both instances exited normally and each logged 29 network state reports.
All 29 matching ticks had identical complete player-state lines on both sides.
This is a local regression check, not an internet or long-session soak test.
No network implementation, payload or simulation rule changed.

Two hidden map-transition passes loaded Xbox Blood Gulch, CE Death Island,
MCC Mercury Falling and Xbox Blood Gulch again: lighting disabled at 30 FPS
and enabled at 120 FPS. Both completed all three transitions, loaded the MCC
map once, exited normally and logged no exceptions. The enabled pass exercised
Needler lights before transitions. This checks lifecycle and map-family
boundaries on those maps; it does not establish full MCC gameplay compatibility.

The visual fixture uses real projectile definitions with native flight,
collision, homing and impact effects. Separate held-projectile probes test
capacity. Campaign captures wait for the opening cinematic, then disable AI
attacks and make the player deathless to avoid invalidating the capture with
checkpoint reloads. These controls, forced input and capture instrumentation
are absent from the normal playtest executable. Early cinematic/reload captures
were rejected rather than counted as evidence.

Windows portable release builds succeeded. Linux could not be built locally
(WSL is not installed); Android and web build attempts reported unavailable
Ninja targets/toolchains. Those platforms still require CI/device validation.
The normal executable passed a hidden OpenGL stock-settings smoke test and
contains no capture-fixture markers. The capture manifest's source hashes
match the implemented sources. Private test maps were removed; diagnostic
logs, reports and images remain under `build/`.
