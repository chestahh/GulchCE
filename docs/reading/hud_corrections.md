# High-Res HUD artwork corrections

With High-Res HUD enabled, the stock rocket reticle, sniper reticle and supplied
zoom labels use the owner's [original pictures](../../port/assets/hud/corrections/README.md).
Xbox Classic button letters are transparent cutouts. Tutorial thumbsticks use
the existing high-resolution controller path, including animated stick presses.
These are display changes; input bindings, simulation and networking are unchanged.

## Reading route

1. [hud_corrections.py](../../tools/hud_corrections.py) records the crop and atlas
   rectangles. [hud_assets.py](../../tools/hud_assets.py), `build_asset`, converts
   black-background luminance to premultiplied white coverage and proportionally
   resizes the supplied pixels. It does not reconstruct their shapes or font.
2. Xbox retains its existing texture replacement path and atlas layout.
   [hud_hires.c](../../port/linux/src/hud_hires.c), `hud_hires_override_find`, checks
   the setting and original pixel signature. CE's rocket uses its existing
   compatible stock-sheet registration.
3. CE's scope has a different layout and an 8x label. Its separate atlas contains
   only the supplied 2x and reticle. In
   [hud_hires_tags.c](../../port/linux/game/hud_hires_tags.c),
   `hud_hires_crosshair_bitmap` selects those sprites through the existing
   CRC-checked placeholder machinery. Sequence 1, frame 1 keeps the original 8x.
   The small [hud_weapon.c](../../source/interface/hud_weapon.c) hook is needed
   because a whole-texture override cannot preserve one frame of a sequence.
   It runs after the source bitmap is resident and changes no sprite bounds,
   placement, zoom values or tag contents. Other map families return their original
   bitmap. CE embedded/custom art still fails stock registration or pixel checks.
4. [mcc_texture_bridge.c](../../port/linux/src/mcc_texture_bridge.c),
   `mcc_hud_texture_asset`, recognizes the complete converted stock MCC sheets by
   dimensions, format, byte count and CRC, including the `_r` variants. It uses
   independently packed MCC atlases. No Xbox/CE loading or registration is used.
   `xgpu_mcc_texture_get` checks High-Res HUD at draw time; disabling it returns the
   original texture immediately. Failed replacement uploads also fall back.
5. [button_assets.py](../../tools/button_assets.py), `classic`, masks the existing
   letter out of the coloured oval. The bounds in
   [hud_button_layout.h](../../port/linux/game/hud_button_layout.h) match the
   committed alpha mask, keeping the existing visible size and text anchoring.

For example, CE's first sniper zoom uses the supplied 2x picture, while its next
zoom still draws the map's 8x. Neither label changes the actual magnification.
Custom textures that do not match the stock signatures keep their artwork.

## State and checks

CE placeholders belong to the existing map lifetime and are released by
`hud_hires_tags_unloaded`. MCC holds only an asset index in its own texture entry;
the embedded-HUD service owns replacement GPU textures. MCC teardown frees the
entry and its original GPU texture, without deleting the borrowed replacement.

[Correction tests](../../tools/test_hud_corrections.py) check supplied-pixel
packing, transparency and CE's untouched 8x selection.
[MCC bridge tests](../../tools/test_mcc_texture_bridge.py) check stock/custom
identity, settings changes, upload failures and ownership.
[Button tests](../../tools/test_hud_button_style.py) and
[MCC HUD tests](../../tools/test_mcc_hud.py) cover control animation, prompt
geometry and split-screen scaling. Runtime captures are needed in addition to
these isolated tests to verify actual map loading and draw paths.
