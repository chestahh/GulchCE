# M0 gate review and publication status

15 November 2026. The investigation branch is now `network-coop-rewrite`,
rebased onto GulchCE testing `4595247ae6649c6efbc9baf99124f979ebef3a38`.
Its rebased investigation head is `91f8629d599864ec8801b617d5d0b864bfd4d2c6`.
The user authorized branch publication for CI. M1 and the rewrite have not
started. No PR or release is part of this assignment.

Earlier reports retain their original revisions, branch names and observations.
Their statements that the branch was unpublished describe those earlier runs.
Rebasing does not make those measurements observations of the new revision.
Read the [plan](coop_rewrite_plan.md), [semantic inventory](coop_m0.md),
[full recovery ledger](coop_m0_recovery.md) and
[ordinary A10 entry report](coop_m0_campaign_entry.md) with this distinction.

## What M0 must establish

M0 establishes the responsibilities, intended contracts, reproducible failures,
measurement methods and comparison baseline for the entire co-op system.
It does not require repairing every baseline failure or finishing every campaign.
A negative observation is useful evidence when the setup and acceptance rule
are recorded. An untested case remains unqualified.

The existing full exit gate is not silently waived. Linux and Android runtime
remain blocked: no local Linux campaign test environment or Android device/
emulator is available. Hosted CI builds and asset-free tests cannot close
those campaign runtime gaps. The frozen
[desktop comparison budgets](coop_m0_desktop_budgets.json) remain limited to
their recorded hardware, workloads and two-peer arrangement. A universal
low-end/mobile/full-world operating envelope is not established.

Further baseline work should answer a concrete design uncertainty or supply a
missing representative measurement. Exhaustive campaign completion, repeated
intermittent-failure trials and full combinations of platforms, players and
transport conditions remain mandatory qualification in their owning milestones.
Moving their execution later does not classify them as passing, remove them
from scope, or authorize M1.

## Full-system disposition

All 19 original contracts remain in scope. The baseline and recovery ledgers
contain the evidence and known defects; this table identifies the later gates
that must turn those observations into validated behavior.

| Contract | Required design and later validation |
| --- | --- |
| session | M2/M5/M10: live roster and controller ownership, admission/readiness, departure and recovery; qualify multi-local, four-player and dedicated arrangements. |
| transport | M2/M5/M11: bounded reliable/unreliable delivery, stale operation rejection, interruption, reconnect and native/browser/public-internet fault runs. |
| content | M2/M10/M11: content identity, admission, cancellation and failed-load cleanup through the separate Xbox/CE/MCC owners. |
| objects | M3/M5: lifetime identity, construction/deletion, explicit absence and dependency failure; extend sampled reuse/exhaustion baselines to sustained workloads. |
| inventory | M3/M5/M8: ownership, empty state, pending dependencies, ammunition/reload and recovery; receipt is not successful application. |
| movement | M6/M5/M11: prediction, correction, usable placement/collision, teleport, seats/riders and recovery with active input. |
| actors | M7/M5/M11: authoritative decisions and meaningful actor outcomes, scripted animation, death/revival and recovery during encounters. |
| combat | M8/M5/M11: projectiles, explosives, melee, damage and ammunition, with independent host outcomes and client presentation oracles. |
| world | M4/M5/M6: BSP readiness, device/group state, collision/traversability, attachments and absence; ordinary and restored progression must both work. |
| presentation | M9/M5/M11: cameras, animation, crop, HUD/objectives/navigation, timer semantics, audio and lifecycle; numerical agreement alone is insufficient. |
| events | M2/M4/M5: distinguish persistent state, consequential actions and disposable effects; bounded retry/dependency/deduplication/expiry behavior. |
| scripts | M4/M10: general network session predicates, player/input ownership, authored gates and campaign destination semantics. No map-specific bypasses. |
| checkpoint | M5/M10: complete applied-state readiness, restart/revert authority and outcomes, natural saves, repeated restoration and resumed progression. |
| scaling | M7/M10/M5: consumed enemy/pickup/vehicle budgets, held riders and deleted sources across roster/world changes; no duplicate replay. |
| policies | M2/M6/M10: current host settings, friendly fire/collision, AFK, BSP-majority/cooldown and permissions remain independent of saved world state. |
| social | M2/M10/M11: chat/voice/votes/link permissions, session lifetime and cleanup; transient voice is not checkpoint state. |
| mcc | M3–M5/M9/M10: MCC-owned names, extra inventory, script/campaign/sidecar state, package admission and lifecycle without absorbing Xbox/CE behavior. |
| camera | M9/M10/M11: GulchCE camera policy, shoulder/activation ownership, Crossplay agreement and restoration, preserving feature boundaries. |
| multiplayer | M1–M12: preserve competitive multiplayer, solo and local split-screen wherever shared services change; qualify integration explicitly. |

The reported natural A50 grav-lift failure and intermittent A10/C10 door
softlocks remain separately tracked, unconfirmed reports. Bare BSP switching
and selected device probes do not reproduce their complete authored routes.
Their later acceptance gates require every player to reach the intended usable
destination and continue. Map names identify test fixtures, never fix selectors.

## Rebase review

The 17 investigation commits replayed without conflicts. A local backup branch
preserves the pre-rebase head. The only production change relative to testing
remains the previously authorized console compiler guard after failed-map
script-storage disposal; this pass introduces no co-op behavior or wire change.
Protocol 44 is unchanged. The unrelated root checkout was not modified.

A fresh lexical inventory at the rebased head has 61 distributed messages,
35 session messages, 12 co-op event kinds and 312 engine-hook references.
Ignoring line-number movement, these sets match the original index. This is
source coverage, not runtime coverage. The presentation state declaration
changed upstream for unattended cutscene handling. Four indexed settings were
added upstream: third-person crosshair size/smoothing and dedicated team-switch
enable/cooldown. Their existing camera, policy and multiplayer owners remain
in the ledger. The current index and normalized delta are retained locally in
`build/m0/campaign-restore`.

Upstream also changed script-owned campaign destination handling and the
console-versus-script thread distinction. These affect M4/M10 reading routes
and require later campaign qualification. Earlier progression observations
must not be treated as proof that the updated destination path still fails.

## Validation record

The rebased Windows diagnostic build succeeds. Focused checkpoint, epoch,
script-player, vehicle, pose, runner/reducer and console tests pass (239).
Evidence/reducer, queue and campaign-context tests pass (66); current script
destination and camera agreement checks pass (54). These are separate from
runtime campaign evidence and do not close Linux/Android runtime blockers.

The [bounded A10 restoration report](coop_m0_campaign_restore.md) completes the
selected local pass: two natural tutorial checkpoints restored in each player
ordering, followed by ordinary authored progression and supported movement
through the cryo exit. Host-first also covers ordinary client LAN browser entry.
This is a representative observation, not full campaign or full-state sign-off.

Hosted CI at the rebased investigation head passed:

- [Build](https://github.com/chestahh/GulchCE/actions/runs/43701912314):
  Linux, Windows, Android and web builds, plus changelog validation; release
  publication was skipped.
- [MCC map validation](https://github.com/chestahh/GulchCE/actions/runs/43701912367):
  reader/validator, Windows adapter and HUD meter-render checks.

The final branch update adds documentation only. The tested implementation and
diagnostic sources are unchanged. A final fetch still resolves testing to
`4595247ae6649c6efbc9baf99124f979ebef3a38`.

## Gate decision

The agreed local restoration pass and full-system disposition review are
complete. M0 is **not fully signed off**: Linux/Android campaign runtime is
blocked, and the universal operating envelope remains unqualified. The existing
desktop budgets are useful comparison baselines within their stated limits.
Those limits must receive an explicit disposition before advancing; publication
for CI does not waive them or authorize M1.

The next decision is whether to accept the documented desktop baseline with
explicitly deferred platform/envelope gates, or keep the full M0 exit gate open
until the missing environments and representative workloads are available.
Either choice keeps all 19 domains and their later acceptance obligations in
scope. No rewrite, map-specific repair, production co-op change, PR or release
was introduced by this pass.
