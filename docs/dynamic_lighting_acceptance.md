# Dynamic lighting visual acceptance

This records milestone 2 on `dynamic-lighting-test`, based on `7c1309bd`,
on 25 October 2026. All eight settings remain off by default. CStencil and
Modern are outside this milestone. Stock effects, simulation, network protocol,
tag eligibility and Xbox/Custom Edition/MCC loading paths are unchanged.

## Findings and changes

The held Needler's broad downward fade left a coloured patch around a crouched
player's feet. The analytical fade alone did not describe its rendered reach:
linear filtering between the 16-cube texture's samples extends the transition,
especially in the larger specular volume. `needler_falloff_factor` in
[ammo_counter_falloff.c](../port/linux/game/ammo_counter_falloff.c) now fades
between 0.06 and 0.12 of the radius below the source, instead of 0.06 and 0.24.
At the stock 1.5 radius, the analytical transition spans 0.09–0.18 world units.
Horizontal/upward spill, the full-strength region immediately below the
needles, radius, colour, brightness and crystal emission remain unchanged.
This changes one feature-owned tuning constant, adding no allocation, raycast,
texture upload or work per frame. AR and shield policies are unaffected.

The [visibility regression](../tools/harness/tests/plasma_occlusion.c) samples
between texture slices at the observed crouched-player and short-Grunt heights,
with collision occlusion on and off and a doubled specular radius. It checks
no diffuse foot pool at a 0.38-unit source height, no specular pool at 0.25 or
0.38 units, faint near-contact diffuse spill at 0.25 units, and retained bright
spill immediately beneath the needles. This remains a filtered approximation;
it is not a floor plane or a geometric shadow of the holder. Very low or unusual
poses and custom light radii still require judgement.

The Light Occlusion help incorrectly implied it controlled Warthog blocking.
The existing `enabled` and `field_for` policy in
[plasma_occlusion.c](../port/linux/game/plasma_occlusion.c) makes muzzle blocking
automatic with muzzle lighting. The independent occlusion toggle controls
projectile and emitter lights. The wording is corrected in
[port_settings.py](../tools/port_settings.py), and its generated Dynamic Lighting
XML was regenerated; other screens are unchanged. No default or rendering
policy was changed to accommodate the wording.

## Capture method

The hidden Windows portable release runs use OpenGL 4.5 on an NVIDIA RTX 5070 Ti,
640x480, at 30, 60 and 120 FPS. At most two game instances run concurrently,
with separate data/save folders. Screenshots and render-time diagnostics are
both examined; a successful load or selected-light counter alone is not a
visual pass. Timings include capture readback and are bounded fixture averages,
not general performance guarantees.

[plasma_visual_build.py](../tools/plasma_visual_build.py) temporarily installs
[the capture fixture](../tools/fixtures/plasma_lighting_capture.c), then restores
production sources and rebuilds the normal executable. The normal game contains
none of its forced controls, spawns, camera override or instrumentation.
[plasma_visual_run.py](../tools/plasma_visual_run.py) supplies isolated runs:

```sh
python tools/plasma_visual_build.py --output-dir build/acceptance-capture
python tools/plasma_visual_run.py --data-root <game-data> --capture-dir build/acceptance-capture --fps 60 --enabled 1 --label crouch --emitter needler-player --grounded --crouch --face-camera --hide-viewmodel --occlusion 1
python tools/plasma_visual_run.py --data-root <game-data> --capture-dir build/acceptance-capture --fps 60 --enabled 1 --label grunt --map c40 --emitter needler-grunt --grounded --face-camera --hide-viewmodel --occlusion 1
python tools/plasma_visual_run.py --data-root <game-data> --capture-dir build/acceptance-capture --fps 60 --enabled 1 --label turret --muzzle warthog --exterior --occlusion 1
```

Use unique labels per run, repeat at each cap, and compare `--enabled 0`.
Grounded actors are placed with a structure collision trace. The player-model
fixture enters the normal combat stance, crouches using persistent unit controls,
then stands again; logged crouch samples must be 1, 1, 0. This is a rendered
player-model pose test, not a remote player's movement test. The exterior view
keeps the Warthog and ground visible while the seated player fires and aims
forward, sideways and backward; its actual muzzle-marker directions are logged.
Campaign skip input stops when the cinematic ends, avoiding accidental jumps.

Early setup attempts with an alert, non-crouching actor, a poorly framed Grunt,
and an unguarded pre-initialization cinematic query are excluded. They exposed
test-fixture defects, not production-game crashes. The corrected fixture guards
the cinematic state, frames the grounded holder and verifies the crouch cycle.
Earlier gunner-camera captures are supplementary; exterior captures provide
the chassis-blocking evidence.

## Scope of acceptance

These are controlled stock Xbox-map scenes: Blood Gulch for crouched players,
first-person weapons and the turret; c40 for the Grunt and shields; b40 for the
Marine's AR. Both blue and gold Jackal shields and the stationary shield are
driven through damage, depletion and recovery using the engine's damage state.
Their evaluated A-out colour animation and the emitted RGB values are logged.
This establishes colour synchronization, not natural combat damage timing.

The exact stock held-Needler tag remains `weapons\needler\needler`.
This milestone does not add CE `mp_needler` or arbitrary custom weapon aliases.
Map-family transition and networking checks, where recorded below, are lifecycle
and smoke checks; they do not establish full MCC campaign, cross-platform
internet play, split-screen or every-GPU visual compatibility.

## Results

Thirty-nine accepted visual runs passed their activation and 5% frame-cap
checks. Their measured averages span 30.00–30.06, 59.41–60.04 and
119.78–120.04 FPS. Representative frames at each rate were visually inspected:

| Case | Coverage | Observation |
| --- | --- | --- |
| Needler player model | Crouch/stand at 30/60/120; disabled 60; occlusion off 60 | Broad foot patch removed; weapon and upper-body spill retained |
| Needler Grunt | Grounded c40 at 30/60/120; disabled 60; occlusion off 60 | Crystal/body illumination retained without a broad floor pool |
| First-person Needler | 30/60/120; disabled 60 | Crystal emission and nearby weapon spill retained |
| AR | First person and b40 Marine at 30/60/120; disabled 60 | Screen/weapon and upper-body illumination retained; no new floor pool |
| Shields | Blue/gold Jackal and stationary at 30/60/120; disabled 60 | Added spill follows red damage, depletion and restored base colour |
| Warthog | Exterior forward/side/rear aim at 30/60/120; disabled 60; separate occlusion toggle off 60 | Chassis-covered ground stays dark; exposed ground receives warm pulses |

Nine shield sequences also passed a runtime RGB check: the damage phase's
emitted colour becomes over 97% red while the shader's evaluated damage
function approaches one. Warthog marker logs confirm three distinct actual
turret directions; the added warm spill is visible in enabled/disabled pairs.
Small collision-edge leakage remains possible; these are not exact mesh shadows.

The AR/shield captures use the unchanged policies from the initial matrix.
The revised Needler, exterior Warthog and menu captures use the final build.
The earlier Needler results that exposed the foot patch are excluded from the
accepted count. Source manifests identify both versions.

Twenty normal-executable menu cases passed: fresh defaults; all eight on/off
and saved; fresh-process reloads of both; Cancel button and controller Back;
Defaults then Cancel; Defaults then OK; unchanged OK; eight independent
one-toggle saves; Video Setup entry/return; and 1280x720 layout. Configuration
values and logs were checked, and captures show loaded/pending values and the
corrected help. The usual 640x480 layout was inspected too. These use scripted
controller navigation through the real menu, not a mock UI or physical-device
coverage of every input method.

Retained local evidence is under `build/plasma-visual/*-m2-accept-*`,
`*-m2-verified-*`, `build/m2-menu/`, `build/m2-accepted-visual-results.json`,
`build/m2-final-results.json` and `build/m2-shield-colours.json` in the worktree.
The `m2-fix-*` and `m2-final-rates-*` comparison sheets show the correction and
frame-rate comparisons. Rejected/setup runs are retained separately with their
logs and are not counted as acceptance.

The final combined review covered the feature mask, diffuse/model/specular
consumers, disabled fallback, texture reuse, generated help, fixture lifecycle,
regressions and documentation. No new shared-engine integration or broader
refactoring was needed. The final Windows portable release build passed;
97 focused lighting/menu/XML/settings tests passed, including the deliberate
old-fade negative control. Light-storage checks passed in ordinary and full-LTO
COFF builds and detected deliberate legacy pointer corruption. Local Android
and web targets are unavailable in this Windows configuration; CI supplies
platform build coverage, not runtime visual coverage.

Xbox c40 → MCC Mercury Falling → CE Death Island → Xbox Blood Gulch completed
with lighting off at 30 FPS and on at 120 FPS. Each run exited normally, with
all three transition commands, a confirmed MCC load, CE model conversion and
the final Xbox load, and no exception. A normal-build two-peer Windows Blood
Gulch smoke test used all eight options on for the host and off for the client:
36/32 two-player snapshots, eight scripted shooting events each, normal exits,
and no logged assertions, exceptions or shader failures. These establish
bounded lifecycle and mixed-setting session operation, not full simulation
equivalence or internet/browser crossplay. No packet or simulation agreement
changed, so the network version is unchanged.

Copied maps, generated map caches and capture-only executables were removed.
Selected PNGs, contact sheets and logs remain; redundant captures were trimmed.
The 1,205 inactive test-save files (about 1.86 GiB loose) were archived losslessly
in `build/m2-inactive-saves.zip` (about 5.2 MiB), with original relative paths
and SHA-256 hashes in its manifest. Every archive member was verified before
removing its loose copy. Roughly 300 MiB of evidence remains, including comparison
sheets. Original maps, user saves, other assignments and installed toolchains
were preserved. See `build/m2-cleanup.json` for the bounded cleanup totals.

## Shield Glow and animated Overshield addition

The optional Shield Glow now uses the shared soft world/model lighting path,
selected Shield Look and authored colour channels. Animated Overshield supplies
one shared layer hue and charge coverage to the surface, arms and emitted light.
The light rises with the luminous body, follows its rendered pose, and releases
smoothly instead of changing reach with every hit. Eight lights are selected by
contribution and distance, with an incumbent bonus to limit selection churn.
Light Occlusion uses the existing bounded collision budget. The borrowed light
template's flags, including no-specular, are preserved.

The implementation review covered ownership, selection, repeated render views,
map/disable cleanup, shader colour evaluation and host policy. Overshield model
caches and queued arm identities now reset explicitly; duplicate activation
checks were removed. Feature logic remains in its dedicated modules. The only
new engine hooks are shield radius bounds and shutdown reset in object_lights.c,
where private light storage requires integration. Existing map loaders and
network messages are unchanged. See the [shield lighting reading guide](reading/shield_lighting.md).

Shield Glow and Glow Intensity moved to Dynamic Lighting without changing their
saved keys. Animated Overshield remains independent in Extra Effects. Both
switches remain off by default. The host's existing shield-effects policy still
overrides local shield preferences unless it delegates to PLAYERS' OWN.

The final focused run passed 165 tests; four reference-map tests were skipped.
This includes the actual shield-light module in a synthetic world, selected
colour channels, camouflage, green/pink layer agreement, 30/60/120 Hz release
equivalence, bounded selection, host overrides and cleanup. Windows portable
release builds and ordinary/full-LTO light-storage checks passed. The latter
also detected the deliberate legacy pointer-corruption negative control.

Thirteen additional real-menu cases passed: fresh defaults, all nine toggles
and Light Show saved, fresh-process on/off reloads, Cancel button, Back,
Defaults/Cancel, Defaults/OK, unchanged OK, Shield Glow alone, intensity alone,
and 1280x720 layout. Both 640x480 and 1280x720 captures were inspected. Existing
eight-option acceptance above remains evidence for those independent controls;
the new cases exercise their combined operation with the two added rows.

Six local web tests could not pass in this Windows environment: five assume
Unix path separators and one requires pthread.h. The remaining web checks
passed. Android and web Ninja targets are unavailable in this configuration;
cross-platform CI is required separately. These limitations do not count as
successful platform validation.

The Xbox c40 -> MCC Mercury Falling -> CE Death Island -> Xbox Blood Gulch
transition was repeated with all lighting off at 30 FPS and on at 120 FPS.
Both completed all three commands, logged the MCC load and CE model conversion,
returned to the final Xbox map and exited without exceptions. A normal-build
two-peer Windows smoke test retained 36/32 two-player snapshots and eight
scripted shooting events per peer, with no logged assertions, exceptions or
shader failures. Its host requested glow plus overshield while both peers'
local shield preferences were off; other dynamic lights were on for the host
and off for the client. These are bounded session/lifecycle checks, supplemented
by the policy harness, not internet crossplay or simulation-equivalence proof.
Initial sandboxed attempts could not connect over loopback and are excluded.

Nineteen shield capture runs passed the activation and 5% frame-cap checks:
ordinary classic hit/recharge, classic overshield charge and enhanced overshield
charge at 30/60/120 FPS; enhanced first-person arms at all three caps; classic
and enhanced overshields with added lighting off at 60; enhanced ordinary hits,
crouching, Light Show, occlusion off, and an Elite at 60. Contact sheets and
representative frames were inspected. Green/pink surface and light layers agree,
the radial source follows crouching, ordinary hits release, and disabling glow
leaves the animated overshield surface intact. Off runs submitted no added lights
or occlusion updates. The unoccluded run retained light with zero occlusion
updates. The c40 Elite run exercised world specular draws; carousel's borrowed
template suppresses them, as authored. None of these checks establishes exact
mesh shadows or appearance on every map and GPU.

The initial ordinary-hit fixture did not clear its damage-decay timer. Those
four captures are superseded by corrected hit/recharge runs and are excluded
from the nineteen accepted cases. Charge and first-person captures remain valid.
The final code differs from their manifest only by removing redundant overshield
activation checks; supplemental captures, transition runs and final harnesses
use the resulting implementation. Frame-cap averages include screenshot overhead
and initial shader compilation, and are not a general performance guarantee.

Local evidence is in `build/shield-visual-results.json`,
`build/shield-decay-results.json`, `build/shield-supplemental-results.json`,
`build/plasma-visual/*shield-*`, `build/shield-menu/`,
`build/shield-network-verified/` and `build/shield-transitions-verified/`.
The corrected 60 FPS classic hit case is `60-1-shield-decay-normal-classic`.

Cleanup removed copied maps, generated map caches, capture-only binaries and
temporary test executables. The 870 inactive test-save files (about 900 MiB
loose) were archived in `build/shield-inactive-saves.zip` (about 1.8 MiB), with
their original relative paths and SHA-256 hashes. All members were verified
before removing loose copies. About 313 MiB of capture/log evidence remains;
`build/shield-cleanup.json` records the totals. Original maps, user saves,
previous playtests and other assignments were preserved.
