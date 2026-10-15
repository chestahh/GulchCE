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
| Jackal minor / major | `characters\jackal\jackal`, `characters\jackal\jackal major` | `shield light` | 3.6 |
| Stationary shield | `scenery\c_field_generator\c_field_generator` | `shield light` | 2.8 |
| Fuel rods | `weapons\fuel rod gun\fuel rod` (weapon group) | `lens flare` | 1.6 |

Shield colour comes from runtime change colour C, the stock shield shader's
colour source: minor blue, major gold and stationary violet. Blue Jackals use a
wider radius and a (0.30, 0.42, 1) minimum blue profile for more visible spill.
Both stock generic shield shaders blend this colour towards red with A-out in
stage 3. The feature samples the same evaluated outgoing function, without a
separate hit timer or colour smoothing that would lag behind the shield. Gold shares the 3.6-unit reach, with green/blue floors of 0.78/0.24 for
brighter warm spill. The resting stationary violet profile is unchanged. Both shield models
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
light-allocation hooks are needed. The radius is 0.95 world units, colour
(0.24, 0.65, 1.0), and strength 0.65, increased after playtest feedback.

A minimal marked hook in `rasterizer_xbox_models.c` lets the feature adjust the
self-illumination colour before shader selection and constant submission. A
feature-only light cannot brighten the screen's own emissive mask, hence this
addition to the shared Xbox/CE/MCC model drawing path. The helper matches only
`weapons\assault rifle\fp\shaders\display`, shared by both stock models, and
raises its emissive minimum to (0.22, 0.75, 1). The authored multipurpose texture
still masks the emission. It does not add global bloom or alter tag data. The
cached shader identity is cleared by the existing map/light-storage lifecycle.
The setting-off path returns before looking up or changing any shader.

Only the exact `weapons\assault rifle\assault rifle` weapon currently held by a
visible unit participates. That includes Marines and remote players without
depending on player ownership. Holstered, dropped, missing-marker and unfamiliar
weapons are excluded. The existing first-person marker API selects the local
visible model; other views use the third-person weapon marker. No tag, ammo,
animation, or networking state is changed. The screen shader receives only the
optional per-draw emissive adjustment described above.

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


## Shield colour and counter refinement validation (2026-10-15)

The normal Windows x86 portable release builds successfully. All 52 focused
emitter/projectile/muzzle/occlusion/falloff/model-shader harness checks pass,
including a negative control removing the shader-driven damage colour. The
screen tests cover disabled behavior, unrelated shaders, missing tags and cache
reset across maps; the profile tests cover both brighter Jackal colours.

`build/colour-capture-summary.json` records 24 accepted cases: blue Jackal, gold
Jackal, stationary shield, first-person AR, player-model AR and Marine AR, each
on at 30/60/120 FPS and off at 60 FPS. All sustain the requested rate within 5%.
Captured RGB telemetry verifies resting and red damage colours for all nine
shield/FPS combinations. The fixture applies stock damage-network state, which
then drives the actual object functions and shield shader; it does not force the
light colour. It also exercises depletion, recovery and deletion. One gold
60-FPS run failed due to a long hitch; its retry passed at approximately 60 FPS.
The failed report is retained rather than counted as a pass.

Images show broader blue/gold spill, matching red shield/light states and stronger
AR housing illumination with readable first-person digits. Third-person captures
show illumination on the held models, but shoulder occlusion and the counter's
small screen size limit assessment of its readability. An additional close
player capture exposes the weapon; the close Marine experiment was off-screen
and is not evidence of screen readability. The standard fixture placement is
retained. These are bounded visual checks, not exhaustive gameplay acceptance.

Source manifests are in `build/emitter-colour-visual` and
`build/emitter-colour-final`; gold uses the latter after its extra brightness
change. The unchanged blue/AR/stationary paths use the former captures. Final
production sources match the final manifest. The normal executable was rebuilt
without fixtures and smoke-tested separately in `build/colour-playtest-final-smoke`.
Its hash is checked against the packaged executable; no capture markers remain.

Xbox c40 -> CE Death Island -> MCC Mercury Falling -> Xbox Blood Gulch
transitions pass off at 30 FPS and on at 120 FPS, with clean exit and no logged
exceptions (`build/colour-transitions`). This checks lifecycle/loading boundaries,
not full map-family compatibility or every custom shader. Android and web builds
were attempted but the generated build has neither target; Linux and browser
transport testing remain unavailable locally. No network format, simulation
contract, map loader, CStencil or Modern implementation changed.


A two-instance native loopback AR session passed with both options enabled:
84 shared snapshots, 83 exact matches and one differing `ed` value (221 vs 220).
Positions, health and ammo matched in that sample. Both instances exited cleanly;
only the host fixture equipped the AR, and the client recorded 856 emitter
samples from the replicated weapon. See `build/colour-network-rifle`. This is a
bounded multiplayer smoke/regression check, not a proof of deterministic or
internet/browser compatibility. All test map copies were confined to private
run folders and removed; original maps and saves were preserved.

## Directional ammo counter refinement

The counter keeps its 0.95 radius, 0.65 strength, colour and emissive screen
floor. Its spill now follows a broad rear/up-facing hemisphere in the animated
weapon marker basis (-0.8 forward + 0.6 up), in both first and third person.
This approximates the sloped display orientation; it is not a measured mesh
normal or an area-light simulation. A smooth angular fade reaches full strength
at a dot product of 0.6 and zero behind the display. This removes downward spill
for a normally held rifle while allowing nearby surfaces when the rifle turns.

`ammo_counter_falloff.c` owns the bounded eight-light, 16-cubed texture pool.
Existing world diffuse/specular and model visibility interfaces consume the
same angular mask. With collision occlusion enabled it multiplies the visibility
field after admission blending. Without an admitted field it uses a separate
mask, so disabling wall occlusion or exhausting its budget does not remove the
directionality. Allocation/upload failure still permits the existing unmasked
fallback; this is not a guarantee under graphics resource failure.
Textures reset with the existing occlusion lifecycle and refresh per view.
No stock source hooks, map loaders, simulation or network formats changed.

Regression tests exercise the actual feature sources, rotated poses, forward
brightness, rear rejection, combined visibility, disabled collision, independent
world/model textures and resource cleanup. A mutation that removes the angular
mask is rejected. The brighter shield profiles and screen material are unchanged.

The directional refinement passes 54 focused lighting, falloff and shader
regression tests. Windows portable release and diagnostic builds compile.
The final capture manifest is `build/directional-sweeps/build-manifest.json`.
Earlier stationary captures use `build/directional-visual`; the only later
production changes are a collision comment and retaining the directional cache
between emitter-only frames when collision occlusion is off.

Visual captures cover first-person AR, Marine and player models at 30/60/120 FPS,
with disabled controls at 60 FPS. The final aim sweeps exercise yaw, elevation
and reloads. First-person screen readability and nearby housing light remain;
the tested held-model scenes have no broad blue foot pool. Their rear camera
angle limits assessment of face illumination and tiny third-person digits.
The angular model is intentionally approximate and needs normal playtest feedback.
Initial weak-input sweeps were superseded after correcting controller dead-zone
coverage; two also failed timing due to frame hitches. Those logs are retained.
Final captures are separate and must sustain the cap within five percent.

`build/directional-capture-summary.json` records 26 accepted capture cases,
including counter-only lighting with collision off at all three frame caps.
Xbox c40 -> CE Death Island -> MCC Mercury Falling -> Xbox Blood Gulch passed
with features off at 30 FPS and on at 120 FPS, clean exits and no logged
exceptions (`build/directional-transitions`). This checks lifecycle boundaries,
not exhaustive map compatibility. Android and web builds were attempted but
both targets are absent from the generated build; Linux and browser transport
remain unverified locally. No simulation agreement or wire format changed.

The two-instance native AR session exited cleanly with 84 shared player snapshots:
76 exact matches; the other eight differed only by one in the logged `ed` field.
Positions, health, ammunition and the other compared player fields matched.
The client recorded 4926 emitter samples without equipping a fixture locally.
This is a bounded replication smoke test, not bit-identical multiplayer proof
(`build/directional-network-rifle`). The normal executable passed a separate
hidden OpenGL smoke test with stock defaults and no capture markers; its hash
matches the final package. Final production hashes match the sweep manifest.
Owned map copies were removed; original maps/saves and diagnostic logs remain.
