# Optional muzzle-flash lighting

`display.muzzle_lighting = true` (`HALO_MUZZLE_LIGHTING=1`) adds short coloured
light pulses to successful shots. It defaults to **false** and is independent
of `display.plasma_lighting` and `display.plasma_occlusion`. Stock weapon flash
effects, particles, sounds, projectiles and explosions are preserved.
Keep render interpolation enabled for smooth fades between simulation ticks.

## Supported weapons

Profiles match exact stock weapon tag names, including both Xbox and Custom
Edition Needler names and the stock Warthog chaingun. Unknown custom weapons keep their
authored behaviour. Radii below are engine world units, durations are seconds.

| Weapon | Colour | Radius | Duration |
| --- | --- | ---: | ---: |
| Assault rifle | Warm amber | 1.8 | 0.10 |
| Pistol | Warm amber | 1.8 | 0.12 |
| Shotgun | Orange | 2.2 | 0.14 |
| Sniper rifle | Warm white | 2.0 | 0.12 |
| Plasma rifle | Cyan | 1.6 | 0.10 |
| Plasma pistol | Green | 1.6 | 0.12 |
| Charged plasma pistol | Green | 2.2 | 0.16 |
| Needler | Pink | 1.3 | 0.09 |
| Rocket launcher | Orange | 2.0 | 0.14 |
| Flamethrower | Orange | 1.4 | 0.08 |
| Warthog chaingun | Warm amber | 5.0 | 0.16 |

The Warthog profile matches `vehicles\warthog\warthog gun`. Read-only Xbox
Blood Gulch tag inspection confirms that this weapon has no model; the vehicle
model provides `primary trigger` on its animated gun. The existing invisible
weapon fallback follows that rendered vehicle marker. Its broader warm pulse
illuminates the hull and nearby ground; rapid shots refresh one bounded light.
No new shared-engine hook or stock flash changes are needed.

## Ownership and integration

`port/linux/game/muzzle_lighting.c` owns the profiles, eight bounded pulse
records, rendered marker placement, smoothstep decay and cleanup. A successful
shot records identities only; rendering creates the light. Dry fire, charging,
reloads and failed shots do not create pulses. Repeated shots refresh the same
weapon's pulse. Excess simultaneous weapons retain their stock effects; an
existing pulse is not evicted. Only muzzles within 25 units of a local camera
are retained.

The pulse follows the rendered first-person muzzle when available, otherwise
the world-model trigger marker. Missing markers are rejected. Position and
brightness update on rendered frames; the radius remains fixed throughout the
fade. These lights use the existing optional soft surface/model attenuation
and collision-occlusion modules. Muzzle occlusion is automatic while this
option is enabled; it does not enable projectile occlusion or projectile
lighting. Specular still respects the borrowed light template's flags.

Two minimal additions to established source implementations are necessary:

* `source/items/weapons.c` records the successful firing event after the
  existing projectile creation call. A feature module cannot reliably infer
  successful shots, including misfire and alternate-ammunition handling, from
  input or weapon animation alone.
* `source/objects/object_lights.c` invokes render preparation and lifecycle
  cleanup, includes the feature radius in visibility bounds, and exposes a
  checked move operation for an unattached, feature-owned glow. Light storage,
  flags and map partition membership are private to this implementation.
  The bridge verifies the salted handle, owner, template and feature flag.

Shared additions are marked `port:`. Existing Xbox, Custom Edition and MCC
loading paths and directories are unchanged. Map changes, failed-load
recovery and shutdown discard feature identities before light storage is
destroyed. Switching/dropping weapons, losing the owner, expiry and disabling
the option release pulses. At frame end lights are blackened and disconnected
from the map so they cannot affect simulation lighting between frames.

No network message or gameplay rule changes: each client renders successful
shots from its local simulation. The network version is unchanged. This work
does not implement CStencil or Modern shadows.

## Validation and limits

The asset-free harness executes the actual module and checked move bridge.
It covers subtick decay, repeated shots, profile admission, the eight-light
cap, stale identities, missing resources, disable, weapon/owner changes,
map reset and frame-end disconnection. Negative controls demonstrate that
tick-only fading, broken cleanup and a non-moving light fail the tests.
The focused muzzle/projectile/falloff/model/occlusion suite has 42 passing
tests. The light-storage ordinary COFF and full-LTO checks also pass.

Windows hidden OpenGL captures on 2026-10-14 exercised the handheld weapon
profiles at 30, 60 and 120 FPS, Blood Gulch and Carousel, CE Death Island,
disabled controls and combined projectile/muzzle lighting. Firing/decay image
sequences were inspected at each frame rate. Frame logs show intermediate
brightness values within the same simulation tick. Accepted runs:

| Cap | Cases | Measured mean FPS range | Maximum active-frame p95 |
| ---: | ---: | ---: | ---: |
| 30 | 15 | 30.00-30.00 | 34 ms |
| 60 | 15 | 57.97-60.03 | 20 ms |
| 120 | 16 | 119.41-120.02 | 11 ms |

Two initial 30 FPS plasma captures had long stalls and failed the timing
threshold; their repeated captures passed and the failed logs remain retained.
The initial Xbox Needler fixture requested the CE tag name and was excluded;
the corrected tag passed at all three rates. These short capture windows are
not a sustained performance guarantee. Capture sources match the implementation
except for a subsequent source-include comment identifying the feature module.

Console-driven Xbox Blood Gulch -> CE Death Island -> MCC Mercury Falling ->
Xbox Blood Gulch transitions completed with the feature off at 30 FPS and on
at 120 FPS, with normal exits and no logged exceptions. The first sandboxed
attempts could not connect to the local console and were excluded; the runs
with local socket access executed all three transitions. This verifies bounded
load/unload coverage, not complete MCC visual compatibility.

A two-instance native loopback game at 60 FPS used host-on/client-off settings.
All 29 common reported player-state snapshots matched through tick 870. The
host recorded 511 muzzle samples from two weapon handles; the disabled client
recorded none. Both exited normally. This used the session's default weapons:
the fixture's early equip attempt preceded spawning. No hits, deaths or respawns
were exercised, and browser/WebRTC interoperability was not rerun because
transport and protocol code are unchanged. The normal release executable also
passed a hidden OpenGL startup/map smoke test with all lighting options off.

Windows portable release builds pass. Android and web builds were attempted,
but this workspace has neither target/toolchain configured; those platforms
remain unverified. Linux was not built: WSL reports that it is not installed.
Approximate occlusion can still leak around complex
geometry. These captures are bounded Windows tests, not proof of every map,
GPU, multiplayer situation or split-screen configuration.
Selected model-light draws were observed. Positive muzzle-specific specular
output was not established in these scenes; the existing shader regression
tests pass, and authored no-specular flags remain authoritative.

Capture-only instrumentation and forced inputs are built by
`tools/plasma_visual_build.py`, which restores sources and rebuilds the normal
executable afterward. `tools/plasma_visual_run.py --muzzle ar` selects a weapon;
use `--both-lighting` to combine it with projectile lighting. Captures retain
source hashes, images, logs and timing reports under `build/plasma-visual`.
Only private copied maps and generated map caches are removed after a run.

## Warthog extension validation

The new stock Warthog profile uses radius 5.0 and a 0.16-second warm pulse.
Capture fixtures confirmed the model-less weapon resolves the vehicle's
`primary trigger` marker and enters the actual `W-gunner` seat before firing.
Final captures at 30, 60 and 120 FPS passed the 5% timing threshold, with a
60 FPS disabled control submitting no muzzle lights. Images show broad warm
ground spill; hull illumination varies with viewing angle. Stock flash and
explosion paths were not changed.

A hidden native Blood Gulch loopback run connected two players for 82 reported
snapshots, with host lighting on and client lighting off. The host logged actual
Warthog shots; the client submitted no muzzle lights. All 82 non-position player
states matched. Vehicle position components differed by up to 0.015 world units,
so this is not a claim of bit-identical simulation. Both exited normally. The
earlier 45-second run timed out before gameplay and is excluded. Logs are in
`build/emitter-network-retry-warthog`; final turret images are in
`build/plasma-visual/*-emitter-release-warthog`.


## Vehicle occlusion refinement

Warthog muzzle pulses now include vehicle collision geometry in the existing
optional visibility field. The emitting Warthog is not excluded: its chassis
can block light reaching ground beneath it, while exposed surroundings can
receive the same broad warm pulse. Radius, colour, duration and stock flash
remain unchanged. Other muzzle profiles retain their existing collision mask.
This requires both muzzle lighting and plasma occlusion to be enabled; both
remain off by default. It is approximate collision-based visibility, not exact
render-mesh shadows, and the existing shared budget and edge leakage apply.
The regression fixture checks vehicle-only obstruction and verifies that an
unrelated muzzle profile does not acquire vehicle blocking.

The chassis refinement was captured on Windows at 30/60/120 FPS, with an off
control at 60 FPS and final larger yaw/elevation sweeps at all three caps.
Images show exposed warm ground spill and a dark region under the chassis;
turning exposes lit hull surfaces. Final sweeps record model and specular draws.
The shared visibility regression tests pass, including vehicle obstruction and
preserving the collision mask for other muzzle profiles. These bounded scenes
do not establish perfect self-shadowing at every hull edge or seat/view angle.
See `build/plasma-visual/*-direct-warthog` and
`build/plasma-visual/*-direct-final-turn-warthog` for the retained captures.
