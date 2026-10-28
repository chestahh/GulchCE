# Shoulder camera on online-third-person

This optional GulchCE camera is off by default. A host can instead select
Player Choice (the default), Forced On or Forced Off for the match. The
policy does not overwrite saved player preferences. Stock Xbox, Custom
Edition and MCC loading paths remain separate; MCC weapon-authored following
cameras, vehicle cameras, scripts and cinematics retain their priority.

Default keyboard controls are **N** for perspective, **B** for shoulder,
and **Left Alt** held for free look. Holding the configurable Camera Distance
modifier (also Left Alt by default) while scrolling adjusts the bounded
distance setting. Ordinary scrolling switches weapons. Menus, console, text
chat and released mouse capture do not consume the wheel for distance.
Saved custom bindings are retained. Camera controller bindings are deferred.
No stencil shadows or cosmetic head tracking are part of this consolidation.

## Reading order

1. [third_person_policy.c](../../port/linux/game/third_person_policy.c):
   `third_person_policy` reads the authoritative game record;
   `third_person_enabled` combines it with the saved preference. Only a host
   creates or edits the match policy. Forced Off clears replicated shoulder
   input; scope, seats and death still use their established cameras.
2. [director.c](../../source/camera/director.c):
   `director_choose_game_perspective` selects the shoulder callback only for
   eligible on-foot play. Passenger display overrides and authored following
   cameras are considered first. `director_camera_is` exposes identity
   without exposing mutable director state. These small shared hooks are
   necessary because the director owns the selected callback privately.
3. [third_person_director.c](../../port/linux/game/third_person_director.c):
   `third_person_camera_update` calls the stock first-person eye calculation
   with the director's exact callback signature, including on WebAssembly.
4. [third_person.c](../../port/linux/game/third_person.c):
   `third_person_camera` resolves the shoulder offset after interpolation,
   collision and free-look orbit. `director_get_perspective` reports that
   resolved view to body/particle rendering. Hidden viewmodel poses, markers
   and sounds continue updating in `first_person_weapons.c` so collision
   fallback does not need to reconstruct them.
5. [third_person_weapon.c](../../port/linux/game/third_person_weapon.c):
   `third_person_muzzle_shot` and `third_person_muzzle_target` share the
   cover-aware muzzle solver. The marker follows authored alternating barrels
   and charged secondary triggers. Explicit custom firing rules retain their
   existing behavior. Spread, projectile gravity and aim assistance remain
   the weapon's own.

## A shot beside cover

The camera can see around a corner while the weapon remains blocked. The
solver checks the eye-to-barrel segment and the muzzle's firing line, then
projects the reachable target into the displaced camera's reticle. A barrel
inside a wall cannot spawn a projectile beyond it. Free look holds gameplay
aim fixed and hides the reticle until the camera returns.

On the host, `third_person_muzzle_history` supplies a muzzle computed from
the host's weapon pose to `distributed_note_targets` in
[network_damage.c](../../port/linux/game/network_damage.c). This small hook
is necessary because that module owns hit visibility history. The feature
does not submit client muzzle coordinates or bypass damage validation. The
existing visibility check requires clear eye-to-muzzle and muzzle-to-target
segments. Ownership, rate limits, target history and damage rules still run.
The upstream optional client-hand origins remain separately controlled.

The changed host acceptance rule uses experimental network versions 32702
(Xbox/CE) and 32703 (MCC). Matching builds are required even with the camera
off. See [networking](networking.md) for the mandatory join envelope and
the intentional OpenCE/main crossplay boundary.

## State and checks

Each local viewport owns its collision, orbit and target state. Map loads and
checkpoint restores call `third_person_reset`; changed units, death, seats,
scope and scripted cameras clear orbit state. A held orbit key must be
released before reacquiring after these transitions. Settings and keyboard
ownership remain shared as documented; two separate processes do not prove
same-process split-screen input independence.

`tools/test_third_person.py` exercises the production camera and muzzle
solver with controlled collision worlds, including near cover, FOV, scope,
free-look release and lifecycle exits. `tools/test_third_person_director.py`
executes director transitions and wheel routing. Camera policy, wire and
admission tests cover host ownership and malformed inputs. The reported-hit
sight and fire-origin harnesses cover visibility and untrusted-origin rules.
`tools/camera_policy_sessions.py` supplies real two-process loopback checks.

These checks have distinct limits: fake-world tests establish algorithmic
behavior; loopback sessions exercise real simulation and replication but do
not establish public internet connectivity or full campaign compatibility.
Rendered map-family transitions, weapon effects and physical controller or
split-screen interaction need their own runtime evidence.
