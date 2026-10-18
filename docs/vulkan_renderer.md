# Native Vulkan renderer development

All renderer development stays on `vulkan-renderer` until explicitly approved
for integration. Its base is `e5347765275bf324513eca4d044b15b3f6fc12b8` on
`render-fix-optimisation`, retaining the working AMD readback and NVIDIA
optimisation for comparisons. Neither `testing` nor OpenGL is replaced.

## Current status: first milestone started

The initial commit establishes the native Vulkan device and independently
authored rendering tests. **The game still renders through OpenGL. There is
no playable Vulkan mode yet, and the first Halo-scene milestone is not complete.**

The native foundation provides:

- Dynamic loader discovery on Windows and Linux, with Vulkan 1.1 and precise
  occlusion-query capability checks. No installed SDK or Vulkan import
  library is required to build it.
- A graphics queue, three command slots and disjoint query ranges. Pending
  command buffers and queries cannot be reused. Errors quarantine a slot.
- Nonblocking fence and query-availability checks. A whole batch is published
  only after it is complete, including legitimate zero results. Query reads
  use neither WAIT nor PARTIAL result flags. Only teardown waits for idle.
- An offscreen GPU test with color and depth attachments, independently
  authored shaders, visible and occluded draws, host image readback and
  checked pixels. Twelve cycles exercise all three slots repeatedly.
- Asset-free delayed-GPU and error regression tests against the actual C
  implementation, with deliberate faults proving the tests detect early
  reads, partial publication, blocking query flags, wrong slots and reuse.

The two-query layout and fixed-size offscreen attachments are foundation-test
scope, not the eventual game renderer's capacities or scene representation.
The test draws triangles, not Halo models; it does not test Xbox shader
translation, textures, shadows, presentation or map compatibility.

## Build and check

Use the existing native compiler setup, then:

```sh
python configure.py --portable --release
ninja vulkan
python -m pytest -q tools/test_vulkan_backend.py tools/test_visibility_readback.py
```

The result is `build/vulkan/vulkan-smoke.exe` on Windows, or
`build/vulkan/vulkan-smoke` on Linux. Both are 32-bit, matching the native
game's process architecture. Linux needs the normal 32-bit development
runtime and a 32-bit Vulkan loader/driver. The test opens no window, changes
no game configuration and reads no maps or saves.

```sh
build/vulkan/vulkan-smoke --capture build/vulkan/depth.ppm
build/vulkan/vulkan-smoke --validation
```

On Windows add `.exe`. `--validation` requires a matching 32-bit
`VK_LAYER_KHRONOS_validation`; if absent the test fails explicitly rather
than reporting an unvalidated run as validated. A passing run verifies
nonzero visible counts, zero hidden counts, a red center and black corner.
Capture is optional and written only to the requested path.

The test SPIR-V is generated from the GLSL files in `tests/`:

```sh
python tools/vulkan_shaders.py --compiler /path/to/glslangValidator
```

Initial compiler: Khronos glslang 16.6.0, `main-tot` Windows x86-64 release
archive SHA256 `520099102c8605488d0352365d363aef1423a49f04eb5ef3d3f88eed7bea723b`.
Generated headers include source hashes. No compiler binaries are vendored.

## Native design and isolation

Keep the existing game-facing Xbox D3D8 ABI. The port implements its calls
as native functions, as it does today. Vulkan will receive translated draws
and resources directly. Do not introduce emulated NV2A registers, a FIFO/DMA
processor, guest GPU instruction scheduling, QEMU or a virtual Xbox GPU.
Translating the shaders and render states supplied by the game is still
necessary; this does not require hardware emulation.

Feature code lives in `port/linux/src/vulkan/`. This first increment changes
no engine implementation or map-family path. The build hook adds an optional
test target; the normal Windows, Linux, Android and browser targets keep
their existing code and defaults. No network or simulation contract changes.

Later game integration needs narrowly scoped dispatch at device creation,
draw, resource and present boundaries because a feature-only module cannot
receive those calls otherwise. Keep the established GL implementation intact
and default during development. Preserve Xbox/CE behaviour, MCC activation
and cleanup boundaries, and separately validate each affected map family.

## Remaining work for the first Halo scene

1. Define the native draw/resource interface and audit all game-facing calls
   used by the test scene. Add opt-in selection only when Vulkan can run it;
   do not silently fall back to GL while claiming a Vulkan run.
2. Adapt our existing NV2A shader decoding to Vulkan shader interfaces and
   SPIR-V. Preserve combiner saturation, viewport conventions and constants.
   Add input/output tests before changing any shared translator behaviour.
3. Implement vertex/index uploads, texture conversion and identity, descriptor
   bindings, render targets, pipeline state and caches. Retain render-target
   sampling, depth/bias and blend semantics needed by Halo.
4. Add SDL Vulkan presentation, resize/minimize handling and explicit resource
   retirement. Use presentation-completion-safe semaphore ownership. Keep
   GPU readback asynchronous and retain previous completed visibility counts.
5. Render a real Halo scene with its materials, depth, shadows and lens flares.
   Compare against OpenGL and test on NVIDIA and AMD. Then test representative
   Xbox, CE and MCC scenes and transitions, not merely successful map loads.

Android needs a dedicated host/guest Vulkan bridge and device validation;
this target does not implement that bridge. Browser rendering remains WebGL;
native Vulkan is not a browser backend. Those platform decisions remain
separate from completing the initial Windows/Linux scene.

Experimental dynamic lighting is later work. Leave room for feature-owned
passes and resources without mixing new lighting behaviour into the stock
Xbox compatibility path. Vulkan alone is not a performance guarantee.

## Reference and provenance

No xemu code, shaders or generated output were copied or adapted. The backend
and test shaders were independently written against the Vulkan API. Existing
OpenCE translations remain the source of Halo behaviour for future integration.
The only vendored material in this increment is the pinned, unchanged Khronos
C declarations and their licenses (`port/third_party/vulkan_headers/`).

References consulted:

- xemu's Vulkan renderer organisation and visibility reports:
  https://github.com/xemu-project/xemu/tree/master/hw/xbox/nv2a/pgraph/vk
  Its report path can request waiting query reads; that scheduling policy is
  not adopted here. Its renderer carries LGPL notices and is a behavioural
  reference, not a source transplant.
- Vulkan query result availability, precise counts and reset rules:
  https://docs.vulkan.org/spec/latest/chapters/queries.html
- Presentation semaphore ownership for the upcoming windowed path:
  https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
- SDL's Vulkan surface integration:
  https://wiki.libsdl.org/SDL3/SDL_Vulkan_GetInstanceExtensions

## Initial validation

Local Windows x86 build: clang, C89, warnings treated as errors. The real GPU
test passed on NVIDIA RTX 5070 Ti, driver 617.42, with all 36 submitted frames
verified. Nine Vulkan tests (including six deliberate-fault controls) and
18 existing GL visibility regressions passed: 27 total.

The local machine lacks the Khronos validation layer; a separate run verifies
that requesting it fails clearly. GPU validation in CI uses Linux software
Vulkan with Khronos validation enabled. Record that job's actual outcome
separately; hardware execution without validation is not equivalent to a clean
validation-layer run. AMD and Android GPU execution remain untested.
