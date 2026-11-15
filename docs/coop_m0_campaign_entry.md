# M0 ordinary campaign entry and authored progression

**Continuation:** The [15 November gate review](coop_m0_status.md) records
the rebased branch and authorized publication. This report retains the
original ordinary-entry observations and their revision boundaries.

This continues the [active-world baseline](coop_m0_active_world.md) on
`investigate-coop-rewrite`, starting at `493abb5e9`. It investigates Xbox A10
without changing the production game, protocol, menus, maps or saves. The
[full 19-domain inventory](coop_m0.md#semantic-contracts-and-ownership-ledger) and
[recovery ledger](coop_m0_recovery.md) remain in scope. M0 is open; M1 has not
started and nothing is published. Linux and Android runtime remain blocked
because no runner/device is available.

## Evidence boundary

The initial control uses the existing active-world diagnostic executable.
The host navigates Multiplayer → Create Game → Local Network → Singleplayer
→ The Pillar of Autumn → Normal → Server Setup → lobby → Start Now.
The client uses the existing automated LAN join. This establishes ordinary
**host** menu entry; it does not establish ordinary client browser navigation.
Both peers render in hidden 2560×1440 windows with private map/profile copies,
audio disabled and no injected transport faults. At most two games run at once.

The local artifacts are under `build/m0/campaign-entry`. Per-run manifests
record executable/input hashes and environment; action records, console logs,
world traces and rendered captures provide separate observations. Archived
evidence retains original relative paths and per-file hashes. Original retail
assets and their decoded scripts stay local and are not distributed.
The [machine-readable results](coop_m0_campaign_entry_results.json) retain the
qualified case hashes, sampled end states, script milestones and open gates.

The [campaign diagnostic generator](../tools/investigations/coop_m0_campaign.py)
extends the existing generated-copy observer with co-operative-query,
script-wake and input-enable observations. An optional trusted local input
file supplies bounded look, movement and use requests through the ordinary
player input blob, before the existing tutorial-action processing, director
restrictions and network serialization. It does not assign player positions,
tutorial action flags, mission flags or door values. Requests expire after
at most five seconds. The [local runner](../tools/investigations/coop_m0_local.py)
records requests and bounds-checks them; requests alone are not proof that
input reached gameplay or advanced a script.

The second input variant additionally supplies Confirm, Back, Jump, Crouch
and Fire requests, with observations after the normal tutorial-action handler.
The first variant's executable, symbols, generated copies and generator are
retained in `input-v1-build.zip` with a verified hash index. The input blob is
downstream of physical bindings and inversion, so it cannot qualify keyboard,
controller or inversion-setting mappings. None of these diagnostic binaries
may be distributed as a game build.

## Stationary ordinary-entry control

`menu-a10-render` reached tick 9114 on the host and 9117 on the client
(approximately 304 seconds of simulation). The opening cinematic was not
skipped. The lobby showed two players; the joining client owned player 0 and
the host owned player 1. The first player was placed in the cryotube; the
second had no unit. After the cinematic the rendered prompt requested looking
around, while movement input remained disabled. No look/move/use inputs were
supplied in this control.

The host's runtime query returned `false` for `game_is_cooperative` and
`normal` for difficulty. Consequently, the earlier stationary fast-entry
observation is not by itself proof of a campaign softlock: a tutorial waiting
for player input can produce exactly that observation. It also is not proof
of successful progression or acceptable co-op startup behavior.

## Input-driven ordinary entry

`menu-a10-input-v2` uses the campaign observer and the same host menu route.
Its host trace records these successive authored stages:

| Host simulation tick | Observation |
| --- | --- |
| 6702 | Opening ends and the tutorial disables movement input. |
| 7231 | The look tutorial wakes. |
| 8258 | Client look requests have satisfied the tutorial; the health demonstration wakes. |
| 8527 | The use-action tutorial wakes. |
| 9179–9399 | After the client's Use request, the authored cryotube-exit cinematic executes and restores input. |
| 9445 | The first movement tutorial wakes. |
| 9520 | The host's second player is placed beside player 0 and gains a unit. |

The last complete samples are tick 18141 on the host and 18144 on the client.
Both peers have input enabled and two living, unseated player units in BSP 0.
The host successfully moves away from an initial overlap with the technician;
the client also walks and turns. Rendered captures show the technician and
the normal movement prompt. The run ends at its fixed time limit in this
tutorial; it does not establish passage through the cryo-room door.

These observations connect client input, host-authored script advancement,
presentation/placement and extra-player spawning. They qualify that bounded
entry sequence, not full-state convergence or the entire campaign. Audio is
disabled, so neither speech playback nor synchronization is qualified.

Three setup pilots are excluded from gameplay conclusions: `menu-a10` stops
before gameplay; `menu-a10-input` schedules menu input before readiness and
exits; `menu-a10-progression` encounters a Windows reader lock while atomically
replacing the private menu request. Its runner terminates both owned peers and
preserves the failure log. A bounded retry for the private file handoff handles
that transient lock; permanent and unrelated permission errors still fail.
None of these setup failures establishes a product campaign defect.

`menu-a10-progression-v2` repeats entry, then walks player 0 around the
cryotube and onto the red square. Host queries observe the movement volume
and red-square conditions becoming true. At tick 14064 the targeted-look
tutorial wakes. Rendered client captures show successive targets turning green.
The last target initially remains yellow even with the camera reticle on it:
the log distinguishes desired aim from the character's head/look direction.
A small ordinary step aligns those directions and the script advances to the
look-choice stage at tick 20928. The second targeting cycle completes and
the client displays the Confirm/Back prompt.

This is an observed stationary-aim discrepancy to investigate, not proof of
a network-specific defect or permanent softlock. In
[hs_library_external.c](../source/hs/hs_library_external.c),
`hs_unit_can_see_object` uses a target's head/bounding centre;
[units.c](../source/units/units.c), `unit_can_see_point`, tests the observing
unit's head origin and `looking_vector`. Camera alignment alone therefore
does not establish this script predicate. Solo/local comparison remains open.

The run stops at the confirmation boundary. Brief Space messages posted to
each recorded hidden SDL window do not establish delivery into the input blob;
they are excluded from confirmation outcomes. Client console queries are
rejected by the existing host-authority guard. The confirmation-capable fixture
is required to separate local input receipt from host script advancement.

## Confirmation forwarding and continued progression

`menu-a10-confirm` repeats ordinary entry with the confirmation-capable
fixture. Client Confirm (request 72, tick 20404) sets its local Accept flag;
client Back (request 76) sets its local Back flag. Host
queries remain false for the corresponding action and `test_looking_choose`
remains true. Host Back (request 79) then changes the authored choice to the
normal-look branch, observed in the client's rendered prompt and the host's
inversion query. Client look-up/down requests satisfy that branch. Host
Confirm (request 85) completes the choice: `test_looking_choose` becomes false,
`mark_tutorial_looking_choose` becomes true, and the next movement stage wakes
at host tick 22409.

This establishes a client-to-host confirmation gap at the tutorial boundary,
with host-local positive controls. It does not establish an unavoidable
session-wide deadlock: the host can supply the missing action. Source inspection
agrees: `player_control_action_test_check_reset_input_blob` records local Accept
and Back, but `distributed_note_input_actions` forwards the action categories
handled by `player_control_action_test_note`, which omit those two actions.
The contract is general scripted player input; a future correction must not
special-case A10. Physical button bindings and the applied inversion mapping
remain outside this fixture's evidence.

The client then walks around the cryotube to the next movement volume and
yellow square. The shield tutorial wakes at host tick 26823 and disables input
at 27881. The client's display shows the shield charging and then its Confirm
prompt. Client Confirm (request 116) again reaches the local input handler but
leaves the host's Accept query false. Host Confirm (request 120) allows the
authored shield test to finish: input returns at 29814, the assistant sequence
wakes at 29815, and `mark_tutorial_hud_shield` becomes true. Thus the confirmation
gap is observed in two script contexts, including one with movement disabled.

The tutorial completes and the authored cryo explosion sequence wakes at
host tick 30697. `cryo_explosion_door_3` reaches position 1, and ordinary
movement takes both players through that exit into the adjoining corridor.
The client then reaches `cryo_explosion_trigger_2`; the next damaged door is
at its authored position 0.3. The pass stops in that corridor. It does not
qualify traversal beyond the damaged door or reproduce the user's intermittent
A10 door failure. One successful exit is not a general door-convergence pass.
The final complete samples are host tick 38160 and client tick 38166, with
both living players on supported floors in corridor cluster 7, BSP 0, and
input enabled. These bounded samples are not a full-state convergence claim.

The host initially cannot move because a gameplay-help widget remains open.
An ordinary menu Confirm dismisses it; movement then works. Input requests
before that dismissal are not evidence of a movement defect. Host and client
presentation, menu input and gameplay input must therefore remain distinct
observations in further runs.

The host observer records `checkpoint_saved` at ticks 22442 and 30808, after
the look-choice and tutorial-completion stages, in addition to the initial
tick-1 save. In [game_state.c](../source/saved%20games/game_state.c),
`game_state_save` invokes `network_coop_checkpoint_saved` only after a successful
write. No fixture save command is used. These observations establish natural
checkpoint creation in this run; restoration and resumed progression from
those checkpoints remain untested.

## Source route and general contract

- [game.c](../source/game/game.c), `game_is_cooperative`, returns whether
  `player_spawn_count > 1`. This variable describes the local campaign setup;
  it is not the network roster count.
- [ui_widget_event_handler_functions.c](../source/interface/ui_widget_event_handler_functions.c),
  `multiplayer_type_menu_initialize`, resets that count to one.
  [menu_functions.c](../port/linux/game/menu_functions.c), `coop_begin`, sets
  it to two for local split-screen campaign setup.
- [hs.c](../source/hs/hs.c), `game_is_cooperative_evaluate`, exposes the same
  predicate to scripts. A10's authored startup chooses its co-op opening
  through that predicate; on Normal the other branch starts its tutorial.
- [players.c](../source/game/players.c), `players_coop_extras_held`,
  `players_coop_room_to_spawn` and `player_spawnable_beside`, hold extra
  players while the opening or a lack of usable controls/safe placement
  prevents spawning. This can hold the host if another machine owns player 0.
- [player_control.c](../source/game/player_control.c),
  `player_control_action_test_note`, and
  [network_distributed.c](../port/linux/game/network_distributed.c),
  `distributed_note_input_actions`, explicitly support clients satisfying
  tutorial action tests. That support must be checked before treating a
  stationary tutorial as a deadlock.

The local-player count and network-co-op predicate are distinct contracts to
resolve in the staged rewrite. Do not change the global count merely to make
this map take another branch: it also participates in local campaign and save
behavior. Any later correction must apply to the underlying session/script
semantics, preserve local solo/split-screen and Xbox/CE/MCC ownership, and
benefit equivalent authored content without registering its map identity.

## Validation boundary and next gates

The combined tooling and documentation were self-reviewed. The only code
changes are private diagnostic generation and runner support; production
`source/`, `port/`, protocol and map-family implementations are unchanged.
The bounded Windows file-handoff retry fixes a demonstrated fixture failure.
No broader refactoring or production fix is introduced.

The Windows diagnostic build succeeds, 124 focused runner/reducer, script-player,
vehicle, pose, checkpoint and epoch tests pass, and Python syntax, local document
links and whitespace checks pass. Normal build configuration is restored to
`--release --portable --pgo=off --lto=off`. Runtime observations are Windows
two-peer loopback, Xbox A10, Normal, unskipped intro, no transport faults and
audio disabled. They do not add Web, CE, MCC, public-internet or four-player
runtime coverage. Linux and Android runtime remain explicitly blocked.

Both owned game processes are stopped. All seven runs retain 2,125 archived
evidence files, including saves and captures, with verified member hashes.
Six original map/profile inputs and 14 generated source copies pass the final
hash audit. About 16.42 GB of disposable map/cache/build and review-image copies
were removed. About 5.40 GB remains under this pass's artifact folder, primarily
5.31 GB of compressed evidence plus the required diagnostic builds, source
copies and logs. The root checkout's ongoing changes were left untouched.

Continue from these specific boundaries before claiming ordinary campaign
progression is qualified:

- Repeat with host/client player ordering swapped, ordinary client browser
  entry, relevant difficulties and a solo/local comparison for tutorial aiming.
- Validate natural checkpoint restore and continued authored progression,
  then later doors, BSP transitions, combat, mission completion and next-level
  entry. Preserve the separate A10/C10 door and A50 BSP cases.
- Cover CE and MCC campaign semantics through their existing owners and repeat
  relevant scenarios with transport faults and native/browser peers.
- Specify general session predicates and script-facing input ownership for the
  later milestones; neither a local-player-count override nor an A10 exception
  is an acceptable substitute.

The full 19-domain inventory remains open as recorded in the existing ledger.
These results narrow particular M0 gaps; they do not sign off M0 or start M1.
