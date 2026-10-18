# Enhanced shields

Video Setup → Extra Effects → Shield Look selects Classic (the default) or
Enhanced. The setting is `display.shield_look`, overridden for one run by
`HALO_SHIELD_LOOK=classic` or `enhanced`. Invalid values use Classic.

Enhanced draws a fine surface lattice and a brighter silhouette during a hit,
moving electrical patches while depleted, and a shell that fades as the shield
recharges. It includes first-person arms when viewmodels are visible. Classic
uses the saved Arm Shields choice; Enhanced does not change it. Shield Color remains
effective, including dark colors. Shield Glow and Dynamic Lighting retain their
own settings and behavior.

This is an independently authored approximation using the supplied Halo
Campaign Evolved recordings as visual references. No code, shaders or assets
from that project are included. It does not change shield strength, damage,
simulation, map data, network messages or another player's settings. No network
version change is needed.

## Integration boundaries

`port/linux/game/shield_look.c` owns eligibility and snapshots the current unit's
shield state for a single draw. `port/linux/src/shield_look_gl.c` owns the draw
uniforms and procedural GLSL. There is no per-object history or allocated map
resource. Each plasma begin clears the preceding scope before setting draw
uniforms, and each shield draw clears it afterward. An unrelated plasma draw
cannot inherit it.

The feature requires a live shielded unit, its own plasma modifier shader and
the original unit animation-values pointer. First-person draws resolve the
held weapon's parent unit. Ordinary plasma materials, non-plasma modifiers,
unshielded units and dead or invisible objects retain their established paths.
Camouflage fades the overlay. These checks are independent of map family:
Xbox, Custom Edition and MCC retain their existing loading and conversion
paths, limits and ownership. No MCC adapter or map-family selection changes.

The existing sorted draw queue retains object identity and animation pointers.
Minimal shared hooks are needed to attach eligible arms, retain zero-flare
submissions and select a shader override for the duration of the draw:

- `first_person_weapons.c` attaches the eligible unit modifier to visible arms
  when Enhanced is selected. The existing Arm Shields branch is preserved.
- `rasterizer_xbox_models.c` submits depleted/recharging eligible shields even
  when the stock flare animation is zero.
- `rasterizer_xbox_plasma_energy.c` uses the existing queued object identity,
  keeps enhanced arm displacement close to the model, and brackets the geometry
  draw with the feature scope. All additions to `source/` have `port:` markers.
- The GL backend includes a separate fragment-shader variant in its cache key
  and uploads three feature uniforms only for eligible draws. The vertex
  translator exposes the original model position so the pattern follows the
  skinned surface. Classic fragments do not consume that extra varying; existing
  vertex calculations and Classic fragment generation remain unchanged.

The menu generator adds Extra Effects immediately below FOV and Viewmodels,
using the existing settings-screen layout, pending edits, Defaults, Cancel and
OK behavior. Root Video Setup rows use 26-pixel spacing to fit the extra submenu
above the existing help panel. Existing Dynamic Lighting entries remain intact.

## Reproducing checks

Run `pytest -q tools/test_shield_look.py tools/test_viewmodel_shield.py
tools/test_extra_effects_menu.py`. The tests execute the production eligibility
and scope code, real menu setting transactions, and generated production GLSL
450 and GLSL ES 300 on a hidden desktop GL context. They cover disabled and
invalid settings, unit/shader ownership, stale parents, unrelated plasma,
camouflage, death, recharge, live changes, dark tints and time-varying output.
The shader tests require built SDL3 and a usable GL driver; Linux runs require
the caller's virtual display. Shared Android and web build graphs are checked
for both modules, which require no new host imports.

For Windows visual checks, run `tools/shield_visual_build.py` in the configured
compiler environment. It temporarily adds the capture-only fixture, records
source/executable hashes, and restores the production sources and executable
in a `finally` block. `tools/shield_visual_run.py --data-root <game-data>
--label <unique-name> --look enhanced` uses a hidden window and isolated copied
maps and saves. The fixture equips a pistol, creates a target, applies a hit,
depletes the test shields and allows normal recharge. `--toggle` switches to
Classic and back during that sequence. This fixture never ships in the game.
Copied maps and generated map caches are removed after each run; saves, logs,
manifests and selected captures remain available as evidence.

## Validation, 2026-10-19

The Windows portable release build passes. Thirty focused pytest checks pass
across shield eligibility, first-person integration, menu transactions, MCC
environment shaders and visibility readback, including actual desktop and ES
shader compilation/linking/rendering. The separate HUD shader regression passes
15,360 ammunition renders. Menu screenshots were checked at 640×480 and
1280×720; the Extra Effects controls, help and buttons use the standard panels.

Hidden Xbox Blood Gulch captures show the body hit shell, depleted electrical
patches, recharge fade and first-person arms. A separate arms-only capture
switches to Classic during depletion, then back to Enhanced. The Classic
captures retain the existing effect. The capture fixture's earlier errors and
superseded captures are retained as diagnostics, not counted as successful
validation. Source/executable hashes accompany the final captures under
`build/enhanced-shields`.

A two-instance loopback Slayer check passed with explicit per-process Enhanced
and Classic overrides, Arm Shields off, and the production executable. Both
players fired 11 scripted shots and depleted shields; the host accepted 11
damage reports with zero rejected, and the client replayed 30 events. Both
exited normally using unchanged protocol 29. An earlier runner wrote settings
to the wrong location; that result is explicitly invalid for mixed-mode
validation. Native configuration is beside the executable, so visual runners
now copy their executable into each isolated run directory.

An enabled Xbox → MCC BetterWithFriends b40 → Xbox loading round trip was
checked with logs and screenshots. MCC tinyhalo/ring_mp also loaded and rendered
through hit/depletion/recharge tests without shader errors. Its test units have
no modifier shader (`NONE`), confirmed at runtime, so they correctly remain
unchanged; this is a negative eligibility check, not positive visual coverage
of Enhanced on MCC shields. The b40 capture did not reach the fixture's playable
state and supplies no shield visual evidence. Custom Edition Death Island was
refused for missing bitmaps.map, sounds.map and loc.map. CE gameplay and recovery
after that failed load were not verified.

Android and web toolchains are unavailable locally (`ninja android` and
`ninja web` have no configured targets). Their source inclusion is checked, but
this does not replace platform builds or device/browser runtime tests. The
existing web pytest suite has six Windows environment failures (five path
separator expectations and one missing pthread.h); these are not shield passes.
Linux runtime, Android devices, browser rendering, internet/co-op play,
split-screen appearance and the full range of map-authored shield materials
remain outside this validation. The renderer had an independent code review;
the primary reviewed integration and captured output.
