# Dynamic lighting release candidate

The `dynamic-lighting-test` branch contains the GulchCE dynamic lighting
baseline. This is a playtest candidate with known performance and memory
limitations, not a claim of sustained 120 FPS or universal map compatibility.
CStencil and Modern shadows remain separate, later work.

## Player controls

Open **Settings > Video Setup > Dynamic Lighting**. All lighting switches
default to **Off**; Glow Intensity defaults to **Default**. Saving applies the
pending choices, cancelling discards them, and Defaults restores the stock
choices for this screen. Saved choices persist across launches.

| Menu row | What it controls |
| --- | --- |
| Projectile Lights | Plasma, rockets and the supported Covenant projectile profiles, including Needler, Shade, fuel rod, Ghost, Banshee, Wraith and dropship fire. |
| Muzzle Flash Lights | Extra short-lived weapon illumination, including the Warthog turret; stock flash and explosion effects remain intact. |
| Jackal Shields | Blue/orange shield spill, following the shield's damage colour. |
| Stationary Shields | Covenant shield-emitter spill, including the red damage flash. |
| Fuel Rod Glow | Illumination from the held fuel rod weapon. |
| AR Counter Glow | Screen emission and directed spill in first and third person. |
| Needler Glow | Needle emission and soft outward spill on the weapon and its holder. |
| Light Occlusion | Optional blocking for projectile and emitter lights. Muzzle lights retain their own collision blocking, including the Warthog chassis. |
| Shield Glow | Player/unit shield spill, subject to the host's shield-effects policy. |
| Glow Intensity | Default or Light Show intensity for Shield Glow; this does not enable it. |

**Animated Overshield** remains in **Video Setup > Extra Effects**, also off
by default. First-person shield surfaces additionally depend on **Arm
Shields**. In multiplayer the host's **Shield Effects** policy remains
authoritative; **Players Choose** permits each player's shield preferences.
Other lighting choices are local presentation settings and do not change
damage, collisions or network simulation. A stock host policy can override
the local shield toggles; that is intentional.

Frame interpolation is a separate setting and supplies smooth poses between
simulation ticks. Existing per-pixel lighting is also separate. Neither is
silently enabled by the lighting submenu. Custom maps must use a supported
tag/profile; this feature does not infer arbitrary modded weapon semantics.
Xbox, CE and MCC retain their separate loading paths.

## Implementation and final review

Start with the [rendering walkthrough](reading/rendering.md) and
[shield walkthrough](reading/shield_lighting.md). The
[implementation history](plasma_lighting.md) preserves earlier decisions and
validation; its initial prototype restrictions are historical. The
[emitter notes](covenant_emitter_lighting.md),
[compatibility report](dynamic_lighting_compatibility.md) and
[stability investigation](dynamic_lighting_stability.md) contain detailed
evidence and its limits.

The Milestone 5 review follows the combined projectile, muzzle, held-emitter,
shield and overshield paths through settings, host policy, temporary light
ownership, rendering hooks, occlusion, shader generation and lifecycle
cleanup. Capture and profiling fixtures remain in tools, outside normal
builds. Bounded storage, salted object identities, authored-light precedence,
platform stubs, collision checks and shader-cache source comparisons are
necessary protections and were retained.

Two feature-local simplifications have a concrete benefit:

- `covenant_visible_profile` in
  [covenant_lighting.c](../port/linux/game/covenant_lighting.c) returns its
  validated profile to the caller. Admission no longer repeats object/tag
  lookup after validation. With ten eligible projectiles and eight light
  slots, the harness observes ten tag queries on initial admission instead
  of twenty-six, and eighteen on a retained update instead of twenty-six.
  The visibility/ownership validation is preserved; profile zero remains
  valid. These are lookup counts, not a measured FPS improvement.
- `overshield_effect_find_values` in
  [overshield_effect.c](../port/linux/game/overshield_effect.c) shares the
  existing active body/arms lookup. Colour selection no longer scans the
  effect array twice or computes unused fade geometry. Activation, host
  policy, charge timing, fade equations and colours remain unchanged.

No shared engine, network protocol, map loader, AMD/NVIDIA workaround or
shader-cache behaviour is changed by these final refactors. Existing shared
hooks remain necessary because feature modules cannot independently intercept
the engine's light submission, interpolated model draw and frame cleanup.
This pass adds no shared hook and does not merge map-family implementations.

## Known limitations

- Sustained 120 FPS is not established. The earlier eight-minute enabled
  busy-combat run at native 2560x1440 averaged approximately 115.98 FPS at a
  120 cap. Short captures at 30/60/120 FPS are visual checks, not endurance
  certification.
- Shader compilation/program linking can still hitch. A capture-free,
  lighting-disabled run recorded a 51.229 ms frame containing 10.977 ms of
  shader compilation and 36.805 ms of program linking. The earlier 412/221 ms
  stalls were not reproduced, and their cause remains unresolved.
- Memory has not demonstrated a plateau. The enabled capture-free run grew
  approximately 3.64 MiB between early/late windows, with a later fitted
  trend of 1.52 MiB/minute. The disabled control also had a late approximately
  27 MiB jump near shader compilation. Allocation ownership is unresolved;
  these observations neither prove a leak nor establish stability. The
  completed shader-cache deduplication does not explain all remaining growth.
- Runtime evidence is representative, not exhaustive: Windows/NVIDIA runs
  do not certify AMD hardware, all drivers, every custom map, internet play
  or Android/web visual parity. Platform builds and targeted regression tests
  establish narrower facts. See the linked reports for actual checks.

The final optimisations above do not claim to fix these limitations. Normal
player acceptance, including subjective brightness and the user's typical
combat workload, remains the final sign-off before calling this baseline 1.0.
