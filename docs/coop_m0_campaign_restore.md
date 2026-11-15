# M0 natural A10 checkpoint restoration

15 November 2026. This continues the [ordinary-entry baseline](coop_m0_campaign_entry.md)
on the rebased investigation head `91f8629d599864ec8801b617d5d0b864bfd4d2c6`,
based on GulchCE testing `4595247ae6649c6efbc9baf99124f979ebef3a38`.
See the [current gate review](coop_m0_status.md) for publication and platform
status. The full 19-domain co-op scope remains; this is a bounded campaign
restoration observation, not a complete persistent-state oracle or M1.

## Method and boundaries

The existing diagnostic generator and two-peer runner were rebuilt against
the rebased sources. Both peers use the same Windows executable, SHA-256
`ae0d310327fe3a2eb77769fb4327a5f907eb8172b108fefcd87478bc09ea337d`.
The sessions use private map/profile/save copies, hidden 2560×1440 windows,
a 30 FPS limit, Xbox A10 on Normal, an unskipped opening, audio disabled and
clean loopback transport. No more than two game instances run at once.

Host entry uses the ordinary menus. The first case uses the existing automated
client join and admits the client first; the second uses the ordinary LAN
browser after the host is already in the lobby. Logs establish the resulting
player ownership. Because entry routes also differ, these are complementary
baselines, not an experiment isolating only the effect of player ordering.

The private fixture supplies bounded ordinary player input and menu events.
It does not set tutorial flags, teleport players, force doors, request saves,
skip scripts or alter map data. Read-only console queries inspect progression.
Host `(game_revert)` invokes the existing restore API; it does not qualify the
network pause-menu command, which remains a separate host-control obligation.
The resulting epoch changes and subsequent gameplay are observed independently.

Natural save evidence is the `network_coop_checkpoint_saved` callback after
successful `game_state_write_to_file`. World samples record player identity,
position, vitality, BSP/cluster, floor support, control/input and device state.
Epoch/readiness samples are periodic and do not measure exact transition
latency or establish complete-state readiness.

## Client-first result

The client owns player 0 and the host owns player 1. The tutorial again takes
the non-local-co-op branch: the network roster has two players while the
local `player_spawn_count` remains one. Client Confirm/Back do not satisfy
the host script predicates in the bounded probes; host input advances them.
The first natural tutorial checkpoint is saved at tick **17944**, and the
tutorial-completion checkpoint at tick **27426**, in addition to the initial
tick-1 save.

| Observation | Result |
| --- | --- |
| First restore | The client first samples epoch 2 ready at tick 18450. At host/client ticks 19104/19107 both players are alive, input-enabled and on supported floors in the cryo room. |
| Continued authored tutorial | Ordinary input reaches the next movement trigger, the technician sight test and the yellow square. The shield/tutorial sequence continues and creates the second checkpoint. |
| Displacement before second restore | Both players cross the open cryo exit into corridor cluster 7. At host tick 32436 their positions differ from the checkpoint: player 0 is approximately `(-51.072, 1.031, 0)` and player 1 `(-50.584, 0.520, 0)`. |
| Second restore | The client first samples epoch 3 ready at tick 32490. At host/client ticks 33192/33198 both players have returned to cryo-room cluster 0: player 0 approximately `(-57.221, 2.253, -0.050)` and player 1 `(-52.888, 0.007, 0)`, with full observed body/shield values, floor support and input enabled. |
| Post-restore continuation | Fresh ordinary steering takes both players through `cryo_explosion_door_3` again into corridor cluster 7. Player 0 reaches `cryo_explosion_trigger_2`; the queried exit-door position is 1 and the next damaged door's authored position is 0.3. |

The first restore occurs before displacement from that checkpoint's player
positions. It establishes an actual recovery operation and continued play,
not a strong position-mutation oracle by itself. The second restore includes
the explicit displacement comparison above. The client retains its live look
direction across the second restore; the host's local heading returns to its
saved direction. Steering is adjusted from the observed headings. This pass
does not claim that camera-direction ownership is qualified or diagnose the
difference as a defect without its intended contract.

The host gameplay-help widget is dismissed with an ordinary menu Confirm
before movement is judged. The final bounded route stops at the damaged door;
it does not qualify passage through it, later A10 encounters, mission completion
or next-level entry. It does not reproduce the reported intermittent A10 door
softlock, C10 door failure or natural A50 grav-lift failure.

## Host-first result

The ordinary LAN browser admits the client into the host's existing lobby;
the host then starts A10. Runtime ownership is host/player 0 and client/player 1.
The unskipped tutorial again follows the non-local-co-op branch. Host Confirm
advances the look and shield prompts. The natural checkpoints are tick **21617**
after the look choice and **33154** after tutorial completion, besides tick 1.

| Observation | Result |
| --- | --- |
| First displacement and restore | At host tick 23643 player 0 has moved from approximately `(-57.333, -2.385, -0.050)` to `(-57.270, -1.751, 0)`. The client first samples epoch 2 ready at tick 23700. At host/client ticks 24390/24393 the player has returned to the saved position; both players are alive, input-enabled and floor-supported. Shields are zero at this pre-shield-tutorial checkpoint. |
| Continued authored tutorial | Ordinary movement and aiming resume the movement/shield sequence. Host Confirm advances the shield prompt, and the authored sequence creates the second checkpoint and opens the cryo exit. |
| Second displacement | Both players cross the exit. At host tick 35031 player 0 is approximately `(-50.687, 1.134, 0)` and player 1 `(-49.972, 0.907, 0)`, with floor support. |
| Second restore | The client first samples epoch 3 ready at tick 35100. At host/client ticks 35454/35457 both players return to their checkpoint positions: player 0 approximately `(-56.740, 2.742, 0)` and player 1 `(-55.458, 0.667, 0)`. Both have full observed body/shield values, floor support and input enabled. |
| Post-restore continuation | At host/client ticks 36816/36822 both players have crossed the cryo exit again into corridor cluster 7, with floor support and input enabled. Player 0 continues to `cryo_explosion_trigger_2`; read-only queries return true for that trigger, 1 for exit-door position and 0.3 for the next damaged door. |

Replaying the first run's timed steering was insufficient after the ownership
swap: the observed heading and route differed. Future queued replay actions
were cancelled and retained in the action record, then ordinary steering was
adjusted from observed positions. This is a fixture limitation, not evidence
of a campaign softlock. No script flags, object positions or doors were forced.

## Checkpoint image provenance

The initial preservation attempt copied the profile's `savegame.bin`. Its
unchanged timestamp/hash revealed that it was not the live checkpoint image.
Those two copies are explicitly marked as excluded checkpoint-image evidence
in the local manifests; they are not used to claim saved-state equivalence.
The first client-first tutorial checkpoint therefore has runtime trace and
restore evidence, but no separately retained checkpoint image from that moment.

The active file is `saves/z/savegame.bin`, opened by
[`game_state_create_or_open_file`](<../source/saved games/game_state_xbox.c>).
Reading it refreshes the observed Windows file metadata; an earlier directory
size of zero was not a reliable indication that the active handle lacked data.
The second client-first checkpoint is preserved from that live file before
revert, with a verified 20,971,520-byte copy and SHA-256
`f26961542f9d92cd7aaa3c40c3c334b2cc940566c2225fc0883d50bc2efcb82a`.

Both host-first checkpoint images were preserved from the live file before
their respective reverts, each 20,971,520 bytes with verified copy hashes:

- First: `8d8cb80a6123ecc879c179aee0af82d83f2ceae2fcc440f632072429057c6d89`.
- Second: `3f9962748578e571a67afc3696c105cfc9d85661267f5ae4d400c6da67955e97`.

These hashes establish retained image provenance, not semantic completeness
or equality between different sessions' checkpoint files.

## Outcome and limits

The bounded pass is complete: both player orderings create and restore two
natural checkpoints, resume the tutorial after the first, and cross the cryo
exit after the second. The host-first route stops at the same damaged-door
boundary as the client-first route. This closes the selected local natural-save,
restore and resumed-progression observation gap; it does not establish complete
persistent-state equivalence, qualify the host pause menu or reproduce the
reported intermittent door failures.

Neither this bounded pass nor green CI waives the full
M0 gate, Linux/Android campaign runtime blockers, CE/MCC campaign semantics,
transport faults, multiple-local/four-player arrangements or later whole-system
qualification. Fixes must remain general co-op/engine corrections, not map
identity exceptions or authored-progression bypasses.

## Evidence, review and cleanup

Local evidence is under `build/m0/campaign-restore`, with separate
`a10-client-first` and `a10-host-first` folders. Each retains the specification,
input action timeline, revision and executable/input hashes, console logs,
named before/after state samples and reduced world/entry reports. World frame
counts are 12831/12833 for client-first host/client and 12536/12538 for host-first.
Both runs record 60 host script wakes and none on the client. These counts
describe sampled execution, not a complete script-state comparison.

Both owned game processes are stopped. The two evidence archives preserve
521 files, including all test saves, captures, metadata and diagnostic CSVs.
ZIP integrity and every member's SHA-256 were verified before loose archived
copies were removed. Original maps/profile files pass the six-input hash audit.
Retail content and captures remain local and are not published with the branch.

About 4.86 GB of disposable copied maps, generated map caches, temporary build
outputs and review-image copies were removed. About 1.82 GB remains for this
pass, chiefly 1.75 GB of verified compressed evidence plus the required diagnostic
executable, symbols, reports and reproduction controls. The previous generated
diagnostic build was separately archived and verified before rebuilding; prior
M0 evidence and the user's saves remain preserved. Normal build configuration
was restored without the diagnostic compiler launcher.

The combined branch delta and final documentation were reviewed. This pass
adds no production behavior change or new diagnostic implementation. Historical
results retain their original revision boundaries; current evidence is linked
separately. All 156 local links in the eight changed documents resolve, named
save/callback symbols were checked against source, and whitespace checks pass.
No further implementation cleanup was justified for this documentation update.
