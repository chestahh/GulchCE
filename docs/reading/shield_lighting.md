# Shield Glow and animated Overshield

Shield Glow adds soft radial illumination to a unit's visible shield. It uses
its selected Shield Look, authored colour channels and animated overshield
layer. Animated Overshield still owns the rising surface and first-person arm
animation; it does not require Shield Glow. Neither feature changes vitality,
damage, tags, projectile responses or map-family loading.

Both switches default off. Shield Glow and Glow Intensity are in Video Setup >
Dynamic Lighting; Animated Overshield remains in Extra Effects. Existing saved
keys are retained. The inherited `network.shield_effects` policy is authoritative
online: stock suppresses both, the glow/overshield combinations select them, and
players delegates to local preferences. No packet or protocol version changes.
Light Occlusion remains a local rendering preference, as for the other emitters.

Android excludes Shield Glow with the rest of the optional lighting renderer,
even if a host requests glow. The host's transmitted policy is unchanged and
still controls Animated Overshield surfaces. The
[Android replacement](../../port/android/guest/dynamic_lighting_disabled.c)
retains `shield_glow_reset`'s call to `overshield_effect_reset`, so surface state
does not survive map/light-array replacement. See the
[Android lighting boundary](../../port/android/README.md#lighting-availability).

## Reading route

1. [network_shield_effects.c](../../port/linux/game/network_shield_effects.c),
   `network_shield_effects`, resolves the existing host policy.
2. [overshield_effect.c](../../port/linux/game/overshield_effect.c),
   `overshield_effect_get`, owns the time-based charge state. The body, arms and
   `overshield_effect_light_sample` use `overshield_effect_layer_color`: green
   above 200% total shield, pink at or below it. `overshield_effect_reset` clears
   model-height caches, units and arm pointers on map/light-array boundaries;
   effective disable also clears this state. `overshield_effect_find_values`
   matches queued body/arm draws for both colour and height fading. Colour
   lookup does not compute unused fade parameters or scan the effect list twice.
3. [shield_glow.c](../../port/linux/game/shield_glow.c), `shield_glow_sample`,
   resolves the same selected shader and colour rules as the surface. Normal
   flares use its evaluated intensity source/exponent and the selected recharge
   envelope. Overshields use visible charge coverage and the shared layer hue.
   A dark authored colour stays dark; only overbright colours are normalized.
4. `shield_glow_update` selects at most eight candidates by strength, radius and
   distance from the nearest local camera. Existing lights receive a 20% score
   bonus to resist churn. New lights preserve short hits; existing lights use
   15 ms attack and 80 ms release time constants. Admission and release use
   separate small thresholds. Selection fades over the last three units of the
   25-unit range. This is a bounded approximation, not a global light allocator.
5. [plasma_lighting.c](../../port/linux/game/plasma_lighting.c) registers shield
   light identities for the existing world/model attenuation and coloured
   specular paths. [plasma_occlusion.c](../../port/linux/game/plasma_occlusion.c)
   admits them to the existing eight-field collision budget when enabled.

For example, an animated green overshield with Shield Glow enabled produces a
green radial light. As its charge rises, the light centre rises from the lower
body towards its middle and its contribution grows. Dropping to the inner layer
turns both the surface and emitted light pink. Disabling only Shield Glow removes
that added illumination while leaving the animated surface active.

## Ownership and limits

Rendered skeleton heights place the radial source and scale its reach. Crouching
lowers its centre and reduces its radius. This approximates a luminous body with
one point source: it is not mesh-based area lighting, an AR-style cone, or precise
self-shadowing. Some illumination of nearby ground is physically appropriate.

The light module owns light handles; it disconnects and blacks them after each
render frame, deletes them on disable or loss of eligibility, and forgets handles
when light storage is reset. Repeated split-screen views at one game time do not
advance the smoothing. Clock rollback releases lights before reselection.

The only additional engine wiring is in
[object_lights.c](../../source/objects/object_lights.c): shield-owned radius bounds
must participate in visibility partitioning, and shutdown must forget shield
state. These private light-storage operations cannot live solely in a feature
module. Existing map-reset hooks also reset overshield state. Xbox, Custom Edition
and MCC continue using their existing loaders and rendering services.

Light Show retains its saved meaning: three times the radius and full intensity
from a third of the flare. The ordinary setting does not multiply radius by hit
strength, avoiding a rapidly expanding luminous edge. Stock light-template flags,
including no-specular, remain authoritative. Occlusion is the same filtered
collision approximation as other dynamic lights, with bounded capacity and
possible leakage; it is not CStencil or Modern.

## Checks

[test_shield_lighting.py](../../tools/harness/tests/test_shield_lighting.py) compiles the actual
module against a synthetic world: host override, selected colours, camouflage,
body placement, 30/60/120 Hz release equivalence, selection and cleanup. The
overshield rendering test checks shared layer hues and model-cache reset.
The plasma lighting/occlusion harnesses cover shield-only activation and cleanup.
Menu checks verify generated ownership, saved keys and the ten-row layout.

Runtime evidence and its limits are recorded in the
[Milestone 2 acceptance record](../dynamic_lighting_acceptance.md). Synthetic
checks do not establish appearance on every map, GPU or platform.
The [Milestone 4 compatibility record](../dynamic_lighting_compatibility.md)
adds mixed-setting multiplayer checks for Xbox, CE and MCC, including received
host policy, respawns, weapon changes and late joining.
