# Covenant emitter lighting

Three independent options default to false: `display.jackal_shield_lighting`,
`display.fuel_rod_lighting` and `display.covenant_shield_lighting`. Their environment
variables are `HALO_JACKAL_SHIELD_LIGHTING`, `HALO_FUEL_ROD_LIGHTING` and
`HALO_COVENANT_SHIELD_LIGHTING`. Projectile lighting is not required.
`display.plasma_occlusion` optionally adds the existing approximate collision
visibility filter. CStencil and Modern are separate work.

`port/linux/game/covenant_emitter_lighting.c` owns eight additional render lights
shared by these options and the optional Assault Rifle counter. Stock effects, tag data, damage, weapons, explosions and
network state are unchanged. Excess emitters retain stock appearance. Stable
admission avoids swapping lights every frame when the budget is full. Lights
fade between 22 and 25 world units from the nearest local camera.

## Profiles and evidence

Read-only inspection of Xbox c40 tags and the existing model/shader interpreter
established these identities; no third-party implementation was copied.

| Emitter | Exact tag | Marker | Radius |
| --- | --- | --- | --- |
| Jackal minor / major | `characters\jackal\jackal`, `characters\jackal\jackal major` | `shield light` | 2.6 |
| Stationary shield | `scenery\c_field_generator\c_field_generator` | `shield light` | 2.8 |
| Fuel rods | `weapons\fuel rod gun\fuel rod` (weapon group) | `lens flare` | 1.6 |

Shield colour comes from runtime change colour C, the stock shield shader's
colour source: minor blue, major gold and stationary violet. Both shield models
have a `shield` region with visible permutation zero and `~shield_off` permutation
one. The marker also exists with the shield off, so marker presence alone is
insufficient. Depleted vitality, depleted flags, shield permutation, destroyed
region and death all suppress illumination. Indestructible scenery with zero
maximum body vitality remains eligible.

Fuel rods use a restrained green light at the back of the weapon, not its muzzle.
A held weapon must be the unit's current weapon; a dropped visible weapon stays
luminous until stock removal. The model-less Hunter weapon is excluded. Renamed
custom tags are deliberately not guessed from substrings.

The rendered marker pose follows existing render interpolation. An 80 ms
exponential response smooths activation, shield damage and collapse using render
time, including frames between 30 Hz simulation ticks. Repeated views at the same
time do not advance the response. World diffuse, model lighting and eligible
specular highlights use the existing soft falloff. Occlusion shares the existing
eight-field budget and is approximate; it is not stencil or shadow-map rendering.
The stationary shield is excluded from its own visibility rays because the
point source approximates its luminous surface. Other scenery and machines,
including other shield emitters, still block it.

## Integration and lifecycle

The small `port:` additions in `source/objects/object_lights.c` are necessary
because feature modules cannot independently enter light collection, extend its
bounds, or observe engine light-storage resets. They call the owned module before
collection, extend only owned light bounds, and forget identities at initialization,
old-map disposal, shutdown and light-storage recovery. Existing light allocation/move interfaces are reused.
Existing feature modules route submission, model masks, optional occlusion and
frame cleanup. Each frame ends with owned lights black and disconnected from the
map, preventing their use by simulation/AI. Disabling removes owned lights;
missing markers/templates and allocation failure fall back to stock appearance.

No map-family discovery or loading code changes. Xbox, Custom Edition and MCC use
their existing paths; only recognized tags with valid models/markers participate.
Network messages and simulation contracts do not change, so no network-version
bump is needed.

## Assault Rifle counter

`display.ammo_counter_lighting` / `HALO_AMMO_COUNTER_LIGHTING` is independently
off by default. `ammo_counter_lighting.c` supplies the rendered source position;
the existing emitter module owns its bounded light, easing and cleanup. No new
shared-engine hooks are needed. The radius is 0.55 world units, colour
(0.15, 0.55, 1.0), and strength 0.22. This is a subtle display glow, not a flashlight.

Only the exact `weapons\assault rifle\assault rifle` weapon currently held by a
visible unit participates. That includes Marines and remote players without
depending on player ownership. Holstered, dropped, missing-marker and unfamiliar
weapons are excluded. The existing first-person marker API selects the local
visible model; other views use the third-person weapon marker. No tag, ammo,
display shader, animation, or networking state is changed.

Read-only Xbox c40 geometry inspection found counter-digit part centroids near
(0.04706, +/-0.00160, 0.06850) in first person and
(0.04734, +/-0.00160, 0.06840) in third person. Both `primary trigger` markers
have identity rotation; their z coordinates differ by 0.002744. An offset of
-0.18 along forward and +0.025 (first person) / +0.027744 (third person) along up
places the light just behind and above the display, tracking the rendered pose.
Custom models reusing the stock tag name may require separate tuning.

## Reproducing checks

The harness compiles the real module and exercises settings, authored colours,
shield permutations, held/dropped eligibility, frame-rate-independent easing,
capacity, cleanup and allocation failure. Negative controls remove the shield
permutation guard or replace render time with tick time and must fail.

Build the diagnostic executable with `tools/plasma_visual_build.py`; normal
sources are restored and the normal Windows executable rebuilt afterwards.
Run `tools/plasma_visual_run.py --data-root <data> --map c40 --emitter <kind>
--fps <30|60|120> --enabled <0|1> --occlusion 1 --label <unique>` where kind is
`jackal`, `major`, `fuel` or `stationary`. The fixture spawns original tags,
depletes/recover shields through engine damage interfaces, drops held fuel rods,
and deletes its emitter. The fuel fixture clears the temporary detonation timer
from creating a loose weapon when its hand attachment completes; later dropping
uses the stock weapon behaviour. These controlled state changes exist only in the capture
build. Logs, timing reports and images remain in `build/plasma-visual`; copied
maps are removed. Visual checks do not establish exhaustive campaign compatibility.

Counter fixtures use `--emitter ammo-fp`, `ammo-marine` or `ammo-player`.
Blood Gulch supplies the first-person/player-model cases; b40 supplies Marines.
`--hide-viewmodel` removes the local weapon from the image for clearer third-person
inspection, using the existing setting that leaves weapon lights active.

## Validation of this milestone

Windows portable release builds passed. The 51 focused real-source harness
checks passed across emitters, muzzle/projectile ownership, occlusion, falloff
and model shaders. These include negative controls for shield permutations,
tick-only easing and loss of the first-person counter pose. Existing light-storage
COFF/full-LTO checks and their corruption negative control also passed.

32 final enabled/disabled capture cases passed: Jackal minor, Jackal major, held
and dropped fuel rods, stationary shields, Warthog fire, first-person counters,
Marine counters and player-model counters. Each has enabled 30/60/120 FPS captures
and a disabled 60 FPS control. All sustained their cap within 5% during the
measured windows. Disabled cases submitted no added feature lights. Shield
collapse/recovery and fuel-rod attachment/drop/deletion were exercised. Images
were inspected at all three rates. An additional b40 first-person counter run
observed both model diffuse and specular draws; Blood Gulch's counter scenes did
not demonstrate positive specular output. Stock shader eligibility is preserved.

Reports/images are under `build/plasma-visual`, with the selected 32 reports
listed in `build/lighting-release-captures.json`. Jackal captures use
`emitter-final-*`, fuel/turret captures `emitter-release-*`, and the latest
stationary/counter captures `ammo-final-*`. The earlier captures precede the AR
addition but exercise unchanged profiles. `build/ammo-ground-visual` records
the final production source hashes. Normal sources were restored, a normal
executable rebuilt, and capture logging/fixture absence checked. Two additional
60 FPS plasma-rifle/Needler regression captures passed before the AR addition.

The initial short b40 counter run did not reach its capture window; the longer
run passed. A stationary fixture initially placed the source below the floor;
the fixture now uses a structure collision ray to place it on the floor. Failed
runs are retained as diagnostics and excluded from the final count.

Android/web builds were attempted but this Windows build graph has neither
target configured. Linux was not built because WSL is unavailable. Browser
networking was not rerun; protocol/transport code is unchanged. These are bounded
Windows scene and lifecycle checks, not full campaign, arbitrary custom-model,
GPU, split-screen or multiplayer acceptance. Approximate occlusion and bounded
light admission retain the limitations described above.

The final two-instance native Blood Gulch run at 60 FPS produced counter samples
on both host and client, including the host's newly equipped AR as seen by the
client. Both exited normally after 84 reported snapshots. 75 snapshots matched
exactly; nine differed during live firing/death/respawn, so this checks bounded
rendering/connectivity rather than establishing deterministic network agreement.
The client fixture did not equip a weapon. Reports are in `build/ammo-network-rifle`.
Additional 60 FPS Marine/player-model captures with the local viewmodel hidden
passed and are retained under `ammo-clear-*` for less obstructed inspection.

Final Xbox c40 -> CE Death Island -> MCC Mercury Falling -> Xbox Blood Gulch
transitions passed with all additions off at 30 FPS and on at 120 FPS. Each run
executed all three transitions, loaded MCC once, exited normally and logged no
exceptions (`build/ammo-transitions`). These checks do not establish full visual
compatibility for arbitrary MCC assets. The exact normal release executable
passed a hidden OpenGL startup smoke test with stock defaults; its SHA-256 is
recorded in `build/ammo-playtest-smoke/report.json` and checked against the ZIP.
