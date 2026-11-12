# Loading, MCC hosting, automatic sharing and Steam Workshop links

Investigated and implemented 12 November 2026. The investigation below records
the original evidence and proposal; the implementation record at the end
supersedes its proposed behavior and validation status.

## Revisions and isolation

- Dedicated branch: `mcc-hotfix-1.1` (renamed after implementation was authorized).
- Base: freshly fetched GulchCE `testing`, `ffe19d7b98fef0de76a50f3c7fd5f71dce66662b`.
- Latest published release: build-665, `80c31f61ee39f46d66477f91edaa72b6e22d03e9`.
- Previous release: build-638, `624eccc41323d6961f77826ba89f55d16d582273`.
- The original checkout and its ongoing changes were left untouched.
- Loading is a shared renderer contract defect. Dedicated hosting requires
  MCC integration, and automatic sharing requires changes in MCC-owned
  policy/planning/validation. Preserve the separate Xbox and CE paths.
- Workshop client links are an MCC prompt enhancement. Keep URI construction
  in MCC-owned code and use the existing platform URL interface. Any web
  integration needs a narrow, separately validated dispatch; it must not
  broaden unrelated URL permissions.

## 1. Missing loading presentation: reproduced

Downloaded both published Windows release archives and verified each against
its release's SHA256SUMS. Ran them sequentially with hidden 1280x720 windows,
isolated copied maps and saves, identical loading.tga, and the same local
MCC a10 co-op host setup. Build-638 displays the animated loading picture;
build-665 displays black during conversion. Both reached the first logged
in-game tick. This was not a complete campaign or multiplayer test.

The responsible code is `texture_entry_result` in
[xbox_textures.c](../../port/linux/src/xbox_textures.c). It clears
`description->linear` for every uncompressed linear texture, without checking
CE ownership. The loading renderer uses uncompressed linear textures for its
320x240 artwork and 128x16 mask, with coordinates measured in texels.
`bind_textures` in [d3d8_gl.c](../../port/linux/src/d3d8_gl.c) divides those
coordinates by the texture dimensions only when `linear` is true.

A compiled probe using the actual function from each revision confirms:

| Input | build-638 | testing |
| --- | --- | --- |
| Artwork: linear, width 320, coordinate 160 | linear retained; normalized coordinate 0.5 | linear cleared; coordinate 160 |
| Mask: linear, width 128, coordinate 64 | linear retained; normalized coordinate 0.5 | linear cleared; coordinate 64 |

The loading draws therefore sample outside the intended texture area. The
problem is shared presentation, independent of which map family requested
it. The live comparison above covered MCC; separate Xbox/CE live comparisons
were not performed.

History locates the blanket override in `4b5c9de2b` (custom UI work), brought
into GulchCE by the upstream integration. Upstream revert `80bbe25e3` removes
it, but the resulting GulchCE hotfix merge `8fb8839af` still contains it.
The current testing and build-665 trees retain the defective block.

Recommended fix: restore texel-coordinate semantics for ordinary linear
textures in their responsible texture implementation. Preserve the separate
compressed CE non-power-of-two handling in `xgpu_texture_describe`; do not
revert all texture changes or add map/artwork-name exceptions. If CE artwork
requires normalized coordinates, establish its actual ownership/format rule
before introducing a bounded CE-specific adjustment.

The existing loading harnesses exercise drawing and controller lifetime but
stub the texture lookup, so passing them does not detect this defect. Add
coverage through the real texture-return path and inspect live loading frames
for Xbox, CE and MCC, both framing settings, and relevant transitions.

## 2. MCC dedicated hosting: reproduced before loading

Build-665 run with `--server`, an installed `mcc_maps/a10.map`, and rotation
`mcc_maps\a10:coop` exits with code 2. Its log reports:

```text
rotation entry 1 (mcc_maps\a10:coop) left out: there is no map file maps/a10.map for mcc_maps\a10
no usable maps ... for the rotation (server.rotation); exiting
```

[server_rotation.c](../../port/linux/src/server_rotation.c) parses an MCC
namespace as `_server_map_path`. `server_rotation_resolve` resolves that kind
against the Xbox `maps` folder using the final path component.
`server_rotation_check_kind` expects version 5 for it, while MCC uses version
13. `server_maps_each` enumerates only Xbox and CE folders. This is missing
MCC dedicated-server integration, not evidence that the MCC loader itself
cannot run headlessly.

A second independent gap remains after resolution: `server_apply_entry` in
[server_director.c](../../port/linux/game/server_director.c) calls
`ui_widget_port_cooperative_level_choose`, which admits Xbox/CE campaigns.
The existing MCC selector is `ui_widget_port_mcc_cooperative_level_choose`
in [ui_widget.c](../../source/interface/ui_widget.c). The automated listen-host
path already selects it in [network_test.c](../../port/linux/game/network_test.c).

Recommended design: an MCC-owned dedicated-server adapter for explicit MCC
names, discovery/header validation and catalog enumeration, with minimal
registration/dispatch in server rotation and the director. Preserve Xbox/CE
lookup priority and checks. Reuse MCC canonical package/Workshop resolution
and the existing MCC co-op selector. Do not copy maps into `maps` or broaden
the Xbox header check to accept MCC files. Validate initial rotation, sv_map,
map voting/listing, campaign and multiplayer setup, invalid packages, missing
resources, and mixed-family rotations with a real client.

## 3. Automatic sharing: feasible, beyond a menu change

The requested policy, including the user's clarification during this
investigation:

- One host `network.map_share` setting applies to CE and MCC.
- Workshop packages are automatically eligible, including packages that need
  shared base resources. No per-package digest list or manual configuration.
- Actual base resources are never transferred. A joiner uses its locally
  installed matching base resources; report missing or mismatched dependencies.
- Keep custom package resources, required metadata and supported previews
  eligible. Preserve the existing client download setting and consent flow;
  the request concerns automatic host preparation.

Current obstacles and implementation boundaries:

1. [port_config.c](../../port/linux/src/port_config.c) has separate
   `network.map_share` (true) and `network.mcc_map_share` (false) settings.
   [port_settings.py](../../tools/port_settings.py) generates the separate
   Share MCC Packages control. Make the existing host setting authoritative
   across both independent transfer paths and remove the separate menu option;
   handle obsolete saved config deliberately so it cannot silently block the
   new unified setting.
2. `mcc_share_admit` in
   [mcc_share_session.c](../../port/linux/game/mcc_share_session.c) requires
   an exact digest and maker entry from `mcc_share.txt`. Remove that manual
   gate for the requested automatic policy, retaining validation, bounded
   names and lifecycle checks. Preserve available attribution/Workshop
   metadata without inventing maker credit or requiring the host to enter it.
3. `mcc_share_package_build` in
   [mcc_share_package.c](../../port/linux/game/mcc_share_package.c) currently
   refuses the entire offer when a required resource resolves outside the
   package. Instead distinguish transferable custom files from external base
   dependencies: omit base files from the manifest and transfer while allowing
   the package. Base-resource exclusion must not be defeated merely by copying
   stock resources into a package folder; identify base copies against known
   resource content where needed, not just their directory names.
4. The existing resource reader can select local shared files during staged
   checking. However, `mcc_validation_publish` in
   [mcc_validation.inl](../../port/linux/game/mcc_validation.inl) explicitly
   refuses validation results that borrowed root/shared resources. It needs
   separate handling for relocated downloaded files and retained local base
   dependencies, preserving stamp checks and failed-load cleanup.
5. `mcc_resources_fingerprint` in
   [mcc_resources.c](../../port/linux/game/mcc_resources.c) includes the map
   and complete used resource contents. Keep that agreement: skipping base
   transfer must not skip identity checking. A matching local base can satisfy
   it; a missing/different base must not produce a joinable package. Consider
   dependency preflight so a player learns about missing base files before a
   large download rather than only at final verification.

Retain bounded chunks, upload limits, consent, content/metadata hashes,
contained staging, separate managed snapshots and cancellation. CE sharing
can keep its implementation unchanged. Do not route CE data through MCC.

This is a moderate MCC policy/validation integration, with reusable transport
already in place. It is not achievable by only deleting the second toggle or
replacing one configuration lookup. The investigation has not demonstrated a
new shared-resource download end to end. Existing file-size and supported
format bounds still apply to otherwise eligible Workshop packages.

The current shared network version is 43. Implementation must review the
agreement under AGENTS.md: new dependency records or changed peer admission
contracts require a coordinated version change. This investigation makes no
protocol change and does not assume a new wire format is necessary.

## 4. Opening Workshop pages in Steam: feasible with a fixed URI

The current Workshop button calls `mcc_share_game_prompt_action(1)` in
[mcc_share.c](../../port/linux/game/mcc_share.c). It constructs
`https://steamcommunity.com/sharedfiles/filedetails/?id=<ID>` locally and
calls `platform_open_url`. The host supplies an item ID and advisory title,
not a complete URL. `mcc_workshop_wire_unpack` in
[mcc_workshop.c](../../port/linux/game/mcc_workshop.c) requires a terminated
ID of 1 to 20 ASCII digits inside a bounded 64-byte record. The title never
enters the URL. The action rechecks the current session or detached manual
prompt state before opening anything; receiving an advertisement alone does
not open the page.

The requested `steam://openurl/` prefix could wrap that fixed HTTPS URL without
allowing the host to supply URI syntax. However, it is a general routing
command, and opening within Steam needs verification for the actual target.
A [first-hand report in Valve's tracker](https://github.com/ValveSoftware/steam-for-linux/issues/10107)
shows `openurl` handing an external HTTP site to the default browser on a
2023 Linux client. That does not establish what a current client does with
a Steam Community URL. Valve Developer Community's protocol page returned
403 during this investigation, so it was not used as verified documentation.

Recommended candidate: `steam://url/CommunityFilePage/<ID>`. The installed
Steam client's `C:\Steam\public\url_list.txt`, entry `CommunityFilePage`,
maps this named page to the shared-file details page with method `internal`.
The Windows `steam` protocol registration points to `C:\Steam\steam.exe`
and passes a quoted URI after `--`. These are local configuration observations,
not a live navigation test or a cross-platform guarantee.

Security assessment:

- Keep a fixed command/page template and the existing bounded digits-only
  decoder at the point of use. Do not accept a host-provided URL, scheme,
  Steam command, executable path or command-line arguments. Quotes, slashes,
  whitespace, percent escapes, query separators and non-ASCII bytes cannot
  enter through a valid ID. Both URI forms fit the existing 128-byte buffer.
- Keep explicit player activation and stale-prompt checks. Use
  `platform_open_url` directly; do not introduce a command shell or concatenate
  a `steam.exe` command line. Label the action as opening the Workshop page;
  opening it is not consent to subscribe, install or run other software.
- No host-controlled command-injection path was found in this flow under
  those constraints. This is not a claim that Steam or its registered handler
  is free of vulnerabilities. Opening the page delegates to an external
  application's URI parser and web renderer; flaws there remain outside
  this repository's assurance. No exploit against Steam was attempted.
- A malicious host can still name an unrelated Workshop item and invent its
  display title. The ID is a navigation hint, not authenticated provenance,
  an MCC-app membership check or proof that the Workshop download matches
  the host. Preserve package/content verification before joining.

Platform and implementation boundaries:

- Native `platform_open_url` in
  [sdl_platform.c](../../port/linux/src/sdl_platform.c) delegates to SDL.
  [SDL's documented contract](https://wiki.libsdl.org/SDL3/SDL_OpenURL)
  supports URI handlers but explicitly does not guarantee that a successful
  dispatch loaded the page. A browser fallback must remain accessible when
  Steam is absent or does not navigate; the wrapper currently returns void,
  so automatic failure detection is not available to the caller.
- Prefer the Steam route on supported desktop builds, with the fixed HTTPS
  page as an explicit fallback. Retain HTTPS on Android unless its handler
  behavior is separately qualified; do not assume the desktop Steam client
  exists on mobile.
- Web currently posts message kind 5 into `showLinkPage` in
  [app.js](../../port/web/site/app.js). That function permits only HTTP(S)
  paths containing `/link?`, for account linking, so it rejects the existing
  Workshop URL as well as a Steam URI. A web implementation needs an MCC
  Workshop dispatch with strict ID validation and a browser-side user click.
  Do not loosen the account-link filter into arbitrary protocol launching.
- Desktop URI selection can stay in MCC-owned modules using existing
  interfaces. Web needs minimal platform wiring because the existing bridge
  cannot express this action. No network version change is needed for a local
  navigation change using the unchanged Workshop ID record.

Before implementation is considered complete, test benign Workshop navigation
on current Windows/Linux Steam clients, Steam running/stopped/absent, invalid
or removed items, and the browser fallback. Check mobile/web behavior, prompt
cancellation, disconnect/map changes and repeated input. These runtime checks
have not been performed; no Steam page was opened during this investigation.

## Validation, evidence and limits

- Existing focused loading and sharing selection: **100 passed, 2 skipped**.
  Command: `python -m pytest -q tools/test_loading_widescreen.py
  tools/test_mcc_loading.py tools/test_mcc_share_rules.py
  tools/test_mcc_share_game.py` with the installed Windows Clang/MSVC toolchain.
  Skips are Linux-only sanitizer runs. Current policy tests confirm the old
  refusal behavior; they are not tests of the proposed policy.
- Item 4 follow-up: `python -m pytest -q tools/test_mcc_workshop.py
  tools/test_mcc_share_game.py`: **71 passed**. This exercises the current
  production decoder and game adapter, including Workshop URL selection and
  stale manual prompt rejection, with the external URL opener stubbed.
  An additional compiled production-decoder probe passed **5,163 checks**:
  every non-NUL byte at every position of a 20-byte ID, lengths 0 through 61,
  and an early NUL followed by hostile title text. The probe never launches
  a URL. These checks support the input-boundary assessment, not Steam-client
  security or the unimplemented navigation change.
- Published build comparison and direct production texture function probe,
  as described above; dedicated-server startup rejection reproduced.
- The POSIX server-rotation harness could not compile on this Windows setup
  because `dirent.h` is unavailable. The release executable reproduction
  independently confirms the resolution failure. No harness was weakened.
- Current testing CI reports successful Build and MCC validation runs for
  `ffe19d7b9`. Those runs did not catch the loading regression.
- No implementation patch, platform rebuild, internet transfer test or
  dedicated MCC gameplay session was completed in this investigation.
- Self-review covered the relevant callers, format boundaries, map/server
  lifecycles and the host/receiver sharing contract. No refactoring was needed
  for this investigation.
- The report's source routes and local links were reviewed. The only repository
  deliverable is this investigation document; no production code was changed.

Local evidence remains under this worktree's ignored `build/`: compact
loading contact sheets/comparison, process logs, texture-probe source/output,
focused-test output and reproducible investigation scripts. Disposable
captures, copied maps, cacheNNN.map files, downloaded release binaries and
compiled probe/test outputs are removed after inspection; generated saves
and useful logs are preserved.


## Implementation and review

The four changes are implemented on `mcc-hotfix-1.1`:

- Restore ordinary linear texture coordinate semantics in `texture_entry_result`.
  Compressed CE NPOT handling remains separate; no content-specific selector.
- Add `mcc_server.c` for MCC canonical paths, version-13 header validation and
  catalog enumeration. Minimal rotation/director dispatch is necessary because
  those shared entry points otherwise never call MCC resolution or its co-op
  selector. Xbox/CE resolution is preserved.
- Use the existing `network.map_share` setting for automatic validated package
  offers. Manual fingerprint lists now supply optional attribution only. Root
  and shared dependencies are omitted; package copies matching installed bases
  are omitted too. Joiner validation uses local dependencies and still requires
  the complete gameplay fingerprint and metadata identity. No arbitrary host
  URL, executable or additional file type was introduced.
- Add fixed numeric-ID Steam Workshop navigation and an explicit browser
  fallback that remains available after dispatch. Web uses its own narrow
  validated dialog, separate from the existing account-link bridge.

The combined review checked module boundaries, lifecycle, disabled-sharing
behavior, staging/publication, stale offers, path containment and URI inputs.
It removed an unreachable MCC-header branch, simplified the added parser
dispatch, preserved the legacy rotation harness with MCC stubs, corrected
obsolete policy tests and regenerated both menu families. Protocol 44 and
camera protocol 32745 deliberately exclude older peers because local-base
admission and automatic sharing change the agreement. Message layouts still
match the upstream 43 fixture. No third-party source was imported.

### Local evidence

- Windows release and web builds pass. Native rendering at 2560x1440 shows
  the restored MCC loading animation and an unclipped download prompt. Xbox
  Blood Gulch and CE Ice Fields reached gameplay; their brief loading interval
  was not retained in the sampled contact sheets.
- A hidden local dedicated server hosted MCC a10 co-op with a connected client
  beyond 1,500 simulation ticks. A second session hosted Workshop Lava Lab
  (`2898867177`) and switched to Xbox Blood Gulch with the client connected.
- A clean client with Workshop discovery disabled downloaded the 156.2 MiB
  Lava Lab package from the host, accepted it without a manual approval file,
  installed it and reached 480 gameplay ticks. HTTP admin login/status passed.
  This tests a self-contained Workshop map; shared-base success/failure and
  copied-base exclusion are covered by file-backed adapter/validation tests,
  not a live shared-base Workshop session.
- Focused suite: 419 passed, 5 skipped. Web JavaScript: 92 passed. The broader
  suite initially had 578 passes, 9 skips, 70 failures and 20 errors on Windows:
  49 legacy rotation cases require POSIX dirent.h; three junction tests and
  socket-based server simulator tests hit sandbox restrictions. The unchanged
  server-page byte-size test also fails with this Windows checkout's CRLF.
- Android is unavailable locally because no NDK is installed. Browser/native
  transport commands were attempted but cannot resolve Playwright locally.
  CI results are recorded separately when available.
- After the combined review, 258 focused tests pass with 4 skips. An
  unrestricted staging/server-simulator rerun has 76 passes; the unchanged
  page-size check fails on CRLF and the simulator PID check observes the
  Windows virtual-environment launcher rather than its Python child.
- URL construction and all 5,100 non-NUL byte/position combinations in a
  20-digit Workshop ID are checked. Arbitrary schemes, paths, switches and
  escaping cannot enter the constructed URI. The fixed URI was exercised from the actual game prompt and the Steam
  client displayed Lava Lab item 2898867177. This is not a guarantee about
  Steam's internals.

Useful runtime evidence is under the ignored `build/local-mcc-server`,
`build/local-mcc-transfer` and `build/capture-hotfix-*` directories. Test data
uses copied maps, isolated saves and loopback-only services, with at most two
game instances. Original installations and the original checkout are untouched.


The live manual-installation prompt exposed stale focus on the hidden download
button. The final follow-up uses the existing `focus_row` helper to update
both the focused widget and list index. An executed prompt test covers desktop,
Android and web visibility changes and preserves a selected browser fallback.
The new MCC server test fixture also needs an include guard under Linux's
strict C89 warnings; the initial Linux MCC run passed 2,510 other cases.

The corrected prompt opened Lava Lab in Steam and then the same fixed HTTPS
page in the browser from its fallback action. Both were observed in their
actual application windows. The affected final rerun passed 104 tests, and
Windows/web rebuilds passed. No subscription or account changes were made.
