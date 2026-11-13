# M0: network co-op inventory and baseline

**Follow-up:** The [12–13 November measurement and qualification report](coop_m0_followup.md)
adds payload/snapshot/correction captures, web and browser interoperability,
revision-delta tests and stronger progression evidence. It updates the open
items below; this original report retains its historical run results. M0's
full exit gate remains open and M1 remains unstarted.

Investigation dated 12 November 2026. Baseline: `a8ac6e6b6`, whose game code
is GulchCE testing `887d9ac9d`; the intervening commits contain investigation
and planning only. Branch: `investigate-coop-rewrite`, local and unpublished.
No game, menu, protocol, configuration default or production test was changed.
The new Python utilities observe/index the existing implementation.

Read this with the [rewrite plan](coop_rewrite_plan.md),
[feasibility assessment](coop_rewrite_feasibility.md),
[source index](coop_m0_inventory.md) and
[machine-readable inventory](coop_m0_inventory.json). Numerical observations
are in the [baseline results](coop_m0_results.json).

**Status:** The source inventory and available Windows baseline are recorded.
The full M0 exit gate is not passed: missing observations and platform coverage
are listed below. No M1 refactoring or replacement implementation has started.

Read-only remote verification at the end found GulchCE testing at `fd3fc7758`
and OpenCE testing still at `e9adcf7a7`. GulchCE advanced three commits from
the source base used here, changing MCC resource selection/validation/sharing,
server catalog access, menu focus, tests and CI. The recorded runs are not
results for that newer revision. Preserve this baseline, then reconcile and
validate the relevant delta before selecting a later implementation base;
do not silently substitute binaries or rebase the evidence.

## Scope and interpretation

The inventory covers the complete co-op responsibility chain, not only the
reported failures: transport/session admission, content, roster, simulation,
objects, controls, vehicles, AI, combat, script effects, presentation, recovery,
progression, social/host policies and disposal. Shared multiplayer and local
campaign behavior are preservation requirements. MCC adaptations remain
separate from Xbox and CE implementations.

The generated index enumerates 61 distributed message constants, including
one retired kind; 35 session-message constants; 12 co-op event kinds;
312 engine references to 126 distinct symbols in 44 C files; 258 selected
file-scope static declarations; and 152 relevant settings. Of the 126 symbols,
124 resolve to function definitions. The remaining two are an authored tag
field and a session address table, explicitly identified in the index.
Saved-state callback lists are included in their existing order.

The index is lexical, not a preprocessed C call graph. It masks comments and
strings, preserves callback registrations and lists producer/dispatcher sites.
It does not establish that a reference executes, enumerate every underlying
transport library's internal packet, or certify every transitive engine state
allocation. Those services have explicit retained boundaries below. File-static
counts exclude function-local statics, global engine arenas and dynamically
allocated state; the semantic ledger includes those lifetimes separately.

Message, event, engine-hook and indexed state entries name a contract below.
That contract supplies authority, expected behavior, lifecycle,
replacement/retention decision and validation route; the index supplies exact
symbols, files and wire IDs. Settings and ordered callback lists support the
same ownership ledger. Retained code is not exempt from later qualification.

## Semantic contracts and ownership ledger

R means replace the co-op responsibility; F means refactor only where an
ownership/test boundary is needed; K means retain and qualify its integration.
These are design recommendations, not changes performed in M0.
H is the authoritative host; C is a client. R/U denote reliable/unreliable
delivery, not an application-level guarantee that the state was usable.

| Contract | Current owner, authority and representation | Expected lifecycle and behavior | Decision and validation route |
| --- | --- | --- | --- |
| session | Engine networking managers and message handlers; 35 encoded session messages; H owns roster/settings/start/end, C requests its own membership/actions | Discovery is not admission; version, identity, slots and content checks precede play. Preserve current membership across restore; clear connection identity on departure. Retired host-recovery names in the enum do not promise supported host migration. | K/F session integration. Join/protocol/startup/teardown tests; actual lobby, late join, leave/rejoin, failed load and host departure. |
| transport | `network_connection.c`, `xnet.c`, native `p2p*.c`, `posix_dtls.c`, web `web_p2p.c`; shared dispatch and batch kind 16 | Bounded decoding and stream/datagram ownership; loading/lobby services may run before gameplay dispatch. Reset batches/timestamps at declared world boundaries. Preserve native/browser framing, connection shutdown and backpressure. | K transport; F dispatcher. Existing network-message/P2P tests, adversarial decode, native/browser and actual internet sessions. Internal transport/control frames remain owned by those modules. |
| content | `map_share*`, `mcc_admission*`, `mcc_load_network*`, `mcc_share*`, map-family loaders; kinds 83–86, 103–106 | Validate matching content before world application; sharing is optional, identity is not. Bounded progress/timeout and cancellation; failed transfer/load cannot leave active-map state. Xbox `maps`, CE `custom_maps`, MCC `mcc_maps` retain their owners. | K/F integration, no format rewrite. Join/admission/load tests, mismatched/missing package, transfer interruption and family transitions. No publication/sharing to external peers in M0. |
| objects | `network_objects.c` plus existing engine allocation; kinds 5, 6, 8, 9 | H owns create/delete, object datum identity and snapshot; C reserves separate space for local effects. R create/delete/snapshot-end; U sampled transforms. Correct slot lifetime and dependency identity through join, reset and failure. An end marker currently proves ordered transfer, not every object's successful construction. | R/F replication, K allocator. Object/epoch/recovery tests plus missing construction, slot reuse, parent-before-child and interrupted snapshots. |
| inventory | `network_objects.c`; kinds 4, 13; H owns possession and pickups, C predicts its own ammunition | U inventory refresh with ordered initial inventory; R pickup notification. Missing unit/weapon entries enter a bounded retry queue. Explicit empty state must clear old possession. Reset queued pickup/inventory history on world change. | R/F. Existing pickup/recovery tests; empty inventory, dropped weapon, late join, seated player, full queue and restore during reload. |
| movement | `network_distributed.c`, `network_objects.c`, engine input/units, riding adapters; kinds 1, 2, 12, 14, 15, 107 | C sends input and its own player/vehicle predictions; H validates ownership, time and motion, then distributes authority. Unit-state encoding is variable/quantized; vehicle states have their own history. Inhibited/scripted poses are host-owned. Input edges, floors/BSP, teleport and seat changes need distinct handling. | R/F co-op control/history; K shared engine physics and competitive behavior. Prediction, elevator, riding, scripted-pose, clock and abuse tests; per-tick trajectories and real latency/loss sessions. |
| actors | `network_actors.c`; kinds 64, 73; host AI drives replicated units | Quantized control, pose, vitality, team, impulses, speech and custom animation; event sequence bytes distinguish repeated actions. Reset actor caches after restore/new map. Spawning, deleting, reviving and temporarily unobserved actors must retain correct identity. | R/F. Actor/animation and combat harness routes; actual encounters, dropships, melee/leap, Flood revival, capacity and late-join scenarios. M0 player logs do not certify AI equivalence. |
| combat | `network_damage.c`, `network_objects.c`, existing damage/projectile engine; kinds 10, 11, 75, 93 | H owns consequences. C reports owned-player hits reliably; H checks weapon/rate/history/reach/sight and replays results. Ordinary projectiles are not covered by the ordinary object-replication mask. Host outcomes and client flight/effect agreement are separate requirements. Clear shot/throw/target/attachment histories at world boundaries. | R/F agreement and reconciliation; retain meaningful validation. Hit-sight, owner lookup, stale-life, impulse/grenade tests; moving targets, AI/vehicle fire, sticky/bounce/splash, shooter departure and restore mid-flight. |
| world | `network_coop.c`, structure/BSP dispatch and existing engine devices; kinds 65, 68, 69, 72, 74, 77 | H owns BSP, groups, names, transforms, looks and devices. BSP changes are reliable with periodic repair; other state is refreshed. Clients suppress local authoritative device movement; host ignores movement from peers in the wrong BSP. Lost deletion/absence must eventually clear old state. | R/F co-op replication, K engine semantics. Device/name/attachment and BSP scenarios; gather/cooldown policies; missing object and family lifecycle tests. Door failures are not established as the same cause as lost events. |
| presentation | `network_coop.c`, `coop_spectate`, `coop_sound_sync`, camera/HUD/sound hooks; kinds 66, 70, 76 | H publishes sampled camera/cinematic/fade/HUD/timer/input/allegiance/vitality state, plus skip-vote state; C presents it with buffering. Dead-player views defer to scripted camera ownership. Skip votes cannot be reused for the next cinematic. Restore/unload must release controls, sound and camera ownership. | R/F. Camera/sound/clock/harness checks, but acceptance requires native-resolution visuals and audio, split-screen, dedicated hosts, late join and interrupted cinematics. |
| events | `network_coop.c`; kind 71 and 12 variants below; kind 67 retired | Script hooks enqueue H's actions. Current bounded queue sends three U copies; receiving a higher sequence consumes it before application succeeds. Some state has periodic repair, some actions do not. Correct contract must distinguish durable state from one-shot action and disposable cosmetic effects. | R. Five production-function fault probes plus desired-behavior regressions later. Dependency order, loss, overflow, removals, wraparound and restore must be explicit. |
| scripts | `coop_scripts.c`, host `hs.c`/`hs_library_external.c`, units/players hooks; MCC scripts remain MCC-owned | Adapt player0/follower behavior for the actual group: volume gates/gathering, teleport, suspension, equipment, boarding/exiting and script safe-to-save. Host runs campaign scripts; C applies results. Failed placement must not advance a gate. Script readiness differs from checkpoint safety. | K/F adaptations; R transport dependencies only. Existing gate/script-player/vehicle tests plus authored encounters, more-than-two-player groups, changed roster and interrupted transitions. |
| checkpoint | `game_state.c`, `main.c`, distributed/object/co-op reset, `coop_vehicles`, `mcc_checkpoint` | Saved gameplay differs from live session state. Ordered epoch boundary precedes new snapshots; host time stays monotonic and script sleep times shift. Preserve current roster, clear stale references/actions, restore/reconstruct every persistent participant. Explicit host operations must report failure rather than silently becoming another operation. | R/F operation orchestration, K validated save machinery. Epoch/checkpoint/timing/roster tests, real save/revert, manual pause controls, repeated transitions and save-integrity checks. |
| scaling | `coop_enemies`, `coop_pickups`, `coop_vehicles`; host-only settings and authored object identity | Extra enemy placement, held dropship riders, pickup copies and vehicle-home/replacement budgets cannot run twice after replay or reset. Preserve per-player/multiplier/none policies and safe spawn checks. Vehicle home state already participates in checkpoint callbacks. | K/F, not optional scope. Scaling/pickup/vehicle/script tests plus changed roster, used budget and restored/deleted source objects. |
| policies | Co-op friendly fire, collision, AFK, BSP-majority/cooldown, `network_aim_assist`, `network_shield_effects`, `network_riding_map` | H owns shared policy; per-player choices apply only under host rules. Session settings are not replaced by old checkpoint preferences. Dedicated host has no local player. Kind 107 is dispatched before the MCC-only range filter. | K/F. Existing policy/riding/friendly-fire tests; defaults, host/client changes, dedicated zero-local-player and map change. |
| social | `network_chat`, `network_voice`, `network_votekick`, `network_map_vote`, `network_link`, AFK and identity/build/ping handling | Preserve role/stream authorization, rate limits, mute/privacy policy, keys, vote lifetime and loading/lobby availability. Session departure revokes permissions. Voice frames are transient; configuration/keys and chat use their own admission/stream contracts. | K services and qualify integration. Chat/link/vote/server tests; real voice and UI remain runtime gates. Sensitive identity credentials are not included in this audit's artifacts. |
| mcc | `mcc_network`, names network, campaign, speed, grenade and checkpoint modules; kinds 97–100 | MCC-only extra slots/names/campaign state, complete snapshots and reliable host speed changes retain independent format semantics. Gate before loading by content identity and during play by active family; clear on unload/failure. Shared world reset must explicitly notify MCC participants rather than relying only on backwards time. | K/F MCC-owned adapters; R interface participation where needed. MCC protocol/network/checkpoint/campaign/lifecycle/admission tests and actual MCC sessions. Never route Xbox/CE through MCC. |
| camera | `third_person_protocol` and feature-owned camera modules | GulchCE-only agreement 32745, locked session selection, kind-102 outer join envelope, host camera policy and shoulder relay byte. Shared baseline version 44 is distinct from OpenCE testing 43. None of these numbers change in M0. | K feature isolation, F minimal wiring only if required later. Camera wire/policy and multiplayer preservation checks; matching versions and rejected mismatches separately. |
| multiplayer | Engine game modes/statistics and shared distributed objects/damage; kinds 3, 7 | Competitive game-state/statistics semantics remain meaningful even though campaign has no competitive engine. A co-op replacement must not remove shared paths because campaign does not use a field. Preserve local campaign and local split-screen separately. | K. Existing game-mode/state tests, Xbox/CE competitive sessions, solo and split-screen qualification. |

## Co-op event and script-effect ledger

All 12 variants use `distributed_coop_event` fields interpreted by kind. The
index gives the actual producers and consumer switch. Shared guards validate
the packet envelope and epoch, but required object availability is checked
later. Current sequence consumption before application means retries do not
necessarily repair a missing dependency.

| Kind | Payload/dependency and current repair | Required state/action policy and recovery test |
| --- | --- | --- |
| sound | Sound kind/tag, source object, scale and elapsed start time. Active looping sounds have a separate 32-entry host table and periodic resend; impulse dialogue has no general snapshot. | Track running loops and stopped/absent state; one-shot dialogue needs expiry/seek semantics. Test silent dedicated host, unavailable source, late join and stop/revert. |
| title | Title index and delay; requires scenario strings. Three event copies. | Consequential presentation with a defined display interval; avoid replaying obsolete titles after join. Test loss and repeated restore. |
| hud | Help text/objective/flash/clear/checkpoint subkind. Objective cached for refresh, but cache update depends on successful event allocation. | Persistent objectives commit independently of send capacity; clear/absence included. Transient messages expire explicitly. Test full queue and lost clear. |
| player_effect | Translation/rotation/start/stop and three real arguments. Sampled presentation can end stale shake. | Reconstruct active effect parameters where needed and explicitly cancel on restored world/unload. Test lost start/stop and cinematic interruption. |
| nav_point | Team/unit target, flag/object marker, nav index or NONE to remove, offset; active-list refresh only. | Versioned presence and absence; bound dependencies and target ownership. Lost removal must converge even when active list becomes empty. |
| unit_animation | Unit, graph, animation, frame, interpolate; unit must exist. | State or consequential action with meaningful phase; defer until object usable, or supersede explicitly. Test event before create, stop and checkpoint at same handle. |
| scenery_animation | Object/name/definition fallback, graph, animation/frame. | Validate actual authored identity; seek/rebuild ongoing state without map-name patches. Test duplicate names, deletion, late join and restored animation. |
| effect | Effect tag at a cutscene flag or object marker. | Classify disposable visual effects separately from gameplay consequences; do not duplicate damage via effect replay. Test missing marker/object and old-world delivery. |
| attach | Attach/detach subtype, parent/child and marker indices; active 32-entry attachment table refreshes attachments. | Own dependency graph and explicit removals; reject cycles and wrong lifetimes. Test lost detach, missing parent, slot reuse and restore. |
| reverted | Notification to stop/reset client presentation; reliable world boundary is separate kind 96. | Keep world transition authority on the ordered operation/epoch path. Presentation cancellation must be idempotent and cannot itself adopt another world. |
| unit_open | Unit and open/closed value, e.g. dropship door state. | Durable state when it affects current pose/access; repeat/loss/late-join and restore tests. Do not assume ordinary door device refresh covers this distinct path. |
| surface_broken | Surface index, BSP and epicenter; host also refreshes broken surfaces. | BSP-scoped persistent breakage with authority and explicit new-world baseline. Test missed break, BSP travel and restored intact glass. |

Effects without their own event kind also remain in scope: player list/gather
semantics, scripted vitality, allegiance, PVS activation, HUD flags/timers,
input inhibition/suspension, object names/looks/transforms, BSP switching,
actor recording/impulses and MCC script effects. Their owners are in the
semantic table and engine reference index. The 12-kind count is not the
number of all campaign script side effects.

## State lifetimes and checkpoint participation

The snapshot mechanism sends current host state to peers. The checkpoint
mechanism restores prior gameplay on the host. They are different operations;
retransmitting a current cache cannot repair a cache that retained the wrong
checkpoint value.

| State/owner | Initialization and ordinary lifetime | Restore / join / teardown audit |
| --- | --- | --- |
| Engine game-state arena, players, objects, scripts, AI | Existing `game_state_malloc` participants and validated save image | Existing allocation/layout checks and before/after callbacks retained. Roster is repaired from live machines after load. Native save bytes are not a network snapshot format. |
| Live machines, tokens, account links, local-controller mapping | Networking session managers; connection/session lifetime | Current roster retained; departed identities must not return from a save. Admission, profile, preferences and save ownership stay separate from simulation. |
| Epoch, synchronized flag, batches and receive times | `network_distributed_new_game`; host advances epoch, client initially unbound | `distributed_restore_checkpoint` resets state; only ordered host boundary adopts new epoch. During initial snapshot, only allowed construction/inventory messages apply. Complete construction remains a separate readiness gap. |
| Prediction, input histories, death/seat/clock state | Distributed arrays and update queues | Checkpoint reset clears motion/action histories and keeps monotonic host clock. Clock-abuse/identity state has a different new-game reset. Check same-tick repeated operations and delayed kills. |
| Object possession, create/delete and reconciliation caches | `network_objects_new_game`, host per-machine tables, client local allocation | `network_objects_reverted` and fresh snapshots repair current objects; provisional client effects retired. Retry inventory queue and pickup selections cannot survive as old-world work. |
| Damage reports, weapon/throw/target/sight and stuck-projectile histories | `network_damage_new_game`, reset on shared checkpoint restore | At most one accepted consequence; old-life and old-world packets cannot affect revived handles. Current reset coverage does not prove every predicted projectile's visual path. |
| Host/client actor tables and action sequence counters | `network_actors_new_game` | Reset on shared restore. Actor control/damage refresh is separate from full AI decisions, retained host-side. Required one-shot actions still need dependency/loss qualification. |
| Presentation ring, event queue, client device/name tracking, skip vote | `network_coop_new_game`; `network_coop_reset_recovery` clears selected transient state | Reset stops dialogue/cinematic camera and clears sequence/buffering. Sampled state later reasserts host presentation. This is not a full rewind of all external co-op caches. |
| `host_devices`, sent transforms/looks/devices/screen effect, BSP membership | New-game baseline and periodic source comparison | Derived caches must be invalidated/reseeded appropriately; a 2-second refresh is not proof every change/removal is represented. Device groups also maintain snap counters and moved flags. |
| `host_hud_state`, `host_attachments`, `players_vitality` | File-static co-op persistent cache, reset on new game | Not serialized to the checkpoint or restored by `network_coop_reset_recovery`. This is an established participation gap, regardless of a short revert session passing player comparison. |
| Host looping sounds, client played-loop table and sound offsets | `network_coop_new_game` and sound hooks; independent table/clock state | Ordinary start/stop and periodic refresh exist. Full checkpoint reconstruction/removal semantics remain to prove; do not treat sound-system stop as restoration of authored state. |
| Enemy scaling and stored dropship riders | `coop_enemies_new_game`/`coop_enemies_reset`; host | Reset held riders on revert; preserve intended encounter counts, placement and spawn ownership. |
| Pickup copies and vehicle homes/replacement budgets | Dedicated co-op modules; configured at new game | Vehicle `before_save`/`after_load` callbacks retain matching home state; pickup identity lives partly on engine objects. Test restored source/deleted copies and bounded replacement budget, not blanket new-game reset. |
| MCC extra inventory, expanded names, campaign and script threads | MCC modules, checked checkpoint tail and MCC lifecycle | `mcc_checkpoint` captures/validates/restores its representation and releases claimed original bytes on disposal. `network_objects_reverted` explicitly calls `mcc_network_new_game`, which also resets names-network state. This is real participation in monotonic shared restore, independent of the additional backwards-clock observer. Preserve it and test all saved components. |
| MCC speed and presentation/script effects | `mcc_game_speed`, `mcc_script_effects`, `mcc_campaign` | Reliable host speed can arrive while paused; lifetime differs from normal tick. Keep active-family/discovery/load-failure guards. Reset and snapshot coverage must be audited with zero speed and failed loads. |
| Session settings, voice keys, votes, chat/mute and camera mode | Respective network/feature owners, not gameplay image | Clear/revoke at appropriate connection/session/round boundaries. Loading must not accidentally process world messages. Camera mode locked until session disposal; preferences remain local. |
| Render/audio/physics caches outside saved image | Existing save callbacks: cloth, lightning, contrails, camera shake, bounds, object functions, cluster refs, sound, camera, riding | Preserve existing invalidation and order. Co-op work does not authorize deleting these because they are outside network files. Required original platform stubs and local-play paths remain. |

Current finite bounds include 128 queued co-op events, three sends per event,
16 active nav points, 32 attachments, 32 looping sounds, eight presentation
samples, a two-tick presentation delay and two-second silence fallback.
Device groups cap at 1,024; damage events/reports at 256 per tick; ordinary
message entry batches at 64. Inventory repair retains at most the player
capacity and expires entries after three seconds. Full-object retry is
rate-limited to ten seconds on the host, with client backoff from eleven
to sixty seconds. These are current behavior, not proposed rewrite budgets.

## Expected behavior and defect classification

The host alone decides gameplay, ownership, membership and accepted operations.
Clients may predict their own player but cannot choose authoritative outcomes.
Persistent state must converge, including empty/removal state. Consequential
actions require once-only application or an explicit superseding/failed result.
Map-family semantics determine compatibility boundaries; names/checksums are
identity and provenance, not fixes.

The five extracted-production-function probes reproduce unchanged defects:
three lost event transmissions expire unacknowledged; missing-object animation
consumes its sequence; sequence gaps are skipped; lost nav removal is not
repaired by active-only refresh; queue overflow prevents objective-cache update.
They characterize bugs, not assertions the replacement should preserve.

Source inspection establishes that ordinary pause handlers reject restart and
revert while co-op is active, including the host. MCC has separate routes;
source inspection alone does not prove working rendered controls. The real
Xbox console-cue test confirms a host revert, then a later request substitutes
respawning after the third-revert-in-a-minute guard. This is separate from
the pause-menu restriction and from automatic roster-compatible team-wipe
policy. M5 must resolve all three deliberately.

The screenshot's remaining cinematics, doors, explosions, animation and
checkpoint reports remain reported symptoms unless a specific scenario below
establishes them. Intentional post-mission lobby return is not automatically a
failed transition. An all-Master-Chief cosmetic option and a changed skip
prompt are product decisions, not prerequisites for correct interpretation of
supported content.

Existing content assumptions also need preservation/retirement evidence:
`network_coop_vehicle_dropped` selects Warthog/Pelican tag-name substrings for
extra dropped vehicles, and `follower_seat_name` interprets `rider` and dash
prefixes for follower seating. These are existing adaptation rules, not
authority to add name-based fixes or delete historical behavior wholesale.
Test equivalent authored semantics, near misses, custom seat labels and the
original campaign case before replacing either selector.

## Baseline procedure and evidence

All runs pin source, build arguments, binary SHA-256, map/resource hashes,
role settings, logs and process-memory observations. Retail assets stay local.
Only two game processes run at once. Each gets private copied maps, executable,
configuration and saves; only the profile's `blam.sav` is copied from the user.
No original checkpoint is loaded or modified. Windows instances use dummy
video/audio, null rendering, hidden windows and offline loopback networking.
They do not publish a lobby. Only processes started by the runner are stopped.

Builds use clang 23.1.4, x86 MSVC 14.44.35207 and Windows SDK 10.0.26100.0,
bundled Python 3.12.14 and temporary Ninja 1.13.2. Baseline configuration:
`--release --portable --pgo=off --lto=off`; the CPU run adds `--profile`.
These are reproducible investigation builds, not shipping-performance builds.
CPU: Intel Core i7-14700K, 20 cores/28 logical processors; reported physical
memory 34,088,050,688 bytes. No Android/browser/low-end performance claim follows.

The initial sandboxed session failed before gameplay: the host could not
connect to its own loopback listener. A separate run with local socket access
worked. Both attempts are retained; the first is an environment failure.

The runner uses `net_compare.judge_run` without changing its thresholds:
position 1 world unit, aim 15 degrees, vitality 0.15, rounds 3; persistent
disagreement windows remain the tool's existing values. At least 300 drift
samples are required and drift share over 0.6 fails. These are player-state
oracles; they do not compare all actors, devices, objectives or sound.

The timing script passed at 30, 60, 120 and 990 frames/second. The five failure
probes confirmed all five existing faults again. The broad harness plus selected
protocol/checkpoint/MCC suites initially reported 2,778 passes, 353 failures and
seven skips across 3,138 cases. UTF-8 Python invocation and a temporary compiler
wrapper defining `_CRT_NONSTDC_NO_DEPRECATE` recovered 67 failures without source
or assertion edits. Final unique outcome: 2,845 passed, 286 unresolved on this
Windows invocation, seven skipped. This is not a green full-suite result.

The remaining 286 comprise 263 compile/setup failures using unavailable POSIX
headers/functions or `-lm`, and 23 text-filter expectations seeing Windows CRLF
output. The master-protocol group first hit encoding and then exposed missing
`setenv`/`unsetenv`; the second failure was not hidden. Skips cover a missing
master repository, case-sensitive filesystem behavior and POSIX/Linux process
lifecycle. These need a suitable Linux runner, not relaxed assertions. The
focused co-op/checkpoint/protocol/MCC tests that ran successfully remain useful
evidence, separate from the unexecuted groups.

Both Windows release builds completed: normal and profiling, with PGO/LTO
disabled. Existing Expat import-link warnings were retained in the build logs.
No Linux, Android or web build/runtime qualification was performed here.

## Scenario manifest and validation coverage

| Scenario | Content/configuration | Oracle and remaining boundary |
| --- | --- | --- |
| B01 | Xbox A30, identical native builds, loopback, two seeded bot inputs | Same-tick player comparison, lifecycle logs and memory; no visual/audio assertion. |
| B02 | Xbox A30, client starts 25 seconds later, 50 ms configured receive delay per peer, 5% configured datagram loss | Late-join player convergence. This is a configured 100 ms added RTT approximation, not measured jitter/reorder or a full P1 fault generator. |
| B03 | Xbox A30, forced test save and two host `game_revert` cues | Observe actual save/revert/respawn path; player comparison cannot certify all checkpoint sidecars. Unsafe-save command is test-only on private saves. |
| B04 | Xbox A30, profiling build | Existing CPU scopes and process memory. Disclose profiler overhead, null renderer and incomplete tail if termination occurs before final flush. |
| B05 | Installed MCC A10, identical native builds | MCC loading plus connected gameplay and player comparator. Increased corrections and transient drift retained; no complete campaign/cutscene qualification. |
| B06 | User-supplied CE B30 with its shared resources | Independent CE campaign loading/connected gameplay; keeps original resource paths untouched. |
| B07 | Xbox Blood Gulch competitive multiplayer | Preservation baseline for shared object/control/damage services, not a replacement for co-op scenarios. |
| B08 | Xbox A30 scripted mission completion with profile capture | Observe session postgame/lobby behavior and close profile at a map boundary; not an authored full mission playthrough. |

The complete implementation matrix remains: Windows/Linux/Android/web;
native/native role swaps, native/browser and native/Android, browser/browser
and Android/browser where supported; Xbox/CE/MCC and failed-load/family
transitions; local campaign, local split-screen, dedicated hosting, camera
policies, multi-controller roster, late join, leave/rejoin, saves and shutdown.
Four independent human peers and representative full campaigns belong to M11.
M0 does not relabel short native sessions as those qualifications.

Existing StageKit and the dedicated/internet runners have Linux and/or browser
requirements; some dedicated tests start three processes. They cannot be run
unchanged under the two-instance rule. WSL is not installed here. Their missing
coverage is explicit; this assignment does not install an OS, change those
runners or launch three game instances.

## Operating envelope and budgets

Freeze the following baseline comparison rules now: same pinned configuration,
same content hashes and seeds; two native peers; each displayed map-family
case separately; unmodified player comparator thresholds; 30 Hz simulation;
LAN and configured delay/loss recorded separately. Do not compare raw process
memory to serialized state or infer wire bytes from message counts.

For M1 preservation, a refactor must retain canonical wire output, dispatch
ordering, lifecycle effects and the same observable fault behavior until a
separate behavior-changing commit addresses it. Relevant passing cases must
remain passing; platform failures must remain visible, not become skips without
explanation. Preserve meaningful negative controls.

For M2 onward, the plan's zero stale-world/wrong-generation applications,
once-only consequences, bounded queues, two-second steady-state repair target,
explicit operation failure, capacity-boundary tests and repeated/soak tests
remain the acceptance contract. No later failure can justify silently weakening
a threshold. Historical capacities of 128 peers/players, 1,024 actors and 8,192
objects are limits to audit, not demonstrated simultaneous gameplay loads.

Some numeric budgets cannot honestly be frozen from this baseline alone:
per-peer byte bandwidth, full-snapshot bytes/construction completion time,
dependency-queue occupancy, per-tick correction magnitude and low-end/mobile
CPU/memory. Existing logs report counts and once-per-second player state;
they do not expose these quantities. CPU traces supply scope cost, but no
wire-byte count. Specify additional observation at the M0 exit gate, before
behavior changes. Do not invent measurements or begin the rewrite to obtain
them. Small baseline scenarios also cannot justify high-actor-load budgets.

## Reproduction and review

`coop_m0_inventory.py` regenerates the source index and JSON from the current
checkout. A future revision is a new inventory, not a silent update of this
baseline. `coop_m0_sessions.py --help` describes the Windows observer; pass
the pinned executable, retail map directory, copied profile source, a new
output directory and scenario-specific arguments. Its manifests record every
HALO/SDL override, hashes and process IDs. It never changes the source data.

`coop_m0_summarize.py` regenerates the numerical report from retained manifests,
comparisons, memory observations, JUnit results and profile parts. Repeated
JUnit cases are replaced by their later invocation result, not counted twice.
Profile distributions use nearest-rank percentiles after the first five
seconds of each recording, preserving distinct recordings and map identities.
Restore archived profile parts before regenerating the report.

The broad pytest invocation used `-q -p no:cacheprovider`, `tools/harness`,
and these files under `tools/`: `test_checkpoint_epoch.py`,
`test_coop_checkpoint.py`, `test_coop_checkpoint_recovery.py`,
`test_join_protocol.py`, `test_network_messages.py`,
`test_network_test_sampling.py`, `test_network_test_startup.py`,
`test_scripted_player_pose.py`, `test_net_compare.py`, `test_mcc_network.py`,
`test_mcc_checkpoint.py`, `test_mcc_lifecycle.py`, `test_mcc_protocol.py`,
`test_mcc_ui_network.py`, `test_mcc_campaign.py`, `test_mcc_load_network.py`,
`test_mcc_admission.py` and `test_p2p_shutdown.py`. The invocation-only retry
used `PYTHONUTF8=1` and `CC` pointing to a wrapper that adds
`-D_CRT_NONSTDC_NO_DEPRECATE`; it reran the five harness groups
`test_lighting_migration.py`, `test_master_protocol.py`, `test_player_link.py`,
`test_server_toml.py` and `test_coop_script_players.py`. Both JUnit reports
and logs are retained, including every unresolved failure.

## Measured native baseline

These are single short runs, not repeated statistical qualifications. The
comparison interval excludes startup and its existing three-second skip.
Transient disagreements below the judge's persistence thresholds are retained
in JSON rather than concealed by the word "pass".

| Case | Compared seconds | Maximum compared position difference (world units) | Host drift samples apart / sampled | Client corrections in judged interval | Existing judge |
| --- | ---: | ---: | ---: | ---: | --- |
| B01 Xbox A30 LAN | 59 | 0.07458 | 61 / 1769 | 19 | Pass |
| B02 Xbox A30 late join, delay/loss | 62 | 0.38433 | 185 / 570 | 0 | Pass |
| B03 Xbox A30 save/revert | 64 | 0.05213 | 27 / 1917 | 19 | Pass |
| B04 Xbox A30 profiling | 55 | 0.04908 | 66 / 1649 | 19 | Pass |
| B05 MCC A10 LAN | 59 | 0.16413 | 51 / 1769 | 152 | Pass |
| B06 CE B30 LAN | 60 | 0.09823 | 74 / 761 | 397 | Pass |
| B07 Xbox Blood Gulch, hit/kill/vehicle/pickup controls | 59 | 0.13149 | 193 / 1770 | 12 | Pass |

Position differences above come from host/client logged player states. Host
prediction drift and client correction counters measure different things;
neither is a per-tick positional correction distribution. B05 includes one
host drift maximum of 12.887218, despite passing the current drift-share
threshold. It remains unexplained evidence, not a clean bill of health.
B02 includes a transient 17-degree aim disagreement. B07 recorded host damage
application and rejected reports; configuring vehicle/pickup actions does not
prove every intended interaction happened or felt correct.

B03's host cue origin was tick 402. It forced a private test save at tick 702,
requested a revert at tick 1152 and again at 1752. The first restored 450 ticks
while keeping the clock at 1152. An earlier automatic 99-tick revert meant
the next request triggered the third-revert-in-a-minute respawn substitution.
This confirms distinct operation semantics, not complete checkpoint fidelity.

| Content | Host working / private peak MiB | Client working / private peak MiB |
| --- | ---: | ---: |
| Xbox A30 LAN | 204.4 / 426.9 | 195.7 / 426.3 |
| CE B30 LAN | 419.0 / 609.4 | 412.4 / 609.9 |
| MCC A10 LAN | 782.6 / 1047.1 | 774.3 / 1046.7 |

Memory is sampled whole-process usage after more than three player-1 log
records, including engine, map resources and platform allocations. It is not
co-op-only state, a leak test, or a simultaneous worst-case capacity budget.

| B04 scope | Samples | p50 ms | p95 ms | p99 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Host `game_tick` | 1418 | 0.1714 | 0.2588 | 0.3173 | 0.8569 |
| Host `network_distributed_tick` | 1418 | 0.0741 | 0.1140 | 0.1482 | 0.3950 |
| Client `game_tick` | 1427 | 0.1467 | 0.2296 | 0.3037 | 0.5922 |
| Client `network_distributed_tick` | 1427 | 0.0396 | 0.0517 | 0.0740 | 0.2298 |

B04 retained 52.27 host and 52.54 client seconds of flushed profile parts,
with no reported dropped scopes. Both recordings lack their final part:
the planned `profile_stop` cue was later than the runner's observation window
and never executed. These distributions describe the retained parts only.
The null-renderer frame loop ran much faster than 30 FPS while simulation
remained 30 Hz; do not interpret its frame rate as rendered performance.
Profiling overhead and this small opening encounter prevent treating these
numbers as production or high-actor-load CPU budgets. Separate B08 profile
recordings flushed at map unload and are marked complete in JSON, regardless
of the subsequent session outcome.

### B08 mission-completion observation

The `25 game_won` cue ran at tick 1152. Source inspection and logs agree that
`main_won_map_private` calls `network_game_server_port_cooperative_won`, which
sends game-over, resets to pregame and selects A50. Both peers unloaded A30
and reached the lobby; that is the current explicit policy, not seamless
in-mission progression.

The first fixture omitted A50 and the host exited through the missing-map
dashboard path with code 0. Adding A50 exposed further stock multiplayer-map
lookups by the lobby; adding only Sidewinder then exposed Beaver Creek. These
are retained fixture failures, not independent co-op defects. The final run
included A50 and the full stock multiplayer menu set. Neither peer exited
early, but the host subsequently precached Sidewinder and neither logged a
second gameplay round during the 75-second observation window.

Consequently B08 does **not** qualify next-mission play. Its comparator correctly
reports only 37 played seconds out of the requested window (36 compared).
Do not weaken that oracle to manufacture a pass. The private profile fixture
also lacks selected-profile metadata: the host explicitly failed to record
level completion because it could not find the player's profile, and the lobby
logged missing `lastprof.txt` / `lastmpmp.txt`. A normal selected-profile/manual
lobby reproduction is needed to separate fixture/menu automation from a product
selection defect. No original user profile or checkpoint was modified.

## M0 exit gate and next evidence

| Requirement | Status and next action |
| --- | --- |
| Responsibility/ownership and state inventory | Recorded across all 19 semantic domains, declared message/event enums, indexed engine hooks and checkpoint callbacks. Lexical limits above remain explicit. |
| Expected behavior and fault demonstrations | Recorded; all five existing probes reproduce. Screenshot symptoms without a matching observation remain unconfirmed. Desired contracts are separate from current quirks. |
| Available native baseline | Windows normal/profile builds, broad/focused tests and short Xbox/CE/MCC/shared-multiplayer sessions recorded. Failures and fixture limitations remain visible. |
| Bandwidth and snapshot budget | Open. Add observation at existing transport send/receive and snapshot begin/construction/ready boundaries, with byte counts, elapsed time and identity; no behavior changes. Include empty, ordinary and near-capacity states. |
| Corrections, queues and recovery budget | Open. Observe per-tick before/after correction, dependency queue occupancy, loss/reorder and state-usable timing. A message count or snapshot-end marker is insufficient. |
| Platform and preservation coverage | Open. Run remaining POSIX suites on Linux; qualify required native/browser transport and representative Android/mobile performance. Exercise local campaign, split-screen/controller rosters, family transitions and rendered/audio cases under the two-instance rule. |
| Mission progression and profile ownership | Open. B08 established lobby return but not next-mission play or durable completion. Repeat with a normally selected private profile and distinguish UI selection, automated input and session ownership. |
| Revision drift | Open for a newer base. Audit the `887d9ac9d..fd3fc7758` MCC/content/menu delta and rerun its affected cases before attributing this baseline to current testing. |
| Permission to implement | Not granted. M1 and behavior changes remain unstarted. No branch publication, PR or CI-triggering push occurred. |

The next work is to close these M0 observation/qualification gaps, then freeze
numeric budgets against the declared operating envelope. A staged rewrite is
still feasible, but this report does not waive the agreed gate or turn short
player comparisons into full co-op validation.

## Final review and retained evidence

The combined M0 documents and three utilities were reviewed for ownership,
scope, source-backed claims, fixture isolation and honest result classification.
The runner now accepts explicit supporting maps and records cleanup errors
while still attempting to stop both owned peers. The summarizer keeps distinct
profile recordings and checks contiguous parts before calling one complete.
No game implementation, production test assertion or comparison threshold was
changed. The investigation inventory regenerates identically at the pinned
revision; Python syntax, enum IDs/contracts, indexed source references and local
documentation links were checked. All 312 indexed references resolved at their
recorded source locations; 804 local documentation links passed the first
combined check. The final link/diff check includes this evidence section.

Local-only evidence is under `build/m0`: build/test logs, both JUnit reports,
12 session manifests, peer logs, process observations and comparison JSON.
The [archive index](../build/m0/archive-index.json) records the original relative
path, length and SHA-256 of each archived member. The
[evidence archive](../build/m0/baseline-evidence.zip) contains 68 inactive test
save/profile files, verified by ZIP integrity and every member's SHA-256 before
loose copies were removed. Restore relative to `build/m0` to rerun the
summarizer. This is local diagnostic evidence, not a distributable map package.

Cleanup removed 25,868,910,034 bytes of private map/cache/executable copies
across the runs and about 250 MB of disposable build/test/duplicate outputs.
About 2.09 GB of loose save/profile evidence became a verified 137.6 MB archive.
Retained M0 material totals about 249 MB: the archive, necessary normal/profile
baseline binaries and profile symbols, logs/manifests, compiler invocation
record and small Ninja installation. Original maps, saves and installed
toolchains remain untouched. No owned game process remains running. The normal
build configuration was restored after profiling. The root checkout's 20
ongoing changed/untracked paths were left intact; this branch remains local.
