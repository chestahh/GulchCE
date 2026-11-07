# Windows Workshop discovery diagnostics

This diagnostic uses the existing MCC/Workshop discovery and map-admission code.
`debug.workshop_discovery` adds desktop traces to standard error and defaults off.
It does not enable Workshop discovery, change the Steam path, loosen metadata or
map validation, or upload a report. Xbox and CE discovery are unchanged.

## Running the player package

1. Extract the diagnostic executable and `Run-Workshop-Diagnostics.cmd` into the
   existing GulchCE folder beside `halo.exe`, `SDL3.dll` and `config.toml`.
   The diagnostic executable has a different name; keep the normal `halo.exe`.
2. Close the normal game and run `Run-Workshop-Diagnostics.cmd`.
3. Open the same MCC map list that was empty, reproduce the problem, then exit.
4. The launcher prints a report folder under Windows' temporary directory.
   Send only `workshop-diagnostics.log` and `debug.txt` from it. Do not send
   `config.toml` or the saves folder. Logs contain local paths and Workshop IDs;
   inspect them before sharing.

The launcher copies the active configuration (`HALO_CONFIG_FILE`, when set,
otherwise the configuration beside the executable) into a new report folder.
It uses separate saves and logs there. Steam/discovery settings and their existing
environment overrides are retained; diagnostics are enabled only for this run.
Updates and automatic crash uploads are disabled for this run. The original
configuration, saves, maps and normal executable are not modified by the launcher.
No administrator account or security-setting change is required by the launcher.

The application is the testing-branch build with diagnostic logging, not an
unmodified build 512. Build 512's map-discovery implementation is the same;
testing additionally includes the repaired resource discovery for maps that load.
The player's empty-list failure remains undiagnosed until their report is read.

## Reading the report

Follow `discovery_log` in `port/linux/src/steam_workshop.c`, the registry status
lines in `port/windows/src/win32_steam_workshop.c`, and `mcc_map_discovery_log`
in `port/linux/game/mcc_maps.c`. The ordered stages are:

- copied configuration and inherited discovery environment overrides;
- effective Workshop/campaign/collection settings and configured Steam root;
- registry results, root candidates and paths read from library manifests;
- directory access and item/map metadata acceptance or rejection;
- discovered `.map` files, followed by MCC header admission and final map count.

The distinction matters: a discovered file can still fail MCC header admission,
and a valid catalog can still be filtered by the particular menu. Missing files
and inaccessible metadata share a rejection description where the existing
case-insensitive lookup API does not distinguish them. File-open and directory
errors include errno; Windows registry failures include the native status code.
No resource or parser checks are bypassed to make the diagnostic succeed.

The focused tests compare diagnostic-on and diagnostic-off catalog output, and
exercise valid metadata, invalid metadata, another Halo engine, missing map JSON,
missing map files/directories, unsupported map headers and truncated files.
Synthetic test maps contain no retail assets.

## Validation record

The combined change was reviewed for ownership, disabled behavior, error-state
preservation and unchanged admission decisions. Logging stays in the Workshop
and MCC modules; the only shared registration is the default-off debug setting.
There are no gameplay, wire-format or simulation changes.

On Windows, the portable release build succeeded. Discovery, MCC catalog,
resources, protocol and web build-graph checks passed: 243 tests, with nine
platform-specific skips. A hidden real-engine launcher run produced registry,
discovery and admission traces plus both report logs, exited normally and left
the source configuration byte-for-byte unchanged. Its separate saves were
archived and verified; copied map fixtures and generated caches were removed.
This validates the diagnostic mechanism, not the remote player's configuration
or the cause of their missing maps. Cross-platform CI results are recorded with
the diagnostic delivery.
