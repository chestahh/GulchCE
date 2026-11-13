# GulchCE

A fork of [OpenCE](https://github.com/OpenCommunityEdition/OpenCE), bringing experimental features and Halo 1 MCC map support to **Windows, Linux, Android and web browsers**.

[Download](https://github.com/chestahh/GulchCE/releases) · [Discord](https://discord.gg/pRFGPAMZkD) · [YouTube](https://www.youtube.com/channel/UC9Hn6j6BdKwoyu_hGXdfzvw)

GulchCE runs the game's code natively on each platform. It is based on the Halo: Combat Evolved Xbox decompilation and includes online co-op, multiplayer, Custom Edition maps, bots, configurable controls and graphics, and several menu styles.

**No game data is included. You need your own copy of Halo: Combat Evolved for the original Xbox to get started.**

## Getting started

1. Download a **release build** for your platform. Debug builds are intended for troubleshooting and stop when an assertion fails.
2. Start `halo.exe` on Windows, `halo` on Linux, or the Android app. Browser players use the site's import prompt.
3. Select your Xbox Halo disc image (`.iso` or `.xiso`) when prompted. GulchCE extracts the Xbox maps and starts the game. On Windows and Linux, you can also place the image beside the executable.

Xbox data goes in `maps`. Custom Edition and MCC content use their own folders:

| Content | Folder in the game data directory |
| --- | --- |
| Original Xbox maps | `maps` |
| Halo Custom Edition maps | `custom_maps` |
| MCC maps and packages | `mcc_maps` |

Keep these folders separate. Halo PC or MCC data does not replace the Xbox data needed to start GulchCE.

For installation, controls and platform requirements, see the [Windows](port/windows/README.md), [Linux](port/linux/README.md), [Android](port/android/README.md) or [browser](port/web/README.md) guide. Settings are available in the menus and `config.toml`.

## MCC maps and campaigns

GulchCE supports Halo 1 MCC maps, including Workshop multiplayer maps and campaign packages. Compatibility is experimental; some maps or scripted features may not work correctly.

### Windows and Linux

1. Install Halo: Combat Evolved through your copy of MCC on Steam.
2. Subscribe to the Halo 1 Workshop maps or campaigns you want and let Steam finish downloading them.
3. Start GulchCE and open the MCC map or campaign selector.

**Steam Discovery and Campaign Missions are enabled by default.** Existing configurations retain your previous choices. Find these settings under **Settings > Online > Map Sharing > MCC Packages** in Modern menus, or **Settings > Network Setup > Map Sharing > MCC Packages** in the other menu styles.

GulchCE reads installed Workshop packages directly. When a map needs MCC's shared base resources, it can also read them directly from your installed MCC copy. **You do not normally need to extract or copy those resources yourself.** Package-supplied resources take priority, and Steam does not need to stay open while you play.

If a newly downloaded package is missing, reopen the map list or restart GulchCE. If your Steam installation is not detected, set its installation or library folder in the existing `[paths]` section of `config.toml`:

```toml
[paths]
steam = 'C:\Steam'
```

### Android and browser

These platforms use imported files rather than discovering a Steam installation.

- **Android:** copy a complete Workshop package folder or ZIP from your PC. Quit GulchCE, long-press its launcher icon, choose **Import MCC maps**, then select **Workshop package**. Import required base resources using **Singleplayer shared resources** or **Multiplayer shared resources**. See the [Android import guide](port/android/README.md#importing-mcc-packages).
- **Browser:** import your Xbox data first, then use **Custom maps** to import CE files or an MCC package folder. See the [browser import guide](port/web/README.md#custom-edition-and-mcc-imports) for supported layouts and storage limits.

Workshop packages are stored on your PC under `steamapps/workshop/content/976730/<workshop ID>`. Keep their maps, package resources, JSON metadata and artwork together.

### Local installation

Copy the complete downloaded Workshop folder from `steamapps/workshop/content/976730/<workshop ID>` into `mcc_maps`. Keep its contents unchanged: GulchCE reads the nested maps, metadata and previews, showing the authored mod and level names instead of the numeric folder name. Steam Discovery is not required for local packages.

The manual method still works: create `mcc_maps/MyCampaign` and place `.map` files directly inside it, with any supplied `bitmaps.map` and `sounds.map`. Metadata and previews are optional; without them, the menu uses folder/map names and its placeholder picture. Do not mix resources from different packages.

If Steam Discovery is unavailable or disabled, required base resources can be copied from your MCC installation into these locations:

| MCC installation file | GulchCE destination |
| --- | --- |
| `halo1/maps/bitmaps.map` | `mcc_maps/_shared/singleplayer/bitmaps.map` |
| `halo1/maps/custom_edition/bitmaps.map` | `mcc_maps/_shared/multiplayer/bitmaps.map` |
| `halo1/maps/custom_edition/sounds.map` | `mcc_maps/_shared/multiplayer/sounds.map` |

These are MCC's resources, including its own `custom_edition` directory. Self-contained packages do not need shared resources. See the [package layout guide](docs/mcc_maps.md#installation-and-menus) for manual layouts and supported metadata.

### Selecting a map

- **Modern, Stock and Classic PC:** choose **MCC Singleplayer** or **MCC Multiplayer**, open the package and select a level.
- **Xbox menus:** use **MCC Campaigns** on the main menu for campaigns, or **X — MCC Maps** in the multiplayer map selector.

Campaign Missions uses the package's metadata for mission order and progression. Supported titles and previews appear automatically.

## Custom Edition maps

Copy your Custom Edition maps into `custom_maps`, together with `bitmaps.map`, `sounds.map` and `loc.map` from your Custom Edition installation. Select them under **Custom Singleplayer** or **Custom Multiplayer**.

The Gearbox multiplayer maps, such as Ice Fields and Death Island, can also be copied from your Custom Edition installation. They are not bundled with GulchCE. Maps that require OpenSauce are unsupported.

See the [Custom Edition guide](docs/custom_edition_caches.md) for format support and limitations.

## Playing online

- Join through the server browser or an invite link, or host a public or private game.
- Play campaign co-op or multiplayer across Windows, Linux, Android and browser builds with compatible network versions.
- Multiplayer supports up to 128 players, with up to four local players per device in split screen.
- Voice chat, host moderation, bots and additional gametypes are available in the game settings.

**Use matching GulchCE builds when playing together.** This development branch changes the network agreement and cannot join older OpenCE or GulchCE builds. **GulchCE Standard** disables the optional multiplayer shoulder camera; **GulchCE Third Person** uses a separate camera agreement and requires matching clients.

Older cores and checkpoints from before the saved-state layout change are incompatible. Start a new map or campaign session; the game does not migrate those saves. See the [checkpoint guide](docs/reading/console_checkpoints.md).

### Automatic map sharing

The host's existing **Map Sharing** setting covers both Custom Edition maps and MCC packages. It is enabled by default. There is no separate **Share MCC Packages** option or manual package approval list to configure.

- **Custom Edition:** missing maps and required CE resource files can download from the game's players.
- **MCC:** the joining player is offered a download from the host. Workshop packages can also offer a link to their Steam page, with a browser fallback on desktop.
- **MCC base resources stay local:** packages may depend on them, but those shared base files are not supplied by the host. Each player needs matching local resources, discovered from MCC on desktop or imported on Android/browser.

Downloaded MCC packages are validated before use, and existing installations are kept. **Download Maps** controls whether a joining player accepts map downloads; MCC asks before starting the transfer.

### Dedicated servers

Run `halo --server` (`halo.exe --server` on Windows). Dedicated servers support Xbox, Custom Edition and MCC maps, including MCC campaign co-op. Use `sv_maps` to list available maps and their paths.

See the [dedicated server guide](docs/dedicated_server.md) for setup, rotations, administration and deployment.

## Customising the game

Settings include display and frame-rate options, FOV, smooth motion, controller bindings, touch controls, menu styles and optional visual effects. The [settings reference](port/linux/README.md) lists their defaults and platform availability.

Loose-tag mods go in `mods` and can be enabled or reordered from the menu. They apply to Xbox and Custom Edition maps; MCC packages use their own authored resources. See the [mod guide](docs/mods.md).

## Building and contributing

Install Python, Ninja and the tools listed in your platform's guide, then run:

```sh
python configure.py --release
ninja windows
```

Use `ninja linux`, `ninja android_apk` or `ninja web` for the other platforms. Add `--portable` when configuring native builds intended for other computers. The Xbox SDK is not required.

Start with the [code reader's guide](docs/reading/README.md), [repository requirements](AGENTS.md) and [coding standards](CODINGSTANDARDS.md). Platform guides cover build options; the repository requirements list the regression checks.

Release downloads include `SHA256SUMS` for verification. Where signing is configured, verify `SHA256SUMS.sig` with `tools/release_sign.sh verify SHA256SUMS` using OpenSSL 3 and the [release key](deploy/release-key.pub).

## Credits and licence

GulchCE is maintained by Chester ([@chestahh](https://github.com/chestahh)) and its [contributors](https://github.com/chestahh/GulchCE/graphs/contributors). It builds on [OpenCE](https://github.com/OpenCommunityEdition/OpenCE), including the work of Jonas Volman (@bnunu), MrBruh (@cybersecurity) and the [OpenCE contributors](https://github.com/OpenCommunityEdition/OpenCE/graphs/contributors).

The decompilation comes from [bnunu/halo-1](https://github.com/bnunu/halo-1), a fork of [punpckhdq/halo](https://github.com/punpckhdq/halo), and targets Xbox build 2342 (`cachebeta.exe`). Artwork credits are in [AUTHORS.md](AUTHORS.md); bot and Android feature credits are in the [provenance guide](docs/reading/bots_and_android_features.md).

Current revisions are licensed under [AGPL-3.0-or-later](LICENSE), with no warranty. Earlier revisions and releases published under [CC0 1.0](LICENSES/CC0-1.0.txt) retain that licence. Contributions use AGPL-3.0-or-later; adapted GPL code retains its notices, including the [GPL-3.0-only](LICENSES/GPL-3.0-only.txt) bot tactics. Third-party code, fonts and artwork retain their own licences and accompanying notices.

GulchCE is an unofficial fan project, not affiliated with or endorsed by Microsoft, Bungie or Halo Studios. Halo is a trademark of Microsoft Corporation. The project's licence does not grant rights to Halo or its game data.
