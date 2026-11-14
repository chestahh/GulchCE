# M0: sustained pressure, dependencies and interrupted recovery

Local investigation from `8b33d7349` on `investigate-coop-rewrite`, 14 November
2026. This continues the [cross-type/fault baseline](coop_m0_recovery_faults.md).
The [full 19-domain ledger](coop_m0_recovery.md#full-system-recovery-coverage)
remains the co-op scope and exit gate. This pass does not stand in for active
AI/combat, movement/vehicles, world/device, presentation, session, policy,
social, content, transport, MCC, camera or multiplayer recovery obligations.
M0 remains open. No production fix, M1 work, protocol change or publication
is authorized by these experiments.

Linux runtime remains **blocked: no runner**. Android runtime remains
**blocked: no device/emulator**; the earlier Windows shell/CMake Android build
blocker is separate. Native Windows loopback with null rendering and disabled
audio does not qualify browser/public-internet recovery, other platforms or
rendered/audible behavior. No production source or original map/save is edited.

## Fixture boundaries and source ownership

[coop_m0_recovery_stress.py](../tools/investigations/coop_m0_recovery_stress.py)
generates eight hashed diagnostic source copies. It extends the existing
observer without changing the earlier generated modes. Each private process
handles one session; no more than two hidden game instances run concurrently.
Each family pair uses one diagnostic binary for `M0_RECOVERY_DOMAINS=control`
and `stress`. The later MCC pair uses the explicitly pinned-checkpoint variant
described below; the earlier Xbox/CE build is preserved separately.

The [host fixture](../tools/investigations/coop_m0_stress_host.c) uses normal
object/inventory interfaces to give each player one saved weapon, with 20 and
24 primary rounds. It freezes input, saves through the real disk-save callback,
deletes the two weapons before three reverts, and requests a fourth revert
while the client still has unresolved dependencies. Actual absence on both
peers must be observed before each of the first three reverts. The fourth
interrupts recovery without requiring an intervening host mutation.

The schedule after activation is: create at 0 ticks, save at 90, delete at
180, revert at 270; delete at 1620, revert at 1710; delete at 3510, revert at
3600; interrupt at 3870 and capture at 4170. Runs observe for 210 seconds after
two-peer admission. The second and third reverts are 63 seconds apart to
respect the existing third-revert-in-a-minute protection. No policy is disabled.

An unprotected MCC control demonstrated that authored autosaves can replace
the baseline during this longer schedule. The revised MCC pair additionally
sets `M0_PIN_CHECKPOINT=1`. Only after the requested fixture save completes its
real disk callback, the host fixture calls the existing `main_save_cancel`
interface each tick. This is the service behind the script's `game_save_cancel`.
It cancels pending saves; it does not fabricate a saved image, bypass save/read
validation or edit map scripts. The oracle still rejects any actual replacement
save before the fourth revert. Authored save scheduling and scripts that depend
on it are therefore not qualified by the pinned cases. The option is a general
diagnostic control, not a shipping setting or an MCC compatibility path.

The [pool probe](../tools/investigations/coop_m0_stress_pool.c) reserves real
blocks from the existing object memory pool just before saved-weapon creation
in epoch 2. It fills through `memory_pool_block_allocate`, using bounded stable
references that normal compaction can update. It never changes the pool size,
allocator result or object slot capacity. It records total and contiguous free
space and real `object_header_new` failures. Reservations persist for 450 ticks
(15 seconds), then `memory_pool_block_free` releases them. An unexpected map
unload also releases reservations before disposing the pool.

This is controlled exhaustion of the real allocator by diagnostic reservations,
not proof of a naturally occurring world workload, whole-process out-of-memory
behavior, object-header exhaustion, or arbitrary fragmentation. Space freed by
normal gameplay may exist between target attempts; the probe refills it when
another target allocation is attempted. Other client object allocations can
also fail under this global pool pressure; their complete AI/world consequences
are not qualified by the two-weapon oracle. No performance budget is inferred.

The [dependency probe](../tools/investigations/coop_m0_stress_client.c) holds
validated creation descriptions for both saved weapons while ordinary inventory
messages continue. In epoch 3 it releases them after 180 ticks. In epoch 4 it
holds them until the fourth revert starts epoch 5; the fixture discards its own
old descriptions. Production reset observations independently record queued
inventories, failure times and retry intervals before and after the boundary.

The [packet probe](../tools/investigations/coop_m0_stress_packets.c) copies one
received reliable object-change envelope and one inventory envelope in epoch 4.
They still pass normally through their first dispatch. After epoch 5 is ready,
the unchanged bytes are submitted again to `network_distributed_handle_message`.
The existing `distributed_message_epoch_valid` must reject them before object
or inventory application. This tests delayed duplicate application at the real
dispatcher, not a wire/KCP fault or reconnection. The packet copies remain in
private process memory; recorded evidence contains metadata, not map packages.

Read `object_header_new` in [objects.c](../source/objects/objects.c), allocation,
free and compaction in [memory_pool.c](../source/memory/memory_pool.c), and
`distributed_client_create_failed`, `distributed_client_request_objects`,
`distributed_client_retry_inventory`, `distributed_client_apply_retried_inventories`
and `network_objects_reverted` in
[network_objects.c](../port/linux/game/network_objects.c). The epoch gate and
dispatcher are in [network_distributed.c](../port/linux/game/network_distributed.c).
The revert-rate guard is `main_coop_host_revert` in
[main.c](../source/main/main.c). Xbox, CE and MCC keep their existing loader
paths and ownership; the experiment does not add MCC compatibility logic.

## Oracle and queue boundary tests

The [reducer](../tools/investigations/coop_m0_stress_measurements.py) requires
the full fixture sequence, four actual revert callbacks, successful save and
both observed target deletions. It rejects overwritten checkpoints. Every
client tick pairs one readiness/queue observation with both actual weapon and
inventory observations. An empty queue alone cannot count as restoration.
Earlier ready-but-incomplete observations remain failures after later repair.

Separate engine captures compare the two saved full handles, definitions/types,
parent/name fields, inventory references, owner metadata and primary rounds
against the saved host state. Each epoch's final five-second capture window is
reported separately. These fields do not constitute the full-system oracle.

[test_coop_recovery_queue.py](../tools/test_coop_recovery_queue.py) compiles
unchanged extracted production functions against the existing fake world,
expanded to the actual 128-entry inventory queue capacity. It checks filling
the queue, updating the same unit in place, evicting the oldest at overflow,
retention until tick 89, expiry at tick 90 without refresh, and repair only
after a fresh host inventory when a weapon arrives after expiry. A refreshed
inventory must extend its lifetime and retain the newest ammunition values.
Three isolated mutations check that the assertions reject broken expiry,
oldest-entry selection and payload replacement. This does not claim 128 live
players, or queue saturation during a real multiplayer session. The earlier
production-function suite separately checks retry coalescing and capped growth.
The existing `test_recovery_retires_history_without_losing_object_ownership`
in [test_coop_checkpoint.py](../tools/test_coop_checkpoint.py) calls the real
`network_objects_reverted` with a pending failure, non-default retry interval
and queued inventories. It checks their reset while retaining object ownership.
This complements the live dependency interruption; the runtime cases do not
interrupt an active allocation retry, which has already repaired in epoch 2.

## Results

Six qualified runs cover matched controls and stress cases on Xbox `a30`, CE
`b30` and MCC `a10`: 24 actual checkpoint reverts. Two additional pilots are
excluded for the fixture reasons below. Detailed outcomes, source/content/build
hashes, evidence paths and retained comparator failures are in
[coop_m0_recovery_stress_results.json](coop_m0_recovery_stress_results.json).

| Stress case | Failed pool allocation attempts | Peak queued inventories during pressure | First complete after pressure revert | First complete after dependency-delay revert | First complete after interrupting revert |
| --- | ---: | ---: | ---: | ---: | ---: |
| Xbox | 6 | 2 | 33.014 s | 5.982 s | 0.013 s |
| CE | 46 | 6 | 33.025 s | 5.993 s | 0.021 s |
| MCC, pinned checkpoint | 6 | 2 | 33.018 s | 5.984 s | 0.017 s |

Times start at the observed host revert callback and end at the first client
tick with both target objects, their correct slot-0 references and an empty
retry-inventory queue. They measure this instrumented local fixture, not a
network latency guarantee or a performance budget. Attempt counts include
repeated attempts for the same object. CE pressure also affects objects outside
the two targets; this experiment does not qualify their whole-world recovery.

In every stress case, both target allocations fail initially and on a later
retry. The real reservations are released after 450 ticks (15 seconds), but
the two saved weapons do not return until about 33 seconds after the revert.
Observed retry requests occur 330 and then 660 ticks apart. The client records
990 ticks with checkpoint-ready true while the tracked state is incomplete;
object synchronization remains false during this pressure interval. Eventual
repair does not erase the premature checkpoint-ready result.

In epoch 3, holding both creation descriptions for 180 ticks produces 179
observed ticks with both checkpoint-ready and object-synchronized true while
the target state is incomplete. Releasing them repairs the tracked state at
about six seconds. In epoch 4, both descriptions remain withheld: all 270
observed ticks are incorrectly ready by both flags, with no repair before
the next revert. Ordinary incoming inventory refreshes retain the dependencies;
the separate production-function tests establish expiry without refresh.

The epoch-5 boundary clears the two pending inventories, resets the retry
interval to 330 ticks and clears the failure time. Both saved weapons and
their inventory references return; the final capture windows match all tracked
saved fields on both peers. Each stress case replays two unchanged epoch-4
envelopes, and the real dispatcher explicitly rejects both before application.
No subsequent target-gate regression is observed. This is bounded evidence for
pending-dependency reset and stale-message rejection, not complete recovery of
all transient state or arbitrary dependency graphs.

All controls have complete target gates throughout the four epochs. Xbox and
pinned MCC controls also match the broader tracked saved fields in all final
capture windows. CE has the control deviation below. The unchanged
`net_compare` reports no problems or divergences for the controls, but records
weapon divergences for every stress case. These negative observations are
preserved alongside the more specific oracle; the runtime matrix is not an
all-green compatibility result.

The CE control also exposes a limit of a target-only readiness check. About
6.7 seconds into epoch 4, both peers acquire an additional weapon in slot 1
and the original slot-0 weapon's parent becomes `NONE`. Both target handles
and their slot-0 inventory references still pass the per-tick check, but the
broader saved-state comparison records the extra inventory and changed parent.
The subsequent epoch 5 restores the tracked saved fields. This is retained as
a live control deviation whose cause is unqualified, not attributed to the
injected faults or declared a new checkpoint defect. Its detailed comparison
is `build/m0/recovery-stress/ce-control/control-deviation.json`. Player input
is frozen; authored scripts, AI and other engine behavior continue to run.

The first Xbox control retained under `build/m0/recovery-stress/xbox-control`
requested four reverts too close together. Only three actual reverts occurred;
the normal guard logged a checkpoint respawn on the fourth request. It is an
excluded interruption fixture, not a passing recovery case. Its saves/traces,
logs, binary and generated sources are preserved in verified archives. The
revised schedule preserves this policy instead of bypassing it.

The unprotected `mcc-control` run completed four reverts but authored saves at
ticks 2074 and 4163 replaced the fixture save at 712. The second revert rolled
back 258 ticks instead of the expected 1620; later missing original weapons
cannot be judged against the original save. The reducer rejects this run as an
overwritten baseline. Its evidence and exact unprotected build are retained;
it is not counted as a qualified matched run or a new restoration defect.

## Review, checks and evidence retention

The final self-review covered the generated instrumentation, ownership and
unload cleanup, bounded buffers/reservations, fixture qualification, reducers,
queue tests and full-system ledger. It tightened baseline qualification to
reject overwritten saves and removed a vacuous post-save absence result from
this two-target scenario. The CE control deviation remains visible. No
production implementation refactor was needed or authorized for this pass.

The final focused suite passed **188 tests**, with no failures or skips,
including queue boundary tests, three deliberate broken-code checks and
observer checks that retain early failures and reject incomplete record pairs.
The diagnostic Windows build passed. All six diagnostic modes reproduce their
retained generated sources; hashes verify unchanged original engine sources,
maps and profile inputs. Local documentation links and named source routes
were checked. The normal release/portable configuration with PGO and LTO off
is restored, and no game processes from this assignment remain running.

All 745 private save/trace files (998 MB uncompressed) are retained in verified
archives occupying 154 MB, with original relative paths, sizes and SHA-256
hashes. The two superseded diagnostic builds also remain in verified archives.
Cleanup removed 18.09 GB of private map/cache/binary copies and 247 MB of
temporary build/test output. Required diagnostic binaries, symbols, generated
sources, manifests, logs and evidence remain: about 281 MB for this pass and
2.69 GB across the existing M0 directory (decimal units). Original maps and
saves, installed toolchains and the root checkout's ongoing changes are
preserved. Nothing is published.

## Reproduction and remaining limits

Generate with `python tools/investigations/coop_m0_recovery_stress.py`, configure
release/portable with PGO and LTO off and
`--compiler-launcher=build\m0\recovery-stress\generated\compile.cmd`, then build
Windows. Use [coop_m0_sessions.py](../tools/investigations/coop_m0_sessions.py)
with `--seconds 210 --input none`, fresh private folders and the selected mode.
Use `M0_PIN_CHECKPOINT=1` for the pinned variant; omit it for the original mode.
Manifests record binary/content/profile hashes, arguments and environment.
Reduce with `python tools/investigations/coop_m0_stress_measurements.py
<session-folder>`; archived traces are read in place. Restore the normal build
configuration afterward. Diagnostic binaries and retail fixtures must not ship.

Permanent resource loss, naturally occurring workload exhaustion, header-slot
pressure during recovery, arbitrary parent/vehicle dependency graphs, real-game
queue saturation, actual stream interruption/reconnection and other full-state
domains remain distinct obligations. Known HUD/navigation, vitality, attachment,
timer and readiness defects remain baseline failures, not authorization to fix
them here. Successful bounded recovery cannot close the full M0 gate.
