# E6.3 distant sunlight qualification

## E6 sign-off

E6 closed on 26 November 2026 after the Blood Gulch fix passed the Windows
playtest. The approved package was `GulchCE-Distant-Sun-Flicker-Fix-Windows.zip`,
SHA-256 `5e8972f0cedaf4aa82f9d39d64165d3d4f569c8e4a1ce878594c62e30706fdcc`.
The final check covered Hard and Soft at 1024 and 2048 with 8× MSAA.
[Blood Gulch comparisons](#blood-gulch-terrain-flicker)

Sign-off accepted the earlier Two Betrayals measurements with all eight local
slots. Hard 1024 reached 240 FPS in 240 of 240 measured seconds and Soft
1024 in 233 of 240. Those timings predate the flicker fix. [Combat runs](#sharing-the-frame-pacing-fence)

## Fixed-quality combat

The Windows comparison used the E6.2a [desktop prototype](../../tools/fixtures/distant_sun_prototype.patch) on GulchCE `c0ce737e6`. Each row was a fresh launch on the RTX 5070 Ti with performance cores selected, a hidden 2560 × 1440 render target, AA and VSync off, and uncapped presentation. Both sides used the same executable, native 1024 shadow maps, dynamic lighting, sun, fixtures and the Shared flashlight. The sole rendering change between paired runs was `HALO_E6_DISTANT_SUN`. All eight local slots were observed in each run.

The original E5 save was unavailable. The reconstruction used the same Xbox Two Betrayals map (SHA-256 prefix `b5fb3db5`), exterior BSP `c40_c`, fixed camera at (327.8, −649.3, 74.5), and the authored `e3_a` and `c3_base_tier_2` squads. Every run restored the same reconstructed core (`d45bef13`), confirmed 19 living Covenant in the first squad-count check, then verified the camera throughout the measured interval. The AI continued to simulate. Use the paired near-only runs as the control for this checkpoint.

The [E5 measurement procedure](environment_shadow_e5.md#measuring-sustained-fps) counts frame completions in consecutive one-second windows after 25 seconds of warmup. Each sample below covers 120 seconds without screenshots or GPU timer queries. Rows appear in run order. Hitch counts are cumulative.

| Mode | Distant sun | Average FPS | Slower minute FPS | Seconds below 240 | Longest stretch (s) | P99 frame (ms) | Frames over 16.7 / 33 / 50 / 100 ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| Hard | Off | 273.9 | 273.2 | 0 | 0 | 5.34 | 0 / 0 / 0 / 0 |
| Hard | On | 276.0 | 274.3 | 0 | 0 | 5.38 | 0 / 0 / 0 / 0 |
| Soft | On | 274.7 | 273.3 | 1 | 1 | 5.31 | 0 / 0 / 0 / 0 |
| Soft | Off | 272.9 | 270.9 | 0 | 0 | 5.47 | 0 / 0 / 0 / 0 |
| Hard | On | 266.1 | 265.1 | 2 | 2 | 5.45 | 11 / 7 / 3 / 2 |
| Hard | Off | 273.9 | 273.7 | 0 | 0 | 5.37 | 0 / 0 / 0 / 0 |
| Soft | Off | 271.8 | 270.5 | 0 | 0 | 5.50 | 0 / 0 / 0 / 0 |
| Soft | On | 266.9 | 266.8 | 0 | 0 | 5.48 | 0 / 0 / 0 / 0 |

The Soft dip was one second at 234 FPS. The second Hard run with the outer tile enabled reached 155 and 171 FPS in consecutive seconds. Its two longest frames were 287 and 139 ms. The four controls each held 240 throughout. The mean-FPS difference changed sign in reversed order for both modes. These timings establish the player-visible stalls. Assigning a cause to the Hard hitch requires a trace of the event.

A subsequent fresh Hard 1024 run with distant sunlight enabled averaged 275.0 FPS, held 240 in every measured second and had a worst frame of 7.58 ms.

Two additional controls linked the checkout's original near-sun renderer and shaders with the same passive combat measurement fixture. Soft averaged 270.0 FPS (269.9 in the slower minute). One 765 ms frame contributed to a two-second stretch below 240. Hard averaged 264.9 FPS (264.8 in the slower minute), with five isolated seconds below 240 and a worst frame of 10.08 ms. A long stall can therefore occur with the entire distant-sun implementation absent. Its cause and that of the 287 ms event remain unverified.

## Sun pass cost

Separate 22-second combat profiles timed CPU work around `sun_prepare` and sampled asynchronous GPU elapsed-time queries around the actual depth draws. CPU time includes traversal, submission and any driver waits inside the call. The GPU columns cover depth generation. CPU and GPU can overlap. The full-run FPS measurements above also include receiver shading and presentation.

| Mode | Distant sun | CPU sun preparation, mean ms/frame | GPU near posed depth, mean ms/frame | GPU far posed depth, mean ms/frame | Peak shadow texture payload, MiB |
| --- | --- | ---: | ---: | ---: | ---: |
| Hard | Off | 0.149 | 0.012 | — | 287.1 |
| Hard | On | 0.180 | 0.019 | 0.012 | 295.1 |
| Soft | Off | 0.151 | 0.013 | — | 287.1 |
| Soft | On | 0.182 | 0.018 | 0.012 | 343.1 |

Every measured combat frame reused static sun depth. The far BSP and posed targets together used 8 MiB at 1024. The peak column counts the active texture storage for local, spot and environmental shadows, including the shared scene-depth copy, at the formats' nominal sizes. Other shadow textures can be allocated on demand during the fight. The column excludes driver padding, mesh buffers and unrelated renderer allocations.

## Scoped pans and indoor control

On Xbox Blood Gulch, the actual sniper zoom cycled through both magnifications while the aim crossed a ridge and a Warthog. The camera stayed in one place and the aim depth reached 42.69 world units. At Hard 1024, 440 of 31,159 frames rebuilt static depth during the 40-second window. At Soft 1024, 442 of 30,445 did. CPU sun preparation averaged 1.054 / 1.080 ms on rebuilds (Hard / Soft) and 0.043 / 0.044 ms on hits. The additional far static draw averaged 0.013 ms of GPU time on rebuilds in both modes.

For each mode, 11 rebuild frames had a byte-identical far projection and a changed near projection. The remaining rebuilds changed both matrices. The current cache compares floating-point matrices. Measuring how often the **absolute snapped outer grid cell** changes would distinguish genuine far movement from small differences when the far matrix is reconstructed relative to the near origin. CPU submission on a rebuild is the larger scoped cost.

In Xbox Rat Race's two-lamp room, both fixture slots contributed throughout the 12-second Soft 1024 control. With distant sunlight enabled, 4,589 measured frames issued zero sun depth passes or static rebuilds. Map preparation still reserved the far textures. The indoor saving is draw time.

## Outdoor coverage

The Soft 1024 camera comparisons sampled every sixth simulation tick. Each pair had 68 matching camera positions and forward vectors at logged precision (four decimal places). Every retained sample had a valid BSP cluster. These captures ran at a lower screen resolution separately from the timing trials.

- **Xbox Sidewinder.** A staged Warthog used forward and reverse throttle near an authored snow pine, with turns and lateral camera movement in both directions. The camera travelled 16.7 world units. The pine stayed outside near coverage throughout. Its outer weight rose from 0.30 to 1.00 (rounded). An earlier, wider lateral camera path left the BSP and was discarded.
- **MCC Mercury Rising 2.** The camera approached and retreated from the authored Covenant cache at (−151.815, 109.546) in the outdoor plains BSP. Near and far weights at the tracked point went from 0 / 1 to 1 / 0 and back to 0 / 1. This route exercised the overlap. The earlier Blood Gulch rim capture covers the outer-to-stock fade. A capture from the map's opening BSP was discarded.
- **CE Death Island.** A separate Windows map run exercised the enabled sun pass. This was a compatibility smoke check. The matched motion comparisons cover the two maps above.

## Decision after the first comparison

Keep distant sunlight in the opt-in prototype. The combat averages justify further work, but both modes missed the strict sustained gate in the paired runs. The measured scope rebuilds and the unexplained combat hitch warrant different experiments.

1. Capture the next long combat frame with a rolling trace covering CPU submission, local and sun shadow work, texture and shader first use, GPU fences and presentation. Preserve the preceding frames so a wait can be connected to the work that queued it. Use asynchronous GPU timings and separate capture-free throughput runs.
2. Give each static sun tile an independent validity key based on its absolute snapped grid cell, light projection, and BSP/geometry revision. Resubmit only the dirty tile's intersecting static batches. Refresh posed casters wherever their projected shadows can reach visible receivers, including from outside the camera view.
3. Measure receiver shading separately after that change. The shader already skips a range whose blend weight is zero. Investigate conservative draw or region rejection only where a complete projected shadow footprint cannot contribute. Keep the fade, fixed resolution and all eight local slots in each comparison.

## Original renderer hitch trace

The follow-up used the same checkpoint, settings and 120-second measurement window. One executable linked the original near-sun renderer and shaders. Another linked the E6.2a prototype with the outer tile enabled. Both recorded the same per-frame CPU timers. They covered game rendering, shadow preparation, sun preparation, geometry uploads, texture binding, shader compilation and linking, visibility query transfers, the presentation fence and swap. Timers inside the render call overlap. The logged thread CPU clock is quantized and cannot identify short scheduling gaps.

| Traced run, in order | Frames measured | Longest frame | Sun preparation on that frame | Swap on that frame |
| --- | ---: | ---: | ---: | ---: |
| Original Soft | 32,494 | 9.62 ms | 0.19 ms | 0.06 ms |
| Distant Soft before split | 31,761 | 10.26 ms | 0.16 ms | 6.23 ms |
| Distant Hard before split | 32,170 | 12.77 ms | 0.14 ms | 0.38 ms |
| Original Hard | 30,857 | 12.79 ms | 0.18 ms | 0.10 ms |

There were zero measured shader compiles or links in all four traces. The original runs and the distant runs both had frames dominated by work outside sun preparation. The previous 287 ms and 765 ms stalls did not recur in 127,282 traced frames. Their causes remain open.

## Independent static tiles

The outer tile now has an absolute snapped origin. Moving the near projection leaves that origin stable. Each tile tracks its own static depth and cutout dependencies. A BSP or breakable change invalidates both. Failed draws leave the affected tiles dirty for retry. Posed casters still update every frame, including offscreen casters whose shadows reach the view. The receiver shader, blend weights, resolution and local-light selection are identical to E6.2a.

| Blood Gulch scoped pan, 40 s | Near static draws | Far static draws | Mean CPU sun preparation on rebuild frames |
| --- | ---: | ---: | ---: |
| Hard, before | 441 | 441 | 1.102 ms |
| Hard, split | 443 | 0 | 1.021 ms |
| Soft, before | 440 | 440 | 1.193 ms |
| Soft, split | 445 | 0 | 1.065 ms |

The camera reached the same 42.69-world-unit aim depth. Render FPS differed between runs, so the number of intermediate near positions sampled during the pan varied slightly. Both tiles were already populated at the start of the measurement. The stationary Two Betrayals camera reused static depth throughout all six main receiver profiles.

## Receiver shading in the snowy fight

Separate Windows profiles sampled GPU elapsed time every 32nd frame. The BSP column times the sun receiver draw on map geometry. The model column sums complete draws using a sun-enabled model program in each sampled frame, including their material and other lighting work. It cannot isolate the sunlight instructions inside those draws. No elapsed-time query was used in the final FPS comparison.

| 1024 setting | Sampled frames | BSP receiver, mean ms | Sun-receiving model draws, mean ms |
| --- | ---: | ---: | ---: |
| Hard, near-only prototype | 184 | 0.0080 | 0.1114 |
| Hard, distant before split, repeat | 188 | 0.0210 | 0.0945 |
| Hard, distant after split | 174 | 0.0218 | 0.0986 |
| Soft, near-only prototype | 187 | 0.0116 | 0.0991 |
| Soft, distant before split | 190 | 0.0289 | 0.0973 |
| Soft, distant after split | 184 | 0.0274 | 0.1269 |

The first Hard pre-split profile ran at 107 FPS. Its 73 sampled frames averaged 0.0255 ms for the BSP and 0.6083 ms for model draws. An identical fresh launch averaged 0.0210 and 0.0945 ms. Both runs are retained. Against the near-only prototype, the split candidate added about 0.014 ms in Hard and 0.016 ms in Soft to the BSP receiver draw. These samples leave little GPU time to recover by changing that draw. They do not establish a repeatable change in model shading cost.

## Checking the cached result

A separate Windows validation executable read both 1024² static visibility textures, then resubmitted the complete eligible BSP to both tiles at the same simulation tick and camera. The reference bypassed mesh-to-tile culling and kept the original cutout materials. It compared every floating-point texel in each cached image with its fresh counterpart.

| Map and setting | Tile images compared | Differing texels |
| --- | ---: | ---: |
| Xbox Blood Gulch, scoped Hard | 28 | 0 |
| Xbox Blood Gulch, scoped Soft | 28 | 0 |
| CE Death Island, Soft | 16 | 0 |
| MCC Mercury Rising 2, Hard | 36 | 0 |

These 108 images cover initial builds, cache hits and near-only movement. Far-only movement and retry after failure were checked with the actual tile-dispatch code in the asset-free harness. The MCC readbacks establish static BSP parity on sampled frames. The separate 20-second MCC measurement window registered zero posed sun casters. The targeted harness passed 22 tests and the Windows GPU suite passed 20, including receiver overlap, shadow filtering, skinning and animated cutouts.

## Capture-free retest against the original renderer

Runs appear in execution order. Each used a fresh process, a 25-second warmup and 120 complete one-second windows at 2560 × 1440, with fixed 1024 shadow resolution. The camera and starting 19-Covenant squad check passed in every run. Sun shadows were active throughout and the renderer reached all eight local slots including the shared flashlight in every run. The scene determines how many slots contribute on any individual frame.

| Run | Renderer | Average FPS | Slower minute FPS | Lowest second FPS | Seconds below 240 | Longest stretch, s | P99 frame, ms | Worst frame, ms |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | Original near Hard | 270.0 | 267.9 | 234 | 6 | 2 | 5.46 | 15.37 |
| 2 | Split distant Hard | 274.0 | 273.7 | 118 | 1 | 1 | 5.43 | 101.10 |
| 3 | Split distant Soft | 265.2 | 262.0 | 228 | 11 | 7 | 5.51 | 11.34 |
| 4 | Original near Soft | 259.3 | 256.9 | 212 | 6 | 2 | 5.89 | 9.68 |
| 5 | Split distant Hard | 256.8 | 249.9 | 225 | 31 | 4 | 5.69 | 12.20 |
| 6 | Original near Hard | 268.9 | 266.2 | 238 | 1 | 1 | 5.49 | 13.70 |
| 7 | Original near Soft | 238.8 | 211.4 | 144 | 40 | 39 | 8.09 | 11.73 |
| 8 | Split distant Soft | 219.8 | 204.2 | 141 | 66 | 40 | 8.46 | 13.97 |

The split candidate held 240 in 208/240 seconds in Hard and 163/240 in Soft. The original renderer held it in 233/240 and 194/240 respectively. The average difference changed sign when run order reversed in both modes. The Soft candidate and control both had extended slow periods in the reversed pair. The traces from other launches cannot identify the cause of those particular periods.

## Replaying the 101 ms Hard frame

The first split Hard run registered seven frames over 50 ms around simulation ticks 2516–2534. The original Hard control's worst frame over those same ticks was 5.25 ms. Two additional fresh Hard traces replayed the interval with rolling CPU timing and half-second GPU clock and load samples. The split renderer's worst frame in the interval was 6.16 ms. The original renderer's was 8.73 ms. The 101 ms stall did not repeat.

The split trace later spent 57 consecutive seconds below 240. Grouping frames by their one-second window shows where that slowdown accumulated. The timings are inclusive wall time. Sun preparation is part of shadow preparation, and shadow preparation is part of game rendering.

| Hard trace | Window FPS | Frame, ms | Game render, ms | Shadow preparation, ms | Sun preparation, ms | Swap, ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Split | ≥240 | 3.740 | 3.203 | 0.272 | 0.095 | 0.042 |
| Split | <240 | 5.088 | 3.544 | 0.299 | 0.115 | 0.052 |
| Original | ≥240 | 4.006 | 3.368 | 0.260 | 0.078 | 0.046 |
| Original | <240 | 4.447 | 3.632 | 0.282 | 0.091 | 0.059 |

In the split trace, about 1.01 ms of the 1.35 ms mean increase fell outside `main_game_render`. The slowest individual frame was 32.26 ms, of which 5.75 ms was in game rendering and 0.15 ms in sun preparation. The original had a different slowest frame, 30.63 ms, with 25.18 ms inside game rendering. Both traces recorded zero measured shader compiles or links. All 474 hardware samples reported P0, graphics clocks between 2895 and 2925 MHz and a maximum sampled temperature below 51 °C. Sub-second events between hardware polls remain possible.

**Decision.** Keep independent static tiles in the opt-in prototype for the verified scope saving. The strict sustained gate remains open in both modes. The next trace should time simulation updates, frame throttling and the complete presentation call in `main_loop_iteration` separately, alongside thread scheduling. The outer receiver draw and static rebuilds account for too little measured time to explain the sustained split-trace slowdown.

## Following the sustained slowdown

The archived Hard trace contains 11,167 frames in the 57 consecutive one-second windows from second 88 up to second 145. This stretch averaged 195.9 FPS. The 62 windows at or above target averaged 267.4 FPS. Frame timestamps agree across the two archived CSVs. Comparing adjacent simulation ticks separates frames that advanced the game from those rendered between updates.

| Mean wall time per frame | Windows at or above 240 | 57-second stretch |
| --- | ---: | ---: |
| Complete frame | 3.740 ms | 5.105 ms |
| Game rendering | 3.203 ms | 3.546 ms |
| Outside game rendering | 0.537 ms | 1.558 ms |
| Outside game rendering, between simulation ticks only | 0.486 ms | 1.549 ms |
| Buffer swap | 0.042 ms | 0.052 ms |
| Explicit graphics pacing wait | 0.004 ms | 0.003 ms |

The game advanced exactly 1,710 ticks during the stretch, maintaining 30 Hz. Of the 11,167 frames, 9,457 advanced no tick. On those frames the median time outside rendering rose from 0.304 to 1.664 ms. The historic trace timed the visibility-query copies, swap, pacing wait and query readback individually. It omitted the rest of presentation and the full simulation and throttle calls. Assigning the extra 1.021 ms outside rendering to a particular historical call requires evidence beyond that recording.

The follow-up Windows diagnostic timed `game_time_update`, `main_rasterizer_throttle`, `main_present_frame`, and the surrounding input, time, interpolation and render work in [the main loop](../../source/main/main.c). Inside [the desktop renderer](../../port/linux/src/d3d8_gl.c), it separately timed `visibility_send_batch`, backbuffer resolve, clear, blit, swap and pacing. GPU timestamps sampled one frame in 32 and were read at least eight samples later when available. GPU intervals overlap CPU work. The wall times below include any time spent waiting inside a driver call.

Each fresh process replayed the same checkpoint for 120 measured seconds. All four passed the camera and squad checks, observed sun shadows throughout and reached eight local slots. The final run added timers around the individual synchronisation calls.

| Renderer at 1024 | Average FPS | Slower minute FPS | Lowest second FPS | Seconds below 240 | Longest stretch | FPS over seconds 88–145 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Split distant Hard | 261.6 | 261.1 | 236 | 2 | 1 s | 260.8 |
| Original near Hard | 263.0 | 260.9 | 235 | 4 | 1 s | 261.3 |
| Split distant Soft | 268.6 | 268.2 | 244 | 0 | 0 s | 268.1 |
| Split distant Hard, individual fence timers | 266.6 | 266.3 | 237 | 2 | 1 s | 266.9 |

Across those same 57 seconds the new runs measured 0.189–0.197 ms per frame for simulation, 0.001–0.002 ms for the rasterizer throttle and 0.148–0.176 ms for complete presentation. The historical 57-second streak did not recur. In the two split Hard runs, game rendering took an additional 0.384 and 0.325 ms per frame in the below-target seconds compared with the seconds at or above 240. It accounted for most of those shorter drops.

A second comparison isolated late frames where rendering itself still fit comfortably inside 4.167 ms. It used only frames between simulation ticks with game rendering between 2.5 and 3.5 ms. Of these, 204 frames missed budget in split Hard, 317 in original Hard and 129 in split Soft. Submitting the visibility-query batch took 0.894, 0.949 and 0.844 ms respectively on those late frames. A visibility query counts how much of a draw reached the screen. The renderer copies the counts and inserts a GPU fence so it can tell when they are safe to read.

The final Hard run recorded 197 such late frames. They averaged 4.423 ms, including 3.050 ms rendering and 1.155 ms in presentation. Inserting the visibility fence with `glFenceSync` took 0.963 ms, compared with 0.134 ms on the 18,319 in-budget frames in the same render-time band. The explicit pacing wait on the late frames averaged 0.012 ms. Fence insertion measures time inside the driver and may expose work queued earlier. It does not establish that removing the call would recover that time.

**Decision.** Retain the scoped cache. The clearest repeatable cost outside rendering is visibility-fence submission, which both renderers share. The old recording cannot establish whether it incurred that cost throughout its 57-second streak. A focused next experiment would test sharing the existing frame-pacing fence where the two completion markers cover the same query writes. Compare complete frame times, query counts, result age, dropped batches and frame latency with the current renderer. Sustained 240 in Hard remains open.

## Sharing the frame-pacing fence

The Windows experiment kept the scoped tile cache enabled on both sides. In the control, [the desktop renderer](../../port/linux/src/d3d8_gl.c) inserted a fence after copying each frame’s visibility results, then inserted another after swapping the backbuffer for frame pacing. In the candidate, a nonempty mapped query batch and the pacing ring shared the post-swap fence. Both owners could release it independently. Pacing disabled and the diagnostic GL error-checking path retained the dedicated visibility fence. Direct per-query readback retained its existing path.

Each timing run used the same executable and snowy Xbox checkpoint, Hard or Soft 1024, all eight local slots, a 2560 × 1440 hidden window, uncapped rendering, and the default three frames in flight. Each fresh process warmed up for 25 seconds and measured 120 complete one-second windows. The second pair for each mode reversed the order. Result age counts frames from the batch’s submission to the CPU publishing its counts. Millisecond latency covers that same interval, weighted by the number of results in each batch. Every measured submission was matched by its source frame to its eventual delivery, including batches returned after the measurement window.

| Run in test order | Mean FPS | Slower minute FPS | Seconds below 240 | Longest stretch | P99 frame time | Results taking two frames | P99 visibility latency |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Hard, separate | 268.1 | 267.0 | 0 | 0 s | 5.56 ms | 0% | 6.30 ms |
| Hard, shared | 261.2 | 259.8 | 2 | 1 s | 7.16 ms | 6.52% | 9.45 ms |
| Soft, shared | 251.9 | 249.1 | 15 | 7 s | 7.57 ms | 12.62% | 10.22 ms |
| Soft, separate | 269.3 | 268.2 | 0 | 0 s | 5.79 ms | 0% | 6.50 ms |
| Hard, shared | 266.2 | 264.0 | 0 | 0 s | 7.25 ms | 1.97% | 9.22 ms |
| Hard, separate | 268.2 | 262.3 | 0 | 0 s | 6.71 ms | 0% | 7.29 ms |
| Soft, separate | 256.5 | 256.0 | 7 | 2 s | 6.91 ms | 0% | 7.55 ms |
| Soft, shared | 248.2 | 244.6 | 20 | 4 s | 8.32 ms | 8.11% | 10.69 ms |

All eight runs reached eight local shadow slots, with sunlight active throughout. All 5,966,190 measured queries were delivered. The separate fence published every result the next frame. With sharing, 63,852 Hard results and 152,219 Soft results took two frames. The later fence covers work submitted between copying the counts and completing the swap. Until it signals, the renderer continues using the previous counts. Mean CPU time inside buffer swap rose from 0.037–0.045 ms to 0.179–0.234 ms across the runs. Removing the earlier fence did not recover its measured cost from the complete frame.

Outside the timing runs, 206,493 published counts matched direct GPU query reads across Xbox Two Betrayals, CE Death Island, MCC Mercury Rising 2 and the Xbox fallback with frame pacing disabled. Separate ownership tests passed 51 cases covering delayed results, fence failure and cleanup. Four matched camera positions per mode on the Blood Gulch scope route had identical scene pixels. Six of the eight image pairs were identical throughout, and the other two differed by two pixels apiece within the flashlight battery HUD.

**Decision.** Keep the dedicated visibility fence and the scoped sun cache. Sharing reduced mean FPS and worsened 99th-percentile frame time in both modes and both test orders on the RTX 5070 Ti. Separate fences sustained at least 240 FPS in every one-second Hard window. Soft missed that threshold for seven seconds in its second control run. The original 57-second slowdown remains unattributed.

## Windows menu playtest

The portable Windows release build adds Distant Sun Shadows as an opt-in map-load setting. Save, Cancel, Defaults and restart persistence passed in the Modern, PC, Xbox and default desktop menus. A single Soft 1024 process loaded Xbox Blood Gulch, Xbox Sidewinder, CE Death Island, MCC Mercury Rising 2 and Blood Gulch again. The renderer prepared one outer pair on each load. Hard 1024 on Blood Gulch also prepared the pair. Separate launches with the distant setting at its default, or enabled with Sun Shadows off, prepared zero outer maps.

Near-only comparisons covered 240 synthetic 64 × 64 images, including Hard and Soft filtering, the near fade, close contacts and both depth directions. The maximum absolute channel difference from the original near shader was 0.00000003. Allocation-failure tests rejected each member of the outer pair in turn, accounted for partial releases and verified the near cache stayed valid. The timed Two Betrayals results above belong to the earlier qualification build.

## Blood Gulch terrain flicker

With 8× MSAA, an unobstructed hillside could blink darker during a strafe. The terrain receiver reconstructs a world position from one resolved depth per screen pixel. On a slope that depth can describe a sample off the pixel centre. Comparing it against the shadow map as a centre sample could put a triangle in its own shadow. The [terrain receiver](../../port/linux/src/modern_environment_gl.inl) now projects the half-pixel footprint onto the receiver plane and accounts for that uncertainty when comparing terrain against static geometry. Object contact shadows retain their independent depth comparison.

Soft filtering exposed a second error. Near a texel boundary, hardware rounding could select four adjacent depths displaced by one texel from the four depths predicted by the shader. The [sun filter](../../port/linux/src/modern_sun_shader.inl) now gathers at the centre of the chosen four texels and interpolates using the receiver’s original position.

On Windows the same Blood Gulch camera sweep ran at 2560 × 1440, 100° FOV and 8× MSAA. A diagnostic measured direct-sun visibility on an open patch of the hillside across 68 matched camera positions. Each column counts the percentage of 173,667 sun-facing terrain samples that lost more than 32/255 visibility. This is a measurement of that patch, alongside full-colour scene comparisons.

| Mode | Previous | Corrected |
| --- | ---: | ---: |
| Hard 1024 | 14.78% | 0.12% |
| Soft 1024 | 23.06% | 0.61% |
| Hard 2048 | 10.47% | 0.03% |
| Soft 2048 | 14.21% | 0.03% |

The [GPU regression](../../tools/test_environment_shadow_gpu.py) exercises off-centre depth on a known plane in both modes, with a raised static blocker and a thin object contact. A packaged Soft 1024 process also loaded Xbox Blood Gulch and Sidewinder, CE Death Island, MCC Mercury Rising 2 and Blood Gulch again. The default near-only launch allocated zero distant tiles. These runs checked visual correctness and map transitions. The combat frame-time measurements above were taken separately.
