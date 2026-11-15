# M0 pause-menu restart and checkpoint controls

15 November 2026. This continues the [M0 gate review](coop_m0_status.md)
on `network-coop-rewrite` at `b82703bbb0ceaea6f7d2da78aa11328042f95421`.
The preceding [natural-checkpoint pass](coop_m0_campaign_restore.md) used the
host restore API. This pass tests the separate player-facing pause controls.
All 19 co-op domains remain in scope; M1 and production changes are excluded.

## Acceptance and ownership

The existing [M5 operation contracts](coop_rewrite_plan.md#m5--checkpoint-recovery-mission-restart-and-host-controls)
require host-authorized, session-wide operations. Revert must restore the valid
checkpoint; restart must reload the current mission without advancing campaign
completion or losing the roster. Clients must not acquire host authority. A
menu selection, closed dialog or lobby transition alone is not success.

The current implementations differ:

- Xbox and CE use `pause_game_restart_at_checkpoint` and
  `pause_game_restart_level` in
  [ui_widget_event_handler_functions.c](../source/interface/ui_widget_event_handler_functions.c).
  Both return false when `network_coop_active()` is true, including on hosts.
- MCC constructs its own campaign pause tree in
  [mcc_pause.c](../port/linux/game/mcc_pause.c), selected through
  [mcc_pause_runtime.c](../port/linux/game/mcc_pause_runtime.c).
  Its trusted dispatch precedes the ordinary handlers.
  [mcc_ui.c](../port/linux/game/mcc_ui.c) checks host authority and checkpoint
  validity before revert. Restart delegates to
  [mcc_ui_network.inl](../port/linux/game/mcc_ui_network.inl), which retains
  the current game settings, transitions through postgame/pregame and starts
  the ordinary countdown when the matching local lobby arrives.

These source facts do not establish successful runtime outcomes. Any later
repair must address general operation authority and lifecycle in the responsible
owner; it must not select behavior by the test map's identity or combine the
map-family implementations.

## Method

The Windows diagnostic executable is generated from `b82703bbb0` plus the
private pause-input addition in
[coop_m0_campaign.py](../tools/investigations/coop_m0_campaign.py), SHA-256
`02d7fa42cc039c7be5b2a3d69ceb5244c2f60aa99edf923dfa020958d6d009f0`.
Release/portable configuration disables PGO and LTO. The generated-copy
compiler launcher checks original and generated source hashes before compiling.
Each session has two hidden 2560×1440 game windows, private maps/profiles/saves,
clean loopback transport and a 30 FPS cap. Original data is not edited.
The ordinary network-test setup hosts/joins the campaign. With `M0_RAW_PAUSE`
set, controller-zero Start requests in `M0_UI_INPUT` hold the existing test
controller's Start bit for 250 ms, then release it. The real engine polls that
raw state to open the pause menu. Other requests enqueue ordinary menu events;
no pause/restart/revert handler is called directly. This fixture is used with
chat-input automation disabled. It does not establish physical keyboard,
controller-device or touch parity.

Opening cutscene votes may shorten setup through the existing co-op vote path.
Those automatic setup restores are distinguished from the later menu actions.
This does not repeat the preceding unskipped natural-tutorial qualification.
Captures, action timestamps, recovery epochs, map/BSP lifecycle and peer state
provide separate observations. A readiness bit alone does not establish full
persistent-state convergence.

Two restricted-environment attempts using the preceding diagnostic binary
failed before gameplay with a refused
host self-connection. The same binary/automatic setup reached two-peer gameplay
when local networking was permitted. Those failures are retained as environment
evidence and are excluded from the pause-control results. Automatic and ordinary
menu-entry pilots with scheduled virtual-pad Start did not establish a working
pause-input fixture. They are not evidence of a pause-menu defect. The corrected
fixture is qualified by rendered menus, navigation, confirmations and Cancel.

## Xbox A30 result

`xbox-pause` reached two-peer gameplay and an authored setup restore (epoch 2).
Both peers opened the campaign pause menu. Revert and Restart each displayed
the expected progress-loss confirmation. Confirming either action left its
dialog open and performed no operation. Host confirmations occurred at about
74.6 s and 112.9 s; client confirmations at 88.1 s and 129.0 s after runner
start. Both peers remained at epoch 2 with advancing simulation ticks and no
new mission load. Cancel dismissed the dialogs and menus back to gameplay.

This reproduces the reported missing host functionality in a representative
Xbox network session. Client actions did not acquire host authority, but the
UI also did not explain the refusal. Source inspection identifies the general
co-op guards, not an A30-specific condition. This is baseline evidence for M5,
not a repair or a claim that checkpoint contents are complete.

## Custom Edition B30 result

`ce-pause` uses the user's `custom_maps/b30.map` and its shared resources.
This is modified campaign content with an authored pause layout, not an
assertion of stock CE campaign coverage. The opening sequence completed before
the tested pause requests. Both peers displayed and navigated the menu.
“Load Last Checkpoint” and “Restart Level” opened their progress-loss dialogs.
Host confirmations at about 155.6 s and 218.6 s, and client confirmations at
170.1 s and 236.9 s, left those dialogs open without restoring/reloading.
Epoch 1 continued throughout. Cancel returned both peers to gameplay.

The observed behavior agrees with the ordinary handler guards. This pass does
not exhaustively audit this custom map's authored widget/script dispatch or
qualify every custom menu. Its native-resolution captures also retain the
existing layout overflow and rendering artifacts; these were not repaired.

## MCC campaign result

`mcc-pause` uses the locally installed `mcc_maps/a10.map`. Its rendered snowy
canyon, vehicles and search-for-survivors objective identify custom campaign
content; the filename is not evidence of stock Pillar of Autumn coverage.
The client confirmed Revert at 97.3 s and Restart at 149.9 s. Both displayed
“Only the host can restart or revert this game.” Neither changed the world.

The host's Revert at 172.9 s closed the UI and advanced both peers from epoch
1 to 2. The engine logged a 662-tick restore from the latest authored save,
while retaining the monotonic network clock. Both players subsequently moved
about 4.58 world units through bounded ordinary player input. The first
operation was stationary and cannot independently demonstrate positional rollback.

Host Restart at 398.1 s returned both players to the same-map lobby, displayed
the countdown and reloaded the mission at tick zero. Sampled gameplay epochs
changed from 2 to 4 across the unload/load. Both players remained in the roster;
the initial cinematic and spawn sequence ran again, then both had live units,
input and supported positions. Both moved about 4.58 units in the reloaded world.
The network-test helper logged only its initial immediate-start request. Its
next-variant route was not activated; the restart used MCC's normal countdown.

A second host Revert at 492.3 s advanced both peers to epoch 5 and logged a
540-tick restore. An intervening authored save had already captured the moved
positions, so unchanged positions in this sample are not a restore failure.
Moving both players again and confirming Revert at 543.2 s, before the next
authored save, restored the earlier positions on both machines: approximately
`(21.290, 260.977, 17.152)` to `(18.805, 257.137, 17.130)` for the host player,
and `(20.659, 260.747, 17.148)` to `(18.174, 256.906, 17.125)` for the client
player. The engine logged a 2,067-tick restore and both peers reached epoch 6.
No direct save/revert command or forced position was used for these operations.
After that restore, another bounded input moved both players about 1.16 units;
both peers retained input and returned to rendered gameplay.

These samples establish menu authority, an actual saved-position rollback and
same-mission reentry with usable players in this fixture. They do not qualify
every persistent domain, campaign-completion/profile invariants, empty/invalid
checkpoint handling, interrupted operations, roster changes, the 20-operation
M5 endurance gate or all authored MCC campaigns. The
[full recovery ledger](coop_m0_recovery.md) continues to track known missing
state and dependency failures.

## Validation record

The Windows generated diagnostic build passed. Final focused checks passed
(159): `test_coop_m0_local`, `test_pause_campaign_context`, `test_mcc_pause`,
`test_mcc_ui`, `test_mcc_ui_network` and `test_mcc_pause_runtime`. They supplement
the runtime record; they do not qualify Linux/Android campaign runtime, keyboard/touch
parity, dedicated hosting or faulted transport. Those boundaries remain explicit.

The final review covers the generated input code, source-owner interpretation,
native-resolution captures, action/epoch/state records and updated milestone
links. No production code, protocol, original map or original save changes are
needed for this bounded baseline task. M1 remains unstarted and M0's platform
and operating-envelope gates remain open.
All 68 relative file links in the three changed reports resolve. Post-run
SHA-256 checks match all ten unique original map/resource/profile input files.

Local evidence is under `build/m0/host-controls`: each case retains its input
specification/manifest, action schedule, engine logs, state samples and operation
summary. Lossless evidence archives retain captures, CSV traces and inactive
private saves, with per-member SHA-256 verification before loose-file removal.
The generated-source manifest and diagnostic binary remain available for
reproduction. Retail/custom assets and private saves are not published.
The seven runs/pilots removed 14.32 GiB of private map/cache/executable copies;
the disposable build removed another 0.22 GiB after its retained binaries were
hash-checked. Evidence and the required diagnostic build retain about 3.83 GiB,
principally verified capture/trace/save archives. All owned game processes ended,
and the original configure options were restored without the diagnostic launcher.
