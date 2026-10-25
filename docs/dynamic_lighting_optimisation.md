# Dynamic lighting performance follow-up

This follows the [first sustained matrix](dynamic_lighting_performance.md).
The target is less redundant work with unchanged visual quality, defaults and
host policy. Field size (16 cubed), 1536 rays per refresh, eight admitted fields,
trace invalidation, temporal fades, light radii/colours and shader equations are
unchanged. CStencil and Modern remain outside scope.

## Profile and changes

`tools/lighting_performance_build.py --profile` adds CPU timers around tracing,
field reprojection, texture generation/upload and model binding. Binding time
includes any of the other operations it invokes and must not be added to them.
The test-only GPU fixture wraps draws in `GL_TIME_ELAPSED` queries every 32nd
frame. It polls availability before reading and skips sampling if its bounded
ring fills, rather than synchronising the CPU with the GPU. Query semantics are
from the [Khronos timer-query specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt);
the fixture is independently implemented. GPU rows classify other draws (0),
models with selected lights (1), selected world diffuse passes (2), and selected
specular passes (3). Model timing includes the complete material draw, not just
the additional lighting instructions. Query overhead can perturb rendering;
the sustained matrix uses a separate build without GPU queries or phase timers.

Two 120-second profiles after 30 seconds of warm-up used native 2560x1440,
the same quality settings as the first matrix, and fifteen Heroic Carousel bots.
Before optimisation, mean trace/field/texture CPU times were 3.63/0.64/0.32 ms
per rendered frame. Sampled GPU model/diffuse/specular draws averaged
0.217/0.118/0.075 ms per sampled frame; other draws averaged 4.44 ms.
This identifies CPU tracing as a larger target than changing lighting shaders.

Three conservative changes remove repeated work:

- `plasma_occlusion.c` prepares history coordinates once per axis rather than
  per cell. It keeps the exact coordinate arithmetic, sample accumulation order
  and out-of-range fallback. Existing collision and fade logic is retained.
- `plasma_occlusion_gl.c` resolves uniforms once per linked program and uses
  one immutable sampler instead of repeatedly looking up uniforms, assigning
  sampler units and setting texture parameters on each model draw. A small
  record in the backend's program entry is necessary because the backend owns
  shader linking and lifetime; a feature-global cache keyed only by GL names
  would be unsafe if names were reused. The feature owns the implementation.
- Equal diffuse/specular radii now share the same attenuation/visibility texture
  upload. Their expressions are identical, including directional masks. Other
  radii retain separate textures, and per-view invalidation still applies.

The follow-up profile measured 2.09/0.39/0.25 ms for trace/field/texture work.
It also had fewer selected lights and refreshes, so neither those frame averages
nor the higher profile FPS establish an isolated speedup. Field CPU time per
update fell from about 0.107 to 0.086 ms in these samples; scenes still differ.
Sampled GPU model/diffuse/specular draws averaged 0.057/0.093/0.077 ms, with
3.26 ms in other draws. No lower-quality shader or reduced sampling budget was
introduced to obtain these numbers.
These profiles precede the final identical-texture sharing change; sustained
and final regression checks validate the combined implementation.

## Acceptance

The resulting implementation is undergoing sustained and focused visual checks.
The final result must distinguish measured improvement from achieving a locked
frame rate. The first matrix's 60/120 acceptance gap remains open until validated.
