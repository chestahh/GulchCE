# Staged network co-op rewrite plan

Status: proposed plan, 12 November 2026. **Implementation is not authorized by
this document. Nothing is to be published.** All milestones below are planned,
not completed. The prior [feasibility assessment](coop_rewrite_feasibility.md)
records investigation evidence; its tests do not validate this future design.

Planning baseline: GulchCE testing `887d9ac9d`, with investigation commit
`da6446d39` on local branch `investigate-coop-rewrite`.
Recheck upstream and working-tree changes before any authorized implementation;
do not silently rebase or discard ongoing work.

## Scope and completion definition

The full network co-op system is in scope: admission and roster lifecycle,
loading, world synchronization, object identity, inventory, player and vehicle
prediction, AI, combat/projectiles, scripts/devices, presentation, checkpoints,
host controls, BSP travel, campaign progression, dedicated hosting, transport
integration, cross-platform behavior and teardown.

Every subsystem must receive a documented replacement, justified refactor or
validated retention decision. Retaining a sound transport or engine service
does not remove it from integration and acceptance testing. Movement, AI and
combat have mandatory milestones; they are not optional follow-ups to the
reported bugs. The screenshot provides examples, not the scope boundary.

Completion means the supported co-op behavior works end to end under the
declared operating conditions, all responsibilities have explicit owners,
known migration defects are resolved, the superseded co-op implementation can
be retired safely, and the complete resulting implementation passes final
review and regression gates. A playable vertical slice is an intermediate gate.

Existing competitive multiplayer, local campaign, local split-screen and
Xbox/Custom Edition/MCC separation remain preservation requirements. This
does not introduce host migration, OpenSauce support, new map formats, arbitrary
changes to authored campaign semantics, a new renderer or a new transport
library. Automatic seamless mission progression, new character customization
and changed skip-prompt policy are separate product decisions; existing behavior
is preserved unless explicitly changed.

## Rules governing refactoring

Read [AGENTS.md](../AGENTS.md) and [CODINGSTANDARDS.md](../CODINGSTANDARDS.md)
before implementation. The assignment combines fixes to existing general
co-op behavior, a replacement of co-op-owned port logic, necessary integration
and separate MCC compatibility work.

Necessary refactoring is part of the programme, with these limits:

- Start in the port-owned `network_*.c` and `coop_*.c` modules. Separate
  serialization, replication decisions and engine-side application only where
  doing so establishes an ownership or test boundary needed by the rewrite.
  Do not create a generic networking framework or relocate code for aesthetics.
- Retain established engine functions and Xbox/CE loading paths. Do not extract,
  rename or restructure inherited engine implementations merely to make the
  new design convenient. General defects may be fixed in their responsible
  implementation with evidence; new integration uses existing interfaces or
  minimal necessary hooks.
- For every shared hook, record why existing feature-module interfaces cannot
  do the job, the affected callers/map families, activation guards, cleanup
  and validation. Mark changes under `source/` with `port:`.
- MCC interpretations, extra names/inventory, checkpoint storage, camera
  semantics, package admission and progression remain MCC-owned. Shared
  services may coordinate lifecycle without absorbing those semantics.
- Separate behavior-preserving extraction from protocol/behavior changes in
  reviewable commits. A claim of preserved behavior needs checks, including
  wire output, ordering and lifecycle where affected.
- A phase cannot depend on known structural defects being cleaned up later.
  Review ownership, interfaces, failure paths, state lifetime and complexity
  at its start and after each meaningful integration.
- If a concrete refactor conflicts with isolation, stop that change and document
  the specific conflict. Continue independent compliant work. This plan is
  not an exception to the isolation rules.

The existing network guide and [NETCODE.md](../port/linux/NETCODE.md) remain
the starting reading route. Update those and the relevant MCC boundary
inventory as implementation changes their described behavior.

## Target contracts

These are proposed design constraints to prove, not already implemented APIs.

| Concern | Required contract |
| --- | --- |
| Authority | The host decides world state, damage, spawns, inventory ownership and lifecycle operations. Clients predict their own actions within validated bounds. |
| Identity | Every reference identifies the correct session/world and object lifetime. Reusing a slot cannot apply an old action to a new object. Define how existing datum salts map to that contract before changing handles. |
| Session versus world | Connections, roster, local-controller mappings and permissions outlive checkpoint restoration. Map/world generations, prediction histories and gameplay actions do not. |
| Persistent state | Record authoritative state independently of transport capacity. Snapshots and updates represent both presence and absence, including empty inventories and removed attachments/nav points. |
| Events | Classify each action as persistent state, consequential one-shot action, sampled presentation or disposable cosmetic effect. Give each a delivery, dependency, deduplication and expiry policy. |
| Dependencies | Receiving is distinct from applying. Defer required actions while dependencies arrive, within bounded queues/timeouts; reject cycles and impossible references. Never silently mark failed application complete. |
| Snapshots | Identify the captured world/tick and component versions. Validate bounded fragments, reconcile mutations during transfer and acknowledge required construction/application, not merely receipt of an end marker. |
| Readiness | Peers progress through explicit admission, content-ready, world-ready and playing states. A missing cosmetic effect must not prevent play indefinitely; a missing player/BSP/inventory dependency must not be declared ready. |
| Time | Keep network ordering and operation identity distinct from restorable simulation/script time. Define pause, frame stalls, reconnect and wraparound behavior. Presentation may interpolate behind authority without delaying local input. |
| Operations | One accepted world transition at a time, with operation identity, authority checks, duplicate handling, completion/failure and cancellation rules. Late packets or stale UI actions cannot trigger another transition. |
| Bounds | Fixed budgets for memory, snapshot size, per-peer work, queued dependencies and traffic. A slow/malformed peer cannot monopolize the host or force unlimited recovery. |
| Diagnostics | Correlate peer, map identity, world, operation, object generation and tick. Explain reject/defer/expire/timeout decisions with bounded logging and useful retained summaries. |

Proposed responsibilities are a small co-op session/lifecycle owner, protocol
codecs, snapshot/repair orchestration, world replication, actions and
presentation. Use existing dedicated modules where suitable. The names and
exact file split are decided in M1; a global context containing every
subsystem's internal state is not the goal.

Network serialization uses explicit widths, byte order, lengths, ranges and
versioned meaning. Do not transmit raw engine structs or checkpoint memory.
Preserve C89, 32-bit ABI, deterministic-maths and Android guest-import rules.

## Migration and rollback

- Keep the baseline path available during development. The experimental
  replacement defaults off and is selected before session creation, locked
  for that session. There is no live toggle during a mission.
- A session uses one coherent implementation. If a staged build delegates
  an unmigrated responsibility to existing code, document the adapter and
  make it the only active owner. Disable duplicate old send/apply paths for
  migrated responsibilities. Avoid combinatorial per-feature protocol modes.
- Pure shadow comparison may observe or serialize state; it must not apply
  damage, run scripts twice, allocate gameplay objects or send duplicate events.
- Cross-peer agreement is explicit. Raise `HALO_PORT_NETWORK_VERSION` for
  changed wire data or simulation agreements. Revalidate version/fixtures at
  each such phase; do not retain version 44 merely because a build is private.
  The exact future numbers are assigned against the then-current upstream.
- Old clients are refused before gameplay when incompatible. Retaining an
  old local path does not promise compatibility with old binaries.
  Coordinate eventual OpenCE agreement separately; GulchCE camera modes and
  MCC admission remain distinct responsibilities.
- Roll back by leaving the session and using the known baseline build/path.
  Do not downgrade a live world or reinterpret a new snapshot as an old one.
  Keep checkpoints/profile compatibility explicit; test on isolated save copies.
- No branch pushes, PRs, upstream coordination messages, default change or
  release publication are authorized now. Later permission to implement
  would not by itself authorize those external actions.

## Refactoring map and state ownership

These are responsibility boundaries, not permission to move all named source
files. Candidate new filenames are illustrative and are not created by this
planning assignment.

| Current area | Planned separation or retained boundary | Why it is needed |
| --- | --- | --- |
| [network_coop.c](../port/linux/game/network_coop.c) | Small existing facade plus co-op lifecycle, persistent world, action delivery and presentation owners; candidates `coop_session.c`, `coop_world.c`, `coop_events.c`, `coop_presentation.c` | Separate lifetimes and testable contracts; prevent camera/HUD policy and transport queues from owning authoritative gameplay state. |
| [network_distributed.c](../port/linux/game/network_distributed.c) | Existing shared dispatch/tick boundaries with narrow co-op protocol and operation dispatch; extract shared codecs only where both modes need the same verified service | Avoid a second authority path and preserve competitive multiplayer. Keep new co-op policy out of generic dispatch. |
| [network_objects.c](../port/linux/game/network_objects.c) | Retain engine object construction/allocation integration; isolate validated identity, snapshot and application interfaces as needed | Allow generation-safe dependency repair without replacing the engine allocator or running two object owners. |
| [network_actors.c](../port/linux/game/network_actors.c) and [network_damage.c](../port/linux/game/network_damage.c) | Explicit capture/decode/apply and history lifetimes within their responsible modules; small helpers when justified | Make action replay, authority and restored-world cleanup testable without discarding existing safety checks. |
| [coop_scripts.c](../port/linux/game/coop_scripts.c) and scaling helpers | Keep authored script adaptation and extra-enemy/pickup/vehicle policy independent of transport | A resend or checkpoint must not execute policy twice or change the campaign's semantics. |
| [players.c](../source/game/players.c), [game.c](../source/game/game.c), [main.c](../source/main/main.c) | Preserve established implementations; existing hooks first, minimal additional lifecycle/application hooks only if necessary | These own the engine boundaries. Their presence does not authorize broad source refactoring. |
| [game_state.c](<../source/saved games/game_state.c>) | Existing save/load boundary plus explicit participation of co-op-owned checkpoint data where required | Saved gameplay, network sessions and derived caches require different restore behavior. |
| [mcc_network.c](../port/linux/game/mcc_network.c), [mcc_checkpoint.c](../port/linux/game/mcc_checkpoint.c) and other MCC modules | MCC-owned participation in common lifecycle interfaces | Restore extra inventory/names/script/campaign semantics without passing Xbox/CE through MCC code. |

For each extracted responsibility, record the old callers and state, proposed
interface, activation, initialization/disposal, error handling, affected family
paths, regression tests and old-code retirement condition. If the change merely
moves lines without improving one of those contracts, do not perform it.

| State class | Owner and lifetime | Revert/restart rule |
| --- | --- | --- |
| Peer identity, permissions and local controllers | Session manager; connection/session lifetime | Preserve eligible current members; revoke departed members. Never restore old access rights from a save. |
| World/object identity and component versions | Replication owner; current world/object lifetime | Advance generation at the declared boundary; reject old references and renew state for surviving objects. |
| Script-authored persistent world/presentation state | Respective co-op or MCC owner; checkpoint-participating state | Restore a validated saved representation or reconstruct from authoritative engine state with a documented rule. |
| Transient input, hits, effects and event dependencies | Producing subsystem; current world/action lifetime | Discard cancelled-world work, clear histories and rebuild only after the new baseline is ready. |
| Derived send/interpolation caches | Respective sender/presentation owner | Invalidate or seed deliberately; do not confuse an empty cache with an empty authoritative world. |
| Player configuration, progression and original saves | Existing configuration/profile/save owners | Preserve user data; mission restart does not imply reset of preferences or campaign progress. |

No single blanket `memset` or universal reset callback should decide all these
lifetimes. Snapshot and checkpoint participants must declare whether they are
saved, reconstructed, retained or discarded, with ordering dependencies.

## Milestone overview and dependencies

| Milestone | Deliverable | Main dependency |
| --- | --- | --- |
| M0 | Complete behavior/state inventory and measurable baseline | Existing investigation |
| M1 | Minimal refactoring and ownership boundaries | M0 |
| M2 | Protocol, session lifecycle and bounded delivery foundation | M1 |
| M3 | Object identity, inventories and usable snapshots | M2 |
| M4 | Script-driven world state and dependency-aware actions | M3 |
| M5 | Complete restore/restart mechanism and host controls | M3–M4 |
| M6 | Player movement, vehicles and prediction | M3, M5 |
| M7 | AI, recorded actions and actor lifecycle | M3–M6 |
| M8 | Weapons, projectiles, damage and death reconciliation | M3, M6–M7 |
| M9 | Camera, animation, sound, HUD and spectator presentation | M4–M8 |
| M10 | Campaign/session policies and all integration routes | M5–M9 |
| M11 | Whole-system platform, transport, capacity and campaign qualification | M0–M10 |
| M12 | Controlled cutover, retirement and final review | M11 |

Each milestone includes relevant platform/transport checks when it changes
those paths; M11 is final comprehensive qualification, not the first Android,
web or internet test. All newly migrated state must immediately enter the
join/revert/restart tests. M5's early gate covers the implemented domains;
its full gate is rerun with every domain through M10 and at M11/M12.

### M0 — Inventory, expected behavior and baseline

**Work:** Build a state-and-action ledger from every message kind, producer,
consumer, script side effect and checkpoint participant. For each entry record
authority, identifiers, serialization, dependency, update/removal, late-join,
restore, BSP change, unload and failure behavior, plus tests and diagnostics.
Cover all exposed co-op settings and supported controller/player arrangements.
Classify entries as replace/refactor/retain with rationale.

Use the five investigation probes as fault demonstrations, then specify
desired-behavior tests rather than adopting tests that require bugs to persist.
Capture baseline behavior, bandwidth, tick cost, correction magnitude and
recovery latency in repeatable scenarios. Extend observation where existing
logs only report player positions once per second. Plan asset-free fixtures
for scripts/events and identity faults: current StageKit's simple stages do
not already supply complete campaign-script coverage.

**Gate:** Every current co-op message and engine hook has a ledger entry;
each entry has an owner, expected behavior and validation route. Known faults
are reproducible or explicitly unconfirmed. The operating envelope, numeric
budgets and representative content/platform matrix below are frozen before
behavior changes. Unsupported cases and historical quirks are named, not hidden.

**Evidence:** Inventory, baseline revisions/configs, scenario manifest, logs,
performance distributions and a runnable comparison/fault-injection plan.

### M1 — Refactor only what the replacement needs

**Work:** In co-op-owned port code, introduce narrow interfaces for state
capture, validated decode, engine application and lifecycle reset. Separate
transport enqueue success from authoritative state mutation. Initially preserve
existing externally observable behavior; actual fault corrections belong in
separate behavior changes. Inventory static state and select explicit lifetimes.
Retain existing engine-facing entry points where that avoids shared-source churn.

Begin with one boundary, prove its preservation, then extend the same pattern.
Keep shared multiplayer services intact behind existing contracts; do not move
all of `network_distributed.c` or `source/game/players.c` pre-emptively.

**Gate:** No unintended wire, timing, ordering, save or gameplay changes.
Baseline path passes focused function tests, representative Xbox/CE/MCC sessions,
disabled-path lifecycle tests and required affected-platform builds.
Byte comparisons are meaningful only for canonical fields, not uninitialized
padding. No new circular ownership or duplicate lifecycle callbacks.

**Evidence:** Before/after call map, rationale per extraction, wire/behavior
comparison and combined review. Protocol version stays unchanged only if the
agreement truly remains unchanged.

### M2 — Protocol and session foundation

**Work:** Implement the selected experimental-session binding, typed codecs,
sender/role/ownership checks, generation-aware dispatch and operation lifecycle.
Retain existing transport initially, but define reliable versus sampled traffic,
maximum message/fragment sizes, scheduling priorities, rate limits and queue
backpressure. Avoid loading bulk snapshots ahead of every urgent control
message on an unbounded ordered stream.

Define which messages are legal while loading, synchronizing, paused, playing,
reverting and disconnecting. Pin native/browser behavior to the same gameplay
contract. Represent a transition's timeout/failure explicitly.

**Gate:** Malformed/truncated/oversized/unknown messages, forged sender,
wrong world, wrong direction, wraparound, duplicates and flood attempts cannot
mutate unauthorized state or allocate unbounded resources. Identical fixtures
decode consistently across native, Android guest and web builds. Incompatible
builds fail admission. Native/browser transport exercises framing, ordering,
disconnect and reconnection with the new contract before wider adoption.

**Evidence:** Wire specification/fixtures, adversarial codec tests, resource
bounds, version rationale and transport traces.

### M3 — Entity identity, inventory and snapshots

**Work:** Migrate create/delete/reuse, player-unit bindings, seats/parentage,
weapons/ammunition/grenades, persistent object state and client-only effects.
Define snapshot capture and catch-up for a world that may change during transfer.
Use generation-safe references, explicit removals, dependency ordering, bounded
repair requests and readiness for required objects. Reuse validated allocation
and object construction services; changing network identity must not require
rewriting the engine allocator.

Preserve MCC extra inventory and name adapters; test their participation without
putting their formats into Xbox/CE handlers.

**Gate:** Create-before-state and state-before-create, delete/recreate same slot,
same-handle checkpoint revival, empty inventory, missing parent/weapon, failed
construction, late join during mutation, partial fragments and reconnect all
converge or fail explicitly within budget. No ghost player, stolen inventory,
stale resurrection or action applied to a replacement object. Duplicate snapshot
ends do not reset active gameplay. Competitive multiplayer retains its contract.

**Evidence:** Canonical entity/component comparisons, fault traces and real
two-instance join tests in each map family.

### M4 — Script/world state and actions

**Work:** Migrate devices/groups, names, transforms, looks, attachment graphs,
breakable surfaces, allegiances, scripted vitality and other persistent script
outputs. Implement action policies for consequential one-shot operations.
Persistent state commits before send attempts. Complete replacement snapshots
and/or versioned removals repair lost deletions.

Classify every script side effect identified at M0. Keep host-only script
execution and existing more-than-two-player script adaptation in their proper
owners. Define prerequisite resolution, deadlines, supersession and bounded
retry. Invalid cyclic attachments or impossible object references fail safely.

**Gate:** The demonstrated loss, missing-object, sequence-gap, removal and
overflow failures have desired-behavior regressions that pass and meaningful
negative controls where appropriate. Devices, names and attachment graphs match
host authority after recovery. Equivalent authored semantics work under unrelated
map names/checksums. Xbox/CE paths and MCC-specific semantics remain isolated.

**Evidence:** Completed script-effect ledger, state/removal tests and scenarios
covering device use, scripted creation/deletion and checkpoint-sensitive state.

### M5 — Checkpoint recovery, mission restart and host controls

**Current evidence:** The ordinary
[pause handlers](../source/interface/ui_widget_event_handler_functions.c),
`pause_game_restart_at_checkpoint` and `pause_game_restart_level`, return
false whenever `network_coop_active()` is true, including on the host.
[MCC UI](../port/linux/game/mcc_ui.c) has distinct host-checked routes:
`mcc_ui_event_function` calls the host revert path and
`mcc_ui_restart` delegates to `mcc_ui_restart_network_game` in
[mcc_ui_network.inl](../port/linux/game/mcc_ui_network.inl).
These are inspected code paths, not proof of working live UI.
Dedicated `sv_restart` is a server-process restart request, not evidence of
mission restart. Automatic team-wipe recovery is another distinct operation.

**Work:** Specify and implement host-requested revert and restart as authoritative
session operations. Validate eligibility before destructive world work; queue
at a safe engine boundary. Retain current membership/controller mapping and
host options; cancel old-world gameplay and restore/rebuild every migrated
state owner. Hold joining peers at a defined admission boundary while a
transition runs. Resume each peer only after required state is usable.

| Operation | Required user-visible behavior |
| --- | --- |
| Revert checkpoint | Restore the latest valid checkpoint for this mission/world identity, including participating sidecar state. Keep the current session roster; do not resurrect departed players or erase late joiners. Clearly reject missing, incompatible or unsafe saves. |
| Restart mission | Reload this mission from its beginning with current roster, difficulty and host options; invalidate the old world's network state. Do not mark the mission completed, advance the campaign or restart the server process. |
| Duplicate/busy request | Execute one accepted operation once; acknowledge duplicates and report a distinct busy/rejected result for conflicting requests. |
| Client request | Clients cannot invoke host authority through UI, console or forged packets. An optional request/vote system is not implied. |
| Failed operation | Report the failure and a defined recoverable session state. Do not silently substitute respawning for an explicitly requested revert or leave only the host restarted. |

Make both controls discoverable in applicable Xbox/CE and MCC pause paths;
keep MCC UI construction separate and regenerate CE XML through its generator.
Provide a role-checked dedicated-admin route if no equivalent exists.
Reuse ordinary confirmation conventions, with host eligibility checked again
when the confirmation is accepted. Cover keyboard, controller and touch.

Separate automatic script-failure loop protection from deliberate host commands:
the present `main_coop_host_revert` has a third-revert-in-a-minute guard.
Preserve protection against runaway scripts, but make manual limits/failures
explicit rather than inheriting a silent respawn fallback. Default team-wipe
policy changes require their own decision.

**Gate:** Test host/client role, no checkpoint, stale map checkpoint, save-write
failure, in-flight save, roster changes, dead/seated/spectating players,
mid-cutscene, BSP change, late join, disconnect and repeated requests.
Run at least 20 successive revert/restart operations in the qualification
scenario, including two reverts requested at the same simulation tick.
No stale effects/hits/input, stranded clients, roster loss or save corruption.
Visual confirmation and errors work at native resolution and split-screen sizes.

**Evidence:** Operation state/transition traces, save-integrity checks, UI
captures, two-instance sessions for every map family and rerun results as later
subsystems join the recovery contract.

### M6 — Player movement, vehicles and prediction

**Work:** Refactor/rebuild the co-op input, history, prediction acknowledgement
and correction paths under the new identity/time contract. Cover local and remote
players, aiming, scripted input inhibition, suspension, teleportation, crouch,
jump, falling, damage impulses, seats, boarding/exiting, moving platforms and
standing on vehicles. Account for stock and custom vehicle semantics; retain
MCC-specific movement/seat/camera adapters.

Choose prediction depth and reconciliation based on measured behavior.
Do not assume full world rollback is necessary. Specify how a lost input edge,
stalled client, missing floor/BSP or changed seat affects simulation and recovery.

**Gate:** Responsive local input and host-approved convergence under the network
profiles below, with bounded correction distributions, no accumulated speed/
position exploitation and no repeated one-shot input. Vehicle/floor transitions
survive latency and checkpoint/restart. Simulation checks and required host/client
swaps pass; multiplayer and disabled-path behavior are preserved.

**Evidence:** Per-tick trajectories and authoritative correction histories,
moving-platform/vehicle scenarios, input abuse tests and native/browser play.

### M7 — AI and scripted actor behavior

**Work:** Retain host AI decision ownership while rebuilding actor replication
and action/animation dependencies where required. Cover spawn waves, squads,
idle/active transitions, movement, perception-driven state visible to players,
seats, dropships, fleeing, melee/leaps, death, Flood revival and recorded actions.
Keep extra-enemy scaling, actor capacity and overflow behavior explicit.

**Gate:** Clients never run competing authoritative AI or spawn duplicate actors.
Actors recover after missed updates/creation and interest changes; important
actions are applied once or superseded according to contract. Restore/restart
rebuilds current actor state without replaying obsolete speech/actions.
At actor/object capacity boundaries, rejection and resource use stay bounded.

**Evidence:** Scripted encounters, actor-state/event traces, recorded-path and
vehicle scenarios, synthetic maximum/boundary tests and representative gameplay.

### M8 — Combat, projectiles and authoritative outcomes

**Work:** Audit and replace/refactor weapon state, firing, reload, ammunition,
grenade throws, melee, projectile identity, bounce/attachment/detonation,
splash/chained effects, friendly fire, kill ownership and damage aftermath.
Define a host-accepted shot/throw identity linking predicted visuals to
authoritative outcomes. Select policies by projectile behavior, not map name.

Preserve weapon ownership, rates, history, line-of-sight, finite-number and
damage bounds. Resolve the interaction between shooter reports and host
simulation explicitly so each consequence has one authority. Include AI fire,
vehicle weapons, destructible scenery and a shooter disconnecting while an
explosive is active.

**Gate:** No duplicate damage, unauthorized hits, old-life kills or persistent
ghost explosives. Authoritative targets, kill credit and inventory agree;
predicted effects reconcile without duplicated explosions/impulses. Test lost,
delayed and duplicated reports, moving targets/vehicles, sticky grenades,
chain reactions, misses, reload/cancel and checkpoint/restart during flight.
Existing damage/security regressions remain meaningful and pass.

**Evidence:** Shot-to-outcome traces, negative/forged-report tests, visual
projectile comparisons and cross-platform combat sessions. Matching health
alone does not pass projectile presentation.

### M9 — Complete co-op presentation

**Work:** Integrate a consistent presentation timeline for camera, letterbox,
fades, screen effects, authored unit/scenery/recorded animations, actor speech,
script dialogue/music, HUD/objectives/nav points, mission timers, shake,
allegiances, input inhibition and dead-player/spectator views.

Represent ongoing animations/music as reconstructible state where appropriate.
Late joining must seek or intentionally expire past transient effects rather
than replaying everything from the start. Use authored camera relationships
where available; document validated fallbacks instead of guessing by map identity.
Cover dedicated hosts without rendering/audio and local split-screen observers.

**Gate:** Rendered host/client comparison at native resolution, including
2560x1440 on the available desktop when applicable, plus mobile/browser and
split-screen layouts. Validate timing/alignment, clipping, letterboxing, camera
anchors, animation phase and audible starts/stops through join/revert/restart.
Controls and camera ownership recover after interrupted cutscenes.
HUD-state agreement alone cannot pass visual/audio acceptance.

**Evidence:** Synchronized captures and sound observations tied to ticks/worlds,
late-join and skip/revert traces, and completion of every presentation ledger entry.

### M10 — Full campaign and session integration

**Work:** Complete the campaign loop and operational policies: level completion,
next-map selection, explicit restart, difficulty/options, team wipe/respawn,
safe spawning, friendly fire/AFK rules, extra enemies/pickups/vehicles,
votes, player departure/rejoin, dedicated rotation and session end.

Preserve chat/voice, browser/LAN discovery, map sharing, content identity,
authorization and load-progress integration. They need not be rewritten
wholesale, but must work during loading, transitions and cleanup.
Preserve GulchCE camera policy and other host-approved feature decisions.
Keep Xbox campaign mapping, CE standalone behavior and MCC package progression
in their respective owners.

**Gate:** Every advertised entry and exit route is exercised. Mission completion
does not accidentally destroy the session; intentional return to lobby is
distinguished from failure. Missing next content has an actionable result.
Run Xbox → CE → MCC → Xbox transitions, failed loads and repeated teardown.
Verify public/private and passworded-host policies and local/split-screen
behavior. No untracked legacy owner remains in the experimental co-op path.

**Evidence:** End-to-end session/progression matrix, settings/feature parity
ledger, failure diagnostics and cumulative recovery results.

### M11 — Whole-system qualification

**Work:** Run the complete final matrix below against pinned candidate builds.
Profile representative low/high actor loads and constrained peers. Exercise
all supported platform roles and native/browser transports, actual internet
play, full campaign traversal and extended sessions.

**Gate:** All functional, security, lifecycle, presentation and performance
criteria pass within the frozen operating envelope. No unexplained campaign
blocker, world divergence, crash, unbounded queue or leak is accepted as a pass.
Missing assets, hardware, testers or internet evidence leave that coverage
explicitly incomplete; they do not become silent waivers.

**Evidence:** Reproducible result index with exact revisions/builds/configs,
map/content identities, network profiles, role assignments, logs/captures,
failure resolutions and remaining support limits.

### M12 — Cutover and retirement

**Work:** Review the complete resulting implementation across milestones.
Remove superseded co-op-only paths, temporary migration adapters and stale
documentation after mapping every responsibility/test to its replacement.
Retain code still required by multiplayer, local play, platforms or MCC.
Retire the experimental selector only with an explicitly authorized default/
rollout decision. Keep a recoverable known-good build/commit and save policy.

**Gate:** Review followed by the required final builds and affected full
integration/regression matrix on the actual post-cleanup code. Default/off-path
behavior and upgrade/rejection behavior are proven. No obsolete co-op writer,
handler or cache remains reachable; no meaningful diagnostic/test was removed
for a line-count target. If review or validation changes code, rerun affected
gates before completion.

**Evidence:** Retirement map, final review and validation record, player-facing
documentation and release-readiness decision. Publication remains a separate
authorized action; no PR description is produced unless requested.

## Validation matrix and measurable criteria

These are proposed acceptance requirements, not measured results. M0 must
name the test hardware/content and freeze budgets; changes need a recorded
reason before comparison, never a weakened threshold after a failure.

### Network conditions

| Profile | Injected conditions | Required outcome |
| --- | --- | --- |
| P0 | Loopback/LAN, no artificial loss | Functional baseline and timing/cost measurements. |
| P1 | 100 ms added RTT: 50 ms each direction, up to 20 ms jitter, 5% datagram loss | Normal play, recoverable state/actions, bounded correction and repair. |
| P2 | 250 ms added RTT, up to 50 ms jitter, 10% datagram loss plus seeded duplicate/reorder schedules | Stress convergence, bounded queues and explicit recovery/failure; record degraded feel separately. |
| P3 | 500 ms–2 s bursts of datagram loss while reliable traffic is delayed independently | Missing-event and dependency repair; no lost persistent removals or stale-world application. |
| P4 | Full transport stalls around configured timeout, interrupted transfer, disconnect/reconnect | Timely explicit session outcome and clean recovery/rejoin; no indefinite waiting or corrupted live roster. |
| P5 | Truncation, corruption, invalid lengths/IDs/times/roles, floods and exhausted resource limits | Reject safely, with bounded CPU/memory and useful diagnostics; no unauthorized side effects. |

Measure delivered delay/loss and distinguish application message scheduling
from transport fault injection. The current latency/loss knobs do not by
themselves prove deterministic reordering, reliable-stream stalls or identical
conditions on every platform. Add test capability where necessary.
Do not imply that traffic can converge during a permanent disconnection.

### Functional and resource criteria

- Zero application of an old-world or wrong-object-lifetime action.
- At most one authoritative application of each accepted consequential action;
  eventual application or explicit supersession/expiry/failure while the peer
  remains connected and dependencies are satisfiable.
- Exact agreement for discrete persistent state at a quiescent observation
  point; numeric state within declared encoding/physics tolerances. Do not
  compare raw memory, cosmetic random seeds or asynchronous clocks.
- Proposed steady-state repair target under P0/P1: within two seconds after
  delivery resumes and required dependencies exist. Snapshot/content loading
  has separate size/bandwidth/platform budgets; do not impose that target on
  complete map conversion or package transfer.
- Every in-flight operation has a progress deadline and a hard resource bound.
  M0/M2 define numeric deadlines for content loading, snapshot transfer,
  dependency repair and resynchronization independently. No timeout may be
  satisfied by silently dropping required state.
- On declared supported test hardware/content, the host sustains the 30 Hz
  simulation budget. Record p50/p95/p99 tick cost, queue peaks, bandwidth per
  peer, snapshot bytes/time and correction distributions. Fix numeric memory,
  bandwidth and correction budgets from M0 evidence before adoption.
- Capacity tests cover zero, normal, maximum supported and one-over-limit inputs.
  The inspected headers declare 128 network players/machines, 1,024 actors and
  8,192 objects. These are capacity contracts to audit, not evidence that a
  128-player campaign runs acceptably. Do not silently lower advertised limits.
- Repeated-operation qualification: at least 20 join/leave or world-transition
  cycles and 20 restore/restart operations in deterministic scenarios; soak for
  at least two hours on representative sessions. No increasing retained-state
  count or unexplained monotonic memory growth. Baseline engine allocations
  and bounded caches must be distinguished from leaks.
- Complete all ten stock campaign missions in connected co-op, including
  transitions and representative skips, deaths/reverts and late joins.
  Use full representative CE/MCC campaigns where supplied content supports
  them, plus a feature-based fixture matrix; do not claim all custom maps are
  certified. Four-player human sessions must be included before a four-player
  experience is declared qualified.

### Platform, player and map-family coverage

Automated tests run **at most two game instances concurrently**, with isolated
data/save folders and hidden windows or a virtual display. Two peers with
multiple local players can exercise four-player roster/script cases where
supported; that does not establish four independent network-peer behavior.
Use a separately arranged human multi-machine playtest for that evidence.

Build Windows and Linux, Android guest/app as affected, and web throughout.
Exercise compatible native role swaps and native ↔ Android, native ↔ web,
web ↔ web and Android ↔ web pairings where each role is supported; log any
unsupported hosting role explicitly. Verify same-build peers, previous-build
rejection and eventual matching OpenCE crossplay separately. Socket-level
success does not establish gameplay interoperability.

Use representative Xbox, CE and MCC content for each affected gameplay
contract. Include dedicated hosting, local split-screen, solo campaign,
experimental mode off, competitive multiplayer, and relevant GulchCE feature
policies. Check failed-load/unload and subsequent different-family sessions.
Native/browser WebRTC and actual internet play are required, not just loopback.

### Existing tools and coverage gaps

| Area | Starting tools/tests | Additional evidence required |
| --- | --- | --- |
| Engine logic | [harness](../tools/harness/README.md) | Tests execute real migrated functions, boundary cases and meaningful fault controls. |
| Real-engine scenarios | [StageKit](../tools/stagekit/README.md) | Extend scenario/observation coverage for campaign state; its current generator/runner is Linux-only. |
| Comparison | [net_compare.py](../tools/net_compare.py), [test_net_compare.py](../tools/test_net_compare.py) | Keep drift checks for unchanged agreements; add component-level oracles. The comparator's own tests do not validate gameplay. |
| Epoch/restore | [epoch tests](../tools/test_checkpoint_epoch.py), [checkpoint tests](../tools/test_coop_checkpoint.py), [recovery tests](../tools/test_coop_checkpoint_recovery.py), [timing script](../tools/test_checkpoint_timing.py) | Full world/presentation coverage, manual host actions and repeated mixed-domain restores. |
| Movement/combat | [elevator prediction](../tools/harness/tests/test_elevator_prediction.py), [hit validation](../tools/harness/tests/test_reported_hit_sight.py) | Vehicle/actor/projectile outcomes and adversarial traces, beyond player positions. |
| Protocol/MCC | [join protocol](../tools/test_join_protocol.py), [MCC protocol](../tools/test_mcc_protocol.py), [MCC checkpoint](../tools/test_mcc_checkpoint.py), [MCC lifecycle](../tools/test_mcc_lifecycle.py) | New agreed codecs, world transitions and strict family isolation. |
| Transport/server | [WebRTC runner](../port/web/tests/webrtc_native.mjs), [internet runner](../port/web/tests/internet.mjs), [dedicated tests](../tools/test_dedicated_server.py), [shutdown tests](../tools/test_p2p_shutdown.py) | Review runner concurrency first; real host/client co-op under new traffic and stalls. |

Retain `net_compare.py --swap` where versions legitimately interoperate.
For an intentional incompatible rewrite, do not bypass admission to compare
old/new builds in one session: compare each mode's matched peers against the
same scenario oracle, and test old/new rejection separately. This does not
waive drift checks for changes that retain the same agreement.

## Per-milestone completion record

Each milestone produces one concise record:

1. Exact revision, implementation scope, retained services and protocol/default
   decision; state-ledger entries migrated and still pending.
2. Design/self-review outcome and necessary simplifications, with shared-hook
   rationale and updated reader documentation.
3. Actual tests/builds/roles/content/settings/network profiles and results;
   separate source inspection, harness evidence, runtime, visuals, internet
   and campaign completion.
4. Known failures and missing coverage, their impact and the gate they block.
   “Most tests passed” is not completion of a failed required gate.
5. Verified cleanup, preserved evidence and retained artifact sizes.

Implementation phases may be broken into smaller commits without splitting an
invariant across an unusable state. If later submitted upstream, follow the
current review-size guidance without discarding coverage or provenance.
No subagents are required by this plan.

## Decisions and risks to settle at the gates

| Decision/risk | Resolution point and planning assumption |
| --- | --- |
| Scope of required shared-engine hooks | M0–M1: prove necessity and isolation compliance before code depends on them. |
| Exact protocol numbers and OpenCE integration base | M2 and every agreement change: recheck current upstream; no old-client compatibility promise. |
| Entity identity and snapshot capture/catch-up strategy | M2–M3: prove generation safety and bounded cost in the vertical slice. |
| Manual host controls | M5 required outcome; no-checkpoint errors and busy/duplicate handling are explicit. Preserve current roster, options and map-family semantics. |
| Checkpoint sidecar/save compatibility | M3–M5: define schema, identity and migration/refusal before writing player saves. Original saves remain untouched in tests. |
| Prediction and projectile reconciliation model | M6/M8: choose using scenario evidence and cost; these milestones cannot be omitted. |
| Automatic next-mission loading versus current lobby flow | M10: preserve current intentional flow unless a separate behavior change is agreed. |
| Default switch and old-path retirement | M12: explicit rollout decision after qualification, not an automatic consequence of finishing code. |
| Four-peer, mobile, browser and full-campaign availability | M0 plans access; M11 remains incomplete where required evidence is unavailable. |
| Growing scope/content exceptions | Every milestone: fix semantic classes; document justified exceptions and stop unrelated expansion. |

Estimate implementation effort after M0 and revise after the M3–M5 vertical
slice. The critical path includes content testing and cross-platform validation,
not just C changes. Do not promise a calendar date from the current function
probes or mark a subsystem complete based on file size.

## Planning-only review and validation

This document was reviewed for full subsystem coverage, dependencies,
ownership/isolation, migration rollback, host controls and measurable gates.
Source inspection verified the ordinary pause-action guards and the separate
MCC routes. Document links and referenced named functions were checked.

Only planning documentation was changed for this assignment. No implementation,
test harness, menu generator/output, protocol, configuration or game behavior
was changed; no game/build tests were rerun for these document edits. The prior
feasibility evidence remains dated evidence, not validation of this design.
No publishing or external coordination was performed.
