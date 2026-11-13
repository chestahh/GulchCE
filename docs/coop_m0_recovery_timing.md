# M0: running countdown and held-ammunition recovery

**Continuation:** The [identity/ownership pass](coop_m0_recovery_identity.md)
adds bounded weapon-slot reuse and replacement-inventory observations. The
full-system ledger remains authoritative; earlier findings below are retained.

Local investigation on `investigate-coop-rewrite`, starting from `c75240132`
on 13 November 2026. This continues the [domain recovery pass](coop_m0_recovery_domains.md)
and retains the [complete 19-domain ledger](coop_m0_recovery.md#full-system-recovery-coverage).
It changes diagnostic tools and evidence only. No production code, protocol,
defaults, original maps or original saves changed. M0 remains open; M1 and
publication remain unstarted.

Linux runtime is **blocked: no runner**. Android runtime is **blocked: no
device/emulator**; the earlier Android build blocker is separate. These are
Windows native loopback observations, not browser or public-internet recovery
qualification. Null rendering and disabled audio do not qualify visual or
audible output. Engine HUD logic can still execute with the null renderer.

## Method and acceptance

The `--timing` mode of [the diagnostic generator](../tools/investigations/coop_m0_recovery.py)
creates seven isolated source copies under `build/m0/recovery-timing/generated`.
It reuses the existing observer and adds a [bounded fixture](../tools/investigations/coop_m0_recovery_timing.c).
Both older diagnostic modes and their evidence remain unchanged. Compilation
checks the original and generated source hashes. The diagnostic driver lives
outside checkpoint storage so it can request two reverts of the same save.

Each run uses two hidden native peers, private copies of maps and a profile,
unchanged authored scripts, and a 90-second observation after two-player
readiness. The fixture waits until the opening cinematic ends and both players
have units and weapons. It freezes input, selects one held weapon per player,
and calls the normal engine interfaces:

1. Set primary total/loaded rounds to 20 and start a visible 20-second countdown
   with a five-second warning cutoff.
2. After 90 simulation ticks, request `game_save_totally_unsafe`. Require the
   successful disk-save callback, not merely the console command.
3. After another 90 ticks, change both primary magazines to four rounds and
   replace the timer with a 50-second countdown.
4. Request `game_revert`, then a second revert nine seconds later. Require
   advancing epochs and record readiness separately from actual state.

The [reducer](../tools/investigations/coop_m0_timing_measurements.py) checks
both weapon handles, their owning unit/inventory slots, and primary total and
loaded counts. It compares against saved host values, including the weapon
owned by the client's predicted player. This does not exercise firing, reload,
pickup, weapon replacement or queued dependencies.

For a running timer, compare expiry ticks rather than raw countdown samples
taken on different ticks. The expected expiry after each revert is the revert
clock plus the remaining duration recorded at the successful save. Compare
both peers independently with that expectation: two agreeing peers can both
be wrong. The expired `NONE` sentinel is reported separately and must not
be interpreted as a continually moving expiry deadline. Readiness, complete
captures, epoch sequence, intended mutations and resolved references are
independent evidence checks.

## Runtime results

Three cases completed: Xbox `a30`, Custom Edition `b30` and MCC `a10`.
All seven fixture phases ran, the intended mutations occurred, neither peer
exited early, and client readiness advanced through epochs 1, 2 and 3.
The [machine-readable results](coop_m0_recovery_timing_results.json) retain
values, sample windows, source/content/binary hashes and evidence locations.

| Observation | Xbox | Custom Edition | MCC |
| --- | --- | --- | --- |
| Both held primary magazines: saved 20, changed 4, restored 20 twice | Pass | Pass | Pass |
| Saved timer remaining | 510 ticks | 510 ticks | 510 ticks |
| Immediately after first revert | 330 ticks | 330 ticks | 330 ticks |
| Immediately after second revert | 60 ticks | 60 ticks | 60 ticks |
| Active expiry error after first revert | -180 ticks | -180 ticks | -180 ticks |
| Active expiry error after second revert's warning adjustment | -270 ticks | -270 ticks | -268 ticks |
| Second recovery client samples / observed span | 38 / 37 seconds | 31 / 30 seconds | 38 / 37 seconds |

At 30 ticks per second, the first revert loses six seconds immediately and
the second loses fifteen. HUD warning processing then adds six seconds in
Xbox/CE and 182 ticks in MCC, leaving expiry nine seconds early in Xbox/CE
and about 8.93 seconds early in MCC. Both peers agree on these wrong deadlines.
The first sampled expired sentinel still had 253 expected ticks remaining
in Xbox, 244 in CE and 262 in MCC. One-second sampling brackets that sentinel;
the table's exact active deadlines come from the timer's stored state.

Every included recovery-window sample had the correct held ammunition and
resolved player, parent and inventory references; client readiness was true
throughout those windows. There were no discarded final captures. The existing
`net_compare` judge reported no problems or divergences in all three runs;
its agreement checks do not test restoration of the timer's saved duration.

The authored MCC level saved again at tick 2344, after the final fixture phase
and roughly 39 seconds after the second revert. The initial reducer rejected
any later save, so its first incomplete classification is retained as
`build/m0/recovery-timing/mcc-initial-qualification.json`. The corrected rule
rejects any replacement before or between the two reverts and ends the second
observation window at a later save. Tests check both boundaries. No runtime
failure, early exit or intervening save was discarded to qualify this case.

## Source explanation

In [network_coop.c](../port/linux/game/network_coop.c),
`network_coop_reverted` advances the restored simulation clock to its prior
value, restamps the revert and shifts sleeping script threads. It does not
shift the HUD timer's restored reference time. In
[hud_messaging.c](../source/interface/hud_messaging.c),
`scripted_hud_get_timer_ticks` computes a running countdown from stored ticks
plus reference time minus current game time. The expired sentinel is a
separate branch. The diagnostic captures at the revert boundary distinguish
the immediate clock effect from later HUD processing.

`hud_render_timer` also rewrites timer storage on entry to the warning region:
it sets stored ticks to the cutoff and reference time to
`cutoff - remaining + current_time`. In these cases the second revert enters
that region immediately. This can extend the countdown after the initial
clock-induced loss; the report must retain both observations. The subsequent
expired sentinel is not a new formula failure. No timer correction is made.

Held ammunition is carried by `distributed_inventory` in
[network_objects.c](../port/linux/game/network_objects.c), unlike the basic
ground-object change record. The earlier ground-weapon mismatch therefore
does not establish held-ammunition recovery failure. These tests actively
check that distinction through the existing save and replication paths.

## Scope still open

This pass characterizes running countdown state and two held primary magazines
on the available family fixtures. It does not close the entire presentation
or inventory contract. Ordinary menu restart/revert, failed saves/loads,
changed rosters/options, forced salted-slot reuse, real-engine allocation and
dependency failures, reload/pickup histories, moving vehicles and prediction,
active AI/combat/projectiles, animation/regions, active world/device/surface
state, cinematic/camera effects, audio output, MCC names and broader script
state, consumed scaling budgets, social/policy lifetimes, local/competitive
preservation and end-to-end interrupted/browser recovery remain open.

The earlier objective, navigation, vitality and attachment failures remain
characterized defects. Known defects do not need fixing to record an M0
baseline; missing qualification must remain explicit. No finding here grants
permission to start M1 or reduces the full co-op scope.

## Reproduction and review

Generate with `python tools/investigations/coop_m0_recovery.py --timing`.
Configure release/portable, PGO off and LTO off with
`--compiler-launcher=build\m0\recovery-timing\generated\compile.cmd`, then
build Windows. Use a fresh session folder with
[coop_m0_sessions.py](../tools/investigations/coop_m0_sessions.py),
`--seconds 90 --input none`, the appropriate family/map arguments and
`M0_RECOVERY_DOMAINS=timing`. That environment switch activates the generated
fixture; it is not a shipping setting. Exact arguments, environment, content
and executable hashes are in the per-run manifests. At most two instances
may run concurrently; never use original save output folders.

Reduce with `python tools/investigations/coop_m0_timing_measurements.py
<session-folder>`. Archived traces are read directly. The
[observer tests](../tools/investigations/test_coop_m0_timing_measurements.py)
cover differing sample times, agreement on a wrong restored deadline, the
expired sentinel, inconsistent timer values and missing held-weapon state.
They also check warning-state representation and reject a checkpoint overwrite
before either revert while recording an autosave after both completed.
Final self-review covers generator isolation, source contracts, sentinel and
warning semantics, evidence completeness, original-data hashes, cleanup and
the full-system ledger. No delegated review or remote CI is performed.

The final Windows diagnostic build succeeded. After review and the reducer
corrections, **166 focused tests passed, with no failures or skips**: the
16 observer tests plus existing checkpoint recovery, epoch, timing, roster and
MCC checkpoint suites. All three archives were reduced again; source and
generated copies, executable, original map/profile hashes, ZIP integrity and
every archived member hash were checked. Both older modes still reproduce
their seven retained generated copies exactly; the timing mode also matches
the copies used to build these runs. Local document links, named source routes,
Python syntax and Git scope were checked. No production change calls for a
new Android/Web build; this diagnostic Windows build does not qualify those
platforms. The source explanation remains a diagnosis, not a tested fix.

Cleanup removed 6,827,893,202 bytes of private map/resource/cache/executable
copies and 247,104,356 bytes of temporary build/test outputs. The 278 private
save and trace files, 238,288,038 bytes raw, are preserved in three verified
lossless archives totalling 27,404,564 bytes. Adjacent indexes retain original
paths and hashes. Required binaries/symbols, generated copies, manifests and
useful logs remain: about 94.4 MB for this pass and 2.10 GB across all M0 work.
Original maps, all original saves and installed toolchains are untouched.
Normal release/portable, PGO-off/LTO-off configuration is restored; no owned
game process remains. The root checkout's 20 ongoing changes are preserved.
Nothing was pushed or published, and no M1 or production fix was started.
