# Controller detection and button styles

GulchCE adds AUTO BUTTON DETECTION to Graphics, off by default. With High-Res HUD
on, the manual Button Style row offers all eleven styles. Turning detection on
restricts that row to the controller assigned to the player editing the menu;
the caption identifies P1 through P4. Devices are identified even while the
option is off. This feature does not add controller remapping or change input.

For example, a DualSense assigned to P2 exposes PS Classic, PS Gems, PS Dark Gems
and PS Modern. Choosing PS Modern saves the PlayStation preference. Turning
automatic detection off restores the independently saved manual choice. A
simultaneous Xbox player uses the Xbox preference in their own gameplay viewport.
Players with the same family share that family's style preference.

## Reading route and boundaries

1. [controller_buttons.c](../../port/linux/src/controller_buttons.c),
   `controller_device_read`, `controller_device_update` and
   `controller_button_style_for_port`: SDL identity snapshots and style policy.
2. [xinput_sdl.c](../../port/linux/src/xinput_sdl.c), `connected_gamepads` and
   `XInputGetState`: minimal hooks publish identity using the existing port
   assignment. A feature module alone cannot know when that assignment changes.
3. [port_config.c](../../port/linux/src/port_config.c) and
   [port_settings.py](../../tools/port_settings.py): saved defaults, environment
   overrides and generated menu rows. Existing saved Xbox style names are kept.
4. [controller_button_menu.inl](../../port/linux/game/controller_button_menu.inl),
   `controller_button_rows_show`: pending selections, family filtering, focus and
   spacing. The existing menu callback wires this in; Save, Cancel and Defaults
   continue through the normal settings machinery. Hidden family selections do
   not replace the manual choice.
5. [controller_glyphs.c](../../port/linux/game/controller_glyphs.c),
   `controller_glyph_style`, `controller_glyph_draw`, `controller_glyphs_dispose`:
   viewport-to-controller resolution, optional artwork and resource lifetime.
6. [hud_hires_tags.c](../../port/linux/game/hud_hires_tags.c), `hud_hires_button_draw`,
   and [mcc_hud_draw.c](../../port/linux/game/mcc_hud_draw.c), `mcc_hud_icon_draw`:
   each established renderer identifies its own controller entries before calling
   the feature. These hooks are necessary because only those renderers have the
   map's icon identity, cursor and text context. Xbox/CE loading paths and MCC
   ownership guards remain independent; weapon/pickup art is not replaced.

## Identity and lifecycle

SDL identifies Xbox 360/One-family devices, PS3/PS4/PS5 and Switch Pro/Joy-Con
variants. Xbox Series and third-party Xbox-compatible pads depend on the type
reported by SDL and their driver. USB/Bluetooth names, instance IDs and vendor /
product IDs are retained on native desktop and web builds when SDL exposes them.
The Android guest currently exposes the SDL type only: its displayed device name
is a family description and the other identity fields are zero.

Detection describes the device visible to SDL. Steam Input or another controller
wrapper can present a PlayStation pad as an Xbox pad; the game cannot recover the
hidden physical identity. An unknown or disconnected controller falls back to the
manual selection instead of guessing. Input polling refreshes snapshots and clears
disconnected ports, including existing keyboard/gamepad split-screen assignments.

Automatic detection and each family preference are global configuration settings.
The active menu uses its player port; gameplay uses the viewport's player-to-port
assignment. Changing families does not overwrite other saved preferences. Disabling
High-Res HUD preserves the established low-resolution paths and hides both rows.

New artwork is cached by asset, not by current player, so alternating viewports do
not evict each other's glyphs. Each allocation retains its originating renderer's
optional unbind callback. The existing high-res map teardown releases all feature
textures before map tags disappear, including MCC resources. Repeated cleanup is
safe; a failed allocation falls back to the established glyph for that map.
The existing Xbox styles keep their geometry. Xbox Classic's letter transparency
is corrected as described in [HUD artwork corrections](hud_corrections.md).

## Artwork, geometry and checks

See [asset provenance and generation](../../port/assets/controller_buttons/README.md).
The new renderer fits visible artwork within the existing face/control cells,
preserves aspect ratio, centers on the text's capital midpoint, retains cursor
advances and applies the existing 0.75 split-screen scale. Switch uses printed
B/A/Y/X at physical south/east/west/north; this is a label choice, not remapping.

[Controller tests](../../tools/test_controller_buttons.py) exercise production
identity and policy code with SDL fakes, pending menu behavior, all generated art,
and production draw geometry, allocation and cleanup. Existing Xbox/CE/MCC HUD
regressions remain relevant. These tests establish software behavior; they do not
prove that every physical controller/driver reports the expected SDL type.
Hardware checks should cover USB/Bluetooth, hotplug, mixed-family split screen,
Save/Cancel/Defaults and Xbox/CE/MCC map transitions. Those suggested checks are
not a claim that every device and transition has been tested.

This changes local presentation only: no network packet, input mapping or shared
simulation contract changes, so the network version remains unchanged.
