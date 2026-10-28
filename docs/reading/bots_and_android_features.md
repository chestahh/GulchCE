# Optional bots and Android controls

These are GulchCE additions. Their defaults preserve OpenCE's local Slayer
bots and Android's saved touch layout. Xbox, Custom Edition and MCC keep
their existing map loaders, limits, rules and resource ownership.

## What players can enable

- `bots.enhanced = true` adds CTF and King of the Hill goals, team roles,
  precision-weapon vantage choices and powerup selection. It is read at map
  load. Slayer combat, spawning, pathfinding and unstick behavior remain
  owned by the existing bot implementation.
- `bots.routes` selects `direct` (default), `lanes` or `mixed`. A lane is an
  alternate route through two intermediate destinations. Unreachable lanes
  are abandoned for that bot's current life.
- `bots.vehicles = true` lets enhanced bots seek ground-vehicle seats and
  turrets. They use ordinary player controls to enter, steer and leave.
  Aircraft navigation is not implemented. Switching this option off asks
  seated bots to leave; disabling enhanced bots takes effect on the next map.
- `bots.network = true` permits bots on a network host. Set it before
  creating the host; `bots.count`, `bots.skill` and `bots.teams` still apply.
  Admission/removal occurs in the lobby. This does not add live bot commands.
- Android's **Settings > Gamepad > Android Controls > Gyro Aim** offers
  **Touch Settings**, **Off**, **On** and **Zoom Only**. Touch Settings is
  the default and preserves the layout's saved gyro switch and sensitivity.
  Explicit modes use independent horizontal/vertical sensitivity and invert
  settings. Zoom Only discards motion while unzoomed.
- Android's **Cheat Menu** option adds **Cheats** to touch **Options**.
  It defaults off and is available during offline gameplay only. It exposes
  invincibility, ammunition, bottomless clip, super jump, weapon/powerup
  spawning and camouflage through existing engine cheat APIs.

For example, set `bots.count = 6`, `bots.enhanced = true`,
`bots.network = true` and `bots.routes = "mixed"`, then create a CTF host.
Six bots join as ordinary players if the lobby has space. The host decides
their goals and publishes their controls. A client needs the same map and a
compatible OpenCE protocol, but does not run the new bot AI. MCC sessions
still require GulchCE's existing MCC support and negotiation.

The complete settings and environment variables are in
[the port settings table](../../port/linux/README.md) and
[the Android guide](../../port/android/README.md).

## Read the bot flow

1. [bots_network.c](../../port/linux/game/bots_network.c):
   `bots_network_session_begin` latches admission policy;
   `bots_player_ownership` records synthetic players actually created by
   this host. A high machine number alone never grants bot ownership.
2. [network_server_manager.c](../../source/networking/network_server_manager.c):
   `network_game_server_port_bots` retains the existing lobby, team and
   capacity rules. An opted-in host reserves the final 15 machine IDs;
   ordinary hosts retain all human machine slots.
3. [bots.c](../../port/linux/game/bots.c): `bots_update_actions` runs only
   on the host. `bots_choose_goal` asks the enhanced module for urgent
   objectives before the inherited combat/item/roaming decisions.
4. [enhanced_bots.c](../../port/linux/game/enhanced_bots.c):
   `enhanced_bots_update` caches objective state once per tick;
   `enhanced_bots_goal` selects flag, carrier, hill or vantage destinations.
   A flag carrier keeps advancing while fighting. A bot on the hill holds
   its position; an off-hill bot advances toward it.
5. [bots_roles.c](../../port/linux/game/bots_roles.c),
   [bots_routes.c](../../port/linux/game/bots_routes.c) and
   [bots_vantage.c](../../port/linux/game/bots_vantage.c):
   `bots_tactics_update` advances bounded graph preparation;
   `bots_routes_search_goal` supplies lane waypoints to the original path
   search. A vantage point is a position overlooking useful map locations;
   `bots_tactics_goal` chooses one suited to a bot's weapon and role.
6. [bots_vehicles.c](../../port/linux/game/bots_vehicles.c):
   `bots_vehicle_goal` reserves reachable seats and
   `bots_vehicle_action` supplies ordinary action/steering inputs. The
   engine checks reach, occupancy, hostility and attachment itself.
7. [player_queues_new.c](../../source/game/player_queues_new.c):
   `update_server_note_bot_action` puts the host's chosen input in the
   existing update for the tick it executed. Clients replay the same
   player actions through the existing simulation.

The ~7,000-line fork bot core is not imported. The focused objective module
extends the existing core; it does not duplicate combat, respawning,
path search or collision handling. Route/vantage preparation retains
bounded working memory (32,768 walkable nodes and a 262,144-entry surface
lookup at most) and releases it when finished. No separate bot
thread, network packet, deterministic simulation rule or protocol version
is added. Network version remains 39. See
[NETCODE.md](../../port/linux/NETCODE.md) for authority and transport rules.

Map initialization resets objective, precision-cache and vehicle state.
Map disposal frees graph/grid allocations, including partial preparation.
Respawn releases seat reservations and vantage claims. Session disposal
clears bot ownership and the network reservation. Map-loading and MCC
cleanup paths remain unchanged.

## Read the Android flow

[TouchControls.java](../../port/android/app/src/main/java/com/halo/decomp/TouchControls.java)
retains its existing sensor lifecycle and
[GyroscopeAim.java](../../port/android/app/src/main/java/com/halo/decomp/GyroscopeAim.java)
integration. Explicit gyro modes bypass swipe sensitivity, reset accumulated
motion on mode changes and leave legacy mode behavior intact.
[host_touch.c](../../port/android/host/host_touch.c) transfers scalar mode
and motion values under its existing mutex.
[touch_gyro.c](../../port/linux/src/touch_gyro.c) applies per-axis settings;
`halo_linux_gyro_filter` discards unzoomed motion in Zoom Only mode.

[AndroidCheatMenu.java](../../port/android/app/src/main/java/com/halo/decomp/AndroidCheatMenu.java)
uses the touch overlay's existing dialog/input-reset path. It sends typed
IDs through [host_android_cheats.c](../../port/android/host/host_android_cheats.c),
an eight-entry mutex-protected queue. No console text or guest pointers cross
this boundary. [android_cheats.c](../../port/linux/game/android_cheats.c)
rechecks offline permission on the game thread and consumes at most four
requests per poll. Cinematics suspend requests without losing owned toggle
values. Disabling the menu or leaving the map restores values it changed,
unless a later console/script edit changed them again. One-shot spawned
items remain ordinary game objects; they are not undone by closing the UI.

## Why the shared hooks are needed

- CTF flag/carrier and King hill geometry are private to their engine
  modules. The `game_engine_ctf_flag_state`, `game_engine_king_hill` and
  `game_engine_king_point_on_hill` accessors expose read-only state to
  feature code; they do not change scoring or game rules.
- The lobby owns player admission and machine IDs, and the tick queue owns
  authoritative input publication. Feature code cannot safely replace
  either ownership boundary. Their small hooks call the feature modules.
- `player_control` knows the actual zoom state when it consumes input;
  the gyro hook needs that state to discard unzoomed motion.
- `cheats_dispose_from_old_map` is the map boundary at which menu-owned
  switches and pending Android requests must be cleared.

All additions in `source/` carry `port:` markers. General-feature logic
stays in feature modules. No MCC compatibility logic was moved into the
Xbox or Custom Edition implementations.

## Credits and source provenance

The reference fork is
[thelinkin3000/halo-ce-universal at d92cc933](https://github.com/thelinkin3000/halo-ce-universal/tree/d92cc93385edf03d0a4a21cfe4c58dd578a3e786).
Its current project licence is AGPL-3.0-or-later; inherited code retains its
original notices and licences. Selected adaptations are distinguished from
design references below; this is not a wholesale cherry-pick.

| Feature | Authors and reference |
| --- | --- |
| Existing local bots | **BirchWoodGod**, OpenCE commit `4a275b60`; retained as the base implementation. |
| Routes, roles, vantage points and vehicle reference | **BirchWoodGod**'s [halo-ce-vita](https://github.com/BirchWoodGod/halo-ce-vita), adapted by **thelinkin3000** in [88df94fd](https://github.com/thelinkin3000/halo-ce-universal/commit/88df94fd9fb7dc76b3762ef28bfd5d624fe4bba5). The retained tactical code and derived portions keep GPL-3.0-only; its full text ships in [LICENSES/GPL-3.0-only.txt](../../LICENSES/GPL-3.0-only.txt). Vehicle controls are a focused adaptation using this engine's public seat/action interfaces. |
| CTF and hill decisions | **thelinkin3000**, [ef77e2b4](https://github.com/thelinkin3000/halo-ce-universal/commit/ef77e2b4d73fbd81622279ae7cd1f5e5d40771c2) and [91871681](https://github.com/thelinkin3000/halo-ce-universal/commit/918716812b155eb889f73c125b11bd53bb475564). Those commits credit **id Software** and **ioquake3 contributors**: Quake III CTF/team rules and key-area defense inspired the decisions. Quake III did not supply a Halo hill implementation; no additional Quake source was imported here. |
| Network bots | **thelinkin3000**, [ed6cdaed](https://github.com/thelinkin3000/halo-ce-universal/commit/ed6cdaeda8871b7bee781c76bff3a4da2836c1bf), for publishing bot actions through the existing tick queue. GulchCE adds explicit host ownership and opt-in machine reservation. |
| Gyro settings | **thelinkin3000**, [29638d60](https://github.com/thelinkin3000/halo-ce-universal/commit/29638d603d90283e16f95b2ac78d1ab99639912a), as the settings/zoom-mode reference. Built on **MrBruh**'s existing OpenCE Android controls (`fc610044`), preserving their legacy behavior. |
| Android cheat menu | **thelinkin3000**, [406f793f](https://github.com/thelinkin3000/halo-ce-universal/commit/406f793f4c08751f854a3615ed244ea3b1f90b88), whose overlay credits **hardran3**'s **OpenCE-Ultrahand** and **HaloDrones** packages. GulchCE uses a new small typed Android dialog/queue and existing cheat APIs; the fork's general console and renderer overlay are not imported. |

Code credits belong here and in player-facing commit notes, not in the
art-only `AUTHORS.md`. Release packaging includes the retained GPL text and
this provenance; web distributions include both in `licenses.txt`.

## Checks and limits

The focused [harness tests](../../tools/harness/tests/) exercise real source
for lobby defaults/opt-in/ownership, partial and repeated cleanup, gyro
mode/sensitivity behavior, and the Android cheat queue's permissions,
capacity and lifecycle. [test_touch_layout.py](../../tools/test_touch_layout.py)
covers Java layout, gyro integration and sensor-state transitions.
[test_bots.py](../../tools/test_bots.py) exercises real local games.

Validation of the reviewed implementation on Windows:

- All six real-game bot regressions passed: stock Blood Gulch and Chill
  Out Slayer, enhanced Blood Gulch Slayer/CTF/King, and bots absent from a
  network game when the option is off. These checks establish movement,
  combat and clean exits, not flag captures or complete objective strategy.
- Unmodified OpenCE build 309 (`22947203`) joined a GulchCE host with six
  bots. Xbox Blood Gulch had 68 paired seconds and CE Death Island CTF had
  68, with eight players and no sustained divergences under unchanged
  comparator tolerances. With vehicles enabled, bots entered and drove
  vehicles and the same OpenCE client agreed for 67 paired seconds.
  A CE late-join run admitted both players and all bots, but its samples
  did not align: that run establishes joining/lifecycle behavior only.
- Windows, Android host/guest and web compile/link checks passed. Android
  and web used temporary Windows command adapters for Unix shell steps
  in the existing build graph. Android Java sources compiled against SDK
  35; the Java layout/gyro regressions and 36 web page tests passed.
- The focused regression set passed 89 checks with five platform skips.
  The larger harness/net-comparison run had 1,292 passing checks and six
  skips after correcting a Windows test-compiler macro. All 312 remaining
  failures reproduced on an unchanged export of the branch's `origin/main`
  base (`e94810f1`); they are not a green full-suite result.
- MCC DigsiteMP/1 initially timed out in the host's connection handshake,
  before bots joined. With the existing debug
  `global_connection_dont_timeout` override, local gameplay with six bots
  ran for 46 logged seconds; a two-instance GulchCE network game then
  agreed for 72 paired seconds with six bots and two human players and
  clean exits. Default-timeout MCC startup remains unverified; no timeout
  or MCC compatibility behavior was changed to make it load.

Compilation and automated checks do not establish Android touch/gyro feel,
all vehicle/map combinations, full campaign compatibility or Internet
interoperability. The native/browser WebRTC harness could not run on this
Windows host (POSIX build/path assumptions); the Internet game test also
lacked its MQTT broker. Android device/visual checks and live cross-family
map transitions remain outstanding. Record subsequent checks with the
implementation commit; do not treat suggested checks as results.
