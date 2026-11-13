# M0: cross-type reuse and reconstruction failures

Local investigation on `investigate-coop-rewrite`, starting at `e52993ad4`
on 13 November 2026. This extends the [weapon-identity pass](coop_m0_recovery_identity.md)
and retains the [19-domain recovery ledger](coop_m0_recovery.md#full-system-recovery-coverage).
Production source, protocol, defaults, original maps and original saves are
unchanged. M0 remains open; M1 and publication are unstarted.

Linux runtime is **blocked: no runner**. Android runtime is **blocked: no
device/emulator**; the earlier Android build blocker remains separate. Runs
use Windows native loopback peers, hidden windows, null rendering and disabled
audio. They do not qualify Linux, Android, browser, public-internet or actual
visual/audio recovery.

## Experiment boundaries

[coop_m0_recovery_faults.py](../tools/investigations/coop_m0_recovery_faults.py)
creates eight hashed source copies in `build/m0/recovery-faults/generated`.
It reuses the existing identity observer and host allocation/deletion helpers.
The [host fixture](../tools/investigations/coop_m0_recovery_faults.c) and
[client controls](../tools/investigations/coop_m0_faults_client.c) exist only in
that diagnostic build. No fault is injected when `M0_RECOVERY_DOMAINS=control`;
the same executable uses `faults` for the injected run.

The host saves a created weapon with 20 primary rounds in one player's
inventory; the other private player's inventory is empty. It deletes the
weapon and uses the normal allocator to place a biped in the same physical
slot with a different generation and definition. The biped definition comes
from a loaded player biped, without editing tags or adding an actor brain.
The search is bounded to 64 normal allocations, retaining intervening objects.
Both peers must observe the replacement; host-only reuse is insufficient.

The schedule, relative to fixture activation after the opening cinematic,
is A at tick 0, save at 90, replacement B at 180, first revert at 270, a second
replacement at 990, second revert at 1080 and a final observation at 1440.
The 24-second interval before replacing the restored weapon again lets the
first recovery be observed independently. Every case uses a fresh private
data/save folder and 120 seconds of observation. No more than two game
instances run concurrently.

Each diagnostic process handles exactly one map/session. Its fixture counters
and injected failures are process-local and intentionally survive the two
checkpoint restores. This is not a reusable production lifecycle service.

Two distinct failures are exercised in each injected case:

- **First revert, allocation refusal:** once, for the saved weapon handle,
  refuse the object-memory allocation after its slot header has been reserved.
  The existing `object_header_new` failure branch frees the header and returns
  failure through `object_new`; the ordinary client handler requests recovery.
  This is a controlled failure return at the real allocation boundary, not
  natural memory exhaustion or an allocator-capacity test. Pool contents and
  limits are not changed.
- **Second revert, delayed dependency:** hold validated create descriptions
  for that weapon for 180 simulation ticks while inventory messages continue.
  Release the retained description through `distributed_client_create`, then
  let the existing inventory retry logic run. This is a controlled application
  delay, not packet loss or a stalled KCP stream. A changed epoch cancels the
  held description; the experiment must record that instead of applying stale
  data to another epoch.

A 12-byte native diagnostic file in the private session folder identifies the
saved handle, unit and definition. It is orchestration metadata only: it does
not populate game state, alter protocol messages or choose reconstruction
results. The native helper avoids the game's data-root path translation.

## Oracle

The [reducer](../tools/investigations/coop_m0_fault_measurements.py) requires
the successful disk-save callback, the full fixture sequence, both observed
cross-type mutations on both peers, advancing epochs and completed diagnostic
observations. It rejects checkpoints overwritten before or between reverts.
A later authored autosave ends the second observation window and is recorded.

Per-tick client observations record the checkpoint-ready flag, object-sync
flag, queued inventory count, actual target existence and the actual inventory
slot immediately before the gameplay/prediction readiness gate. They report
every ready-but-incomplete observation and the first subsequently complete
target/inventory state. Readiness is never the restoration oracle.

Separate engine captures compare against the saved host state: full object
handle, definition, type/name/parent, inventory references, owner metadata,
item flags, primary ammunition and absence of all explicit post-save
allocations. Captures occur every 30 simulation ticks and at phase boundaries.
The final five seconds of each observation interval are reported separately
from its earlier mismatches, so eventual repair cannot erase early failure.
Complete target/inventory observations do not certify the full 19-domain contract.

## Results and fixture exclusions

Six matched Windows runs completed: one control and one injected run for
each map family, each with a successful disk save and two advancing recovery
epochs. Both peers observed both cross-type replacements in every run.
The [machine-readable results](coop_m0_recovery_faults_results.json) retain
the measurements, fault events, original comparison failures and evidence hashes.

| Family | First complete target/inventory sample after first revert (allocation refusal) | Ready while object sync pending | Ready with missing dependency | First complete target/inventory sample after second revert |
| --- | --- | --- | --- | --- |
| XBOX | 0.020 s | 330 ticks | 179 ticks | 5.986 s |
| CE | 0.014 s | 330 ticks | 179 ticks | 5.981 s |
| MCC | 0.017 s | 330 ticks | 179 ticks | 5.984 s |

**Cross-type controls:** weapon-to-biped reuse restored the saved weapon
handle, definition/type, inventory, owner metadata and primary ammunition;
the post-save biped was absent. Neither revert produced a sampled incomplete
target/inventory gate in the controls. The tracked fields in the final capture
windows matched the saved state on both peers in all six runs, with no
unresolved references.

**Allocation refusal:** each injected run recorded exactly one refusal and
the real client create-failure handler. The target and inventory were already
correct at the first observed gameplay gate; these runs do not demonstrate
a prolonged missing object after the single refusal. The checkpoint-ready
flag nevertheless remained true for 330 observed ticks while the separate
object-sync flag was false (about 11 simulation seconds). Existing traffic
reconstructed the target before that synchronization retry completed; this
does not establish that the delayed retry caused the initial repair.

**Delayed dependency:** withholding creation for exactly 180 ticks left
one inventory queued. Checkpoint readiness and object synchronization were
both true during the 179 sampled ticks with the weapon and inventory missing.
The normal creation and inventory retry paths repaired them when the held
description was released. No later incorrect target/inventory sample appeared
in the measured epoch window. Thus receipt/synchronization flags are not a
complete reconstruction barrier. The unchanged network comparator also
reported weapon divergence in the fault runs; those negative results are
preserved, not reclassified as passing because eventual repair occurred.

Times use local monotonic timestamps relative to the actual revert callback.
They describe these instrumented runs, not network latency or performance
budgets. The full saved-state captures remain separate from the narrower
per-tick target/inventory checks. These observations extend the negative M0
readiness baseline; they are not an implementation or proof of full recovery.

Two Xbox scenery pilots are retained separately, with their own generated
sources and binaries in verified lossless archives. Unnamed scenery is excluded
from `NETWORKED_OBJECT_TYPES` in the ordinary object-replication path.
Both pilots showed host-side reuse
but no client replacement, so neither qualifies end-to-end cross-type reuse.
The first pilot also lacked client diagnostic metadata because the game file
layer normalized the parent path into the host's data root. The second pilot
verified the native metadata route. These are fixture limitations, not newly
claimed product defects. Neither pilot's evidence was discarded or counted
as a passing matched case.

## Source route

Read `object_header_new` and `object_new` in
[objects.c](../source/objects/objects.c), and `datum_new`, `datum_new_at_index`
and `datum_delete` in [data.c](../source/memory/data.c). The injected refusal
uses their existing failed-allocation cleanup path. It does not exercise
fragmentation, global out-of-memory behavior or failure of the slot allocator.

In [network_objects.c](../port/linux/game/network_objects.c), follow
`distributed_client_create`, `distributed_client_create_failed`,
`distributed_client_request_objects`, `network_objects_handle_synchronized`,
`distributed_client_inventory_whole`, `distributed_client_retry_inventory`
and `distributed_client_apply_retried_inventories`. A failed create marks the
object set unsynchronized and schedules a retry; the initial retry interval
is 330 ticks. Missing inventory dependencies are retained for later application.

`distributed_checkpoint_ready` in
[network_distributed.c](../port/linux/game/network_distributed.c) returns
the checkpoint synchronization flag. Ordered checkpoint receipt and successful
object/inventory reconstruction have separate state. The client tick uses
that checkpoint-ready flag before prediction/gameplay-related work. The
runtime observations determine the consequences; source inspection alone
is not a completed test.

## Remaining scope, reproduction and review

Natural pool exhaustion during recovery, header allocation failure, repeated
or permanent failures, queue saturation/expiry, multiple missing dependencies,
arbitrary cross-type pairs and generation churn, transport interruption and
late old-epoch delivery remain distinct obligations. These tests do not
qualify active AI/combat/projectiles, vehicle/prediction histories, animation,
world/device/surface state, cinematic/audio output, broader MCC state, changed
rosters/options, scaling budgets, policy/social lifetimes, failed saves/loads,
ordinary restart/revert controls or local/competitive preservation. The
existing HUD, navigation, vitality, attachment and timer failures remain open
product defects within the full-system baseline; no fix is authorized here.

Generate with `python tools/investigations/coop_m0_recovery_faults.py` and use
release/portable, PGO off and LTO off, with
`--compiler-launcher=build\m0\recovery-faults\generated\compile.cmd`.
Build Windows, then run [coop_m0_sessions.py](../tools/investigations/coop_m0_sessions.py)
with `--seconds 120 --input none`, the family/map arguments, a fresh private
output folder and `M0_RECOVERY_DOMAINS=control` or `faults`. Manifests record
exact arguments, environment and original/content/executable hashes.
Reduce using `python tools/investigations/coop_m0_fault_measurements.py
<session-folder>`; archived traces can be read directly.

The [observer tests](../tools/investigations/test_coop_m0_fault_measurements.py)
retain early incomplete readiness after later repair, distinguish readiness
from successful reconstruction, require the client replacement, and reject
mispaired per-tick observations. Final self-review includes fault isolation,
activation and lifetime, actual failure-path execution, evidence exclusions,
map-family boundaries, all 19 ledger entries and cleanup. No delegated review
or remote CI is performed.

The Windows diagnostic build succeeded. Final review removed an unused reducer
import; the resulting focused suite passed **178 tests with no failures or
skips** (28 observer tests and 150 existing checkpoint/epoch/timing/roster/MCC
tests). All six runs were reduced again; generated/source/binary, original
map/profile and archived member hashes were verified. The four earlier modes
still reproduce their seven generated files each; this mode reproduces eight.
Python syntax, local documentation links, named source routes, the 19-domain
ledger and final Git scope were checked. No production-source change requires
a new Android/Web build, and no such platform qualification is claimed.

Cleanup removed 17,582,381,988 bytes of private copied
maps/resources/caches/executables and 241,973,409
bytes of temporary build/test output. All 744 private save/trace
files (708,982,896 raw bytes) remain in eight verified archives
totalling 89,449,987 bytes. Two inactive pilot builds were archived
losslessly from 126,804,487 to
49,937,919 bytes. Adjacent indexes record original
paths, lengths and SHA-256 hashes; loose copies were removed only after verification.
Required diagnostic binaries/symbols, generated sources, logs and manifests
remain: about 214.0 MB for this pass and
2.41 GB across M0. Original maps/saves, toolchains
and the root checkout's 20 ongoing changes remain untouched. Normal
release/portable, PGO-off/LTO-off configuration is restored. No owned game
process remains. M0 stays open; no production fix, M1, push or publication occurred.
