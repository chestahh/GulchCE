# M0: BSP travel, devices and active gameplay

Local investigation from `637157de1` on `investigate-coop-rewrite`,
14 November 2026. This continues the [sustained recovery baseline](coop_m0_recovery_stress.md).
The [full 19-domain ledger](coop_m0_recovery.md#full-system-recovery-coverage)
remains the scope. **M0 remains open; M1 has not started. Nothing is published.**
This pass adds diagnostic tooling and evidence, not a co-op fix or rewrite.

Linux runtime is **blocked: no runner**. Android runtime is **blocked: no
device/emulator**; the earlier Windows shell/CMake build blocker is separate.
These Windows loopback observations do not qualify browser/public-internet
recovery, four-player play, mobile performance, or full campaigns. Xbox,
Custom Edition and MCC continue through their separate existing loaders.

## What the observations can establish

[coop_m0_world.py](../tools/investigations/coop_m0_world.py) generates eleven
hashed source copies. Original production files remain untouched. The private
[observer](../tools/investigations/coop_m0_world.c) extends the existing recovery
capture with 10 Hz object poses, unit controls/seats, vitality, BSP indices,
device position/power and player floor rays. It records actual BSP switching,
script teleport requests, teleport results and device-group application.
It retains loaded script metadata locally for inspection; retail script dumps
and original maps are not committed or distributed.

Every session uses at most two hidden processes with private maps and saves.
Rendered cases use the existing screenshot request API, capped at 24 captures
per peer. Other cases use null rendering. Audio is disabled throughout.
These heavy observations are not performance-budget measurements.

The host fixture normally waits for two non-NONE player units and the end of a
cinematic. Its trusted local schedule uses wall time, so checkpoint clock
restamping cannot replay schedule entries. Commands use the existing script
compiler. The later direct controls call the same engine save, revert,
teleport and device APIs without compiling a new script after the checkpoint.
Vehicle entry/exit uses the existing `network_test_vehicle` helper. Intro
skipping, where requested, casts one vote through the existing co-op vote API
on each peer; it does not change the vote threshold. A device-only fixture
may explicitly admit one unit, which is not two-player gameplay coverage.

Device rays cross the selected fixture object's origin at three heights on
two axes, using the living-biped collision flags. A useful closed/open control
requires an observed collision with that device when closed and a changed
collision result when open. All-zero rays do not qualify a door. Rays do not
prove a player walked through, activated the next volume or completed a mission.
Content names select test fixtures only; no shipping behavior selects maps,
checksums, names, tag paths or asset fingerprints.

[coop_m0_world_reduce.py](../tools/investigations/coop_m0_world_reduce.py)
keeps frame occurrences and full object handles, checks boundaries and required
fields, excludes and flags incomplete final frames, and retains absent devices
as missing evidence. Its before/after samples use the common local QPC clock,
not an assumption that matching tick numbers identify the same recovery epoch.
Checkpoint epochs/readiness are read separately from the distributed observer.
The host's readiness field is not a client-ready verdict. Earlier floor samples
read an undefined object-index union member for structure hits; the reducer
explicitly discards that field, and the final observer guards it by hit type.
Position extents are sampled ranges, not travelled distance or an AI oracle.
Short-lived projectiles can occur between samples, so unequal sample counts
alone do not establish replication loss.

## Source findings and required general contracts

- [network_distributed.c](../port/linux/game/network_distributed.c):
  `distributed_send_structure_bsp` sends a changed BSP reliably and refreshes
  it through the ordinary stream. `distributed_handle_structure_bsp` validates
  the index and queues `main_switch_structure_bsp` on a co-op client. The host
  orders BSP transmission before player transmission. This ordering is not
  proof of an atomic BSP/pose/collision transition on every transport.
- [scenario.c](../source/scenario/scenario.c):
  `scenario_switch_structure_bsp` disconnects, loads and reconnects the world.
  The registered script command is `switch_bsp`; it supplies an index, not a
  player destination. A bare switch is distinct from a scripted transfer.
- [hs_library_external.c](../source/hs/hs_library_external.c) and
  [players.c](../source/game/players.c): `hs_object_teleport` reaches
  `player_teleport`, including its placement validation, and the co-op follower
  helper. The test records rejection rather than treating a command request
  as a successful move.
- [network_coop.c](../port/linux/game/network_coop.c): device-group change
  delivery already has rotating refresh and client snapping for a missed
  change. `network_coop_handle_object_names` checks the current BSP and requires
  two consecutive differences before recreating/deleting a named object.
  MCC names retain their separate owning adapter. Intermittent doors cannot
  be diagnosed merely as an event sent too few times.

The future gates remain semantic: transition ownership, valid player placement,
loaded collision, late/staggered peers, current device identity, desired and
actual state, and post-restore traversal/progression. Any fix must benefit a
new valid map with the same semantics without registration. A50, A10 and C10
are regression examples, not implementation selectors. No evidence here
authorizes replacing legacy Xbox/CE paths or absorbing MCC compatibility into
the shared implementation.

## Runtime record

The [machine-readable record](coop_m0_active_world_results.json) lists all 20
sessions, including discovery runs, unsuccessful setups and controls. A runner
exit status or a complete archive is not itself a gameplay pass.

### A50: distinguish the transfer from a bare BSP command

The staged lift-tail cases execute the existing cinematic, switch to BSP 1,
teleport both named players to the authored ship flags, then wake the existing
gravity-room successor. They do not traverse the earlier combat areas or prove
that the natural mission triggers reach that sequence correctly.

Both ordinary and 50 ms receive-delay/5% loss cases reached BSP 1, cluster 20,
with a floor at approximately Z=-21.697 for both players. Their first captured
BSP-1 frames already contained both players above that floor. The actual client
BSP-load completion followed the host by about 25 ms and 70 ms respectively.
Those are two local observations, not latency budgets. Host and client 1280x960
captures were inspected in the ship interior. The reported natural grav-lift
failure was **not reproduced** by these staged transfers.

The bare `(switch_bsp 1)` control did reproduce unsupported placement: both
peers retained their outdoor coordinates around X=31, Y=-33, Z=87, with cluster
NONE and no floor-ray hit in the newly loaded BSP. It did not execute the lift's
destination teleports. This establishes the bare command's outcome, not a shared
root cause with the reported authored sequence. No automatic destination or
map-specific repair is proposed.

The active gravity-room successors also killed the stationary players and
triggered ordinary recovery: two later epoch changes in each transfer case.
The explicit-kill recovery run recorded six epoch changes and the existing
third-revert respawn guard. Its repeated deaths are not a clean isolated pose
restore oracle. The direct-control continuation pauses AI thinking before
saving, moves one player through the existing teleport API, and requests revert;
its isolation must not be mistaken for active-combat recovery qualification.

The single three-unit move was accepted by `player_teleport` but returned to
its old coordinates before the requested revert. That run cannot qualify pose
restoration. The distributed implementation's 3.5-unit host acceptance and
3.0-unit local correction tolerances are a relevant source lead, not a proven
causal diagnosis. The follow-up applies two successive moves before observing
the displaced state and reverting. The resulting approximately six-unit offset
persists on both peers until the request; one new recovery epoch restores the
saved ship positions and floor contact on both. AI thinking is paused in this
control. It qualifies that bounded post-arrival pose restore, not the entire
checkpoint world or prediction history.

### Doors: state, collision and recovery are separate observations

`c10-door-direct` stages the control-room BSP and teleports the second player
ten seconds after the first, under 50 ms receive delay and 5% loss. Both
teleports succeed; the delayed player's temporary out-of-BSP placement is
deliberately induced by this fixture. Both peers ultimately have valid floor
contact. This is not a natural staggered mission traversal.

For `control_door_a`, both peers observe position 0 and a collision with that
door on the low forward ray; position 1 clears that ray. The host saves open,
closes the door, then reverts. Both peers restore position 1 and the clear ray,
with a new recovery epoch. Other rays still hit surrounding geometry. Rendered
host/client room placement was inspected; no player walks through the opening.

`a10-cryo-direct` uses `cryo_explosion_door_3` in loaded BSP 0, also with delay
and loss. Power/position change from 0/0 to 1/0, then 1/1 on both peers.
Closing after the save returns the door's collision on the low/middle forward
rays; revert restores 1/1 and removes those door hits. The rays still hit other
geometry, so this establishes the selected door's collision change, **not a
clear passage**. This is the explicit one-unit, input-disabled device probe,
not two-player A10 gameplay. Neither controlled door case reproduced the
reported intermittent client-only closed door.

### Active movement, actors and combat

| Local content/run | Duration after two-peer log admission | Host non-cinematic actor identities sampled | Host projectile identities sampled | Highest host dealt-report counter |
| --- | --- | --- | --- | --- |
| Xbox A30 | 100 s | 5 | 90 | 44 |
| CE B30, 50 ms delay / 5% loss | 100 s | 99 | 1,626 | 45 |
| MCC local A10 package | 160 s | 65 | 263 | 167 |

These runs use the existing bot input, shooting, kill, vehicle and pickup test
controls. They observed nonzero throttle/control samples, moving units,
projectiles, damage reports and deaths/respawns on both peers. The unchanged
`net_compare` judge reported no divergences/problems for these three runs;
its raw position/aim/state mismatches and correction statistics remain retained.
That judge does not validate AI targets/decisions, explosive outcomes, every
projectile lifetime, melee, animation, event uniqueness or recovery of all
those states. The dealt value counts reports, not damage units or unique kills.

None of those three scheduled boarding attempts logged a successful driver
entry. Vehicle motion/brief seating samples include authored activity and do not
prove player driving. A separate controlled vehicle case creates an existing
authored vehicle through the script API, then uses the existing test helper to
enter and exit its driver seat around a checkpoint.

`xbox-vehicle-direct` successfully enters vehicle `e4d5000d`, seat 0, saves the
seated state, exits, and restores the same parent/seat on both peers after
revert. The trace includes nonzero controls and moving vehicle coordinates.
Player poses while seated are parent-relative; they are not compared as world
coordinates. This qualifies the bounded seat restore, not vehicle driver
handoff, standing riders, sustained driving or a rendered vehicle playtest.

### Setup failures and limits retained as evidence

- The first lift pilot could not read an external absolute plan path. It was
  stopped and excluded; subsequent runs copy the plan into each private data
  directory. Its accidentally frequent screenshots were archived and verified,
  then replaced by bounded capture requests.
- The first working lift tail omitted its successor. It reached the ship but
  retained a magenta fade. A late attempt to append the successor arrived after
  the run ended; the retained fixture note records that fact. The corrected
  transfer cases explicitly wake the successor. The pilot is not evidence of
  a product fade bug or completed mission progression.
- The unskipped Xbox A10 fast-network-entry case never reached two player units
  within 300 seconds. A subsequent ordinary skip-vote case also retained one
  unit in a cryotube with input disabled. These are observed failures of this
  entry path, not a claim that all A10 launch paths are broken. Neither qualifies
  normal intro completion or two-player door traversal.
- The first A10 selected door was outside the final loaded BSP. The first C10
  control-room probe selected BSP 1 and both teleports were rejected. Their
  all-zero collision rays cannot qualify passage. Corrected fixtures select
  loaded cryo geometry and C10 BSP 2 respectively. These are fixture corrections,
  not game fixes or alterations to retail maps.

Natural A50 progression, the precise intermittent A10/C10 softlocks, player
walking through restored doors, subsequent trigger activation, four-player
gathering and broad map-family recovery remain open. The user has not supplied
the exact A10/C10 door/section or content family. The examples remain mandatory
regression cases even when a bounded device control succeeds.

## Reproduction, review and evidence

The source baseline and each binary hash are pinned in local manifests under
`build/m0/active-world`. The `cases` folder, command JSON, private session runner
and `run_case.py` retain the exact schedules, environment and original input
hashes. Source-copy manifests and build archives distinguish discovery, lift,
door-ray and direct-control variants. Do not silently substitute the final
binary when reproducing an earlier pilot.

Generate with `python tools/investigations/coop_m0_world.py`; configure with
`--release --portable --pgo=off --lto=off` and
`--compiler-launcher=build\m0\active-world\generated\compile.cmd`; build Windows.
Reduce a retained case with
`python tools/investigations/coop_m0_world_reduce.py build/m0/active-world/<case>`.
The reducer reads loose evidence or the case's verified archive directly.
The local runner requires a new output directory and the private Windows build
environment. These diagnostic executables must not be shipped.

Final self-review covered the full observer/generator/reducer changes, actual
engine callers, fixture activation/lifetime, source-copy isolation, test oracles,
all case exclusions and the combined M0 ledger/plan. Review separated collision
with a door from an unobstructed ray, parent-relative poses from world positions,
and the transient three-unit move from a persistent recovery mutation. It also
removed undefined structure-hit object-index data from interpretation and kept
the incomplete-shutdown markers. No production refactor was needed or performed.

After that review, **116 focused tests passed**: the eight evidence-parser
cases, script-player/follower tests including negative controls, vehicle tests,
scripted player poses, checkpoint/recovery and checkpoint epochs. The initial
run had 104 passes and 12 Windows compilation failures from `strlwr` deprecation
under `-Werror`. A private compiler wrapper adds the Windows CRT's
`_CRT_NONSTDC_NO_DEPRECATE`; no extracted engine function or test assertion was
changed. The final combined run and JUnit record are retained. Windows diagnostic
variants built successfully; this does not claim new Web/Android/Linux runtime
coverage. Python syntax, document links, named source routes and diff scope were
checked. No production protocol/default changed.

The final integrity audit verified **13 original map/resource/profile inputs**,
all eleven final original/generated source hashes, and **25 archives containing
2,573 members** against their size/SHA-256 indexes. Every completed case retains
its private saves and useful traces; build archives retain the earlier exact
binaries/symbols/copies. The pilot capture archive alone is about 245.9 MB and
preserves the failed run. It is retained evidence, not a distributable asset pack.

Cleanup removed about **41.19 GB** of private copied maps/caches, duplicate
binaries, temporary build/dependency outputs and test scratch files. Original
maps/saves, the installed toolchains and the root checkout's 20 ongoing paths
were preserved. About **1.54 GB** remains for this pass, including the required
final diagnostic binary and archives; all retained M0 material is about
**4.23 GB**. These sizes use decimal GB and exclude later small report changes.
No owned game process remains. The normal release/portable, PGO-off/LTO-off
configuration was restored. The investigation branch remains local and
unpublished; the full M0 gate is still open.
