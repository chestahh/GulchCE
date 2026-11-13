# M0 real-engine snapshot load and desktop budgets

Investigation baseline: `1e021e462` on unpublished `investigate-coop-rewrite`,
13 November 2026. This extends the [full co-op inventory](coop_m0.md) and
[baseline measurements](coop_m0_followup.md), without starting M1 or changing
production code, protocol, capacity constants, original maps or saves.

## Method and interpretation

[coop_m0_load.py](../tools/investigations/coop_m0_load.py) builds on the existing
generated-copy trace launcher. It adds the
[load fixture](../tools/investigations/coop_m0_load.c) to a private generated
copy of `network_objects.c`. Before the first client snapshot, the host selects
the first existing weapon or biped's loaded tag and calls
`object_placement_data_new` and `object_new` for the requested extra population.
Allocation stops at the first failure. The ordinary replication, construction,
inventory and synchronization handlers remain in use. No original content or
allocator limit is changed; the fixture and generated binaries must not ship.

Objects are placed on a grid relative to the selected template. The fixture
uses the normal never-automatically-delete placement flag. It does not attach
AI, force activation, disable collisions, repair failed allocations or alter
the client's readiness rules. Many added weapons are inactive. Consequently
these runs measure populated snapshot and allocation pressure, not thousands
of simultaneously active combatants, visible objects or rendered frames.
Biped cases add inventory-bearing units, not an AI population.

At snapshot boundaries the trace records actual networked object count, unit
count, total object count, highest occupied networked slot, and a 32-bit
fingerprint of the ordered object identities and tag IDs. A matching count and
fingerprint is a useful consistency observation, not a collision-free full
state oracle. Inventories, scripts, devices, parents and presentation still
need the separate [state recovery qualification](coop_m0_readiness_transport.md).
The existing ready marker does not establish all of those states are usable.

`objects_information_get` supplies total/active object counts and the fraction
of the 8 MiB pool unavailable as a contiguous tail allocation. That fraction
includes fragmentation; it is not interchangeable with live allocated bytes.
The actual object-slot limit is 8,192 across all object types. Campaign scenery
and other non-networked objects consume slots too, and the byte pool can run
out before the slot limit.

The diagnostic build uses Windows release/portable, PGO off and LTO off, on an
Intel Core i7-14700K. Each session has two hidden native processes, a null
renderer, disabled audio, private copied maps and saves, and loopback networking.
No render/GPU or audio performance claim follows. Synchronous CSV logging adds
overhead; timings include it. Do not compare directly with uninstrumented
profiling figures or infer mobile performance from this desktop.

[coop_m0_load_measurements.py](../tools/investigations/coop_m0_load_measurements.py)
reduces these observations alongside the existing socket/correction reducer:

- Snapshot construction starts after fixture allocation and count observation.
  Fixture allocation time is recorded separately and is not network CPU cost.
- Offered bytes count reliable application messages, not acknowledged wire
  bytes. Socket rates exclude IP/TCP headers, acknowledgements and retransmits.
- Initial receipt timing starts at the first full snapshot's beginning and
  ends at the first client count observation. Cached retry replies do not
  restart that stopwatch. This is an initial-world fixture, with no requested
  restore or map transition.
- Steady windows start five seconds after the first object-ready observation.
  Simulation `game_tick`, the complete `network_distributed_tick`, and the host
  object sender have separate wall-time distributions. Their percentiles must
  not be added and described as a measured full-frame percentile.
- Process private bytes and working set are sampled once per second across
  the session, including loading. They are not isolated co-op allocations or
  leak qualification. Full transition cycles and soak remain separate gates.

## Results

Four 30-second pilots established the load ladder and receiver attribution.
Ten final 60-second runs use the same instrumented binary, including ordinary
controls for all three map families, repeated Xbox/CE saturation and one
delayed/lossy Xbox case. The [result record](coop_m0_load_results.json) preserves
both groups, all hashes, distributions, comparisons and budget evaluations.
Pilots are excluded from the final budget checks. Repeated runs show variation;
these small samples do not estimate a population-level latency percentile.

The saturated weapon cases request 8,000 extra objects. Xbox creates 7,099
before the 8 MiB byte pool refuses another allocation; its contiguous free
tail is below 0.01%. CE creates 7,233 and MCC creates 7,635 before reaching
8,192 total slots. Their pools still have byte capacity: object size and the
map's pre-existing population determine which resource runs out first.
The 1,024-biped case creates 1,024 units plus their normal child objects, giving
1,044 inventory-bearing units in the initial snapshot; it does not create
1,024 AI actors.

| Final case | Networked objects | All objects | Offered snapshot bytes | Host construction ms | First count receipt ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Xbox control | 107 | 568 | 14,592 | 0.216 | 1.859 |
| Xbox saturation 1 | 7,206 | 7,667 | 954,504 | 12.096 | 589.767 |
| Xbox saturation 2 | 7,206 | 7,667 | 954,504 | 13.772 | 592.713 |
| Xbox saturation, delayed/lossy | 7,206 | 7,667 | 954,504 | 7.970 | 630.905 |
| Xbox, 1,024 extra bipeds | 2,155 | 2,616 | 351,472 | 5.278 | 161.367 |
| CE control | 163 | 959 | 25,988 | 0.770 | 2.013 |
| CE saturation 1 | 7,396 | 8,192 | 983,636 | 18.964 | 646.850 |
| CE saturation 2 | 7,396 | 8,192 | 983,636 | 13.267 | 719.741 |
| MCC control | 188 | 557 | 30,388 | 0.137 | 2.253 |
| MCC saturation | 7,823 | 8,192 | 1,041,256 | 8.265 | 532.221 |

Times above include observation overhead and exclude fixture creation. The
delayed case sets the existing receive delay to 50 ms per peer and datagram
loss to 5%; it does not implement the full P1 jitter profile or independent
reliable-stream faults. The snapshot receiver observed matching initial
counts and identity/tag fingerprints in every final case. All final cases
recorded zero client object-create failures, zero queued inventory peak and
zero event-queue overflow. This does not contradict the separate host-side
allocation exhaustion, or qualify all persistent state.

| Steady host payload | Mean KiB/s | p95 complete-second KiB |
| --- | ---: | ---: |
| Xbox control | 27.61 | 32.83 |
| Xbox saturation, P0 repeats | 27.62–27.64 | 32.76–32.80 |
| Xbox extra bipeds | 94.04 | 98.83 |
| CE control | 111.15 | 144.07 |
| CE saturation repeats | 562.91–563.39 | 620.42–625.30 |
| MCC control | 131.80 | 145.02 |
| MCC saturation | 131.69 | 147.25 |

All final simulation-tick p99 values are below 4.98 ms and complete network-tick
p99 values below 1.62 ms. These short null-renderer opening scenes do not prove
30 Hz under active combat or rendering. Maximum observed active object counts
were 218 in the Xbox load cases, 805–828 in saturated CE and 450 in MCC, far
below the total object counts. Steady CE networked population decreased to
roughly 6,600–6,700; these are initial saturation snapshots, not a sustained
8,192-object soak.

Peak private commit was about 426–440 MiB per process for Xbox,
658–694 MiB for CE and 1,047–1,050 MiB for MCC. The largest increase over the
same-family, same-binary control was 12.87 MiB for a host and 28.53 MiB for a
client. CE's host peak was lower in the saturation runs than its control:
loading/caching variation makes this a coarse process gate, not isolated
network memory accounting. No leak conclusion follows from one-minute runs.

No process exited before controlled cleanup, and the unchanged player judge
reported no overall problems/divergences. Nevertheless, the first CE saturated
run recorded one aim mismatch (maximum 21 degrees); its repeat did not.
MCC saturation recorded a 143.27-world-unit kind-0 object state write at tick
323, absolute slot 23, versus a 13.58 maximum in its control. These observations
remain unexplained; an all-object write is not equivalent to a local-player
correction or perceptual jitter. No correction threshold was relaxed or
invented to turn them into a pass. CE shader-type errors occurred in its
ordinary control too and are retained, not attributed to load without evidence.

Most critically, saturated MCC logs report later campaign object creation
failures and critical zero-slot garbage collection. Saturated Xbox reports
critical byte-pool pressure. Snapshot receipt and a passing player comparator
therefore do **not** establish healthy campaign capacity. The instrumentation
keeps the original slot/byte limits and records the refusal; it does not lower
advertised limits or repair behavior.

The frozen gates below expose snapshot-call overruns in every final load case,
the CE bandwidth overrun in both repetitions, and the Xbox pool-headroom
violation. All measured final cases meet the initial count-receipt, offered-byte,
steady tick, process-growth and observed queue gates. Their independent
functional/capacity limitations remain. Because logging is synchronous, the
snapshot-call exceedances establish an instrumented comparison baseline, not
proof of an equivalent uninstrumented production stall; a performance change
must also verify its production cost with lower-overhead measurement.

## Frozen desktop gates and proposed rewrite targets

The [versioned budget record](coop_m0_desktop_budgets.json) fixes the following
numbers for this declared desktop envelope. It was written after the Xbox
control/load pilot and before the remaining family saturation results. A failed
case remains failed; changing a budget requires a new version and rationale.
These are comparison gates and design targets, not evidence that the entire
co-op system can operate within them on all supported platforms.

| Measurement | Gate | Basis and practical limit |
| --- | ---: | --- |
| Simulation tick p99 | 25 ms | Reserves part of the 33.33 ms simulation interval for networking and other work; rendering and incoming-message processing are not included here. |
| Complete distributed network tick p99 | 5 ms | Reserves roughly 15% of a 30 Hz interval for network tick work. Separate percentiles do not establish the combined worst case. |
| One synchronous snapshot construction call | 5 ms | Same responsiveness allocation. The measured saturated Xbox call already exceeds it; this is a rewrite target, not a retroactive baseline pass. |
| Initial object-count receipt on P0 | 1,000 ms | Approximately 1.7 times the first fully instrumented saturated Xbox result; excludes fixture creation, map loading and complete world readiness. |
| Initial receipt with the existing 50 ms receive-delay setting | 1,500 ms | A separate stress allowance to test, not a claim that the knobs reproduce the entire plan's P1 jitter/loss profile. |
| Offered object/inventory snapshot | 2 MiB | Above the 1,609,776-byte maximum-slot extracted-function fixture; other world components and transport buffering need separate budgets. |
| Steady sent payload per peer: mean and p95 full-second bucket | 256 KiB/s | Above the earlier 150,566-byte maximum family p95 with explicit headroom. This is application payload, not an internet upload requirement. |
| Process private-commit increase over same-family control | 32 MiB | A coarse growth gate allowing four current object-pool sizes. Does not assign those bytes to co-op or replace future allocation ownership accounting. |
| Contiguous object-pool headroom | At least 20% | Matches the existing garbage collector's free-memory target. Saturation probes deliberately violate it and must not be certified as supported play. |
| Client create failures, inventories pending at ready, event overflows | Zero | Necessary observations for these controlled fixtures; insufficient to qualify full persistent-state convergence. |

The distinction between frozen **reference observations** and proposed
**operating targets** is deliberate. The implementation is allowed to fail a
target in M0: M0 records the baseline and the defect. No production threshold,
advertised player/actor/object capacity or gameplay behavior is changed here.
Full-world memory ownership, map/content transfer deadlines, sustained active
AI/combat and platform-specific budgets remain unqualified. The two-hour soak,
repeated transitions and complete state oracle remain separate gates.

## Reproduction and evidence

With the existing Windows compiler environment configured:

```text
python tools/investigations/coop_m0_load.py
python configure.py --release --portable --pgo=off --lto=off --compiler-launcher=build\m0\load\generated\compile.cmd
ninja windows
```

Use `coop_m0_sessions.py` with the generated Windows binary, private output
directory, original map/profile **source** paths and the documented family
arguments. Set `M0_LOAD_OBJECTS` to the extra count (`0` for the instrumented
control); set `M0_LOAD_BIPEDS=1` only for biped cases. The diagnostic fixture
activates only in the host's co-op snapshot handler. These are single-world
processes: the fixture intentionally runs once, not after every map/restore.
After each run, reduce its CSVs with:

```text
python tools/investigations/coop_m0_load_measurements.py <session-output>
```

The local `build/m0/load` directory retains per-case commands, manifests,
binary/content hashes, detailed reductions, logs and archive indices. Each
case's `evidence.zip` contains its inactive saves and CSVs under their original
relative paths. ZIP integrity and every member's SHA-256 are verified before
loose copies are removed. Restore into that case directory to repeat reduction.
Disposable private map/resource copies, executables and generated map caches
are removed after each run. No `savegame.bin` is discarded. Restore normal
configure arguments after diagnostic builds; do not ship the instrumented
binary or publish retail assets.

## Platform and scope limits

Linux runtime is **blocked**: no Linux/WSL/VM runner is available. Android
runtime is **blocked**: no device or emulator is available. The Android build
has its separate previously recorded Windows shell/CMake blocker. None of
these checks is waived or marked passing. Browser high-load performance,
native/browser high-load interoperability and internet-path performance are
unmeasured by this Windows-only diagnostic fixture; earlier transport checks
do not qualify them. Large active AI/combat populations, moving vehicles,
multiple local players across peers, dedicated-host pressure and more than
two machines also remain outside this measurement envelope. The full co-op
system remains in the staged plan and validation matrix.

## Review, validation and disposition

Final self-review covered the generated-only instrumentation, engine creation
and counting interfaces, timer boundaries, compatible control selection,
failure interpretation, combined M0 ledger and preservation rules. The reducer
separates a first full snapshot from cached retries; the report rejects memory
comparisons across different binaries or content and leaves missing metrics
unmeasured. No production refactoring, protocol bump or behavior change was
needed or authorized. Protocol 44 and the existing capacities are unchanged.

The instrumented Windows build completed; all 14 measurement sessions completed
without early process exit. Three existing numerical-reducer tests passed.
The final evidence check verified all 14 records, ten complete final gate sets,
actual counts against serialized entries, fingerprints, binary/generated/source
hashes, unchanged original map/profile hashes, archive member indices, fixture
cleanup, Python syntax and local documentation links. Budget target failures
above are retained results, not failed test infrastructure or waived criteria.

Per-run cleanup removed 29,790,787,220 bytes of private map/resource/cache and
executable copies. Final cleanup removed another 231,524,720 bytes of temporary
Windows build output, retaining the required diagnostic executable, symbols,
SDL library, generated sources and manifests. New load evidence/build material
occupies about 91.5 MB; all retained M0 material totals about 1.77 GB. Private
saves and traces are preserved in verified archives. Original maps, saves,
source checkouts and installed toolchains were preserved. No owned game process
remains, and normal release/portable, PGO-off/LTO-off configure arguments are
restored.

The Windows initial-snapshot measurement gap is covered for these declared
fixtures. Full operational capacity, complete persistent-state convergence,
correction semantics, production instrumentation-cost calibration and
platform-specific operating budgets remain unqualified. The full M0 exit gate
is still open. No M1 work, rewrite, branch publication, PR or push occurred.
