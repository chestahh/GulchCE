# M0: saved-state restoration and client recovery

13 November 2026, local `investigate-coop-rewrite`, starting at `360857a59`.
This is characterization of the existing implementation, not M1 or a fix.
Read with the [19-domain inventory](coop_m0.md),
[readiness and transport probes](coop_m0_readiness_transport.md) and
[remaining M0 gates](coop_m0_followup.md#remaining-exit-gate).
The full co-op scope remains intact. Linux and Android runtime remain blocked:
no runner/device is available. The previous Android build blocker is separate.

The [subsequent domain-mutation pass](coop_m0_recovery_domains.md) extends
these observations to object presence/absence, changed inventory, attachments,
scale, primary ground ammunition, a visible paused timer, allegiance bits and
non-default MCC state. It records an additional attachment recovery failure
and preserves the remaining full-system obligations.

The [active-world continuation](coop_m0_active_world.md) adds BSP transfers,
selected door collision/state controls and active movement/AI/combat observations
with a bounded vehicle-seat restore. Its failed setups and unconfirmed natural
mission reports remain explicit; it does not complete the full-state oracle.

## Contract and observation boundary

A complete recovery must satisfy two obligations: the host restores the saved
world, and clients reconstruct that world while retaining current session
membership and policy. Peers agreeing with one another does not establish the
first obligation. Receipt of an ordered marker establishes neither obligation.

[coop_m0_recovery.py](../tools/investigations/coop_m0_recovery.py) generates
private diagnostic copies of seven translation units. The original production
sources are unchanged. Its fail-closed compiler launcher checks original and
generated hashes. The [fixture](../tools/investigations/coop_m0_recovery.c)
calls existing engine interfaces; it introduces no shipping feature or setting,
new wire format, alternative checkpoint implementation or map-specific fix.

Each hidden Windows run has two peers, private map/resource/profile copies,
unchanged campaign scripts and null rendering. The fixture waits for the
cinematic to end and both player units to exist, then performs this sequence:

1. Set state A: hidden HUD/help, one authored objective, a team navigation
   marker, player maximum body/shield vitality of 1/1, and one authored device
   group at 0.25. Freeze player input through the existing interface.
2. Three seconds later request `game_save_totally_unsafe`. Observe
   `network_coop_checkpoint_saved`, which production `game_state_save` calls
   only after a successful file write. A request alone is insufficient.
3. Change to B: visible HUD/help, another authored objective, remove the marker,
   player maxima 2/3, and device group 0.75.
4. Request `game_revert`, observe recovery, then request a second revert to
   the same checkpoint. The host clock remains monotonic by design.

The reducer rejects missing phases or a checkpoint overwritten between B and
the second recovery observation. It verifies that A-to-B changes were actually
observed. This deliberately unsafe diagnostic save tests the underlying
mechanism; it does not qualify ordinary checkpoint safety, pause-menu Revert,
Restart Mission or normal campaign progression.

The [reducer](../tools/investigations/coop_m0_recovery_measurements.py) compares
saved engine values with each peer, separately from comparing peers. It joins
private HUD/navigation/epoch observations by the capture's QPC time interval,
not by tick number alone. Objectives are canonical authored message indices,
not process addresses. Navigation includes logical ownership; local slot
indices are not equated between peers. Team-marker rows are compared because
both tested players belong to the same team.

Controlled values are exactly representable (0.25/0.75 and integer maxima),
so these comparisons use exact equality. Moving positions and raw clock/timer
origins are recorded but are not subjected to false exact-equality gates.
Snapshots from the two processes can be up to 1.1 seconds apart; the report
records actual skew. Recovery windows start two seconds after each revert
request and retain every observed mismatch, including after client readiness.
Instrumentation performs synchronous CSV writes: these are semantic recovery
observations, not production performance measurements.

Incomplete captures are rejected. When the session manifest explicitly records
controlled termination with neither early exit nor cleanup error, a single
unfinished final capture may be excluded and listed in the results. It is never
used as state evidence. This occurred once in the MCC baseline; the complete
save/change/two-revert sequence and earlier recovery windows were retained.

## Observed restoration

The [machine-readable results](coop_m0_recovery_results.json) retain each
case, selected before/after states, completed saves, epochs, peer disagreement,
reference checks and recovery-window counts. Raw logs remain under
`build/m0/recovery`, with maps and retail resources excluded from the archives.

The Xbox `a30`, CE `b30` and local MCC `a10` baseline runs reproduced three
failures through both reverts:

| State | Host after revert | Client after revert |
| --- | --- | --- |
| HUD/help visibility | Restored A | Restored A |
| Tested device group | Restored 0.25 | Restored 0.25 |
| Objective | Restored authored A | Retained authored B |
| Team navigation marker | Restored presence | Remained absent |
| Player maximum body/shield vitality | Retained 2/3 instead of 1/1 | Retained 2/3 instead of 1/1 |

The client reported ready at epochs 2 and 3 despite those failures. They
persisted through every sampled post-settling recovery frame, including about
39 seconds in the second Xbox recovery window and 30 seconds in the second CE
recovery window. The roster, player unit identities, weapon references, empty seat
relationships and BSP remained as saved in those windows. These unchanged
relationships are preservation observations, not changed-roster, boarding or
inventory-mutation qualification.

The Xbox run with the existing receive-delay setting of 50 ms per peer and
5% drop setting reproduced the same failures through both reverts. These are
configured engine receive impairments, not a measured public-network RTT or
independent transport-stall schedule. Its second client recovery window had
39 ready samples over 38 seconds, all retaining the wrong objective, absent
marker and wrong vitality maxima.

The MCC second recovery window contained 52 ready client samples over 51
seconds with the same three wrong values. The generated-only objective fault
control suppressed four applications; the observer measured no client
objective even in the saved-A window, whereas the ordinary Xbox control had
matching A objectives. Both controls still reported readiness. This validates
that objective agreement is measured from the engine, not inferred from
delivery or the host's outgoing cache.

All five sessions completed without early process exits. The unchanged
`net_compare` judge reported no problems or divergences in any of them; its
sampled player comparison does not cover these durable HUD/cache failures.
All selected peer samples were less than 0.4 seconds apart. Reference checks
found no absent player unit, object parent or inventory weapon in the selected
captures. Stable object comparisons retained four existing Xbox mismatches,
two CE mismatches and none in the MCC case, both before and after recovery.

### Source explanation and limits

Read [network_coop.c](../port/linux/game/network_coop.c), specifically
`network_coop_reset_recovery`, `players_vitality_keep`, `host_send_hud_state`
and `network_coop_reverted`. The host HUD cache and vitality override are
file-static state outside the checkpoint arena; recovery does not restore
them. `players_vitality_keep` reapplies the retained override. Objective resend
uses the host cache, and active navigation resend cannot reconstruct an entry
removed from that cache after saving.

The actual HUD objective and navigation state use `game_state_malloc` in
[hud_messaging.c](../source/interface/hud_messaging.c) and
[hud_nav_points.c](../source/interface/hud_nav_points.c). Thus host engine
state can restore while its outgoing cache still describes B. The runtime
captures distinguish these actual values from the host caches. A fix must
address state ownership and recovery, not special-case the tested maps.

The observer also records timer/presentation flags, allegiances, all enumerated
objects, tags, names, parents, positions, scales and vitality; unit weapons,
grenades and seats; device positions/power/groups; attachment and sound-loop
caches. It checks reference resolution and compares sampled stable ordinary
replicated object identities. Xbox and CE already have some named AI/parent
differences before the fixture save. Those observations are retained, not
silently excluded or asserted to be new checkpoint regressions. Parent/name
mapping and authored actor behavior require further qualification.

An empty attachment/loop table or an inactive timer is not successful active
state restoration. A host cache is not proof of audible or rendered state.
No observation here certifies camera placement, animations, cutscene crop,
audio playback, projectiles, AI decisions or MCC-specific extra fields.

## Full-system recovery coverage

This maps every original semantic contract to its recovery obligation. The
fixture above is one part of the complete oracle, not a replacement for it.

| Contract | Evidence and remaining recovery qualification |
| --- | --- |
| session | Two admitted peers and stable roster retained. Join/leave during restore, multiple local players, four players, dedicated zero-local-player hosts and current options versus saved options remain open. |
| transport | Existing real-loopback stream fault probes and epoch tests remain applicable. This pass adds actual game observations; the [sustained-recovery continuation](coop_m0_recovery_stress.md) adds replay of captured old-epoch object/inventory envelopes through the real dispatcher. Actual interrupted streams, reconnect and end-to-end browser/public-internet recovery remain open. |
| content | Separate Xbox, CE and MCC loader paths and unchanged original file hashes. Missing content, transfer cancellation and failed recovery load are not qualified by these successful loads. |
| objects | Actual IDs/tags/parents and reference resolution observed; earlier construction-failure probes remain negative baselines. The domain-mutation pass adds saved presence/post-save absence and scale restoration, plus a failed attachment restore. The [identity continuation](coop_m0_recovery_identity.md) adds normal-allocator weapon-slot reuse with different generations, definitions and owners. The [cross-type/fault continuation](coop_m0_recovery_faults.md) adds weapon-to-biped reuse and a controlled object-memory allocation refusal. The [sustained-recovery continuation](coop_m0_recovery_stress.md) adds real object-pool exhaustion through bounded reservations and release, with actual failed reconstruction. Naturally occurring workload exhaustion, other type pairs, repeated churn, header-allocation failure, arbitrary dependency order and region/permutation/animation restoration remain open. |
| inventory | The domain-mutation pass adds filled-to-empty and empty-to-filled mutations around a checkpoint. The [timer/ammunition continuation](coop_m0_recovery_timing.md) checks non-default primary counts in both players' held weapons. The [identity continuation](coop_m0_recovery_identity.md) checks replacement in a reused slot with different inventory ownership and owner metadata. The [cross-type/fault continuation](coop_m0_recovery_faults.md) observes a queued inventory while its weapon creation is withheld for 180 ticks, including premature readiness and eventual repair. The [sustained-recovery continuation](coop_m0_recovery_stress.md) adds two missing weapon dependencies and interruption by another epoch, plus extracted-production-function checks at the actual queue capacity and expiry boundary. A late extra-weapon/parent deviation in the CE control is retained without a recovery-cause claim. Real-game queue saturation, arbitrary dependency graphs, reload and player-driven pickup/replacement histories remain open. |
| movement | Earlier fixtures freeze input. The [active-world pass](coop_m0_active_world.md) observes Xbox/CE/MCC input and movement and restores a saved Xbox vehicle driver-seat relationship after exit. Prediction queues, small host teleports, correction history, vehicle handoff, standing riders and broad moving-state recovery remain open. |
| actors | The active-world pass samples active authored actors and inspects rendered A50 combat. Decisions, targets, control histories, death/resurrection and animation recovery still require independent outcome oracles. |
| combat | Reproduced stale maximum-vitality override. Active Xbox/CE/MCC shooting, dealt reports, deaths and sampled projectiles are now observed. Damage history, explosive/projectile outcomes and lifetime, melee, ammo/grenades and duplicate-effect suppression across restore remain open. |
| presentation | Actual HUD visibility/objective/navigation fields actively changed; objective/navigation fail. The domain-mutation pass adds visible paused timer/layout restoration. The [timer/ammunition continuation](coop_m0_recovery_timing.md) measures running countdown loss, warning-state changes and early expiry. Camera, timer visuals and campaign expiry actions, screen effects, titles, audio loops/absence, seek and rendered animation remain open. |
| events | Application-fault control is separate from network loss. Earlier sequence-gap/dependency/event-loss probes remain negative baselines. Every durable event needs explicit reconstructed presence/absence; transient actions need expiry rules. |
| scripts | Real authored scripts and save/revert APIs execute. The active-world pass stages the A50 lift tail/successor and C10 staggered placement, with a bare-switch unsupported-placement control. The [ordinary-entry pass](coop_m0_campaign_entry.md) completes A10's Normal opening tutorials, observes two natural checkpoints and both players traversing the cryo-room exit, and confirms a remote Confirm/Back gap with host-local positive controls. The earlier stationary probe did not establish a softlock. Natural-checkpoint restoration, broader progression, safe-save eligibility, suspended threads, full-roster gathering and interrupted actions remain open. |
| checkpoint | Successful disk-save callbacks and advancing recovery epochs observed. The [sustained-recovery continuation](coop_m0_recovery_stress.md) interrupts two pending weapon dependencies with another real revert and observes queue reset and tracked-state restoration. Full-state convergence still fails. Ordinary host menu controls, failed save/read, restart, interruption at other recovery stages and persistent profile ownership remain open. |
| scaling | Existing callback owners retained. Consumed enemy/pickup/vehicle budgets, held riders and deleted sources across changed-roster recovery need active checks. |
| policies | Existing host settings used. The [sustained-recovery pilot](coop_m0_recovery_stress.md) observes the third-revert-in-a-minute respawn guard; subsequent runs preserve it. Changed live policy versus checkpoint policy, AFK, friendly fire, collision, BSP-majority cooldown and dedicated ownership remain open. |
| social | No chat/voice/vote/link mutation in this fixture. Permission/session lifetime, in-flight operations and cleanup across recovery remain open; do not replay transient voice as saved state. |
| mcc | Local MCC campaign uses MCC-owned adapters. The domain-mutation pass exercises non-default extra grenade counts, speed and campaign effects/music gains. The [sustained-recovery pair](coop_m0_recovery_stress.md) pins its checkpoint through diagnostic save cancellation, so authored save scheduling is not qualified. Names and broader campaign/script/sidecar state remain open; numerical gains are not audio playback qualification. |
| camera | No camera protocol or policy change. GulchCE third-person activation/shoulder/ownership through recovery and the Crossplay path remain open. |
| multiplayer | No competitive or local-only implementation changed. Shared reset/replication preservation still requires competitive, solo and local split-screen recovery coverage. |
| world | The active-world pass restores selected A10/C10 open door values and their door-collision changes after closure; C10 also clears a sampled ray. A10's device-only probe has one unit/input disabled and its rays still hit other geometry. Actual traversal/progression, intermittent reported failures, other switches/runtime groups, broken surfaces, named-object deletion and attachment absence remain open. |

Known failures are useful M0 baselines and are not authorization to repair
them. The complete persistent-state oracle and full M0 exit gate remain open.
M1 is unstarted and nothing is published.

The [reported A50 BSP/grav-lift and A10/C10 door softlocks](coop_rewrite_plan.md#reported-bsp-transitions-and-scripted-door-softlocks)
add explicit pending cases across movement, world, scripts, policies and
checkpoint recovery. The reports concern ordinary progression too; no common
cause or recovery-specific cause has been established. Checkpoint tests must
respect the saved door/BSP state and subsequent authored progression, while
ordinary transition tests must establish actual traversal for every player.
The active-world pass reproduces unsupported placement after a bare BSP switch;
it does not reproduce the natural A50 lift or intermittent A10/C10 softlocks.
Bounded device controls do not establish successful mission traversal.
Any later fix must address the underlying co-op/engine behavior for all maps
using the affected semantics. These map names identify regression cases only;
map-specific overrides and bypassing authored progression are excluded.

## Reproduction and review

Generate with `python tools/investigations/coop_m0_recovery.py`; configure the
Windows diagnostic build with `--release --portable --pgo=off --lto=off` and
`--compiler-launcher=build\m0\recovery\generated\compile.cmd`, then build
`ninja windows`. Retain the executable, symbols, SDL library and manifest.
The session command manifests record exact map/profile paths and hashes,
binary hash and overrides. Set `M0_RECOVERY=1` only for the diagnostic run,
then use [coop_m0_sessions.py](../tools/investigations/coop_m0_sessions.py)
with `--seconds 90 --input none`, a fresh private output directory and the
appropriate `--family`, `--level` and `--extra-map`.

`M0_DROP_OBJECTIVE=1` is a generated-only negative control that discards
objective application after sequence consumption. It is explicitly not a
packet-loss simulation or an observed unmodified-production fault schedule.
Reduce with `python tools/investigations/coop_m0_recovery_measurements.py
<session-folder>`; the reducer also reads archived CSV evidence in place.
Never ship a diagnostic executable or the private retail fixtures.

Final self-review covers observation ownership, actual-save evidence, canonical
identities, missing-data rejection, unchanged peer-versus-saved-state criteria,
complete scope and archive reproducibility. The oracle tests deliberately
check agreeing-but-wrong peers, missing references, parent/identity mismatch,
reused ticks and truncated captures. Production checkpoint/epoch/timing/MCC
tests remain separate from actual game evidence. No production refactoring,
protocol bump or implementation platform matrix is claimed for diagnostic-only
tooling. Restore normal configure arguments after building.

The final focused suite passed **155 tests, with no failures or skips**:
the five observer tests plus `test_coop_checkpoint_recovery.py`,
`test_checkpoint_epoch.py`, `test_checkpoint_timing.py`,
`test_coop_checkpoint.py` and `test_mcc_checkpoint.py`. The Windows diagnostic
build succeeded. Final evidence verification re-reduced all five archives,
checked the actual mutations and epoch advances, source/generated/binary
hashes, original map/profile hashes, archive integrity and every member hash.
Python syntax, document links, named source routes and all 19 ledger entries
were checked. These checks validate the evidence, not the defective recovery.

Cleanup removed 10,754,379,218 bytes of private map/resource/cache/executable
copies and 231,577,269 bytes of temporary Windows build output. The 466 private
save/trace files (369,243,333 bytes) remain in five losslessly verified archives
totalling 40,512,289 bytes, with original paths and hashes recorded in adjacent
indexes. Required diagnostic binaries/symbols, logs, generated copies and
manifests remain. This pass retains about 110.8 MB; all M0 material is about
1.88 GB, including earlier useful captures/builds. Original maps and saves,
installed toolchains and the root checkout's ongoing changes were preserved.
No owned game process remains. Normal release/portable, PGO-off/LTO-off
configuration is restored. No implementation, M1, publication, PR or push
occurred.
