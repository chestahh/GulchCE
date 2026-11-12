# Network co-op rewrite feasibility

Investigation dated 12 November 2026. No game implementation was changed.
Local branch: `investigate-coop-rewrite`.

## Decision

A replacement is technically feasible. The strongest evidence supports
rebuilding campaign replication and its lifecycle first, while retaining the
host-authoritative simulation and existing transport initially. A complete
replacement of shared object, movement, AI and combat replication remains a
possible later scope; this investigation does not establish that every one
of those implementations must be discarded.

This is more than reorganizing a large file. The replacement needs different
contracts for state, events, dependencies, world transitions and recovery.
Renaming functions, splitting files, increasing resend counts or switching
socket libraries would leave the demonstrated failures possible.

The recommended route is a staged rewrite with measurable acceptance gates,
not an accumulation of map-specific fixes. It must cover the full campaign
lifecycle, not just the examples in the supplied screenshot.

## Revision and comparison

- GulchCE testing: `887d9ac9d6ac7a647f2cc3fada30c602b1497f82`, fetched before
  creating the branch.
- OpenCE testing: `e9adcf7a70ea1c3a069c5c8d1d0a0cc93ebb065e`.
- OpenCE main: `315285c6dcc876c2b6feb792de50c4c95d25537a`.
- `network_coop.c`, `network_actors.c`, `network_objects.c` and the
  `coop_*.c` helpers match OpenCE testing. The shared dispatcher and damage
  module have GulchCE shoulder-camera additions. Shared engine callers also
  differ in bots, rendering, menus and other integration.
- Code sharing does not mean current wire compatibility: GulchCE testing
  advertises base version **44**, whereas inspected OpenCE testing declares
  **43**. No version was changed here.
- The base revision's public [Build run](https://github.com/chestahh/GulchCE/actions/runs/43309946257)
  and [MCC validation run](https://github.com/chestahh/GulchCE/actions/runs/43309946367)
  were both successful when checked. Those are upstream CI results, not a
  campaign playthrough performed for this investigation.

Recent history includes co-op sound changes on 7–8 November, MCC/shared
checkpoint integration on 9 November and vehicle riding on 10 November.
The subsystem has received changes recently. That does not establish that
reported gameplay failures are fixed, nor does it justify judgments about
individual contributors.

## What “the entire co-op system” includes

The five central C files total 18,103 physical lines at this revision;
six `coop_*.c` helpers add 2,627. This excludes headers, transport, session
managers, MCC modules, engine hooks and tests. A search for
`network_coop_` or `coop_scripts_` alone reaches 23 C files under
`source/`. These counts describe the integration surface, not code quality
or a target for reduction.

| Responsibility | Current owners and contracts | Rewrite implication |
| --- | --- | --- |
| Discovery, admission, roster, lobby, disconnect | [network_game_manager.c](../source/networking/network_game_manager.c), [network_server_manager.c](../source/networking/network_server_manager.c) | Preserve session identity and admission; distinguish connected, loaded and synchronized peers. |
| LAN/internet transport | [NETCODE.md transport](../port/linux/NETCODE.md#transport), [p2p.c](../port/linux/src/p2p.c), [p2p_webrtc.c](../port/linux/src/p2p_webrtc.c), [web_p2p.c](../port/web/src/web_p2p.c) | Retain initially. No transport defect was reproduced here. Native and browser validation remains required for implementation. |
| Clocks, inputs, local prediction, correction, dispatch | [network_distributed.c](../port/linux/game/network_distributed.c) | Shared with competitive multiplayer. Changing it substantially expands the rewrite and crossplay risk. |
| Object identity, creation/deletion, inventories, vehicles | [network_objects.c](../port/linux/game/network_objects.c) | Replicas use host datum handles; local effects use a separate allocation range. Replacement must preserve or deliberately migrate this contract. |
| AI and animation | [network_actors.c](../port/linux/game/network_actors.c), [game.c](../source/game/game.c) | Host runs AI; clients apply controls and authoritative state. Animation impulses and some actions use limited repetition. |
| Hits, explosives, deaths and aftermath | [network_damage.c](../port/linux/game/network_damage.c), [damage.c](../source/objects/damage.c) | Host validates client hit reports. Local projectile simulation and authoritative damage are separate concerns. |
| Script/world/presentation replication | [network_coop.c](../port/linux/game/network_coop.c), [network_coop.h](../port/linux/game/network_coop.h) | Main candidate for replacement: events, devices, names, attachments, sounds, camera, HUD and session policies share one owner. |
| Campaign semantics and player lifecycle | [coop_scripts.c](../port/linux/game/coop_scripts.c), [players.c](../source/game/players.c), [coop_spectate.c](../port/linux/game/coop_spectate.c) | More-than-two-player scripts, spawn safety, death/wipe, spectating and BSP gates must remain correct, independently of packet delivery. |
| Checkpoint/mission lifecycle | [game_state.c](<../source/saved games/game_state.c>), [main.c](../source/main/main.c), distributed/object/co-op modules | Saved gameplay and live roster/network state have different ownership and lifetimes. |
| MCC boundaries | [mcc_checkpoint.c](../port/linux/game/mcc_checkpoint.c), [mcc_network.c](../port/linux/game/mcc_network.c), [mcc_object_names_network.c](../port/linux/game/mcc_object_names_network.c), [mcc_main.c](../port/linux/game/mcc_main.c) | Keep compatibility semantics and package progression MCC-owned; never route Xbox/CE through these adapters. |

The system is already host-authoritative. “Make it host-authoritative” is
therefore not a sufficient redesign. Clients still simulate physics, weapons,
effects and remote actors; the hard problem is making the resulting replicas
converge without replaying obsolete actions or corrupting ownership.

## Demonstrated contract failures

The [reproducible probe](../tools/investigations/coop_replication_probe.py)
extracts the current production functions and compiles them for a 32-bit C
target. Engine side effects and packet delivery are controlled fixtures.
All five observations below executed successfully. These are bounded
function-level reproductions, not full game reproductions or diagnoses of
the screenshot's particular sessions.

1. **Finite repetition is not reliable delivery.**
   `event_new` and `host_send_events` queue up to 128 script events, send
   each three times and retire it without acknowledgement.
   `send_to_clients` selects the unreliable batched path in
   `distributed_send`. The probe drops all three transmissions and confirms
   that nothing remains to retry. At 30 Hz those send opportunities span
   roughly a tenth of a second; burst loss or a delayed object stream need
   not resemble independent random packet loss.

2. **Receiving an event is mistaken for successfully applying it.**
   `network_coop_handle_events` advances `applied_number` before calling the
   action. `client_apply_unit_animation` returns if its unit has not arrived.
   Delivering the unit and retrying the same event then does nothing.
   A fresh sequence applies correctly in the probe's positive control.
   Reliable object creation and unreliable events can cross in transit;
   sending objects first is not a cross-channel delivery guarantee.

3. **Sequence numbering deduplicates but does not recover gaps.**
   Delivering event 2 before event 1 causes event 1 to be rejected permanently.
   There is no gap buffer or missing-event request. The dispatcher also
   rejects older ticks per message kind. Describing this as “once, in order”
   overstates the contract: it accepts a monotonically increasing subset.

4. **Periodic active-state resends cannot repair every removal.**
   `network_coop_note_nav_point` removes a deactivated point from the host's
   active list. After the removal event's three copies expire,
   `host_send_hud_state` sends no empty set or removal record.
   The probe confirms this. A client that retained the old point cannot infer
   its removal from absence in these incremental messages.
   Attachment removal has a similar source-level pattern in
   `network_coop_note_detach` and `host_send_attachments`; that path was
   inspected, not independently executed by this probe.

5. **Queue pressure can corrupt the recovery source itself.**
   `network_coop_note_hud` updates the cached objective only after
   `event_new` succeeds. Filling the queue, then changing the objective,
   leaves the prior objective cached; a subsequent join resend sends that
   old value. The probe confirms it. Transport backlog must not prevent the
   host from recording authoritative persistent state.

These are systemic failure classes independent of map identity. Not every
cosmetic event needs guaranteed playback: an obsolete sound or flash may be
better expired. Persistent changes and consequential actions need explicit
delivery, reconstruction and expiry policies rather than one policy for all.

## Further architectural findings and uncertainties

**Checkpoint support is substantial but incomplete as a world contract.**
`distributed_message_epoch_valid` rejects stale worlds, validates an ordered
host boundary and gates traffic until an ordered snapshot end marker.
`network_coop_restore_players` preserves the current roster.
Same-handle corpse replacement, inventories, pending effects and histories
have real recovery tests. Retain those protections.

The initial snapshot contains ordinary objects and inventories.
Campaign presentation/world channels catch up separately.
Its end marker establishes stream ordering, not successful construction of
every object: `network_objects_handle_synchronized` can still retain a failed
creation and request repair later. Thus “gameplay messages admitted” and
“world usable” are different states today. A redesign needs explicit readiness
for critical dependencies and bounded repair, without making a cosmetic
failure block a session forever.

`network_coop_reset_recovery` clears selected transient state but does not
restore `host_hud_state`, the attachment list or `players_vitality` to their
checkpoint values. These are file-static caches outside the saved game image.
Some current-world state is reconstructed or pruned later; that is not a
complete rollback specification. The existing
[networking guide](reading/networking.md) explicitly records the authored
HUD/nav/attachment limitation. Actual stale-checkpoint presentation outcomes
still need game-level reproduction.

**Different identity and timing models coexist.**
Units/items use authoritative handles; scenery/devices may resolve by name,
or by slot and matching definition even when salts differ. Named-object
presence needs two matching observations. Presentation buffers eight samples
and plays about two ticks behind; actors and other objects follow their own
update/correction cadence. `camera_object_get` can infer its anchor from a
nearby moving vehicle before falling back to the scripted camera target.
Those are real implementation choices, not proof of a particular visual bug.
Explicit dependencies, authored camera references where available and a
consistent presentation timeline deserve evaluation.

**AI and projectile replication are separate workstreams.**
AI clients replay host controls, with state correction; some impulses repeat
three times. Projectiles are not among the four ordinary replicated object
types. A client reports its own hits, while the host's corresponding
projectiles normally suppress that client's damage. This has validation and
duplicate-prevention machinery worth preserving. Matching damage does not
establish matching projectile trajectories, detonation timing or visuals.
A projectile redesign would need identities for shots/throws, authoritative
detonation/attachment outcomes and reconciliation of predicted effects.
It must cover player and AI fire, splash, sticky explosives, chained effects,
moving targets and disconnects. No such replacement was implemented or
benchmarked here.

**Doors already have repair paths.**
Device groups rotate through periodic updates; changed device positions and
power are refreshed. Therefore a permanently closed client door cannot simply
be attributed to the three-send event issue. Trace identity, group ownership,
BSP, activation and checkpoint state in a real failing session before choosing
a correction.

**Campaign progression is also session policy.**
`network_game_server_port_cooperative_won` intentionally returns to pregame
with a next-map selection. Stock campaign mapping, standalone custom maps,
MCC package progression, missing content and dedicated rotation have different
paths. Seamless next-level loading would be a behavioral decision in addition
to replication work. Returning to a lobby, losing the session and falling back
after a load error must be diagnosed separately.

The supplied screenshot is a useful sample of reports, not an exhaustive
specification. Its cutscene, animation, projectile, door and checkpoint
reports fit the areas above, but their exact causes and revisions remain
unknown. Skip-prompt visibility and a uniform Master Chief appearance are
separate UX/customization requirements, not evidence that transport must be
rewritten.

## Replacement design to prove

Keep existing interfaces where they suffice. Establish a small co-op-owned
session/lifecycle component, with independently testable snapshot, persistent
world-state, event and presentation responsibilities. These are proposed
responsibilities, not a demand for a framework or one file per label.

- **Explicit lifecycle:** disconnected, loading, receiving a world, repairing
  required dependencies, playing, changing BSP, reverting and changing mission.
  Every transition defines allowed sends/applies, state ownership, timeout,
  cancellation and cleanup. A checkpoint advances the world generation while
  retaining the live session roster.
- **Complete baseline plus repairable updates:** publish bounded snapshots
  for persistent state, including explicit empty sets and removals.
  Record authoritative changes before enqueueing messages. Define versions
  per entity/component where partial messages need independent freshness.
- **Dependency-aware events:** reliable ordered delivery for consequential
  one-shot actions; bounded pending application until referenced objects exist.
  Use existing reliable transport first. Deduplicate within a world and
  distinguish received, applied, superseded and expired actions.
  Replace repeatable state changes with state whenever practical.
- **A usable-world acknowledgement:** join/revert completion covers required
  objects, player bindings, inventories, BSP and campaign state.
  Permit targeted retry and explicit failure. Snapshot contents must have a
  defined capture tick and a bounded way to catch up changes made during transfer.
- **Separate presentation time from authority:** sampled camera/pose data can
  be interpolated without delaying local control. Match animations and camera
  anchors to the relevant object generation and timeline; define timeout
  behavior instead of silently letting stale samples own the view.
- **Bounded cost and observability:** limits for queues, snapshot bytes,
  dependencies and per-peer work; diagnostics for why an action was rejected,
  deferred or expired. Measure bytes/second, correction magnitude, repair
  latency and tick cost with representative actor populations.

Do not copy the host's raw checkpoint buffer to clients as the wire format.
Existing saved-state allocation checks, pointer-bearing engine state and
platform layouts require explicit serialization. Likewise, running every
script/AI simulation independently under lockstep would reintroduce
whole-world determinism and latency requirements. Full rollback would additionally
need a much broader auditable state snapshot than the current checkpoint path.
Neither is the recommended first prototype.

## Options and delivery gates

| Approach | Feasibility and tradeoff |
| --- | --- |
| Repair the existing implementation | Feasible and lowest immediate disruption. Can address the demonstrated failures, but needs explicit state/lifecycle contracts to avoid continuing ad hoc cache rules. |
| Replace campaign replication in stages | Recommended. Addresses the strongest evidence while preserving transport, validated ownership and shared multiplayer behavior initially. Still substantial work across script/world/presentation and lifecycle integration. |
| Replace the entire shared replication stack | Technically feasible, much larger. Adds player/vehicle prediction, AI, combat, allocation/identity migration and competitive multiplayer regression risk. Justify each expansion through evidence. |
| Replace transport or adopt a networking library | Not a solution to missing campaign semantics. Consider only after profiling or reproducing a transport problem; review platform and licensing constraints separately. |

A credible staged programme:

1. **Inventory and acceptance baseline.** Catalog replicated state and
   checkpoint ownership, classify all script side effects, record known
   failures and add deterministic packet loss/reordering/dependency fixtures.
   Establish baseline gameplay and bandwidth/tick-cost measurements.
2. **Prove one complete vertical slice.** Join and revert a world containing
   a player, a scripted object, a device, an attachment, an animation and a
   persistent HUD item. Delay object creation, lose updates and revert twice.
   Require correct final state, no stale action and bounded recovery.
3. **Replace campaign state and transitions.** Cover all required script
   outputs, saves, roster changes, BSP and mission changes with the same
   contracts. Test successful and failed loads and repeated session teardown.
4. **Complete presentation and AI behavior.** Validate camera, animation,
   sound and HUD timing visually; prove actor creation/action dependencies and
   recovery. Dedicated hosts and dead/spectating players need explicit coverage.
5. **Decide shared combat/prediction scope.** Use measured reproductions to
   decide whether to retain, repair or replace those modules. A full rewrite
   commitment includes this work; the campaign layer alone cannot promise to
   fix all explosive or movement desync.
6. **Qualify and retire the old path.** Complete platform, transport, map-family,
   campaign and soak validation. Remove replaced logic only after its
   responsibilities and meaningful coverage have migrated.

For one developer this should be planned as a substantial, potentially
multi-month engineering and validation effort, not a short file rewrite.
That is a planning judgment, not a measured schedule. A defensible delivery
estimate comes after the baseline and vertical slice establish content coverage,
snapshot cost and shared-engine work. Four-player gameplay matters to the
reported use case, but development automation must obey the repository's
two-instance limit; larger human sessions need a separate validation plan.

## Isolation, compatibility and validation gates

This investigation concerns existing general co-op behavior. Future focused
fixes may modify the responsible general implementation. New replacement logic
belongs in co-op-owned modules; shared-engine hooks must be minimal, justified
and marked `port:`. MCC semantics, expanded names, extra inventory,
checkpoint storage and package progression stay in their MCC modules.
Preserve Xbox/CE paths, local campaign and split-screen behavior. Do not use a
rewrite to move compatibility logic across those boundaries.

New wire layouts or simulation agreements require a coordinated network-version
change and rejection of incompatible peers before gameplay. Coordinate with
OpenCE rather than promise old-client compatibility through a rewrite.
If an experimental implementation is added, keep it off/stock by default
until explicitly authorized otherwise; never let peers silently choose
different replication models within one session.

Implementation acceptance must include:

- Actor/unit/object creation, deletion, slot reuse, inventory/seat ownership,
  deaths, revivals, respawns and current-roster checkpoint restores.
- Scripts, devices, named and unnamed objects, attachments, recorded/custom
  animations, audio, camera, HUD, allegiances and input inhibition.
- Initial join, late join, reconnect, missing/failed construction, repeated
  reverts, cinematic skips, BSP travel, mission progression and failed loads.
- Loss bursts, delay, duplication, reordering, backpressure, malformed inputs,
  interrupted snapshots and bounded recovery under capacity pressure.
- Representative Xbox, CE and MCC campaigns and transitions between families;
  gameplay with the replacement disabled and enabled; competitive multiplayer
  checks wherever shared code changes.
- Windows/Linux builds, Android guest and web builds, native/browser WebRTC
  and internet gameplay, host/client role swaps, split-screen and dedicated
  hosts, plus `net_compare.py` for affected simulation agreements.
- Rendered cutscene/HUD/camera checks at native resolution, long-session
  stability and campaign completion. A load, a green harness or matching
  player positions cannot substitute for these.

## Validation actually performed

The unchanged base implementation was inspected through session, tick,
object, actor, damage, script, checkpoint, BSP and progression paths.
Transport architecture was reviewed through its owning documentation; this
was not an exhaustive transport implementation or security audit.

Executed locally with bundled Python 3.12, clang 23.1.4, the x86 MSVC 14.44
runtime and Windows SDK 10.0.26100.0:

- Five extracted-production-function probes, all confirming the observations
  above. The animation probe includes a positive control. These characterize
  present defects; they deliberately are not installed as CI expectations
  that the bugs must remain.
- Focused pytest selection: 262 passed initially, 12 compiler failures.
  All 12 were in `test_coop_script_players.py`, due to the Windows CRT
  marking `strlwr` deprecated under `-Werror`. Re-running those unchanged
  cases with a temporary compiler wrapper defining
  `_CRT_NONSTDC_NO_DEPRECATE` produced 12 passes. No game/test source was
  altered to obtain these passes: 274 selected cases ultimately executed
  successfully across the two runs.
- `tools/test_checkpoint_timing.py` executed separately and passed at
  30, 60, 120 and 990 rendered frames per second.

Pytest selection: `test_checkpoint_epoch.py`, `test_coop_checkpoint.py`,
`test_coop_checkpoint_recovery.py`, `test_mcc_network.py`,
`test_mcc_checkpoint.py`; and harness tests `test_coop_gate.py`,
`test_coop_monitor.py`, `test_coop_script_players.py`, `test_coop_wipe.py`,
`test_coop_host_looping_sounds.py`, `test_coop_looping_sounds.py`,
`test_coop_sound_sync.py`. The standalone checkpoint timing script is not a
collected pytest test.

No game processes, new platform builds, live co-op sessions, internet tests
or campaign playthroughs were run for this investigation. The screenshot's
individual failures were not reproduced. These limits prevent claims about
their exact causes, the frequency of the demonstrated defects in real sessions,
or full compatibility of a proposed replacement.

Self-review checked the probe against production code, separated observations
from hypotheses, and reviewed the proposed boundaries and lifecycle.
The report's local links and named implementation functions were checked.
Temporary test dependencies, compiler wrapper, binaries and generated caches
were removed after retaining the source probe and this validation record.
No original maps, saves, builds or root-checkout edits were changed.
