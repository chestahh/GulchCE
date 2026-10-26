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

## Milestone 5 validation and delivery

The final functional changes are in `7b1e8c02`; `152142a4` adds documentation
and corrects an overshield menu-location comment. The Windows playtest uses
`152142a4`, built with `--portable --release --lto=off --pgo=off`.

| Check | Observed result |
| --- | --- |
| Focused local regressions | 205 passed; five POSIX process/launcher cases skipped on Windows. Includes ownership, off/reset paths, falloff/occlusion, shield policy and body/arms effects, fragment-cache and visibility contracts, settings and MCC networking. |
| Touch-menu unit | Passed separately with the installed Windows toolchain's explicit x86 target and CRT link flags. The generic pytest invocation did not supply those flags; no touch implementation change was needed. |
| Short rendered captures | Xbox Needler at 30/120 FPS, CE Needler at 60 FPS, and Xbox overshield charge at 60 FPS with Shield Glow on/off all passed the runner's checks. |
| Mixed-setting peers | Two hidden Windows instances at 1280x720/60 FPS, Xbox Blood Gulch, with opposite local lighting choices and a host-forced shield policy. Both observed two respawns of the same player and inventory changes. All 139 stable policy samples matched; 81 seconds of paired snapshots had no comparator divergence. |
| Map lifecycle | One enabled Windows process loaded Xbox c40, MCC Mercury Falling, CE Death Island and Xbox Blood Gulch, with three tick resets, expected shield activation and no exception. |
| Platform CI | [Windows, Linux, Android and web passed](https://github.com/chestahh/GulchCE/actions/runs/40266569136), including Linux tests, browser/native WebRTC checks and Chromium/Firefox web checks. [MCC validation also passed](https://github.com/chestahh/GulchCE/actions/runs/40266569124). |
| Delivered ZIP | Archive integrity and every packaged file verified; extracted executable started and displayed the Dynamic Lighting menu. All nine lighting booleans plus Overshield remained false, intensity Default and host policy Stock. No diagnostic fixture strings were present. |

The five short visual scenes use 640x480 isolated captures. Frame-log averages
were 29.19/119.99 FPS for Xbox Needler, 59.96 for CE Needler and 58.80/60.00
for shield spill on/off; all met the runner's five-percent tolerance. Image
inspection showed purple projectile spill, the retained overshield charge
rise and green-to-pink layer transition. With Shield Glow off, the independently
enabled overshield surface remained and the added spill disappeared. These
are scoped visual observations, not sustained performance measurements or
exhaustive aesthetic acceptance. The capture executable predates only the
menu-location comment; source hashes and that exact comment-only difference
were checked. The multiplayer/lifecycle probe and normal build use `152142a4`.

Local Android/web targets are unavailable in this Windows configuration;
fresh CI supplies those build checks. CI uses debug builds on this branch,
while the local playtest is release. There was no new AMD hardware test or
retail internet game. The broader Milestone 4 map/policy matrix and prior
menu interaction evidence remain in the linked reports; this pass did not
repeat that entire matrix. The initial test selection exposed a harness
extraction dependency on the new overshield helper, which was corrected
before the final passing suite. No assertion or compatibility guard was
removed to obtain a pass.

Review also confirmed that the final changes leave `source/`, platform
renderer code, map loaders, host-policy code and network version untouched.
The normal build was restored after each diagnostic build. No new shared
hook, gameplay mutation, forced policy or diagnostic is shipped. The review
retained necessary bounded caches, checks and platform stubs instead of
removing them solely to reduce code size. Fifty-three local documentation
links and the newly named functions were checked.

Delivery is `GulchCE-dynamic-lighting-windows-152142a4.zip` under the primary
checkout's `build/playtest/`, with an adjacent `-PLAYTEST.txt` and `.sha256`.
The 13.4 MiB ZIP includes its own `PLAYTEST.txt`, stock configuration,
`BUILD.json`, source download links and licence notices; it contains no maps
or saves. Its SHA-256 is
`bce57d6b01ef3733c9f374257ba49070f1306eef04070d9e055cc0ecd724ff06`.

Evidence remains under the active checkout's `build/milestone5-*`,
`build/plasma-visual/*-m5-*` and `build/lighting-compatibility/m5-*`.
All owned processes exited. Runners removed staged maps/caches; final cleanup
removed approximately 60.6 MiB of test binaries/objects and 186.7 MiB of
redundant capture frames. Representative projectile images, all shield/menu/
network/transition captures, complete logs and manifests remain. All 167
archived test-save files (180.1 MiB uncompressed) passed SHA-256 verification.
Approximately 50.3 MiB of diagnostic evidence remains, besides the delivery
ZIP and required normal Windows build/symbols. Original maps, saves,
toolchains, other worktrees and earlier milestone evidence were preserved.
