# Dynamic lighting Milestone 4: compatibility and multiplayer

This pass covers representative map families, joining, respawns, inventory
changes and different local lighting settings. It does not reopen the sustained
performance investigation. Production code is unchanged from `98897e66`:
optional lighting remains off by default, host shield policy remains authoritative,
and Xbox, Custom Edition and MCC retain their existing paths. AMD/NVIDIA renderer
fixes and the network protocol are unchanged.

## Method and multiplayer results

Four sequential Windows loopback sessions used two hidden, rendered 1280x720
windows at 60 FPS. Each peer had its own executable, config, data and saves.
All lighting preferences, including animated overshields, were on for one peer
and off for the other. A passive diagnostic build sampled the received host
policy, effective Shield Glow activation and local preferences once per second.
It did not modify gameplay, networking or light selection.

| Map/case | Host/client lighting | Host shield policy | Two-player snapshots, host/client | Observed respawns per player, on both peers |
| --- | --- | --- | --- | --- |
| Xbox Blood Gulch | On / off | Glow + overshield | 92 / 88 | Player 0: 2 |
| CE Death Island | Off / on | Stock | 91 / 87 | Player 0: 4 |
| MCC Digsite Albatross (`DigsiteMP/1`) | On / off | Players choose | 76 / 72 | Players 0 and 1: 2 each |
| Xbox Blood Gulch, late join | Off / on | Players choose | 73 / 69 | Player 0: 1; player 1: 4 |

All four accepted sessions exited normally, showed changing weapon inventories
in both logs, and met their lifecycle and policy checks. Policy samples after
tick 600 all matched the host's decision: 140 samples for Xbox, 138 for CE,
108 for MCC and 140 for late join. Forced-on and forced-stock policies overrode
the opposite client preference; Players Choose retained opposite local choices.
These are actual receiver/activation observations, not only launch settings.

The existing [network comparator](../tools/net_compare.py) found no persistent
divergence in the three normal-join sessions, with 81, 80 and 65 paired seconds
respectively. Xbox and CE had no compared movement ticks outside rounding error
(2,223 and 1,584 samples). MCC had 20 of 1,756 movement comparisons apart and
two snapshot mismatches each for position, alive state, kills and deaths around
respawn events. Its largest transient position difference was 24.74 world units;
these recovered within the existing tolerances. This is not bit-identical
frame-by-frame simulation, and tolerances were not relaxed to obtain a pass.

The late client started 32 seconds after the host. The host logged 19 solo
snapshots before the second player appeared at tick 600. Host-measured synchronous
movement drift was within rounding error for all 1,633 compared ticks. The two
once-per-second log streams have different phases, outside the comparator's
two-tick pairing window, so there are **no paired snapshot seconds** for this
case. Its pass covers joining, lifecycle, received policy and host drift;
it does not establish paired inventory/score equivalence at the same tick.

MCC's map script validator refused the map's `host_spawn` script at
`debug_camera_load`. That existing refusal is retained, not bypassed to make the
map run. The result is bounded to the supported gameplay paths exercised here.

Both single-process transition runs passed: Xbox c40 -> MCC Mercury Falling ->
CE Death Island -> Xbox Blood Gulch, with all lighting off and then on. Each
logged the MCC load, CE conversion and three clock resets, retained the expected
shield activation, and exited normally without exceptions. Seven captures per
run were retained; the contact sheets and multiplayer captures were inspected.
Startup/loading frames are black; the retained scene frames show rendering
continuing across the map families. These short visits do not establish campaign
completion or full visual acceptance of each map.

## Tool corrections and reproducibility

- [lighting_compatibility_build.py](../tools/lighting_compatibility_build.py)
  builds a passive policy probe and restores the original source bytes,
  configuration and normal Windows build even after failure.
- [lighting_compatibility_run.py](../tools/lighting_compatibility_run.py)
  stages the two peers before starting either process and requires observed
  deaths-to-respawns, changing inventories, actual policy and normal exits.
  HSC object-list ordering differs from network player numbering; both peers
  must observe the same network player respawn, whichever player the cue targets.
- [lighting_compatibility_transitions.py](../tools/lighting_compatibility_transitions.py)
  exercises Xbox c40, MCC Mercury Falling, CE Death Island and Xbox Blood Gulch
  in one process, with separate all-off and all-on runs.
- The network log judge now imports on Windows without POSIX `SIGHUP`.
  POSIX launch/process-group tests remain enabled on Linux; five unsupported
  cases skip on Windows. This does not port the POSIX launcher to Windows.

Example commands, from a configured Windows checkout with local game data:

```powershell
python tools/lighting_compatibility_build.py
python tools/lighting_compatibility_run.py --data-root C:/local/game --case xbox
python tools/lighting_compatibility_run.py --data-root C:/local/game --case ce
python tools/lighting_compatibility_run.py --data-root C:/local/game --case mcc
python tools/lighting_compatibility_run.py --data-root C:/local/game --case late
python tools/lighting_compatibility_transitions.py --data-root C:/local/game --profile-root C:/local/test-profile/saves --enabled 0
python tools/lighting_compatibility_transitions.py --data-root C:/local/game --profile-root C:/local/test-profile/saves --enabled 1
```

Run these sequentially, with Windows loopback access. Runners refuse existing
evidence folders. Retail maps, resources and captures stay local. Original data
and profiles are copied, never modified. Save archives include original relative
paths and verified SHA-256 hashes; copied maps/caches and binaries are removed.

Pilot runs are excluded from accepted results. They exposed a misparsed config
log line, ineffective scripted damage on Xbox, malformed HSC command wrapping,
and a sandbox connection failure. The first late-join run also crashed when an
unguarded HSC `unit_kill` was given `NONE` after another death command removed
the unit. The corrected cue uses `unit_get_health`, whose existing implementation
safely returns -1 for a missing object, before calling the live-unit-only command.
The crash log is retained; no production damage or unit implementation was changed.

The retained runtime probe originally also printed an unused end-of-frame light
count. That sampling point does not establish active lighting draws; the column
was not an acceptance criterion and was removed during final review. Policy
sampling is unchanged. These sessions establish compatibility with the settings,
not new visual acceptance of every emitter or proof that every optional light
was visible in every sampled frame.

## Platform coverage and limits

The unchanged production revision passed refreshed
[Windows, Linux, Android and web CI](https://github.com/chestahh/GulchCE/actions/runs/40261321949),
including the workflow's Chromium/Firefox and native WebRTC checks. These are
debug CI builds; release jobs are skipped on this branch. Local diagnostic and
restored normal Windows release builds also succeeded.

Local rendered coverage uses Windows/NVIDIA. This is not an AMD hardware run,
full campaign completion, every custom-map variant, or a mixed-platform retail
internet game. Browser transport CI and synthetic MCC networking checks support
the result but do not replace those unperformed scenarios. The outstanding
[performance and memory limits](dynamic_lighting_stability.md) are unchanged.

The final local regression selection passed **170 tests**, with five Windows
skips for POSIX launcher/process-group cases. It covered projectile/emitter,
muzzle, shield and occlusion modules, fragment-source sharing, GPU visibility
contracts, MCC networking/UI and the compatibility report's acceptance rules.
The combined review checked probe ownership, source restoration, config
isolation, observed lifecycle coverage, absent paired-snapshot coverage and
cleanup. It removed the unused light-count diagnostic and added a guard against
counting zero paired snapshots as a normal-join comparison pass. Final diagnostic
and restored normal release builds succeeded after these changes; production
source hashes match and the normal binary contains no policy probe.

Local evidence is under `build/lighting-compatibility/`, with accepted sessions
`xbox-final`, `ce-verified`, `mcc` and `late-guarded`. Failed pilots remain clearly
separate. Logs, reports, manifests, captures and archived saves are retained;
no retail data is committed.

Cleanup verified 664 archived save files (377,805,438 uncompressed bytes) in
18 archives. All staged maps/caches and per-run binaries were removed. Final
cleanup removed another 21,055,200 bytes of diagnostic executable/object and
temporary driver scripts. About 56.7 MiB of evidence remains in 254 files,
including failed-run diagnostics. Original maps/saves, toolchains and the normal
Windows build were preserved. `build/milestone4-cleanup.json` records the totals.
