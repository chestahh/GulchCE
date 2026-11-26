# Modern lighting and shadows

This branch ports GulchCE's Modern Dynamic Shadows v1.1 to OpenCE. It uses the existing Xbox, Custom Edition and MCC loading paths. [Source release](https://github.com/chestahh/GulchCE/tree/build-766)

Open Video Setup and choose Dynamic Lighting to enable emitters. Under Shadows, choose Hard or Soft and load a map. Classic is the default. During play, a plasma bolt passing a biped can project its silhouette onto a wall. Shadows follow the bolt until the light expires or leaves the selection. Desktop shadow maps range from 128 to 2048 pixels per side.

- [Light collection and preferences](../../port/linux/game/lighting_config.c) and [projectile lighting](../../port/linux/game/plasma_lighting.c) supply the render-time preferences and light poses.
- [Shadow selection](../../port/linux/game/modern_shadows.c) handles map lifetime, per-view selection, owner exclusions and caster submission. Eight local lights can cast simultaneously per view. The active flashlight shares that budget unless the extra flashlight slot is enabled.
- [Spotlights and fixtures](../../port/linux/game/modern_spots.inl) handle the flashlight and up to two independent nearby fixtures. [Sun shadows](../../port/linux/game/modern_sun.inl) use separate resources for environmental coverage. Distant Sun Shadows extends that coverage and accounts for scoped views.
- [OpenGL rendering](../../port/linux/src/d3d8_gl.c) owns the depth targets, cached draw data and receiver filtering. Map preparation and cleanup run through the game and renderer lifecycle. The Android build excludes enhanced dynamic lighting and modern shadow rendering. Web uses its OpenGL ES path without desktop shadow maps.

The [settings table](../../port/linux/README.md) lists the individual controls. The [feature tests](../../tools/harness/tests/test_modern_shadows.py) exercise selection, ownership and resource limits.
