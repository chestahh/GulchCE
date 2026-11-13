# M0: readiness, reliable delivery and local rosters

13 November 2026 continuation from `9584b417d`, on local
`investigate-coop-rewrite`. Read with the [19-domain inventory](coop_m0.md)
and [remaining M0 gates](coop_m0_followup.md#remaining-exit-gate).
The full co-op system remains in scope. This work changes investigation
fixtures and documentation only; no production implementation, protocol,
defaults, map data or original saves change. The [result record](coop_m0_readiness_transport_results.json)
contains the measured cases and content/build hashes. M1 has not started and nothing
has been published. Linux and Android runtime remain explicitly blocked by
the absence of a runner/device. Earlier Android build failures remain separate.

## Ordered receipt is not complete reconstruction

[coop_m0_ready.py](../tools/investigations/coop_m0_ready.py) extends the
existing [checkpoint recovery fixture](../tools/harness/tests/coop_checkpoint_recovery.c)
in generated copies. It extracts the actual epoch gate, object reconstruction,
inventory retry, synchronization handler and client tick. Engine construction,
tag lookup, the checkpoint-reset callback and outgoing gameplay callbacks retain the existing fake-world
boundaries. These are characterization probes, not tests requiring a defect
to remain in the eventual rewrite.

| State when the ordered marker arrives | Checkpoint ready | Object synchronization flag | Unresolved dependency | Gameplay callbacks reached by the next client tick |
| --- | --- | --- | --- | --- |
| Successful object construction | Yes | Yes | None in this fixture | Six |
| Failed object allocation | Yes | No | Object absent; repair requested | Six |
| Inventory refers to an absent weapon | Yes | Yes | One queued inventory | Six |

All three executions completed their characterization checks. Later successful
construction repaired the missing object; creating the missing weapon let the
existing retry path apply the queued inventory. The positive control verifies
that a normal object is present. These results do not mean the two incomplete
states satisfy the desired readiness contract.

The source explains the distinction:

- [network_distributed.c](../port/linux/game/network_distributed.c):
  `distributed_message_epoch_valid` accepts the ordered marker and sets
  `distributed_checkpoint_synchronized`; `distributed_checkpoint_ready`
  returns that flag without consulting object or inventory completion.
- [network_objects.c](../port/linux/game/network_objects.c):
  `network_objects_handle_synchronized` preserves the separate failed-object
  state, but `network_objects_client_tick` gates its gameplay callbacks on
  `distributed_checkpoint_ready`. The callback count proves those call sites
  are reached, not that six real gameplay actions occurred.

This narrows a previously source-only concern into two reproducible negative
baselines. It does not implement a new barrier or repair policy.

### State outside that marker

The following are source-established boundaries, not new runtime claims.
They explain why passing an ordered-object test cannot certify the whole world.

| State | Current owner and delivery route | Still required for a complete persistent-state oracle |
| --- | --- | --- |
| Object identity, descriptions and standard inventory | `network_objects_client_asked` sends the checkpoint boundary, ranked descriptions, live inventories and marker reliably. | Prove construction, binding, parent/seat relationships, deletion/absence and pending-inventory completion, including failed dependencies. |
| Actor behavior, movement and vitality | `network_actors` and distributed unit/object state handlers have their own update paths. | Compare authoritative discrete state and encoded numeric tolerances after dependencies exist. |
| Devices, names, transforms, looks, attachments and broken surfaces | `network_coop_host_tick` invokes separate state producers; `send_to_clients` uses the ordinary datagram destination. | Compare current state and absence, including lost removals and state that arrives before or after the marker. |
| Camera, HUD, screen effects, loops and script events | Separate presentation/state/event producers in `network_coop`; earlier probes establish event-loss and stale-cache failures. | Include current presentation and durable script effects, with expiry/seek rules for transient events; rendered/audio checks remain necessary. |
| MCC campaign and extra grenade slots/names | [mcc_network.c](../port/linux/game/mcc_network.c) and MCC-owned name synchronization use independent snapshots/refresh paths with family guards. | Verify MCC state and lifecycle separately; do not use these adapters for Xbox or CE. |
| Session roster, options and profile progression | Admission/session, checkpoint and profile owners in the existing inventory. | Preserve live membership/options and correct per-profile awards independently of object readiness. |

The complete persistent-state oracle remains open. The expected contract is
explicit: distinguish transport receipt, successful reconstruction and eventual
repair; never use receipt alone as evidence that all persistent state agrees.
The subsequent [real-engine recovery comparison](coop_m0_recovery.md) observes
saved state, deliberate changes and two reverts independently of the marker.
It reproduces stale objective/navigation state on ready clients and a retained
vitality override on both peers. Its coverage table retains all 19 contracts;
the bounded comparisons do not complete the full-state oracle.

## Reliable-stream faults over real loopback UDP

[coop_m0_stream.py](../tools/investigations/coop_m0_stream.py) extracts eight
actual stream functions from [p2p.c](../port/linux/src/p2p.c), including creation,
framing, update, pending-buffer flushing, producer backpressure and disposal.
It compiles [the fixture](../tools/investigations/coop_m0_stream.c) with the
unchanged vendored KCP implementation. Constants and enums come from the owning
source. No alternative transport algorithm is substituted.

Two pre-opened stream endpoints exchange real UDP datagrams through a private
loopback relay. The game-side TCP reader/writer and peer encryption/admission
are controlled boundaries. A deterministic byte pattern of 1,609,776 bytes
exercises the byte stream; its size matches the earlier maximum-slot snapshot
observation, but its contents are synthetic and it is not an actual game
snapshot. Receiver writes are limited to 173 bytes to exercise partial writes.

| Schedule | Faults | Executions |
| --- | --- | --- |
| Normal | 10 ms scheduled one-way delay | Three seeds |
| Impaired | 30–90 ms scheduled delay, seeded approximately 10% stream-datagram drops, duplicates and reorder | Three seeds |
| Wire stall | Hold reliable datagrams scheduled during 100–2,100 ms until 2,100 ms | Three seeds |
| Reader stall | Application writer returns would-block during 100–2,100 ms | Three seeds |
| Combined | Impaired delivery plus wire stall, followed by reader stall during 2,100–4,100 ms | Three seeds |

All 15 final executions delivered the entire byte pattern exactly once and in
order. The maximum sampled sender backlog was 128 KCP messages; the retained
pending buffer reached its 16,384-byte limit during blocked-reader cases.
Independent unreliable heartbeats continued during the reliable stalls. The
relay records actual injected drop/duplicate/reorder counts and its own queue
peak; its 8,192-entry bound is a fixture limit, not a product capacity claim.

Completion took 1,990–8,290 **virtual** milliseconds in these final schedules.
The virtual clock advances without wall-clock sleeps, so these numbers are
scheduled progress observations, not latency, throughput or CPU benchmarks.
Seeds are 1, 42 and 8,675,309. Real OS socket delivery is still an external
boundary; seeded scheduling is not a claim of bit-identical kernel timing.

Two isolated negative controls were detected for the intended reasons:
flipping a payload byte failed the byte-order/content oracle; removing the
production sender-window guard failed the backlog bound. Deliberate faults
never entered the working production source.

The first combined fixture blocked its reader at the same time as all incoming
reliable data, so two seeds never exercised receiver backpressure. The fixture
correctly failed that requirement; the final schedule staggers those stalls.
An initial sandboxed attempt could not deliver loopback traffic; the successful
runs used the authorized local-network execution environment. Neither setup
failure is evidence of a product transport defect.

This closes the narrow component gap for independent reliable delay, bounded
reader congestion, loss, duplication and reorder on native loopback UDP.
It does **not** establish end-to-end co-op recovery, stream-open/close negotiation,
timeout behavior, encryption/replay-window behavior, WebRTC, public internet
or Android/Linux runtime. The game's retained WebRTC evidence remains separate.

## Two local players across map families

The retained UI-instrumented baseline ran one hidden Windows process per case,
using `HALO_NETWORK_TEST=local:<level>:coop`, two attached virtual gamepads,
and the existing generated-copy `M0_UI_INPUT` controller-event fixture.
Controller 1 received A events at 6, 10, 14 and 18 seconds. The diagnostic hook
posts ordinary UI events; it does not directly add players or edit the roster.
The baseline engine sources are `887d9ac9d`; this binary predates the separate
failed-load console fix. Manifests retain its exact executable hash.

| Private session under `build/m0/roster` | Content | Observed result |
| --- | --- | --- |
| `local-events-isolated` | Xbox A30 | Controllers 0 and 1 admitted on machine 0; both living player records and horizontal split-screen gameplay. |
| `local-ce` | CE `custom_maps/b30` and its shared resources | Same two-local-player admission and living records; horizontal split-screen gameplay. |
| `local-mcc` | MCC `mcc_maps/a10` | Same admission; after the opening sequence both players produced living records and horizontal split-screen gameplay. |

Complete 2560×1440 captures were visually inspected for all three. Xbox
`frame02400.bmp`, CE `frame02100.bmp` and MCC `frame02100.bmp` show two views
with HUDs. MCC's last frame was interrupted by fixture termination and is
incomplete; it is excluded from visual evidence. All three processes remained
alive until controlled cleanup. Audio was disabled and no original save changed.

This establishes the short two-local-player baseline through the existing
local-game test route, not the ordinary campaign-menu route, independent
controller movement/combat, multiple local players on each of two network
machines, four-player behavior, completed missions or full split-screen
presentation. A posted UI Start event did not establish pause-menu behavior;
Restart/Revert runtime coverage remains open.

Two exploratory runs are retained but excluded from that result. `local-two`
used the virtual-gamepad script, whose clock is started by the port lobby-open
hook; the legacy local test lobby did not exercise that path and only one
player joined. `local-events` overlapped that host on its shared local server
port and joined it as a second machine. The isolated repeat waited for cleanup
and proved two players on one machine. No more than two game instances ran
concurrently. This fixture overlap is not presented as an intentional network
roster check or diagnosed product defect.

## Reproduction and review

Use the existing 32-bit Windows clang environment described in the M0 report:

```text
python tools/investigations/coop_m0_ready.py
python tools/investigations/coop_m0_stream.py
python -m pytest -q tools/test_coop_checkpoint_recovery.py tools/test_checkpoint_epoch.py tools/test_p2p_shutdown.py
```

The adjacent regression suites passed all 80 tests. The readiness and stream
scripts write source hashes, extracted-function identities and observations
under `build/m0/readiness` and `build/m0/stream`. The stream fixture needs
permission to use private loopback sockets. No game processes are launched by
these two scripts. Production builds were not repeated for investigation-only
changes; the previous build/runtime results retain their stated limits.

The runtime cases use [coop_m0_local.py](../tools/investigations/coop_m0_local.py)
with the retained `local-*-spec.json` files and `local-events-controls.json` in
`build/m0/roster`. Choose a fresh output folder for a repeat; run local-host
specifications sequentially after their cleanup completes. Existing private
loopback addresses alone do not isolate the legacy local server port.

Final self-review checked the extracted-function boundaries, source-owned
constants, synthetic-data limits, readiness interpretation, negative controls,
runtime isolation and combined documentation. The corrected combined fault
schedule and production retry initialization were rerun; all final fixture
observations and the 80 adjacent regression checks above are retained.
Python syntax, local links, named source routes, JSON consistency and diff
whitespace were checked. No production refactoring was needed or authorized.

The roster runner removed about 5.38 GB of private map/cache/binary copies.
`build/m0/roster/evidence.zip` retains 264 inactive save, trace and capture
files: 794.4 MB compressed to 129.2 MB. Its adjacent `archive-index.json`
records original paths, lengths, SHA-256 hashes and incomplete-capture status.
ZIP integrity and every member hash were verified before loose files were
removed. Restore the archive relative to `build/m0/roster`. Original maps,
saves, required game builds and installed toolchains were preserved.
Disposable compiled probe binaries were removed. Total retained M0 material
is about 1.68 GB, including prior evidence. All owned game fixtures finished
cleanup, and the original build configuration is unchanged.

The complete persistent-state oracle, game-level fault recovery, high-load
operating budgets, ordinary campaign-menu entry, multi-local-player network
rosters, broad audiovisual coverage, natural completion/next-round play and
newer-testing runtime coverage remain open. Linux and Android runtime remain
explicitly blocked. M0 is not signed off; this continuation starts no M1 work.

The subsequent [real-engine snapshot load report](coop_m0_load.md) adds
Windows slot/memory saturation measurements and versioned desktop budgets.
It updates that portion of the evidence without closing the remaining
full-state, platform or operating-envelope qualifications.
