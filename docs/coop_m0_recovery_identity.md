# M0: reused object slots and inventory ownership

Local investigation on `investigate-coop-rewrite`, beginning at `ef424531e`
on 13 November 2026. This continues the [timer/ammunition pass](coop_m0_recovery_timing.md)
without reducing the [19-domain recovery scope](coop_m0_recovery.md#full-system-recovery-coverage).
Only diagnostic tools and evidence change. No production implementation,
protocol, defaults, original maps or original saves change. M0 remains open;
M1 and publication remain unstarted.

Linux runtime remains **blocked: no runner**. Android runtime remains
**blocked: no device/emulator**, with the earlier Android build blocker
separate. This pass uses hidden Windows native loopback peers, null rendering
and disabled audio. It does not qualify browser/public-internet recovery or
visual/audio presentation.

## Fixture and oracle

An object handle contains a physical slot and a generation identifier, often
called a salt. Reusing a slot must not make a new object interchangeable with
the deleted object. Restoring a checkpoint must restore the saved generation,
definition and ownership, and remove objects created only after that save.

The `--identity` mode of [coop_m0_recovery.py](../tools/investigations/coop_m0_recovery.py)
generates seven isolated source copies under `build/m0/recovery-identity`.
The compiler checks source and generated hashes. The existing three diagnostic
modes and their artifacts remain unchanged. The new
[fixture](../tools/investigations/coop_m0_recovery_identity.c) uses ordinary
engine APIs and a file-static driver that survives reverts. It never changes
allocator pointers, salts, pool sizes or replica state to manufacture reuse.

After the opening cinematic and two player units with usable weapon definitions:

1. Freeze input and clear both private players' inventories through normal
   immediate deletion callbacks. Create a weapon of the first available type,
   give it 20 primary total/loaded rounds, and add it to the first player's
   inventory. The second player's inventory is empty in this saved state.
2. After 90 ticks, request a save and require the successful disk-save callback.
3. After another 90 ticks, delete the saved weapon. Allocate a different weapon
   definition through the normal allocator until its physical slot is reused,
   with a hard bound of 64 allocations. Keep intervening allocations alive.
   Failure to reach the slot leaves the case unqualified.
4. Give the replacement four primary rounds and add it to the other player's
   inventory. Request a revert, then a second revert nine seconds later.

The [reducer](../tools/investigations/coop_m0_identity_measurements.py) checks
that **both peers actually observed A and B** before accepting the recovery
comparison. It requires different generations and definitions in the same
slot, successful inventory additions, the intended owner and round-count
changes, and absence of post-save allocations at save time.

After each revert it compares actual state with saved host state: the full
handle, definition, type/name/parent fields in the reused slot; both players'
inventory slots; all inventory references to tracked handles; owner-player
metadata and attached/belongs-to-player flags; primary magazines; and explicit
absence of every post-save allocation. Readiness and reference resolution
are checked separately. Comparing only peer agreement or only low slot bits
would miss a stale replacement that both peers retained.

State is sampled every 30 simulation ticks and at fixture phase boundaries.
The report includes every sampled ready-client state in the new epoch,
including the first such sample, as well as a separate steady window starting
two seconds after each revert. This is not an every-frame or instantaneous
readiness guarantee; it must not erase the earlier readiness failures.

The save-window rule rejects overwrites before or between reverts. A later
authored autosave ends the second observation window and is recorded, as in
the earlier timer pass. Original scripts run; no map data is edited to keep
the fixture alive.

## Observations

Three 90-second cases completed on Xbox `a30`, Custom Edition `b30` and MCC
`a10`. All intended mutations were observed on both peers, successful saves
preceded both reverts, and epochs advanced from 1 through 2 to 3. No peer
exited early and no final capture was discarded. The
[machine-readable results](coop_m0_recovery_identity_results.json) retain
full handles, definitions, player/slot ownership, capture checks and provenance.
Full per-case reductions and trace archives remain at the hashed evidence paths;
the committed report summarizes their comparisons rather than duplicating them.

| Observation | Xbox | Custom Edition | MCC |
| --- | --- | --- | --- |
| Reused physical object slot | 11 | 982 | 414 |
| Normal allocations needed to reach it | 1 | 3 | 1 |
| Saved full handle, definition and inventory owner restored twice | Pass | Pass | Pass |
| Post-save replacement and intervening objects absent after both reverts | Pass | Pass | Pass |
| Owner metadata, item flags and saved primary rounds restored twice | Pass | Pass | Pass |
| First ready-client capture after each revert | 0.579 / 0.578 s | 0.804 / 0.796 s | 0.252 / 0.251 s |
| Second steady client window, samples / span | 38 / 37 s | 31 / 30 s | 38 / 37 s |

Every ready-client sample in the new epochs matched the saved fields, including
the first sample. The two-second-onward windows on both peers also had zero
saved-value disagreements and no unresolved player, parent or inventory
references. Selected host/client captures were at most 0.405 seconds apart;
the compared identity/inventory fields were deliberately stable. Sampling
times are observation bounds, not convergence-time or performance guarantees.

CE's two intervening allocations make absence checks meaningful beyond the
reused slot. Both objects disappeared after each revert as required. MCC's
authored autosave at tick 2344 occurred after both reverts and the final fixture
phase; it ended that case's second observation window. It did not overwrite
the checkpoint read by either revert. All saves and allocations remain in the
evidence. The unchanged `net_compare` judge found no problems or divergences
in any case. These results establish no new defect in the exercised path.

## Source route and interpretation

In [objects.c](../source/objects/objects.c), `object_new` calls
`object_header_new`; without an explicitly supplied network handle it uses
`datum_new`. [data.c](../source/memory/data.c) chooses a free physical slot
and assigns its generation. `datum_delete` lowers the free-slot search bound;
other free slots may be allocated before the target. The fixture logs every
allocation instead of assuming that the next allocation reused the target.
`object_delete_immediately` runs the normal recursive deletion callbacks.

In [network_objects.c](../port/linux/game/network_objects.c),
`distributed_client_reuses_object` requires both the full handle and definition
to match. `distributed_client_create` deletes a different occupant before
creating the incoming object. `network_objects_reverted` clears checkpoint
inventory/prediction histories while retaining the host object list needed
for subsequent deletion comparisons. `network_objects_host_reverted` requests
fresh descriptions for admitted clients immediately after the host reverts.
These are source facts; the runtime
measurements establish the bounded outcome rather than assuming it from the
presence of these checks.

`unit_add_weapon_to_inventory` in [units.c](../source/units/units.c) records
the weapon reference; `item_in_unit_inventory` in [items.c](../source/items/items.c)
sets the owner player and item flags. This explains why the observer checks
inventory references and owner metadata independently. It does not substitute
for player-driven pickup, weapon switch, reload or animation validation.

## Remaining scope and reproduction

This is weapon-to-weapon reuse with a different definition and player owner.
It does not qualify arbitrary cross-type reuse, many generations of churn,
deliberately delayed stale messages, real-engine allocation failure, missing
dependencies, changed rosters/options, weapon pickup/reload histories, moving
vehicles/prediction, active AI/combat/projectiles, animation/regions, active
world/device/surface state, scripts/cinematics/audio, broader MCC state,
consumed scaling budgets, policy/social lifetimes, local/competitive
preservation or interrupted/browser recovery. The existing objective,
navigation, vitality, attachment and timer failures remain characterized
baselines. Ordinary menu restart/revert and failed-save/load paths remain open.

Generate with `python tools/investigations/coop_m0_recovery.py --identity`;
configure release/portable, PGO and LTO off, with
`--compiler-launcher=build\m0\recovery-identity\generated\compile.cmd`, then
build Windows. Run the [session utility](../tools/investigations/coop_m0_sessions.py)
with `--seconds 90 --input none`, a fresh private output folder, family/map
arguments and `M0_RECOVERY_DOMAINS=identity`. This is a generated-build control,
not a shipping setting. At most two instances run concurrently. Manifests
record exact arguments, environment and hashes. Reduce with
`python tools/investigations/coop_m0_identity_measurements.py <session-folder>`;
archived traces can be read without extracting them.

The [observer tests](../tools/investigations/test_coop_m0_identity_measurements.py)
distinguish full handles from slot numbers, wrong definitions from correct
handles, inventory references from owner metadata, and stale post-save objects
in other slots. They require intended mutations rather than arbitrary
differences. Review includes the diagnostic boundaries, normal API ownership,
bounded allocation/failure paths, source explanations, original-data hashes,
archive verification and the full-system ledger. This is self-review, with
no delegated review or remote CI.

The Windows diagnostic build succeeded. After the final observer review,
**173 focused tests passed with no failures or skips**: the 23 recovery
observer tests and the existing 150 checkpoint recovery, epoch, timing,
roster and MCC checkpoint tests. The new readiness test catches incorrect
state in an early ready capture instead of hiding it behind the two-second
steady window. All three evidence archives were reduced again, and source,
generated/binary, original map/profile and archived member hashes were checked.
All three earlier modes reproduce their seven retained generated files;
their binaries and archives are preserved. Document links, named source
routes, Python syntax, Git scope and all 19 ledger entries were checked.
No production-source change requires a new Android/Web build; this Windows
diagnostic run does not qualify those platforms.

Cleanup removed 6,827,899,346 bytes of private map/resource/cache/executable
copies and 241,949,023 bytes of temporary build/test output. The 278 private
save/trace files, 240,617,352 bytes raw, remain in three verified lossless
archives totalling 27,754,672 bytes, with original paths and SHA-256 hashes
in adjacent indexes. Required binaries/symbols, generated copies, manifests
and useful logs remain: about 94.9 MB for this pass and 2.19 GB across M0.
Original maps, saves, installed toolchains and other work remain untouched.
Normal release/portable, PGO-off/LTO-off configuration is restored. No owned
game process remains; no production fix, M1 work, push or publication occurred.
