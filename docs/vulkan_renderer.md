# Native Vulkan renderer development

## Testing integration, 22 October 2026

The testing merge retains the OpenCE menu cursor, desktop GL visibility
readback updates and host-only co-op follower ordering. The Vulkan scene
adapter now carries Shield Look's `shield_uv` varying at location 9 and
its three draw-local Honeycomb vectors in the shared uniform block.
The existing shield setter forwards those vectors only while native Vulkan
is active; scale zero disables the surface, and map teardown clears it.
Pattern arithmetic, preset profiles, colours and Arm Shields gating remain
in the incoming shield feature. No MCC texture or loader boundaries change.

Local Windows, Android and web builds pass, along with 124 focused Vulkan,
shield and readback tests (four optional reference-asset tests skipped).
The real-GPU scene test on the RTX 5070 Ti compiles the actual Honeycomb
surface, verifies nonuniform shield alpha and preserved green RGB, then
verifies disabling it restores the original fragment. It also retains the
existing depth, texture, feedback and query checks. Khronos validation was
not enabled on this local run; CI provides separate validation-layer checks.

A hidden native Vulkan Honeycomb host rendered the Xbox Beaver Creek scene
against an OpenGL CLASSIC client for 799 frames, compiling 65 shaders without
a scene error. This is bounded rendering/simulation coverage, not visual
parity across maps or full campaign validation. Native Vulkan MCC support
remains outside this milestone's supported resource boundary.

All renderer development stays on `vulkan-renderer` until explicitly approved
for integration. Its base is `e5347765275bf324513eca4d044b15b3f6fc12b8` on
`render-fix-optimisation`, retaining the working AMD readback and NVIDIA
optimisation for comparisons. Neither `testing` nor OpenGL is replaced.

## Current status: first native Halo scene

Stage three adds opt-in native Xbox D3D8 draw translation, shader compilation,
textures and render targets. Xbox A10's opening space cinematic and ship
interior render through Vulkan on the local Windows NVIDIA test machine.
**OpenGL remains the default. This is an experimental scene milestone, not
full renderer parity or a completed Vulkan port.**

To build the normal game and its optional compiler on Windows:

```sh
python configure.py --portable --release
ninja windows vulkan_compiler
```

Use `ninja linux vulkan_compiler` on Linux. The compiler build needs CMake,
Ninja and a C++ toolchain (clang-cl with the existing MSVC environment on
Windows; clang/clang++ and lld on Linux). It fetches checksum-pinned glslang
16.6.0. CI native packages include the compiler library and licence notices.
OpenGL neither loads the compiler nor requires a Vulkan loader.

To opt in for a manual game run, launch from the normal game data folder:

```bat
set HALO_VULKAN_SCENE=1
halo.exe
```

Remove that variable to return to OpenGL. `HALO_VULKAN_CAPTURE=scene.ppm`
optionally saves periodic completed GPU images to one PPM file. Capturing
adds readback work and is not suitable for benchmarking. Validation is
optional with `HALO_VULKAN_VALIDATION=1`, requiring a matching 32-bit layer.
The separate `HALO_VULKAN_PRESENTATION` test remains available.

### What stage three implements

- Dedicated native game state and dispatch in `vulkan_game.c`; no NV2A
  register/FIFO/DMA emulation, and no OpenGL context or fallback.
- Existing pure NV2A vertex/combiner GLSL translators consumed through their
  public interfaces. `vulkan_shader.c` supplies explicit Vulkan bindings,
  varying locations and a shared std140 uniform block; glslang produces
  SPIR-V for Vulkan 1.1. The translator arithmetic is unchanged.
- Vertex declarations, packed normals, colors, streams, indexed and immediate
  draws, primitive conversion, shader program slots, constants and transforms.
  Indices remain indices at GPU submission to avoid duplicating triangle data.
- Swizzled/linear Xbox textures, mip levels, cubes/volumes and native BC1/2/3;
  independent Vulkan texture caching uses the existing CPU memory-watch API
  to notice writes, including changes within one frame.
- Color/depth targets, blend/depth/stencil state, viewport and bias translation.
  Sampling the current color target uses a separate snapshot. Depth feedback
  is explicitly unsupported. Offscreen scene color is presented by the
  stage-two window owner.
- Three fenced upload/descriptor/query slots. A busy slot defers the whole
  frame. Direct CPU query reads happen only after completion and availability;
  previous completed counts remain usable, and legitimate zero counts are
  published. Normal draws use no queue-idle or forced query waits.
- Map teardown retires scene and presentation references before clearing
  map caches. Shutdown handles partial creation and retains ownership if a
  wait fails. Waiting is allowed at these lifecycle boundaries.

The D3D8 entry points need small guarded dispatch hooks because the game calls
those exports directly. A CPU texture decode wrapper exposes the established
pure decoder without calling its GL cache. The SDL external-window hook
connects the independently owned window to existing input/event services.
The `port:` hook in `game_dispose_from_old_map` is necessary because a draw
module alone cannot know when map storage will be reused. All feature logic
stays in the Vulkan directory; disabled hooks preserve the established path.
Xbox, CE and MCC loading/ownership rules are unchanged. No network protocol or
simulation rule changes, so no network version increment is needed.

### Scope and remaining work

The scene uses the stock 640x480 SDR render size, scaled to the window. HDR,
MSAA/postprocessing, replacement high-resolution HUD handling, per-pixel
lighting enhancements, mouse-menu coordinate mapping, full settings parity and
broad map compatibility are not implemented. Previous-frame front-buffer
snapshots also need a dedicated implementation. MCC's independently owned textures are not treated as Xbox
physical addresses and are not supported by this milestone. CE remains
available through OpenGL; a Vulkan compatibility claim needs separate tests.

Resource limits fail explicitly: 64 MiB of upload space per frame, 512 MiB of
scene image allocations, bounded shader/pipeline/descriptor/query caches.
There is no streaming eviction within a map yet. First-use shader/pipeline
compilation can stall; this stage is not a performance optimisation. Older
drivers without stage two's presentation-maintenance extension remain
unsupported. AMD hardware and Android Vulkan have not been validated here.

### Stage-three validation

- Windows 32-bit build and real NVIDIA A10 rendering, including captured ship
  geometry, textures and interior characters; normal timed shutdown.
- Vulkan A10 -> menu -> A10 with cache retirement, no GL context, and clean exit.
- Default OpenGL Xbox -> CE -> MCC -> Xbox load/transition checks, with captures
  and no logged GL/shader failures. This is smoke coverage, not full map parity.
- Asset-free GPU test: shader uniforms, textures, indexed draws, depth rejection,
  visible/occluded precise queries, repeated slot reuse, target feedback and
  checked image pixels. `tools/vulkan_build.py --scene` builds it; put the
  matching compiler library beside the executable before running it.
- Delayed-GPU regression tests compile the real scene implementation. Fault
  mutations demonstrate detection of early reuse, unavailable samples,
  blocking query flags and late stale results. Failed teardown retains ownership.
- A local Windows Blood Gulch match with an OpenGL host and Vulkan client: 41
  seconds compared by the existing network log judge, no mismatched samples
  under its established tolerances, both processes exited normally.
- Existing foundation/presentation ownership tests remain in the suite.
  CI additionally runs 64-bit software-Vulkan synchronization validation of
  the scene, and the normal Windows/Linux/Android/web build matrix.

```sh
python -m pytest -q tools/test_vulkan_backend.py tools/test_vulkan_scene.py
```

### Published stage-three checks

Implementation `a65d5e56` passed [Build run 39684530855](https://github.com/chestahh/GulchCE/actions/runs/39684530855):

- Windows, Linux, Android and inherited web debug/release builds, including
  native compiler libraries and their licence notices.
- Twenty Vulkan ownership/completion tests; 32-bit offscreen rendering;
  supplemental host-ABI window and scene runs with Khronos synchronization
  validation. The scene checked all GPU pixels, depth rejection, completed
  queries, texture sampling and target feedback. Unused vertex-input warnings
  remain; no validation errors were reported.
- Linux's regression suite: 901 passed, 6 skipped; changelog suite: 177 passed;
  ordinary/full-LTO light-storage checks and both native presentation lifetimes.
- Web build-graph/socket and Chromium/Firefox checks.
- [MCC map validation](https://github.com/chestahh/GulchCE/actions/runs/39684531329)
  also passed. This does not establish Vulkan compatibility with MCC maps.

Local Windows checks finished with 38 passing Vulkan/GL visibility tests. The
final A10 run presented 6,239 frames and 992,705 draws, then shut down normally.
Temporary map copies and caches were removed; inactive test saves and captures
were archived losslessly and verified. The original game data was untouched.

## Foundation and presentation milestones

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

## Stage two: native dispatch and window ownership

The normal executable has an opt-in presentation test. On Windows CMD:

```bat
set HALO_VULKAN_PRESENTATION=1
set HALO_HIDDEN_WINDOW=1
set HALO_VULKAN_CAPTURE=presentation.ppm
halo.exe
```

On Linux, use a virtual display for automated execution:

```sh
HALO_VULKAN_PRESENTATION=1 SDL_VIDEO_DRIVER=x11 xvfb-run -a build/linux/halo
```

Omit `HALO_HIDDEN_WINDOW` for a manually viewed test. The test clears red,
green and blue, presents 60 frames, resizes, suspends presentation, restores,
destroys the device and repeats. It verifies every pixel of each frame using
completion-fenced image readback; an optional PPM contains the final blue
image. A successful log has two `frames=60 generations=3 result=PASS` lines
(or more generations if the window system requests recreation), and two
`submitted=60 verified=60` lines. Any failure exits nonzero. It does not load
maps, start the game or silently fall back to OpenGL. This is diagnostic
presentation, not a renderer performance benchmark.

`debug.vulkan_presentation`, `debug.vulkan_validation` and
`debug.vulkan_capture` are documented in the platform settings table. All are
off/empty by default. Validation requires the matching process-architecture
Khronos layer. Vulkan is loaded dynamically only when the test is selected;
ordinary OpenGL gameplay does not require a Vulkan installation.

The four native entry points `Direct3D_CreateDevice`, `D3DDevice_Clear`,
`D3DDevice_Present` and `D3DDevice_Release` dispatch into the dedicated Vulkan
adapter only during this test. The existing GL bodies remain intact. The
minimal startup hook in `source/shell/shell_xbox.c` is necessary to run the
real native entry points without proceeding into unimplemented game draws.
It is marked `port:` and compiled only for native Windows/Linux. The game
and adapter keep the Xbox ABI; Vulkan/SDL modules use the host ABI through a
plain-C interface. No GPU register, FIFO or DMA emulation is introduced.

Presentation requires Vulkan 1.1, a graphics queue that can present, precise
queries, and `VK_EXT_swapchain_maintenance1` (plus its instance dependencies).
The extension provides presentation fences: a render fence alone is not
proof that the presentation engine has released a semaphore or swapchain.
Unsupported devices fail clearly; an older-driver fallback is not implemented
in this milestone. The test additionally requires a readable RGBA8/BGRA8
UNORM swapchain and coherent host-visible capture memory.

Each in-flight slot owns its acquisition semaphore/fence, render fence and
presentation semaphore/fence. Slots are reused only after all relevant work
completes. Acquire uses a zero timeout. Busy, minimized or zero-size windows
defer a frame. Resize drains completion by polling before retiring the old
swapchain and its readback buffers; it does not wait for device idle each
frame. Out-of-date/suboptimal results request recreation. Surface loss and
device loss end the test explicitly. Teardown waits for owned acquisitions,
submissions and presentation operations; partial initialization is unwound,
and failed submission fences are never waited on as though submitted.

GPU pixel readback belongs only to this diagnostic. Stage three must connect
actual scene resources, shaders and draws, preserving this ownership model.
The query-only foundation test remains independent of window presentation.

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

Feature code lives in `port/linux/src/vulkan/`. The foundation changed no engine implementation or map-family path; the
presentation increment adds only the guarded native dispatch and startup hooks
described above. The build hook adds an optional
test target; the normal Windows, Linux, Android and browser targets keep
their existing code and defaults. No network or simulation contract changes.

Later game integration needs narrowly scoped dispatch at device creation,
draw, resource and present boundaries because a feature-only module cannot
receive those calls otherwise. Keep the established GL implementation intact
and default during development. Preserve Xbox/CE behaviour, MCC activation
and cleanup boundaries, and separately validate each affected map family.

## Remaining work for the first Halo scene

1. Extend the lifecycle dispatch into a native draw/resource interface and
   audit every call used by the first scene. The existing presentation test
   is not scene selection; expose a playable mode only when it can run that
   scene, with no silent GL fallback.
2. Adapt our existing NV2A shader decoding to Vulkan shader interfaces and
   SPIR-V. Preserve combiner saturation, viewport conventions and constants.
   Add input/output tests before changing any shared translator behaviour.
3. Implement vertex/index uploads, texture conversion and identity, descriptor
   bindings, render targets, pipeline state and caches. Retain render-target
   sampling, depth/bias and blend semantics needed by Halo.
4. Connect scene output to the implemented SDL Vulkan presentation and
   retirement path. Integrate asynchronous visibility delivery, retaining
   previous completed counts while new results are pending.
5. Render a real Halo scene with its materials, depth, shadows and lens flares.
   Compare against OpenGL and test on NVIDIA and AMD. Then test representative
   Xbox, CE and MCC scenes and transitions, not merely successful map loads.

Android needs a dedicated host/guest Vulkan bridge and device validation;
this target does not implement that bridge. Browser support is outside GulchCE's planned Vulkan scope. The inherited web
build remains intact; this work does not remove it. Android integration
remains separate from completing the initial Windows/Linux scene.

Experimental dynamic lighting is later work. Leave room for feature-owned
passes and resources without mixing new lighting behaviour into the stock
Xbox compatibility path. Vulkan alone is not a performance guarantee.

## Reference and provenance

No xemu code, shaders or generated output were copied or adapted. The backend
and test shaders were independently written against the Vulkan API. Existing
OpenCE translations remain the source of Halo behaviour for future integration.
The only vendored material in the foundation increment was the pinned, unchanged Khronos
C declarations and their licenses (`port/third_party/vulkan_headers/`).

References consulted:

- xemu's Vulkan renderer organisation and visibility reports:
  https://github.com/xemu-project/xemu/tree/master/hw/xbox/nv2a/pgraph/vk
  Its report path can request waiting query reads; that scheduling policy is
  not adopted here. Its renderer carries LGPL notices and is a behavioural
  reference, not a source transplant.
- Vulkan query result availability, precise counts and reset rules:
  https://docs.vulkan.org/spec/latest/chapters/queries.html
- Presentation semaphore ownership and retirement:
  https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
- SDL's Vulkan surface integration:
  https://wiki.libsdl.org/SDL3/SDL_Vulkan_GetInstanceExtensions

## Initial validation

Local Windows x86 build: clang, C89, warnings treated as errors. The real GPU
test passed on NVIDIA RTX 5070 Ti, driver 617.42, with all 36 submitted frames
verified. Ten Vulkan tests (including seven deliberate-fault controls) and
18 existing GL visibility regressions passed: 28 total. Validation errors
reported during device/instance destruction also fail the smoke test.

The local machine lacks the Khronos validation layer; a separate run verifies
that requesting it fails clearly. GPU validation in CI uses Linux software
Vulkan with Khronos validation enabled. Record that job's actual outcome
separately; hardware execution without validation is not equivalent to a clean
validation-layer run. AMD and Android GPU execution remain untested.

Ubuntu's CI image does not provide an i386 validation-layer package. CI
therefore runs the normal 32-bit test on software Vulkan, and separately
builds the same source with `python3 tools/vulkan_build.py --host-validation`
into `build/vulkan-host/`. That supplemental 64-bit run enables Khronos
validation and synchronization validation. It does not change the game's
ABI or replace 32-bit coverage.

## Stage two validation

Local Windows portable release, 32-bit: real native dispatch passed on the
RTX 5070 Ti (driver 617.42), two device lifetimes, 120 submitted and verified
frames, three swapchain generations per lifetime. The final capture was
inspected. The unchanged offscreen depth/visibility test also passed. The
local Khronos layer is unavailable; a separate run confirmed that requesting
it fails clearly with exit code 1. AMD hardware remains untested for this
milestone.

The ownership suite adds delayed presentation, acquisition timeouts,
suboptimal/out-of-date results, failed submissions, partial allocation,
surface/device loss and teardown checks. Mutation controls ensure missing
presentation waits, blocking acquisition, early retirement and omitted
presentation fences are detected. Together with existing visibility tests,
33 tests passed locally. Tests use the real implementation with fake Vulkan
entry points, and real hardware separately.

CI runs the native 32-bit offscreen test and the actual Linux game dispatch;
a supplemental host-ABI window executable enables Khronos synchronization
validation on Lavapipe under Xvfb. `python tools/vulkan_build.py --window`
builds the SDL window test; `--host-validation` selects the supplemental ABI.
Record CI outcomes against the tested commit; enabling a job is not a pass.

With Vulkan disabled, a hidden Windows OpenGL session rendered Xbox A10,
Custom Edition Death Island, MCC Tiny Halo, then Xbox A10 again. Captures
were inspected, including the returning Xbox scene's thruster flares. The
session exited successfully with no reported GL errors. This checks those
scenes and transitions, not complete campaign compatibility. Temporary map
copies and saves were isolated from the original game data.

This Windows environment has no Linux runtime, Android build target or
Emscripten target. Those builds require CI. The local web build-graph suite
reported five passes and six Windows environment failures (including path
separator assumptions and unavailable native socket tooling); it is not
recorded as a passing web check.

No simulation, networking or map-family semantics are changed, so no network
version increment is required. Vulkan map compatibility remains untested
because this stage deliberately does not render maps.

### Published stage two checks

Implementation commit `d6aef94e` and CI dependency correction `000cf3f6` were
checked in [Build run 39677484208](https://github.com/chestahh/GulchCE/actions/runs/39677484208):

- Windows, Linux, Android and inherited web debug/release builds passed.
- The actual 32-bit Linux game's native presentation dispatch passed under
  Xvfb. The first run needed the container's missing 32-bit X11 runtime;
  the correction installs it without changing renderer code.
- The Vulkan job passed all 15 ownership tests, the 32-bit offscreen test,
  and supplemental host-ABI offscreen/window tests with Khronos and
  synchronization validation. Both window lifetimes verified all 60 frames.
- The Linux regression suite recorded 901 passes with 6 skips; the changelog
  suite recorded another 177 passes. Ordinary and full-LTO light-storage
  checks passed.
- The web build-graph/socket suite passed all 11 tests on Linux, resolving
  the local Windows-environment check limitation. Browser checks passed too.

The separate [MCC map validation run](https://github.com/chestahh/GulchCE/actions/runs/39676468383)
passed for `d6aef94e`; the subsequent code change only adds CI dependencies.
This is build/test evidence, not AMD or Android Vulkan hardware coverage.
