# GulchCE - an OpenCE fork focusing on bringing experimental features to OpenCE

This project is a fork of [OpenCommunityEdition/OpenCE](https://github.com/OpenCommunityEdition/OpenCE) - the Halo: Combat Evolved decompilation project targeting Linux, 
Windows and Android.

The decompilation is of the Xbox build 2342. (`cachebeta.exe`, SHA-256
`4cc87b45f721270392a96f1674ed2b5cd4a7bb4355faeab4531d1cf1884d9520`).

## Download

There are no release or debug builds available just yet.

## Game data

The port does not include the game data. You would need an Xbox disc image
(`.xiso` or `.iso`) of Halo: Combat Evolved. All versions of the game
operate. The maps of the European (PAL) version were made for a slower
console. The port changes them to play as the North American (NTSC) maps do,
so players of the two versions can play together.

Halo 1 MCC custom maps use the separate `mcc_maps` directory and the
**MCC SINGLEPLAYER** and **MCC MULTIPLAYER** menu options. Maps appear in
the appropriate category automatically. This experimental adapter targets
modern version-13 maps.
See [MCC map support](docs/mcc_maps.md) for installation, supported formats,
validation results and remaining compatibility limits.

Halo is a trademark of Microsoft. GulchCE is a fan project, not made or
endorsed by Microsoft, Bungie or 343 Industries, and includes none of the
game's content.
