# Shield Look

Video Setup > FOV and Viewmodels > SHIELD LOOK selects CLASSIC (the default),
CLASSIC WITH SPRINKLES, ENHANCED or ENHANCED HONEYCOMB, in that order. The former ENHANCED 2 profile
is now the sole ENHANCED look.
`display.shield_look = "enhanced"`, or `HALO_SHIELD_LOOK=enhanced`, selects
CMT Snow Grove's plasma pattern and first-person parameters with the approved
small outward body flare, overriding map-authored shield modifiers except custom colour. This is a local rendering feature in `port/linux/game/shield_look.c`.
It does not use the mod loader or change simulation, shield strength, damage,
network messages, map tags, models or cache files. No network-version change
is required.

CLASSIC leaves the established drawing paths intact. Arm Shields is
now authoritative for the added first-person pass in every SHIELD LOOK
mode: OFF adds no arm shield; ON supplies the stock displacement and its
distance/FOV-based cap. ENHANCED restyles that pass with the CMT pattern and
custom map colour (where present) while preserving its animation values and overshield handling.
CLASSIC preserves authored map shields. ENHANCED replaces authored shield
modifiers and omits their duplicate plasma model shells. Overshield Effect remains an
independent, opt-in visual modifier.

## Reference and provenance

The reference is the user-supplied `cmt_Snow_Grove.map`, 131301829 bytes,
SHA-256 `d65da3089cfaf8243f4288bc079326c1ab89a46f8e2f2bed974eb548e5fd8fed`.
It is a stock CE-format cache (609), not an OpenSauce implementation.
The user authorized using its assets. No CMT art, geometry or executable code
is redistributed by this feature: the inspected shaders use the stock
`characters\elite\bitmaps\plasma shield noise` texture. Its entire six-level
32x32x32 mip chain (37449 texels) is identical to the Xbox texture after
addressing and A8Y8-to-ARGB conversion. The existing map therefore supplies
the exact noise data through its existing texture loader.

The multiplayer unit is tag 1880, its body modifier tag 1935; the globals'
first-person hands are model 2157 with plasma material 2163. Both shader
noise references point to bitmap 1126, external CE resource 863. The original
CMT shaders use the following inspected values. The regression test compares
all rendering fields, allowing only the approved body displacement below:

| Parameter | Body | First-person hands |
| --- | --- | --- |
| Intensity source / exponent | B / 1 | B / 1 |
| Offset source / exponent | C / 2 | C / 2 |
| Offset amount | 0.035 | 0.009 |
| Perpendicular alpha / RGB | 1 / (0.909804, 0.772549, 0) | same |
| Parallel alpha / RGB | 1 / (0.937255, 0.694118, 0.0627451) | same |
| Color source | none | none |
| Primary period / direction | 2.1 / (2, 0, -0.5) | same |
| Primary scale | 18 | 45 |
| Secondary period / direction | 3.2 / (0, 2, 0.5) | same |
| Secondary scale | 14 | 62 |

The original CMT profile uses source C rather than B: CMT has two object
functions, and its unused C output is zero. ENHANCED uses the approved body
displacement below; first-person displacement follows the stock modifier
and Arm Shields cap. ENHANCED retains Arm Shields' animation-values pointer,
so its displacement cap and overshield handling apply to the enhanced
profile. CMT supplies duplicate arm parts for the plasma layer: all
68-byte vertices in its four shell parts (660, 574, 222 and 39 vertices)
are byte-identical to their underlying arm parts. The feature uses the
engine's modifier pass on the current map's hands.
This preserves that map's geometry and animation. Controlled render
comparisons on identical geometry are recorded below; the feature does not
replace another map's models to make their silhouettes match CMT's.

## Approved ENHANCED profile

ENHANCED now uses the approved former ENHANCED 2 appearance exactly. The
body displacement is outgoing hit intensity B, exponent 0.5 and amount
0.0125 world units (about 3.8 cm at full intensity). The CMT pattern, animation and brightness remain unchanged; custom authored hue can replace the preset gold.
The first-person offset now follows Arm Shields, as described below.
The former skin-tight body option and separate SHIELD LOOK submenu are gone.

Existing `"enhanced2"` config/environment values normalize to `"enhanced"`
at load, so playtest selections retain their appearance and show ENHANCED in
the menu. Both spellings also select the same stable shader in the feature.
There is only one enhanced body copy. CLASSIC still preserves stock behavior.
No shared-engine hook, map-family adapter, geometry mutation or simulation
change is needed; the existing renderer displaces vertices along normals.

## Map authority and isolation

CLASSIC returns the map's original shaders and submits its authored model
materials unchanged. ENHANCED (including migrated `enhanced2`) takes precedence
over authored shield modifier parameters and plasma shell materials on Xbox,
CE and MCC. Shader/function fingerprints and unrelated transparent model
materials no longer veto the selected appearance. Hit timing and the map's
intensity channel remain unchanged; appearance is applied at draw time.

The selected profile keeps the approved body parameters and the established
Arm Shields stand-off (0.03, exponent 2), with its existing distance/FOV cap.
Arm Shields OFF also suppresses authored plasma hand shells under ENHANCED;
CLASSIC keeps the original map behavior. Visors, generator lights, decals and
other non-plasma materials are retained, since transparency alone does not
identify a shield. No map tags or geometry are rewritten.

The exact stock/CMT noise is found among the loaded map's bitmap tags using
bounded reads and full-mip fingerprints: Xbox A8Y8 `f2f70788`, CE linear ARGB
`e7f72da4`, MCC packed ARGB `18ac1e94`. This prevents custom modifier noise
from changing the selected pattern. Each family retains its existing resource
loader; no foreign map or mod is loaded. Missing/malformed plasma data, no
matching noise resource, or exhausted profile capacity retains the original
shader safely. Other valid modifier types receive the same private plasma profile; the
animated overshield resolves its intensity channel from that selected profile.

Resource discovery and private shader copies reset before map load/unload.
CLASSIC bypasses discovery and overrides; selecting ENHANCED again reuses
stable per-map copies. Original map resources remain owned by their loaders.

## Authored colour inheritance

The feature-owned draw copies retain every approved profile field. A guarded
`shield_look_colors` call changes only the draw-local perpendicular and parallel
RGB constants immediately before the existing Overshield colour operation.
It identifies an eligible body/arms copy by pointer; CLASSIC, unrelated plasma,
non-plasma source modifiers and unregistered draws do not inherit colour.

Static authored RGB is supported, as are plasma colour sources 1 through 4
resolved from the current draw's object colours. The source RGB is multiplied
by that channel, then each nonblack endpoint is normalized to the corresponding
preset maximum RGB component. This preserves preset peak brightness rather
than importing authored opacity or intensity. A black endpoint uses the other
endpoint's hue; two black/unusable endpoints retain the preset. Missing or
invalid colour channels retain the preset. No texture pixels are sampled to
invent a colour for non-plasma or texture-baked effects.

Known stock cyborg and elite RGB pairs and the approved CMT pair (with no
colour channel) preserve preset gold. Comparison uses numeric colour data,
with a 0.00001 tolerance, not tag names or a map-name exception. Consequently,
a map that reuses the stock colour pair retains gold even if it customizes
other shader fields. The shader's other authoring does not imply a custom hue.
Future profiles registered in this feature can use the same colour resolver.

This draw-time hook is necessary for per-object colour channels: cached copies
are shared by instances of a biped definition, and mutating their RGB per unit
would leak colour between deferred or split-screen draws. The shared addition
contains no colour logic and uses existing validated runtime tags and draw
colours. Xbox/CE/MCC loaders, resource ownership and cleanup are unchanged.
The existing Overshield operation runs last and remains authoritative; Arm
Shields still controls visibility and displacement. No simulation or network
contract changes, so no network-version bump is needed.

## Necessary shared hooks

The existing renderer has no registration point for a private modifier
profile. Minimal `port:` additions supply the required integration:

- `render_objects.c`: substitute a private body shader at draw submission,
  after the existing modifier-type check. Animation values remain the unit's.
- `first_person_weapons.c`: supply the hands modifier between the weapon and
  hands draws; CLASSIC and ineligible maps follow the existing Arm Shields
  call. The weapon never receives the enhanced modifier.
- `models.c`: omit authored plasma shell parts belonging to a selected body
  or hands profile before their normal transparent submission. A modifier-only
  replacement cannot suppress these separately submitted model materials.
  CLASSIC and unrelated models/materials retain their existing paths.
- `cache_files.c`: clear eligibility and private shader/value copies before
  load and before unload. Failed loads and Xbox/CE/MCC transitions cannot
  reuse an earlier map's handles.

All eligibility, storage and parameter logic stays in the feature module.
Copies are never recycled within a map because deferred transparent draw
packets can retain their addresses. Arm animation values are separate for
each local player. Disabling takes effect on subsequent draw submissions;
the stable copies remain available for already queued packets until teardown.

## Verification history

The earlier authority-refusal results below describe the previous policy.
The current precedence policy supersedes those eligibility expectations.

`python -m pytest -q tools/test_shield_look.py` compiles the actual feature
module as 32-bit C89 with fake engine services. It covers CLASSIC/invalid
settings, exact profile fields, authored shaders/textures/functions/body and
hands shells/decals, toggling, split screen, reloads, capacity and failed reads.
It also checks generated menus and placement of the draw/lifecycle hooks.
Set `SHIELD_LOOK_REFERENCE_MAP` and `SHIELD_LOOK_STOCK_MAP` to the local CMT
and stock Xbox caches to additionally check exact CMT parameters, all noise
texels, and the actual feature reader's Xbox/CE/MCC-layout fingerprints.
`SHIELD_LOOK_CC` optionally selects a 32-bit compiler wrapper.

Validation on Windows (20 October 2026): 25 Shield Look tests and all 21
existing Arm Shields tests passed, including negative controls. The added
stock-light tests exercise the real generator-light definitions and pixels,
and reject altered materials, stages and textures. The harness used LLVM's
32-bit MSVC target; its existing Unix `-lm` argument was omitted by a local
Windows wrapper because the Microsoft runtime already supplies those calls.

Release, portable Windows, Android guest/host, and web targets built with
LLVM 23.1.4, Visual Studio Build Tools 2022 and Windows SDK, Android NDK r30,
and Emscripten 6.0.10. Windows used the normal project build. The Android and
web builds used local equivalents for generated Unix shell bootstrap/copy
commands and normalized response-file paths; no build-generator changes are
part of this feature. Cross-platform compilation is not device/browser
runtime validation.

Hidden Windows OpenGL runs used isolated data/save folders, at most two
instances, real scripted damage, and bounded copies of the supplied maps.
Original maps were never edited. Script-reference and spawn controls in the
copies made the hits and cameras reproducible; MCC fixtures preserved script
indices while making startup scripts dormant. A temporary diagnostic build
controlled randomness and recorded render-path selection. Its instrumentation
and random-seed change were removed before the final production builds.

The live tests found that the original eligibility filter rejected stock
Spartan generator lights. The exact material/stage/texture checks above fix
that without admitting arbitrary Chicago shield shells.

Visual equivalence was checked separately from eligibility:

- CMT's arm shell has identical vertices **and triangle strips** to the
  underlying hands. On controlled copies with identical geometry, lighting,
  hit timing and interpolation disabled, the original authored first-person
  shell and ENHANCED's modifier pass produced pixel-identical captures.
  Both comparison copies used the same opaque body-underlay control to
  exclude unrelated CMT weapon/lamp materials from eligibility.
- For the body, a temporary diagnostic alternated the original CMT shader
  and the feature's stock-based profile on the same frozen in-game scene.
  Twenty consecutive frame comparisons had zero differing pixels. This
  isolates the shader from different random idle animations or lighting
  phases between processes. The shield was visibly active in the captures.
- Normal runtime captures showed ENHANCED on stock Xbox shields. Render-path
  audits also confirmed both body and hands overrides on stock CE
  `deathisland` and MCC `BetterWithFriends/b30` shield chains.
- Authored magenta/scale variants of those Xbox, CE and MCC maps retained
  their original body shader pointers and received no enhanced arm pass.
  Unaltered CMT body/arm materials also received neither override. Their
  authored appearance remained visible in gameplay.

The visual claim concerns the shield shader, not replacing another map's
armor, hands, animations, lighting or camera with CMT's. Those remain the
map's own. The automated harness covers toggling, split-screen state,
failed reads, reloads and capacity. Live runs cover Xbox UI-to-CE/MCC loads
and shutdown; a full gameplay Xbox-to-CE-to-MCC transition chain, live
split-screen, Android hardware and browser gameplay were not exercised.
No gameplay/network structures were changed.

The final production Windows executable also passed the repository's
two-player network comparison on Beaver Creek, with real rendering,
CLASSIC versus ENHANCED, scripted damage/deaths and swapped host roles.
Both runs compared 22 seconds and passed the existing judge/tolerances
(562 and 660 prediction-drift samples). A local adapter supplied Windows
process handling and isolated file paths; the comparison logic was unchanged.
This does not cover browser transport or internet matchmaking.

ENHANCED 2 follow-up validation: 63 focused tests passed. Both enhanced
settings exercise authored-shield refusal, malformed data, reloads, toggles
and split-screen state. An additional real-module test compares the complete
first-person shader byte-for-byte, checks that only the three body displacement
fields differ, and verifies stable copies across all three settings. Release
Windows, Android guest/host and web builds passed. Hidden Windows OpenGL
runs on a bounded Beaver Creek fixture with scripted damage confirmed the
small body separation at 1280x960; ENHANCED was also captured for comparison.
The initial small offset was retained without increasing it. Existing Xbox,
CE and MCC eligibility checks are unchanged; the previous family-specific
live authority audits were not repeated for this displacement-only extension.
Android/browser runtime and live split-screen remain untested.

The ENHANCED 2 production build passed both host roles in the two-player
Beaver Creek network comparison against CLASSIC, with damage/death events
and real rendering (24 seconds compared per run, existing judge/tolerances).
No networking implementation or version changed.

Profile consolidation: 63 focused tests passed, including exact CMT rendering
fields with only the approved body displacement substituted, both saved
spellings selecting the same shader, unchanged arms, and removal of the
submenu from the generated screens and asset manifest. Earlier ENHANCED
references in this validation history describe the original CMT comparison;
the approved ENHANCED 2 appearance is now the current ENHANCED profile.

The consolidated profile built successfully for Windows, Android guest/host
and web. A hidden Windows OpenGL menu run at 1280x960 confirmed the relocated
row fits and an `enhanced2` environment value displays as ENHANCED. The shader
parameters are identical to the approved profile tested above; gameplay and
network runs were not repeated for this naming/menu consolidation.

## Arm Shields authority and offset

The approved third-person profile is unchanged. ENHANCED's first-person
shader now retains the validated stock modifier's offset source B, amount
0.03 and exponent 2 instead of CMT's unused C displacement. Its pattern,
colors, intensity and noise animation still use the CMT arm profile.

The existing `viewmodel_shield_apply` runs first and remains responsible for
activation, animation values, arm distance and the renderer's FOV-dependent
cap. `shield_look_arms` only replaces its shader on eligible stock maps,
leaving colors and the values pointer intact. OFF leaves no pass to replace.
The minimal `port:` hook reorder in `first_person_weapon_draw` is necessary
because Arm Shields owns the geometry-distance information and pass setup;
no new renderer hook or duplicate displacement calculation was introduced.
CLASSIC and authored-map fallback retain the original implementation.

Validation: 65 focused tests passed, including real Arm Shields renderer-cap
and negative-control tests, enabled/disabled enhanced arms, exact profile
fields and preserved values/color pointers. Windows, Android guest/host and
web release builds passed. Runtime checks and evidence are recorded below.

Hidden Windows OpenGL runs on the same bounded Beaver Creek fixture captured
the old skin-tight ENHANCED arms, the new ENHANCED arms with Arm Shields ON,
ENHANCED with Arm Shields OFF, and CLASSIC with Arm Shields ON at 1280x960.
OFF showed no added first-person shield during scripted hits. The shader
pattern remains CMT's, so apparent brightness differs from CLASSIC even
though the offset source, amount, exponent and cap are the same. Original
maps and saves were untouched. The first-person hook is shared by Xbox, CE
and MCC; the unchanged conservative eligibility cases pass in the harness.
Earlier family-specific live authority audits were not repeated for this
pass-order change. Android/browser gameplay and live split-screen were not
exercised; split-screen values and FOV-limit behavior have harness coverage.

The final production Windows executable passed both host roles in the
Beaver Creek two-player comparison with CLASSIC versus ENHANCED, Arm Shields
ON for both, scripted damage/deaths and real rendering (24 seconds compared
per run, unchanged judge/tolerances). No network protocol or version changed.

## Animated overshield layer colours

Visible Overshields remains independently optional. It now follows the
unit's current shield vitality: above 2.0 it is green (78, 253, 116), and
through 2.0 it is reddish-pink (253, 78, 116). The existing charging rise
remains active while charging; once charging is over, the persistent effect
ends at normal shield strength (1.0). Body and arm draws use the same
per-unit layer snapshot. First-person animation additionally requires Arm
Shields. Original shader brightness, animation speed and geometry are retained.

The layer selection and snapshot live entirely in `overshield_effect.c`.
No source hook, map-family adapter, network message or gameplay value changed.
66 focused tests passed, including actual effect-state refresh, exact 2.0
boundary, charge/drain direction, body/arm colour agreement, brightness,
expired/dead units and both setting guards. Windows, Android guest/host and
web release builds passed. Hidden Windows OpenGL runs used a real overshield
pickup in a bounded Beaver Creek fixture: logged shields charged through
1.97 to 2.97, then scripted damage reduced 2.91 to 1.29. Captures confirmed
green then reddish-pink on body and arms, and the effect ended when shields
were exhausted. Arm Shields OFF kept the arms clear; Visible Overshields
OFF also removed the persistent first-person effect at full charge. Original maps/saves
were untouched. Android/browser gameplay, CE/MCC live maps, live split-screen
and network sessions were not repeated for this visual-only colour change.

## Selected-look precedence (21 October 2026)

The current policy replaces the earlier authored-shield veto. CLASSIC is
still the default and returns the original shader. ENHANCED and its legacy
alias now replace valid authored modifiers; the original approved profile
parameters are unchanged. The material hook also omits plasma shells and
transparent parts using the authored modifier on registered biped/hands
models. Opaque surfaces and unrelated transparent materials stay intact.
The existing Arm Shields and animated Overshield controls retain ownership.

69 focused tests pass, including an exact byte comparison of the approved
rendering fields against the CMT reference, authored shader/noise/function
cases, non-plasma modifiers, CLASSIC restoration, shell suppression,
Arm Shields OFF, the actual overshield intensity-channel resolver, missing
resources, cache reset and capacity. Windows, Android guest/host and web
release builds pass. Only the FOV and Viewmodels menu help XML changed when
regenerated from the settings generator.

Windows OpenGL comparisons use the previous commit `657806db` and the new
build at 1280x960. Stock Beaver Creek CLASSIC/body, ENHANCED/body and
ENHANCED/arms retain their appearance. Disposable cyan-shield variants of
Xbox Beaver Creek and CE Death Island show the authored cyan look with
CLASSIC and the approved gold look with ENHANCED. CMT Snow Grove retains
its authored arm layer with CLASSIC; ENHANCED replaces it, and Arm Shields
OFF suppresses it. Frames come from separate runs, so animation phase is
not pixel-identical; profile equality is checked by the regression test.

The MCC BetterWithFriends/b30 fixture disables its introductory scripts and
uses a steady outgoing B function for the material comparison, because the
scripted damage cue did not reduce shields. This checks material precedence,
not MCC damage behavior. The previous ENHANCED path and the new CLASSIC
control retain cyan; the new ENHANCED path draws the approved gold profile.
Test-only script references must be aligned to the
map's virtual tag addresses, not file offsets. Rejected initial fixtures and
local-network launch failures are retained in diagnostic logs and are not
counted as successful comparisons. Both builds emitted MCC resource/audio
diagnostics; this is not a full campaign compatibility claim.

No original map or save was edited. Temporary runs are removed after capture,
and test saves are archived with verified hashes. Android/browser gameplay,
live split-screen, and two-machine networking were not repeated. Production
changes only affect local rendering and menu help, with no protocol or
simulation changes.


## Colour inheritance validation (2026-10-21)

Compared against `8a4c23c8` in hidden Windows release runs at 1280x960
(stock 640x480 rendering), with screenshots and logs retained locally in
`build/shield-colour/evidence`. No retail map or texture is committed.

- Coldsnap 20th Anniversary XL RC5: player `characters\scyborg\scyborg`,
  plasma `characters\ncyborg\shaders\nshield hit`, perpendicular white,
  parallel black, colour source 3. ENHANCED inherits the red team's hue on
  body and arms while retaining its pattern and layer. CLASSIC retains the
  map's original shield; Arm Shields OFF removes the enhanced arm pass.
- Mercury Falling (`mercury_falling.map`, the user-confirmed map): the
  player's `characters\cyborg\shaders\shield hit` has the stock
  orange/blue pair and colour source 0. Both the tag inspection and CLASSIC
  render confirm this. ENHANCED therefore retains gold on body and arms;
  there is no map-name-based blue override. Mercury 2's blue Spartan-boss
  shader is a different map/character and was not substituted.
- Xbox Beaver Creek: before/after ENHANCED retains the approved gold body
  appearance. The CMT reference test still compares all approved body and
  arm rendering fields byte-for-byte.

Test-only copies fixed spawn positions and added a small damage reference.
Mercury startup/continuous scripts were made dormant and its B function was
held steady for material comparison after scripted damage did not expose a
shield. Its colour fields, textures, models and profile parameters were not
changed. The original maps and saves were untouched. The original long
Mercury package path failed in local network-test setup; an isolated short
`mcc_maps/mr` package loaded successfully. Both reference and changed builds
emit existing Mercury sound/tag diagnostics, visible in the captures. These
runs establish the compared material behaviour, not full campaign support.

The 71 focused shield, Overshield and Arm Shields tests pass, including
fixed and per-object colour inheritance, tiny/invalid/unavailable colour
fallbacks, unchanged cached shader bytes, CLASSIC and unrelated-draw bypass,
map reset, existing toggle/split/capacity cases, and Overshield hook ordering.
Windows, Android guest/host target and web release builds pass. Android and
browser gameplay, live split-screen, two-machine co-op and full campaigns
were not rerun for this render-only change. No network or simulation data
changed. Screenshots are visual comparisons; differences in frame timing
mean they are not claimed to be pixel-identical.

Disposable map fixtures, per-run executables, caches and redundant captures were
removed. Test saves were archived losslessly with verified SHA-256 hashes;
selected screenshots and useful logs remain in the evidence directory.

## Depleted ENHANCED shield arcs

ENHANCED can use Bigass v3's original plasma-bolt sprite atlas while a living
biped's shield is depleted and its recharge stun timer is still running.
The effect stops on recharge, death or selection of CLASSIC. First-person
arcs require Arm Shields and its eligible arm pass; their outward placement
uses the existing Arm Shields offset cap. Normal shield shader parameters,
colour inheritance and animated Overshield rendering are unchanged.

The source asset is `altis\effects\bitmaps\plasma bolts`, referenced by
Bigass's `altis\effects\energy` particle. The original sequence has 31 frames
at 30 fps, 0.5-1 second lifetimes, 0.05 second fade-in and 0.15 second fade-out.
Two 512x512 DXT1 atlases, their original three mip levels and sprite bounds
are retained. The source map's cached flag is deliberately excluded from
our private texture ownership flags.

The emitter is an approximation of placement, not a copy of Bigass's object
attachments or scripts: up to six body sprites or two arm sprites select
non-root model bones independently. This covers head, limbs and feet where
those bones exist. Body radius is 0.05-0.1 world units, with smaller arm
sprites. Hashing object identity, time and slot supplies visual variation
without advancing simulation RNG. The same image sequence, blending and
lifetime ranges are used; exact original emission positions are not claimed.
The local CLASSIC Bigass reference did not show persistent arcs throughout
the waiting period in this port, so the comparison is not evidence of a
frame-for-frame original-emitter match.

### Local asset installation and provenance

The user supplied `bigass_v3.map` and authorized local asset extraction.
The atlas belongs to its original authors; its tag path identifies it as
an Altis effect. No third-party source code, SDK headers or map pixels are
committed or relicensed. Reproduce the local bundle with:

```sh
python tools/extract_shield_arcs.py /path/to/bigass_v3.map /data-root/shield_assets/bigass_arcs.bin
```

The extracted bundle is 345064 bytes, SHA-256
`cb28214c0e5b3dc35650abac199113c8332fdf9093f5c973de1d2ecc32cb3cbd`.
It is a fixed-format local asset file, independent of mods and map-family
resource discovery. Without it, ENHANCED retains its existing appearance
and logs one installation hint per map when an arc would be drawn. Invalid
files and texture allocation failures also retain normal shields.

### Integration and validation

`shield_arcs.inl` extends the existing shield-look feature module. The body
and first-person rendering hooks are needed to access each rendered model's
pose; feature code alone cannot receive those poses. The cache-unload hook
restores the previous tag table before the menu/MCC/mod table owners unwind,
after the texture cache closes. It also covers failed load cleanup. No
existing tag, model, shader, gameplay state or map-family loading path is
modified. The private bitmap tag is appended only on the first eligible
draw and released at the map boundary. Disabling ENHANCED stops draws
immediately; retained private resources are released on unload.

The Windows comparison uses hidden 1280x960 captures (640x480 stock render
resolution). Bigass body and first-person runs exercise depletion, the
waiting period and recharge; Arm Shields OFF removes the new arm effect.
Xbox Beaver Creek also exercises depletion and recharge. Mercury Falling
uses a disposable copy with dormant intro/continuous scripts and an aligned
script damage reference to its actual `elefant\innie\weapons\hose\bullet`
tag. MCC conversion restores tag names, so the temporary damage alias in
earlier attempts did not apply damage. The corrected test shows depleted
body arcs. Original map shaders, textures and models were not modified.
Useful failed-run logs are retained but are not counted as passing checks.

The 85 focused tests cover existing preset/colour/Overshield/Arm Shields
behaviour, depleted-state gates, malformed bundles, texture failure and
repeat load/unload ownership. Windows, Android guest/host and web release
builds pass. Android/browser gameplay, full campaigns and live split-screen
are not verified by these tests. Local captures and the comparison video
are retained in `build/shield-arcs/evidence`.

A hidden two-peer Xbox Beaver Creek run used ENHANCED on the host and
CLASSIC on the client with scripted hits. The repository's log comparator
passed 26 seconds at matching ticks: no state mismatches, no drift in 780
samples and zero client corrections. The initial attempt lacked the lobby's
stock map catalogue and did not host; its logs are retained separately.
This is a local networking smoke test, not an internet/WebRTC or full
co-op regression. No protocol or simulation data changed, so the network
version is unchanged.

Cleanup removed about 6.1 GiB of this assignment's disposable fixtures,
caches and redundant captures. The 558 test save/profile files were archived
losslessly with SHA-256 verification and original relative paths recorded.
About 18 MiB of evidence, logs and scripts remain; the current platform
builds and installed toolchains are retained. The local runtime bundle was
installed in the user's Gulch data root. No original maps or saves were
changed.


## Recharge layer

ENHANCED now reuses its approved plasma profile during actual normal shield
regeneration. The drawing envelope follows shield vitality, easing from 30%
of the existing offset to a subtle body peak 15% above the approved offset.
First-person growth remains within the existing Arm Shields offset/FOV cap.
The layer fades in over the first 6% of shield vitality and contracts/fades
for half a second after reaching full charge. Outside recharge, the approved
hit appearance is unchanged. CLASSIC and map-authored colour selection are
unchanged; animated Overshields retain their separate state and colours.

`shield_recharge.inl` owns all state and envelope calculations. It reads
shield charging, stun, vitality and death state without modifying simulation
or consuming gameplay randomness. Damage, depletion, death and Overshield
charging cancel it. Map reset, setting changes, time rewind, stale objects
and reused first-person pointers invalidate state. Full shields on initial
visibility do not trigger a completion pulse. Capacity exhaustion falls back
to ordinary rendering.

Minimal `port:` hooks register the selected body/arms modifier before model
submission, retain the transparent pass when ordinary hit intensity is zero,
and apply the envelope at the existing plasma draw. Feature code alone cannot
observe those draw packets or prevent their early rejection. Existing shader
values pointers remain intact for Arm Shields and Overshield ownership. No
map loading, conversion or MCC boundary changes are needed.

The 98 focused tests cover progression/retreat, cancellation, arm offset caps,
pointer reuse, capacity, stale frames and cleanup, alongside existing profile,
colour, Overshield and depleted-arc tests. Windows, Android guest/host and web
release builds pass. The Windows captures compare the preceding committed
build with recharge enabled, including Arm Shields OFF. Android/browser
runtime visuals and live split-screen have not been tested.

Windows in-game checks exercised recharge on Xbox Beaver Creek, CE Coldsnap
20th Anniversary XL and MCC Mercury Falling. Coldsnap retains its authored
red plasma colour; Mercury's starting player retains its existing gold
palette. Mercury uses the same isolated dormant-script fixture described
above. These are targeted visual checks, not full map/campaign validation.

A hidden ENHANCED-host/CLASSIC-client local network run passed the repository
comparator: 26 seconds at matching ticks, no mismatched seconds and no drift
across 780 samples. No protocol or simulation changes require a network
version bump. Internet/WebRTC transport was not retested. Captures, logs and
the before/after video are retained under `build/shield-recharge/evidence`.

The CLASSIC in-game control remains clear during recharge. Cleanup removed
about 8.7 GiB of temporary fixtures, duplicate binaries and raw captures.
All 310 generated save/profile files were archived losslessly with SHA-256
verification and their original relative paths recorded. About 27 MiB of
logs, evidence and scripts remain, alongside required platform builds and
toolchains. Original maps and user saves were untouched.


## Classic with Sprinkles

Video Setup > FOV and Viewmodels > SHIELD LOOK now also offers CLASSIC WITH
SPRINKLES (`display.shield_look = "classic_with_sprinkles"`, or
`HALO_SHIELD_LOOK=classic_with_sprinkles`). CLASSIC remains the default and
retains map-authored shaders. ENHANCED retains its approved CMT appearance.

The new preset uses the stock Xbox cyborg plasma surface: periods 2.2/3.0,
noise scales 10/6, primary direction (2, 0, -0.3), secondary direction
(0, 2, 0.5), and stock perpendicular/parallel RGB endpoints. Its rendering
fields match the local Beaver Creek reference byte-for-byte except for the
intentional body layer displacement. Both existing presets and the new one
resolve the same verified noise volume through the active map's loader.
No new texture bundle or mod-system dependency is introduced.

Body and arm offsets, depleted arcs, recharge/retreat and Overshield
precedence use the existing feature interfaces. Arm Shields remains the
owner of arm visibility and its distance/FOV cap. Authored custom colours
continue through the existing draw-local colour path. Preset style overrides
authored materials as ENHANCED does; CLASSIC keeps its original behaviour.
Cache entries include the preset identity, retaining stable shader copies
across option changes rather than mutating queued draws. Map cleanup and
bounded-capacity fallback remain unchanged. No new shared-engine hooks or
MCC loading changes are needed.

Validation: 100 focused tests, including stock-surface bytes, separate preset
pointers, custom colour inheritance, arm suppression and switching back to
unchanged ENHANCED/CLASSIC; Windows, Android guest/host and web builds.
Hidden Windows captures compare CLASSIC, ENHANCED and CLASSIC WITH SPRINKLES
on Xbox Beaver Creek and check Arm Shields ON/OFF. Coldsnap 20th retains its
custom red shield colour. The actual menu was checked for the complete label
at 1280x960; only this setting's selector was widened. The comparison video
was decoded successfully and retained in `build/shield-sprinkles/evidence`.

A two-peer local test with Sprinkles on the host and CLASSIC on the client
passed the comparator over 26 seconds: no mismatched seconds or measured
drift across 780 samples, with one client correction logged. No simulation
or protocol data changed. Android/browser runtime visuals, internet/WebRTC,
live split-screen and full campaign compatibility remain unverified.

Mercury Falling also reached gameplay and showed the new surface through
recharge and retreat. The starting player's existing stock palette was
retained. The first fixture was refused for a misaligned appended script
reference; the aligned copy passed without game-code or original-map edits.
Its failed-run log remains available separately. The fixture only makes
intro/continuous scripts dormant, fixes the test pose and adds a damage
reference; rendering tags and map colour data remain intact.

Cleanup removed about 4.0 GiB of temporary maps, duplicate binaries and raw
captures. All 403 generated save/profile files were archived with SHA-256
verification and original relative paths recorded. About 22 MiB of evidence,
logs and scripts remain, with required platform builds/toolchains retained.


### Sprinkles recharge visibility

The original comparison used independently captured frame numbers, which
made Sprinkles appear to complete recharge sooner. A synchronized diagnostic
capture now associates each image with the actual simulation time, recharge
progress, fade and displacement. At equal simulation times the two presets
already had the same half-second retreat and layer offsets, but the classic
surface's broad bands lost visible coverage faster as intensity decreased.

Sprinkles alone now uses `f * (1 + 0.75 * (1 - f))` for recharge visibility,
where `f` is the existing fade. This keeps the broader surface readable into
the retreat. The envelope is monotonic, starts/ends at zero and retains its
original peak. Recharge progress, full-charge transition, half-second end,
body displacement and Arm Shields distance/FOV limits remain unchanged.
ENHANCED still uses the original visibility formula exactly. This adjustment
changes neither preset's shader profile, texture parameters or RGB selection;
it applies only while the recharge envelope is active. Damage, depletion,
Overshield, map/settings cleanup and Arm Shields OFF still cancel/suppress
through their established gates.

The temporary diagnostic logging was removed from production sources.
The comparison video uses only matching recorded simulation times and
progress across all three columns (ENHANCED, Sprinkles before, Sprinkles
after), with missing capture intervals held equally. It includes body/arms,
a labelled one-third-speed retreat replay and an Arm Shields OFF control.
Distinct classic/CMT surface patterns are preserved; the comparison verifies
recharge behaviour and perceived fade rather than identical pattern pixels.

The 101 focused tests include a regression that checks the Sprinkles-only
visibility compensation, monotonic falloff, unchanged displacement, exact
retreat endpoint and arm suppression, alongside existing profile/colour,
Overshield and lifecycle checks. Windows, Android guest/host and web builds
pass. Android/browser runtime visuals and live split-screen are not tested.

Production Windows checks also passed recharge on CE Coldsnap 20th and MCC
Mercury Falling using the existing isolated fixtures, retaining their colour
selection. A Sprinkles-host/CLASSIC-client local network comparison passed
26 seconds with no mismatched seconds or drift over 780 samples; one client
correction was logged. Internet transport was not retested; no simulation
or protocol changes require a network version change.

Alignment evidence records 125 body and 123 arm sample times shared by all
three captures, with zero difference in recorded time or recharge progress.
The comparison video decodes successfully. Cleanup removed about 6.3 GiB of
disposable output; all 341 generated save/profile files were archived with
SHA-256 verification and relative paths. About 11 MiB of evidence, logs and
scripts remain under `build/shield-retreat`, with required platform builds
and toolchains retained. Original maps and user saves were untouched.


## ENHANCED HONEYCOMB

`display.shield_look = "honeycomb"` (or `HALO_SHIELD_LOOK=honeycomb`)
selects a newly authored procedural surface inspired by the user's Infinite
reference image. It is not an extracted Infinite shader or texture. The
hexagonal lines have broad luminous edges, moving bright patches, and a faint
connecting film. At the user's request the surface is thicker visually;
the approved ENHANCED stand-off from the armour is unchanged.

The immutable body/arms profiles retain ENHANCED's exact parameters. The
cache key additionally records honeycomb ownership so switching presets
cannot restyle queued draws or mutate another preset. The existing colour
inheritance, Arm Shields pass and cap, depleted Bigass arcs, recharge/retreat,
and separate Overshield control remain responsible for their own behaviour.
No mod, map-family loader, gameplay or network state changes are involved.

The current plasma noise cannot produce a structured hexagonal surface.
A minimal `port:` hook around the plasma draw therefore supplies and clears
draw-local surface uniforms and uses the same front-face culling as the
opaque model, restoring the plasma culling state afterwards.
`shield_honeycomb_gl.h` owns the GLSL pattern; `d3d8_gl.c` transports its
uniforms and `nv2a_psh.c` includes the extension. `nv2a_vsh.c` carries the
existing model UVs in one extra varying (zero for fixed-function draws).
This is necessary because the plasma outputs contain volume noise coordinates,
not the model UVs. One grid follows the skin through animation, without the
intersecting projections from the first prototype. Pixel derivatives soften
thin edges. Original UV island seams and texel-density differences can remain
on arbitrary custom models; this does not re-unwrap or replace map geometry.

The procedural pass uses the already resolved colour constants (including
map/object colours and Overshield precedence), the existing shield intensity
and height fade. Its colour is 65% perpendicular / 35% parallel; the structured
surface has its own visibility instead of the plasma noise's angle-dependent
cutoff. Scale zero returns the original fragment result unchanged, including
for all other presets and unrelated map materials. The state is cleared after
each plasma draw; no texture allocation or map-owned resource is introduced.

Validation: 126 focused shield/menu/harness checks pass, including exact
ENHANCED profile parameters, preset ownership, draw-uniform clearing and Arm
Shields suppression. Clean Windows, Android and web production builds pass.
Windows captures cover body/arms damage, depleted arcs, recharge and retreat,
an Arm Shields OFF control, CE Coldsnap 20th's red colour and MCC Mercury
Falling's starting-player gold. These are bounded isolated fixtures retaining
the maps' rendering assets, not full campaign or arbitrary-model validation.
The comparison contains 282 shared body times and 314 shared arm times with
zero recorded time difference. Weapon poses can differ between runs; matching
simulation time is not proof of pixel-identical animation. The video decodes
successfully and representative frames were inspected.

CLASSIC, ENHANCED and CLASSIC WITH SPRINKLES were also captured against the
previous renderer. Their profiles and disabled procedural path are unchanged;
the captures are not pixel-identical (maximum sampled whole-frame mean RGB
differences of 0.121, 0.309 and 1.262 respectively on a 0-255 scale). A local
two-peer HONEYCOMB-host/CLASSIC-client check passed 26 compared seconds,
778 samples without drift and zero client corrections. No network version
change is needed for draw-only state. Android/browser runtime visuals,
internet transport, live split-screen, a full Overshield pickup sequence and
runtime map-family transitions were not retested in this assignment.

Cleanup removed 32.9 GiB of temporary captures, test binaries, fixtures and
caches. All 992 generated save/profile files were archived with relative
paths and verified SHA-256 hashes before removing loose copies. About 37 MiB
of evidence, logs, scripts and archived saves remain in
`build/shield-honeycomb`; production builds and installed toolchains remain.
Original maps and user saves were untouched.

## Honeycomb link refinement

The regular UV hexagons remain the basis of the surface. Their linear size
is reduced by 20% on body and arms (scales 87.5 and 225), increasing the cell
count over the same area by about 56%. The shell displacement, ENHANCED
profiles, inherited RGB, recharge envelope and Arm Shields gates are unchanged.
This correction lives in the existing shield feature and its procedural GLSL;
it adds no shared-engine hooks or map-family compatibility changes.

The edge band is wider with a softer surrounding halo and a small, bounded
contribution from the existing plasma alpha. This gives the surface more
visual substance without moving it farther from the armour. The baseline
is deliberately subdued: a continuous spatial pulse brightens and dims both
links and their glow, rather than leaving a uniformly bright wire grid.
The original per-cell pulse could assign different brightness to opposite
sides of a shared link. Sampling a continuous field removes that discontinuity
while retaining visible variation. Filtering uses the unfolded UV footprint,
since derivatives of the folded edge distance can cancel at shared edges;
unresolved cells fade to their approximate average instead of sparkling.

These changes do not weld model geometry or re-unwrap UV islands. Separate
displaced pieces, UV seams and different texel densities remain visible on
some models. This is a refinement of the regular pattern, not a claim of a
seamless shell on every authored model.

The focused suite passes 164 checks, with the five optional asset checks
subsequently passing using the local stock, CMT and Bigass maps (169 unique
checks in total). The new regression executes the shipped pattern with fixed
pixel derivatives and samples both sides of all six shared edges over time,
checking continuity, a persistent dim link and a distinct pulse. It also
checks convergence when cells become subpixel. This CPU check does not replace
GPU visual testing. Clean Windows, Android and web production builds pass.

Windows body/arm captures compare 70 matched samples each, with zero recorded
differences in simulation time, intensity or displacement. A temporary hook
holds the existing full-hit intensity and displacement from 11 to 12 seconds
for maximum-flare inspection; the video labels this controlled interval and
also shows recharge and retreat. The hook is absent from production sources
and builds. The video decodes successfully. Matching time does not guarantee
identical weapon idle poses.

Clean-production Windows checks cover Arm Shields OFF, CE Coldsnap 20th's
authored red and MCC Mercury Falling's starting-player gold on the existing
bounded fixtures. A two-peer Honeycomb-host/CLASSIC-client check passed 26
compared seconds, 780 samples, no drift and no client corrections. No protocol
or simulation state changed, so no network version increment is required.
Android/browser runtime visuals, internet transport, full campaign play and
runtime map-family transitions were not retested. Other presets retain their
profiles and scale-zero bypass; their visual output was not recaptured in
this refinement.

Temporary captures, fixtures, binaries and generated caches were removed.
All 403 test save/profile files were archived with their relative paths and
verified SHA-256 hashes before removing loose copies. The comparison video,
diagnostic stills, logs and scripts remain in `build/honeycomb-refinement`;
production builds and installed toolchains remain available. Original maps
and user saves were untouched.
