# M0: recovery of changed objects, inventory and persistent domains

**Continuation:** The [running-timer and held-ammunition pass](coop_m0_recovery_timing.md)
addresses those two specific gaps below. This report retains its earlier
fixture results and limits; the complete recovery gate remains open.

Local investigation on `investigate-coop-rewrite`, beginning at `6e5297729`
on 13 November 2026. This extends the [first recovery pass](coop_m0_recovery.md)
and retains its [full 19-domain scope](coop_m0_recovery.md#full-system-recovery-coverage).
It is diagnostic characterization only: no production implementation, wire
format, defaults, map files or original saves changed. M0 remains open, M1
has not started, and nothing is published.

Linux and Android runtime remain explicitly blocked by the lack of a runner
and device/emulator. The earlier Android build blocker remains separate.
This pass uses hidden Windows native peers with null rendering and no audio;
it does not qualify browser, public-internet, visual or audible recovery.

## Fixture and oracle

The `--domains` mode of
[coop_m0_recovery.py](../tools/investigations/coop_m0_recovery.py) generates
separate copies and a separate manifest under `build/m0/recovery-domains`.
The original diagnostic mode still generates the same seven source copies;
its prior artifacts and evidence remain intact. The compiler checks original
and generated source hashes before accepting each intercepted compilation.

[coop_m0_recovery_domains.c](../tools/investigations/coop_m0_recovery_domains.c)
runs only in the generated build under `M0_RECOVERY_DOMAINS=filled` or `empty`.
It selects an existing player's weapon definition from loaded engine data,
creates at most five extra objects through normal allocation, and uses
existing engine/script interfaces. It changes no tag data, capacity or map
identity. It waits for two player units and the end of the cinematic, then
disables player input while authored scripts and the engine continue running.

The fixture establishes A, requests a save, establishes B, then requests two
reverts to A. The observer verifies the successful disk-save callback and
advancing epochs, rather than treating a save/revert request as success.

| Domain | Saved A | Deliberate B |
| --- | --- | --- |
| Object presence | Fixture object exists | Delete that exact handle |
| Object absence | Post-save object does not exist | Create a fifth object |
| Attachment | Two fixture objects attached at their root markers | Detach the child |
| Scale | Spare weapon scale 0.5 | Scale 2.0 |
| Spare weapon primary ammunition | Set total rounds to 20 | Set total rounds to 4 |
| Filled-inventory case | Existing held weapons | Delete those weapons through normal lifecycle callbacks |
| Empty-inventory case | Delete held weapons before saving | Add the fifth object to inventory through the existing pickup interface |
| Visible paused timer | Two minutes, ten-second warning, position 20/30 | Three minutes, twenty-second warning, position 100/80 |
| Allegiance vectors | Ally/friendly relation for unused team 9 toward unused team 6 enabled | Disable that relation |
| MCC speed | 0.75 | 0.5 |
| MCC campaign effects/music gains | 0.25 / 0.5 | 0.75 / 0.875 |
| MCC extra grenade slots, when authored | One in each supported extra slot | Three in each extra slot |

MCC calls remain guarded by the actual loaded-family predicate and use
MCC-owned interfaces. Extra slots are attempted only up to
`mcc_grenades_type_count`; unsupported slots cannot count as completed coverage.
The private MCC `a10` reports four supported types, permitting both extra
slots to be exercised. The two gain values are engine state observations,
not a test of audible volume or music playback.

[coop_m0_domain_measurements.py](../tools/investigations/coop_m0_domain_measurements.py)
uses the existing complete-capture and archive reader. It compares actual
engine state against saved host A independently on each peer. It requires
the intended mutation, not merely any difference: the saved object must
disappear, the new object must appear, the intended attachment must be
removed, and inventory must change in the requested direction. Missing
allocations, missing phases, ambiguous saves and overwritten checkpoints
cannot qualify the sequence.

Required absence is explicit: the post-save object's full salted handle
must disappear on recovery. This does not prove arbitrary allocator-slot
reuse; the fixture does not force reuse. Identity comparisons include the
handle, definition, type and name. Attachment checks read the actual engine
parent, not only the host resend cache. Inventory checks read actual unit
weapon handles and verify reference resolution.

Timers are visible but deliberately paused, so exact remaining-time and
layout comparisons are meaningful. Running timer phase, expiry and clock
restamping remain separate work. Only primary magazine total/loaded counts
are compared: a second fixed storage slot need not be an authored magazine,
and its unused state is not a useful oracle. Raw observations remain archived.

The expected numeric values are exactly representable. Moving positions,
raw process addresses and live tick numbers are not equated. QPC intervals
associate the private HUD and epoch captures; selected peer skew is recorded
and checked against the existing 1.1-second observation bound. Recovery
windows begin two seconds after each request and retain every disagreement.
As before, an explicitly controlled shutdown may truncate the final capture;
that partial sample is excluded and recorded, never used as evidence.

## Results and interpretation

The [result record](coop_m0_recovery_domains_results.json) contains the final
case summaries, mutations, saved/restored differences, readiness windows,
build/content hashes, test results and cleanup counts. Complete reductions,
commands, logs and verified save/trace archives remain in
`build/m0/recovery-domains`. They contain no copied map packages.

All six runs established every intended mutation: one filled-inventory and
one empty-inventory case on Xbox `a30`, CE `b30` and the local MCC `a10`.
Each run completed both reverts without an early process exit or discarded
capture. Selected peer snapshots were less than 0.4 seconds apart, and no
missing unit/parent/weapon reference was found in the selected captures.

Both peers reconstructed the saved object, removed the post-save object,
restored the requested held or empty inventory, scale, paused timer and the
tested allegiance bits. All sampled post-settling recovery frames agreed on
those fields. The host also restored the saved attachment; the client remained
detached throughout both recovery windows while reporting ready.

Both MCC cases restored speed 0.75, effects/music gains 0.25/0.5 and extra
grenade-slot counts 1/1 after changing them to 0.5, 0.75/0.875 and 3/3.
These are independently owned MCC values, not inferred from shared object
agreement. The longest second recovery windows were approximately 39 seconds
on Xbox, 31 seconds on CE and 41.3 seconds on MCC. The slower MCC simulation
accounts for fewer sampled ticks over a longer wall-clock interval.

The unchanged `net_compare` judge reported no problems or divergences in all
six cases. It does not inspect these attachment or ground-ammunition fields,
so that result is not complete recovery acceptance.

### Attachment recovery failure

The attached child existed and was attached on both peers before saving.
After detachment and revert, the engine restored the host's parent/child
relationship but the client reconstructed the child without that attachment.
This is a new recovery negative baseline, separate from the objective,
navigation and vitality failures in the earlier pass.

In [network_coop.c](../port/linux/game/network_coop.c),
`network_coop_note_detach` removes the host cache entry.
`network_coop_reset_recovery` does not reconstruct that cache from the saved
world. `host_send_attachments` walks only remaining cached entries, so the
restored engine relationship alone does not repair the client. No fix or
replacement ownership scheme was implemented.

### Initial scale and ground-ammunition observations

The initial fractional scale did not agree on the client in any of the six
cases before the first save; later B and restored A did agree. `host_send_object_looks` assumes
an object encountered for the first time already looks the same on each
peer, records its current scale and skips sending it. A script changing
scale immediately after creation can precede that first encounter. This
fixture preserves that initial-state observation separately from the
successful scale recovery after a later observed change.

The spare weapon's primary ammunition already disagreed before the first
revert: host 20/20 versus client construction defaults. The earlier discrepancy
must not be described as caused by checkpoint recovery.
[network_objects.c](../port/linux/game/network_objects.c) carries magazine
counts in `distributed_inventory`, not `distributed_object_change`. The spare
weapon is outside an inventory. Ground ammunition, subsequent pickup and
non-default held ammunition need a dedicated end-to-end contract/check before
declaring player-visible damage or ammunition failure. The raw mismatch stays
in the results; it is neither hidden nor treated as healthy convergence.

The generated fixture uses `game_save_totally_unsafe` and direct script
revert, not pause-menu controls. Passing these checks does not establish
safe checkpoint eligibility, restart UI, ordinary mission progression,
running timer behavior, reload, active combat or full campaign compatibility.

## Remaining full-system obligations

The original 19 contracts all remain in scope. These observations narrow the
objects, inventory, world, presentation, checkpoint and MCC rows; they do not
remove session, transport, content, movement, actors, combat, events, scripts,
scaling, policies, social, camera or multiplayer preservation work.

Still required are changed live rosters/options during recovery; independent
end-to-end stream interruption and browser recovery; failed loads/saves;
forced slot reuse and dependency/allocation failures in the real engine;
non-default held ammunition, reload and pickup histories; moving vehicles,
standing riders and prediction history; active AI/combat/projectiles;
animation/region changes and active world/device/broken-surface states;
running timers, cinematic/camera effects and actual audio/visual output;
MCC names and broader campaign/script state; consumed scaling budgets,
social/policy lifetimes; and solo/local split-screen/competitive preservation.
The earlier function probes cover pieces of these obligations but do not
stand in for the missing runtime cases.

Known defects are characterized M0 baselines, not authorization to fix them.
Neither the full persistent-state oracle nor the full M0 exit gate is passed.

## Reproduction, review and retention

Generate with `python tools/investigations/coop_m0_recovery.py --domains`;
configure release/portable with PGO and LTO off and
`--compiler-launcher=build\m0\recovery-domains\generated\compile.cmd`, then
build `ninja windows`. Use a fresh output folder with the existing
[session runner](../tools/investigations/coop_m0_sessions.py), `--seconds 90
--input none`, the appropriate family/map arguments, and
`M0_RECOVERY_DOMAINS=filled` or `empty`. Exact command/environment and content
hashes are in each manifest. Never run more than two game instances or point
save output at original player data.

Reduce with `python tools/investigations/coop_m0_domain_measurements.py
<session-folder>`; archived CSVs can be read without unpacking them.
The [oracle tests](../tools/investigations/test_coop_m0_domain_measurements.py)
check explicit absence, stale inventory, intended mutations and refusal to
qualify unsupported MCC slots. Production-function tests and runtime evidence
are recorded separately. The final review includes generated-only boundaries,
the original diagnostic mode, field semantics, false agreement, incomplete
evidence, map-family guards, source explanations and complete scope.

The Windows diagnostic build succeeded. The focused suite passed **158 tests
with no failures or skips**, covering the eight observer tests and the existing
checkpoint recovery, epoch, timing, roster and MCC checkpoint suites. The eight
observer tests were rerun successfully after adding the unused-magazine-storage
check. Final verification re-reduced all six archives and checked the intended
mutations, epochs, source/generated/binary hashes, unchanged original maps and
profile, archive integrity and every member hash. Document links, named source
routes, Python syntax and Git diff scope were reviewed. This was self-review;
no delegated review or remote CI was performed.

Per-run cleanup removed 13,655,792,548 bytes of private map/resource/cache and
executable copies. Another 236,773,547 bytes of temporary build and test output
was removed after validating its resolved paths and absence of links. The 556
private save/trace files (464,438,819 bytes) remain in six verified lossless
archives totalling 53,287,917 bytes, with original paths and SHA-256 hashes in
adjacent indexes. Required binaries/symbols, manifests, generated copies and
useful logs remain. Original maps/saves and installed toolchains are untouched.
Retained material for this pass is about 125 MB; all M0 evidence and required
builds now total about 2.01 GB, including earlier captures and archives.
Normal release/portable, PGO-off/LTO-off configuration was restored, and no
owned game process remains. No M1, production change, PR, push or publication
occurred.
