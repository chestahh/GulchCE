# M0 follow-up: measurements and remaining qualification

Investigation on 12–13 November 2026, on local `investigate-coop-rewrite`.
Read with the [complete inventory and first baseline](coop_m0.md) and
[staged rewrite plan](coop_rewrite_plan.md). The scope remains all 19 co-op
domains, including admission, content, roster, simulation, objects, AI,
combat, scripts, presentation, recovery, progression and disposal.

**M0 is not signed off. M1 remains unstarted.** This follow-up supplies
previously missing observations and separates unsuccessful qualification
from fixture failures. It does not implement fixes, waive platform checks
or declare the current implementation correct. No production source,
protocol, menu, default, original map or original save was changed.

## Baseline and revision reconciliation

The native and web game sources remain testing `887d9ac9d6ac7a647f2cc3fada30c602b1497f82`;
the investigation started this follow-up at documentation commit `3d893729b`.
Read-only remote verification found GulchCE testing
`fd3fc7758c14f1567370352e6c682f68f5f14a9e` and OpenCE testing
`e9adcf7a70ea1c3a069c5c8d1d0a0cc93ebb065e`. These are observations at verification
time, not a promise that the branches will remain there.

The three-commit GulchCE delta changes MCC resource identity, validated
resource selection and sharing, the dedicated server's cached map catalog,
MCC sharing-menu focus, server UI, tests and CI. It does not change `source/`,
the `network_*.c` implementations or co-op-owned port modules. An exported
source reference at `fd3fc7758` passed 142 affected tests: resource selection,
identity, validation, sharing game/menu and MCC server behavior. No merge,
rebase or new game baseline was substituted. This closes identification of
the delta and its focused source-test check, **not** newer-revision MCC runtime
qualification or the server UI's complete validation.

## Observation method

[coop_m0_trace.py](../tools/investigations/coop_m0_trace.py) produces generated
Windows diagnostic source copies and a compiler launcher under `build/m0/trace`.
Exact anchors and hashes reject source drift. Production files are read only.
The diagnostic build uses the same release/portable, PGO-off/LTO-off
configuration as the original baseline. Changes are file logging and explicit
test-fixture controls, not replacement networking. Generated copies must not
ship. The launcher is removed from the final normal build configuration.

Measurements use common Windows performance-counter timestamps, successful
socket payload lengths, actual object transform writes, object snapshot
construction and the existing client synchronization gate, inventory retry
occupancy and event-queue allocation/overflow observations. The synchronous
logger adds overhead. The first pilot used insufficient numeric precision to
distinguish peer IP addresses; it is excluded from byte measurements.

[coop_m0_measurements.py](../tools/investigations/coop_m0_measurements.py) reduces
the captures into [machine-readable measurements](coop_m0_measurements.json).
Its window begins five seconds after the first observed object synchronization
and ends at the last client tick. One-second distributions exclude the final
partial bucket; total/rate calculations include it. Repeated game ticks after
a revert are separate observations. Tests cover peer filtering, partial
windows, tick reuse, between-tick queue peaks, incomplete rows and absence of
samples.

All four measured sessions used two hidden native peers, private copied maps
and saves, 30 Hz simulation, dummy audio and a null renderer on the previously
recorded i7-14700K Windows host. Their unmodified player comparison passed.
These are short opening encounters, not performance limits or full campaigns.

| Session | Measured seconds | Host sent payload bytes/s | Client sent payload bytes/s | Host p95 bytes in a complete second |
| --- | ---: | ---: | ---: | ---: |
| Xbox A30, save/revert | 62.50 | 27,785.84 | 4,144.91 | 28,061 |
| Xbox A30, join after 25 s, 50 ms receive delay per peer, 5% loss | 60.73 | 27,875.43 | 4,146.19 | 30,413 |
| Custom Edition B30 | 56.96 | 119,497.17 | 3,311.93 | 147,494 |
| MCC A10 | 56.62 | 133,346.66 | 4,134.18 | 150,566 |

These are successful application socket payload bytes to the other peer,
including session traffic, **not** IP/link wire traffic. TCP acknowledgements,
retransmissions, transport headers and public-internet overhead are excluded.
UDP counts and receive-side observations are retained separately in JSON.

| Object synchronization | Serialized object entries | Offered reliable message bytes | Host construction ms | Host begin to client object-ready ms |
| --- | ---: | ---: | ---: | ---: |
| Xbox A30 initial | 107 | 14,592 | 0.263 | 1.387 |
| Xbox A30 opening reset | 107 | 14,592 | 0.158 | 1.573 |
| Xbox A30 explicit revert | 99 | 13,664 | 0.104 | 1.244 |
| Xbox A30 delayed/lossy join | 106 | 14,460 | 0.149 | 53.264 |
| CE B30 initial | 163 | 25,988 | 2.254 | 4.105 |
| MCC A10 initial | 188 | 30,388 | 4.114 | 6.310 |

Construction includes observation overhead. Offered message bytes are neither
acknowledged bytes nor a whole-world snapshot size. The client-ready marker
means the existing object synchronization handler accepted its end marker;
it is **not** proof that every persistent script, UI, parent or checkpoint state
is usable. All observed inventory retry queues were empty at that marker.
The host's object-slot high-water mark is not used as an actual object count.

| Session | p95 maximum state-write displacement per observed tick | p99 | Maximum | Event queue peak |
| --- | ---: | ---: | ---: | ---: |
| Xbox A30 save/revert | 0 | 0.02574 | 0.04218 | 11 |
| Xbox A30 delayed/lossy join | 0.02546 | 0.03135 | 0.05363 | 11 |
| CE B30 | 0.13832 | 0.32534 | 1.63454 | 4 |
| MCC A10 | 0.06395 | 0.09790 | 12.88209 | 5 |

Displacements are engine world units for kind-0 network state writes across
objects, including hard and blended writes and zero-write ticks. They are
not exclusively local-player reconciliation, player disagreement, jitter
or a perceptual quality score. Other write kinds are separate in JSON. The
large MCC maximum is retained, not removed as an outlier. No observed object
creation failure, inventory retry backlog or event overflow occurred in these
short captures. That does not validate capacity behavior. The existing five
extracted-production fault demonstrations were rerun and still reproduce,
including cache loss when the event queue fills.

## Progression and private profile ownership

The first follow-up repeated B08 with look-only input. Unlike bot input, it
does not periodically press A in the lobby. A50 stayed selected; the previous
Sidewinder selection is therefore not established as a product selection bug.
Both peers remained alive, but no second gameplay round was observed.

Two fixture defects were then isolated. `lastprof.txt` identifies the last
profile but the automated fast network setup does not activate it. In
addition, Xbox-style profile enumeration needs `SaveMeta.xbx` alongside
`blam.sav`; the original fixture copied only the latter. The runner now copies
both when metadata exists, never the original `savegame.bin`. Generated
`network_test.c` fixture code can explicitly call `player_profile_get` and
`player_ui_set_active_player_profile`, the same public APIs used by menu
profile selection. This is not a change to production fast setup.

The copied profile had all ten completion bytes already set to `0x0f`, so
unchanged bytes could not demonstrate a new completion. The final fixture
instead created a fresh profile through `player_profile_new` and activated
it. Both peers logged successful activation. After the host's `game_won`
cue on A30, its new on-disk profile contained `02 00 00 00 00 00 00 00 00 00`:
A10 normal completed, A30 still incomplete. The fixture also observed
`main_get_current_solo_level()` returning 0 during the network A30 game.

The source explains that mismatch: `main_won_map_private` obtains the network
map for choosing the next mission, but `player_profile_save_level_completed`
obtains its level through `main_get_current_solo_level`, which reads
`main_globals.soloplayer_map_name`. See [main.c](../source/main/main.c) and
[player_profile.c](../source/saved%20games/player_profile.c). This is a concrete
automated-launch reproduction and an ownership hazard. It is not yet a
normal-menu launch reproduction, and the fixture did not reload the new
profile in a fresh process to qualify every persistence path.
The new client profile's completion bytes remained zero. The copied source
profile was unchanged on both peers and in the original data folder.

After completion the host again selected A50 and both peers reached pregame.
One explicit `network_game_client_request_immediate_start` call, ten seconds
after lobby return, changed no map or variant and did not start the next round
within the observation window. The original comparator correctly failed the
requested full duration. This does not prove that every normal lobby control
fails, nor does a successful profile write qualify mission progression.
Rendered menu interaction and the admission/countdown state still need an
independent oracle. Host pause-menu Restart/Revert remain source-confirmed
restrictions from the original inventory; console revert is a distinct path.

## Platform and transport evidence

- Windows diagnostic builds completed; all production game files remained
  unchanged. A diagnostic-host/normal-client session played A30 before
  returning to the lobby; its whole-duration progression oracle failed.
- The unchanged web release build completed with installed Emscripten 6.0.10.
  The first invocation failed because Emscripten selected Windows' Python
  alias; setting its Python interpreter and PATH resolved that setup failure.
- Browser page tests: 92 passed. Web graph/tests: 16 passed, one explicitly
  skipped because its native socket harness requires POSIX pthreads.
- A Windows OS adapter for the existing native/browser transport harness
  compiled unchanged production WebRTC, DTLS and crypto. Chromium and Firefox
  each returned 320/320 clean-link messages. With the existing one-in-ten loss
  injection they returned 700/960 and 728/960 respectively, passing the original
  loss oracle. Socket-restricted attempts failed ICE; the unrestricted local
  run passed. No transport threshold was relaxed.
- The Android build was attempted with the installed r30 NDK. It failed in
  generated Windows build commands: the SDL CMake command received shell
  redirection/`&& ninja -C` tokens as arguments. Other generated Android rules
  also require POSIX utilities. No Android build or APK pass is claimed, and
  no production build-script fix was started.
- The user confirmed no Linux/WSL runner or Android device is available.
  Linux suites, native/Android interoperability and mobile performance remain
  unavailable. Desktop or browser results do not replace them.

### Actual-game browser admission

The Windows adapter for [internet.mjs](../port/web/tests/internet.mjs) passed
its original four admission predicates: two browsers by invite, native host
with browser client, browser host with native client, and dedicated server
with browser client. A supplemental A30/co-op run passed the first three
peer arrangements with the same admission predicates. Both runs used Chromium,
the unchanged normal native executable and the unchanged web build. Firefox
coverage in this follow-up is the transport harness, not these game cases.

These predicates require gameplay logs containing both player slots (one
slot for the dedicated-server case) and the relevant WebRTC connection checks.
They can pass while a player's unit is still dead/unspawned during an intro.
They do not require a sustained comparison, both players controllable,
visual correctness, an authored mission completion or internet NAT traversal.
The co-op logs explicitly identify A30 and `game 1, co-op`; the transient
server-browser listing may still name the lobby's earlier multiplayer map.

Fixture history is retained rather than counted as product failures:

- AMQTT 0.12.1 could not negotiate the game's initial MQTT 5 connection.
  Its payload error was not proof of an invalid game Will message. That run
  was stopped, and its saves/logs were retained. The replacement was the
  official Mosquitto 2.1.2 Windows package, extracted privately; no installer
  or service was run. Package URL and SHA-256 are in the adapter manifest.
- The first Mosquitto run used an empty master-server file, which falls back
  to built-in public masters. In the reverse host direction, the native
  test attempted to join an unrelated public listing. The fixture was
  stopped immediately; no gameplay on that external server was logged.
  That run is invalid for local admission qualification. The final successful
  runs explicitly set `network.listings=brokers` on every peer, use the
  loopback broker, disable map sharing/downloading and restrict page HTTP
  requests to localhost. Existing STUN address discovery is distinct from
  listing a game. No public master publishing was enabled on the dedicated
  fixture.

Evidence: `build/m0/m0b-internet-local.log`, `m0b-internet-coop.log`, the
`run-gg0MMa` and `run-4RLyuX` directories under `build/m0/internet`, and their
adapter manifests. Failed/interrupted setup logs remain alongside them.

## Remaining exit gate

| Gate | Current disposition |
| --- | --- |
| Full-system inventory and validation routes | Recorded in the existing 19-domain ledger and generated index; scope retained. |
| Ordinary two-peer payload, object snapshot and correction observations | Added for Xbox, CE, MCC and delayed/lossy joining/revert. Coverage limits are explicit above. |
| Empty/near-capacity world snapshots, full persistent-state ready oracle, independent reliable delay/reorder/stall cases | Still open. Existing bounded-queue fault probes are not high-load gameplay measurements. |
| Final numeric operating budgets | Still open for near-capacity and low-end/mobile cases. Freeze the measured desktop references for like-for-like comparison, not as universal ceilings. |
| Mission progression/profile ownership | Stronger negative baseline recorded; normal menu selection, reload, next-round play and full ownership qualification remain open. |
| Platform/preservation matrix | Linux and Android runtime unavailable; Android build failed on Windows tooling. Local campaign/split-screen rosters, rendered/audio behavior and family/failed-load transitions remain unqualified here. |
| Current testing delta | Source audit and 142 focused tests complete; newer-revision runtime coverage remains separate. |
| M1 and publication | Not authorized and not started. |

A baseline may contain known defects; M0 does not require fixing them. Its
open items are missing qualification, measurement or reproducibility, not a
request to repair these defects during investigation. Full campaigns and
four-player human qualification remain later-stage requirements, not claims
made by two-instance automated sessions. Advancing to M1 would require the
remaining M0 evidence or an explicit change to the gate; this report makes
neither change implicitly.

## Reproduction, review and retained evidence

Use the existing Windows build environment described in the original report.
The diagnostic launcher is generated with
`python tools/investigations/coop_m0_trace.py`, then selected with
`python configure.py --release --portable --pgo=off --lto=off --compiler-launcher=build\m0\trace\compile.cmd`.
Build `ninja windows`. A newly added instrumented translation unit needs an
explicit rebuild if an existing Ninja object has not yet recorded the generated
copy as a dependency; the profile-fixture run explicitly rebuilt that object.

Each native session manifest under `build/m0/sessions/m0b-*` records its exact
arguments, content hashes, peer executable hashes and HALO/SDL/M0 overrides.
Use a new output directory when invoking
[coop_m0_sessions.py](../tools/investigations/coop_m0_sessions.py).
`--activate-profile`, `--fresh-profile` and `--start-next-lobby` only operate
in the generated fixture build. They are not production settings or fixes.
The ordinary measurement runs did not enable those fixture controls.

[coop_m0_webrtc_windows.py](../tools/investigations/coop_m0_webrtc_windows.py)
builds the transport adapter and invokes the existing Chromium/Firefox oracle.
[coop_m0_internet_windows.py](../tools/investigations/coop_m0_internet_windows.py)
takes the built site, private map-folder and native executable paths, plus
optional `--coop`. It requires the pinned privately extracted Mosquitto and
`NODE_PATH`/`M0_CHROMIUM`; the transport runner also uses `M0_FIREFOX`. These
adapters retain upstream acceptance predicates, and do not alter production
browser/game files. Their OS adaptations and supplementary co-op mode mean
the recorded runs are not an assertion that the original POSIX runner works
unchanged on Windows.

Final review covered all follow-up tools, generated-copy boundaries, fixture
activation, evidence interpretation and the combined M0 documents. The review
added generated-copy hash checks, counted inventory enqueue peaks between
ticks, separated reused ticks after reverts, preserved failed-run evidence
and made broker-only discovery explicit. Three numerical-reducer tests pass;
the five production-function fault probes still reproduce. Python syntax,
generated JavaScript syntax, local documentation links, named source routes
and Git diff scope were checked. Production source/protocol/defaults are
unchanged, so no protocol increment or implementation build matrix is claimed.

Local logs, JUnit results, manifests and necessary native normal/profile/
diagnostic binaries remain under `build/m0`. The unchanged web baseline is
retained under `build/web/site`. The
[follow-up archive index](../build/m0/followup-archive-index.json) records 1,380
inactive save/trace members, their original relative paths, lengths and
SHA-256 hashes. All members and ZIP integrity were verified before loose
copies were removed. About 784.1 MB of evidence became a 5.34 MB archive.
Restore it relative to `build/m0` to rerun the measurement reducer. The prior
baseline archive remains intact. These local archives are not map packages
for distribution.

Cleanup removed 22.11 GB of copied native fixture maps/cache/binaries, and
approximately 15 GB of browser map storage and disposable dependency/build
outputs. Browser saves were exported before deleting private browser profiles,
including the two interrupted runs. Windows read-only files in the private
SDL clone required a verified, force-removal cleanup retry; the original
toolchains and source checkouts were untouched. Retained M0 material totals
about 327.8 MB, plus 20.94 MB for the required web baseline. Original maps,
profiles and checkpoints were preserved. No owned game or broker process
remains. The normal release/portable PGO-off/LTO-off build configuration was
restored. The root checkout still has its 20 ongoing changed/untracked paths;
this branch remains local and unpublished.
