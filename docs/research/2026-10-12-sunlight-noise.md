# Moving sunlight noise: evidence and capture

Status: high priority, locally reproduced and narrowed to directional virtual
shadow softening on October 13. The user supplied a clear CW screenshot, and
agent captures reproduce it. This supersedes the earlier third-party-only and
uncaptured-local statuses. Temporary rendering comparisons are restored; no
material or persistent configuration change has been made. A release workaround
still needs visual, performance and broader outfit coverage.

The user supplied Vistani's report of noise outdoors in direct sunlight during
movement, following installation of more CSS outfits and Shared Assets. The
reported driver, hardware-ray-tracing and denoiser experiments are not controlled
comparisons available to this session. They neither establish nor exclude a
cause here.

The [reported image](https://i.imgur.com/XiAVEbK.png) was retrieved and inspected.
It shows a blocky dark/light pattern across exposed thighs and an arm; the pale
garment does not show the same obvious pattern. This static image cannot verify
motion dependence or identify the rendering pass. Do not equate it with Peony's
previous broad dark/bright appearance concern.

The original image and URL/hash receipt are retained in
`work/three-outfit-rig-physics/sunlight-noise1`. Its SHA-256 is
`83a9718e75a65479701dd9097268bc1e838a021ca1758e39acfe0704633154b5`.

## Read-only live baseline

`tools/diagnostics/capture_reflection_settings.py` uses the existing CSSX
reflection bridge and UE's string console getter. It invokes no console setter,
appearance change or gameplay input. A deliberately nonexistent variable must
return an empty string; missing real variables are recorded as unavailable.

The first capture, `sunlight-noise1/settings.json`, completes with 20 available
variables. Two consecutive reads match. The player reference, process and CSS
customization also match before/after. These are this machine's values, not
Vistani's configuration, and are not synchronized to a screenshot.

| Console setting | Captured value |
| --- | --- |
| Reflection method / dynamic GI method | 1 / 1 |
| Ray tracing / Lumen hardware ray tracing | 1 / 1 |
| Ray-traced shadows / virtual shadows | 0 / 1 |
| Shadow denoiser / temporal accumulation | 2 / 1 |
| Directional virtual-shadow rays / samples per ray | 4 / 2 |
| Contact shadows | 1 |
| Lumen reflections allowed / temporal / bilateral | 1 / 1 / 1 |
| Reflection history maximum frames | 12 |
| Anti-aliasing method / screen percentage | 4 / 58.328 |
| Shadow / reflection / GI scalability | 2 / 2 / 2 |

Console values do not establish the final blended view, which GPU passes execute,
which upscaler is active, or whether the screenshot symptom is present. The
capture does not verify camera stability. There is no visual pass/fail result.

## Boundaries for the next comparison

After the motion relaunch, `settings-after-motion-relaunch.json` records the
same 20 console values and unchanged process/player/customization across the
read-only capture. `peony-world-baseline.jpg` was obtained through the existing
CSSX Steam screenshot bridge without gameplay or camera input. Its receipt is
`peony-world-baseline.json`. The rear three-quarter view beside outdoor stairs
does not show an obvious matching blocky pattern on the small visible skin
areas. It is a single image, not a matched moving/standing comparison, and was
not synchronized to the console capture. Together with the user's clarification,
this leaves the reporter's issue open rather than establishing a local fix.

Epic's [UE 5.6 virtual-shadow documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine?application_version=5.6)
describes sampling noise in soft shadow edges and its dependence on directional
ray counts. Its [Lumen documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine?application_version=5.6)
also describes a reflection-quality/noise/GPU-cost tradeoff. These establish
multiple rendering paths to distinguish, not a diagnosis from the image.

The retained UE 5.6.1 source `Renderer/Private/LightRendering.cpp`, lines 82-105,
defines shadow-denoiser value 2 as the global denoiser interface, which a plugin
may override. A captured value of 2 does not identify that implementation or
prove a broken denoiser. The local source is an API reference, not proof that
the shipped game uses an identical renderer.

Next obtain a repeatable moving/standing comparison under the same sunlight,
appearance and camera. Preserve the reference settings and original materials.
Once the symptom is reproducible, isolate skin/material, direct shadow and
reflection contributions one at a time. Compare native appearance and CSS
appearance, then package combinations if evidence warrants it. Restore each
temporary change and measure performance and visual costs before considering a
user-facing toggle. No speculative denoiser override is part of the prepared
Peony motion trial.

## October 13 local reproduction and isolation

The user's `/home/eins0fx/Pictures/screenshot-2026-10-13_00-12-14.png` matches
the supplied chat image. Its preserved copy is `user-local-CW-20261013.png`;
the adjacent JSON receipt includes the source hash and subsequent runtime status.
Blocky bright/dark patches cover CW's arms, thighs and clothing. Two passive
Steam captures also show the symptom, without camera or gameplay input.
Runtime identifies `eins0fx.commanderwhite/original`, `SK_Facial1`, Faithful
Genessa and the newly loaded pawn. This establishes a local occurrence on CW,
not an issue confined to Peony. It does not establish a common cause on every
outfit or prove the upstream game alone is responsible.

`settings-local-report-20261013.json` reads the same 20 renderer values as the
October 12 baseline, twice. Settings, customization, player and process are
unchanged across those reads. The user image and console reads are not atomic.

`tools/diagnostics/capture_render_isolation.py` records baseline, one temporary
change, and restored screenshots, with reflected signature checks, CVar
readback, camera/actor samples and restoration in `finally`. Each case ran
through the same live CSSX bridge. No input, camera, appearance, material or
animation change was sent. Iris Out continued, so animation phase differs.

| Evidence folder | Single temporary change | Inspected result |
| --- | --- | --- |
| `reflections-off1` | `r.Lumen.Reflections.Allow`: 1 to 0 | Blocky noise remains |
| `contact-off1` | `r.ContactShadows`: 1 to 0 | Blocky noise remains |
| `virtual-off1` | `r.Shadow.Virtual.Enable`: 1 to 0 | Pattern clears; returns after restoring 1 |
| `cache-off1` | `r.Shadow.Virtual.Cache`: 1 to 0 | Blocky noise remains |
| `adaptive-off1` | `r.Shadow.Virtual.SMRT.AdaptiveRayCount`: 1 to 0 | Noise remains, but camera changed, so not a matched comparison |
| `directional-hard1` | `r.Shadow.Virtual.SMRT.RayCountDirectional`: 4 to 0 | Noise clears into sharp foliage shadows; restores to noise at 4 |

All six cases finish with their original values restored and the same
customization, player and process. Camera/FOV and actor-position samples match
through five cases (roundoff below 3e-11 in the reflections case). The adaptive
case has camera movement and must not support a comparative quality claim.
The tool now also offers a local-light case, which has **not** been run.

The decisive directional case preserves virtual shadows and eliminates the
stochastic soft-shadow pattern, exposing sharp leaf-shaped shadows on skin,
clothing and ground. This narrows the responsible path, rather than establishing
why its sampling/temporal reconstruction is failing. The retained UE 5.6.1
`VirtualShadowMapArray.cpp` documents zero SMRT ray count as disabling that
tracing path. Epic's [virtual shadow documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine?application_version=5.6)
describes the soft-shadow sampling and noise tradeoff.

Do not ship `r.Shadow.Virtual.Enable=0` as the fix. The narrower directional
zero-ray result also changes softness visibly and is diagnostic evidence, not
an accepted default. Next compare native and custom appearances in the same
reproduction and distinguish sampling quality from temporal/velocity behavior.
Evaluate a narrower workaround's quality, cost and exact restoration before
adding a MISC option. The unrelated sidearm investigation retains its discovery
of the current Naylshotte owner in `sidearm-visibility1/discovery.json`.

## Original-model and sampling controls, October 13

The user requests a MISC bug-fix toggle, preferably default on, and explicitly
authorizes research into the game and engine implementation. The current
recommendation is default on only for a verified correction without a material
quality or performance regression. A quality/performance compromise remains
an optional, default-off workaround. No new renderer setting is shipped yet.

`capture_native_shadow_control.py` temporarily selects the discovered
`css.original_shells/genessa` catalog entry. Its available Iris Out override
is used for a moving-pose comparison. The original Genessa model also shows
speckling across its face and light-colored areas. Camera/FOV and pawn position
are unchanged. CW, its customization, idle override, process and byte-identical
saved state are restored. Receipt: `sunlight-noise1/native-genessa1/result.json`.
CSS and the installed packages remain active, and animation phases are not
matched. This rules against an issue unique to CW's materials, not every possible
CSS interaction. Do not call this a clean unmodded-game reproduction.

Additional one-variable controls use the same baseline/trial/restored screenshot
loop. Images were inspected; these are visual observations, not GPU benchmarks:

| Evidence folder | Change | Observation |
| --- | --- | --- |
| `directional-rays16` | Directional ray count 4 to 16 | Visible noise remains |
| `directional-samples8` | Samples per directional ray 2 to 8 | Fine grain remains; no accepted fix |
| `directional-dither0` | Directional texel dither 2 to 0 | Blocky noise remains |
| `directional-extrapolation0` | Directional slope extrapolation 5 to 0 | Noise remains |

All four cases restore their numerical settings and preserve player, process and
customization. All four preserve sampled camera/FOV and pawn position (roundoff
below 6e-11 in the extrapolation case).
The native-model control also preserves those samples. None controls animation
phase or proves a frame-time improvement.

The retained game export
`extensions/perf-tuner/work/research/ini/MortalShell2/Config/DefaultScalability.ini`
sets directional rays to 4 and samples per ray to 2 in `ShadowQuality@2`, matching
the live baseline. This is evidence that these particular low sampling values
come from the game's quality preset, not proof that they explain the entire
failure. The export is a retained extraction, not a new full game-dump audit.

Epic's [virtual-shadow documentation](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine)
describes SMRT sampling noise and the quality/cost tradeoff. The current web page
is UE 5.8; exact local semantics were checked against the retained UE 5.6.1
`VirtualShadowMapArray.cpp` and `VirtualShadowMapProjectionDirectional.ush`.
The shader uses per-frame random ray directions, texel offsets and finite depth
samples. Disabling texel dither alone did not resolve the captured symptom.

### Restoration limitation and implementation requirement

The diagnostic console bridge restores numerical values, not their original
console-variable priority. UE 5.6.1 `ConsoleManager.cpp` sets commands with
`ECVF_SetByConsole`, above scalability. Thus changing the game's quality menu
may not update a touched variable again until restart, even with its old number
restored. No persistent Engine.ini change was made. Existing claims of restored
settings mean read-back values only, not restored priority metadata.

A shipping MISC option must handle ownership and priority explicitly, respect
later user/other-mod changes, and restore on disable and unload. A restart is
needed before treating ordinary video-menu scalability changes as a fresh
control after these diagnostic commands. Next isolate temporal reconstruction
and native animation behavior rather than shipping the current hard-shadow
control or a blind sample-count increase.

## Source and dump review

Reviewed the retained UE 5.6.1 source, the extracted game configuration above,
and `MortalShell2/Binaries/Win64/ue4ss/CXXHeaderDump/Sparta.hpp`, `Engine.hpp`,
`DLSSBlueprint.hpp` and `DLSSBlueprint_enums.hpp`. The dump exposes types and
function declarations, not the game's private C++ implementation. Engine source
is a matching-version reference, not proof that the game has no custom patches.

| Evidence | What can go wrong | Consequence for a fix |
| --- | --- | --- |
| `VirtualShadowMapProjectionDirectional.ush`, `TraceDirectional` | Per-frame random rays, depth samples and overlapping occluders can produce unstable soft-shadow estimates | Preserve softness if possible; test the actual moving scene rather than assuming more rays is sufficient |
| Extracted `DefaultScalability.ini`, `ShadowQuality@2` | Four rays and two depth samples limit sampling; 512 VSM pages also constrain storage | Do not blindly force maximum settings; sampling tests have not cleared the artifact, and more pages need memory/cost evidence |
| `TemporalAA.cpp`, `GetMainTAAPassConfig` | A third-party temporal upscaler overrides the generic AA method | Detect the active path before applying TSR-only changes |
| `LightRendering.cpp`, ray-traced versus shadow-map branches | `r.Shadow.Denoiser` selects the ray-traced shadow denoiser, not a universal filter for SMRT | Do not present changing that selector or enabling ray reconstruction as a demonstrated VSM fix |
| `SkeletalMeshSceneProxy.cpp:136` | Incorrect previous-frame bone data can still defeat temporal reconstruction, even when velocity output is enabled | Inspect actual velocity/reprojection and compare ordinary native motion, not only a boolean or CSS Iris Out |
| `ConsoleManager.cpp:2988` | Console writes take priority over scalability settings | Restore ownership/priority and respect subsequent user choices when toggling a production fix |

The read-only `temporal-readback-20261013.json` verifies signatures before calling
the dumped DLSS library. It reports DLSS enabled, DLAA off, ray reconstruction
off, and DLSS mode 4 (Quality in the dumped enum). The generic AA setting remains
4 and temporal-upscaler selection 1. `bPerBoneMotionBlur` on the current pawn mesh
is true; the engine uses this to request velocity, which does not establish
correct previous-frame transforms. The empty FSR3/4 CVar responses mean unavailable
through that getter, not confirmed disabled implementations.

The game/live AA quality-2 settings disable TSR flicker analysis and reprojection
field, but this is not a reason to enable them blindly while DLSS reports active.
Epic's [temporal-upscaler documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-upscalers-in-unreal-engine)
describes motion-vector and reprojection diagnostics shared by these paths.

`renderer-temporal-control1` sets `r.TemporalAA.Upscaler` from 1 to 0 and back to 1.
UE 5.6.1 describes 0 as forcing the renderer's built-in temporal upscaler. Visible
noise remains in the trial and restored captures. Camera/FOV/pawn samples,
customization, player and process match. This control does not identify the final
per-view TAA/TSR pass with a GPU capture, and does not prove every DLSS interaction
absent, but bypassing the third-party selector did not clear the local symptom.

Next distinguish motion-history failure from SMRT's scene sampling limitations:
compare ordinary native idle/motion with CSS playback, inspect velocity and
reprojection if the shipping build includes those diagnostics, and measure any
candidate correction at unchanged resolution. Avoid material recolors, global
shadow removal and unrelated denoiser replacements as substitutes for that test.
The 20 original renderer values are confirmed unchanged in
`settings-after-native-sampling-20261013.json`; the later temporal selector also
restores numerically to 1 in its own receipt. No GPU cost measurement or universal
fix has been established.

## Online workaround review, October 13

The closest firsthand developer report is
[moving VSM artifacts on Epic's forum](https://forums.unrealengine.com/t/virtual-shadow-maps-for-high-quality-rendering-cinematics/1922741).
Its author reports success from disabling adaptive ray counts, and separately
from disabling backface culling for bright spots. Another user confirms the
adaptive workaround. This is evidence for their scenes, not a verified fix for
this game. Local follow-ups:

| Receipt folder | Change | Inspected result |
| --- | --- | --- |
| `adaptive-off2` | `r.Shadow.Virtual.SMRT.AdaptiveRayCount`: 1 to 0 to 1 | Visible blocky noise remains |
| `backface-culling-off1` | `r.Shadow.Virtual.CullBackfacingPixels`: 1 to 0 to 1 | Visible blocky noise remains |

Both receipts record exact equality of all six sampled camera/FOV/pawn views,
the same player/process/customization, successful trial readback and numerical
restoration. Animation phase remains uncontrolled. Neither supplies a GPU
benchmark. The console-priority limitation above still applies.

There are also firsthand [Expedition 33 shadow-grain reports](https://steamcommunity.com/app/1903340/discussions/0/592895271730506185/).
The thread includes different outcomes from post-process changes. It establishes
similar reported symptoms, not a common renderer defect or transferable fix.

[Epic's VSM documentation](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine)
explains the ray-count/noise tradeoff and zero-ray hard-shadow mode. That agrees
with the local directional control. It does not make a quality-reducing hard
shadow switch an unconditional correction. More rays and samples separately
have already failed to clear this reproduction.

[UE-290828](https://issues.unrealengine.com/issue/UE-290828) is a distinct 5.5/5.6
groom-related bug when samples per ray are zero, targeted for 5.7. Our positive
sample count and visible symptom do not match it. The diagnostic now rejects
zero directional samples before contacting the game. Ray count zero is a
different control. No samples-zero trial was performed.

### User-supplied article and Fab asset

Read [Prographers' shadow-noise article](https://prographers.com/blog/fixing-shadow-noise-in-unreal-engine-5-a-guideline-with-practical-tips-for-lumen-and-ray-tracing)
and checked its actionable suggestions against engine source and Epic docs:

- Its SMRT examples change `RayCountLocal` and `SamplesPerRayLocal`. The observed
  successful isolation here concerns directional sunlight; directional sampling
  variants have already been tried separately.
- Higher render resolution and temporal accumulation remain relevant diagnostic
  leads. Their GPU cost and active upscaler behavior need measurement.
- Its Nanite fallback and two-sided-geometry examples concern hardware
  ray-traced shadows. [Epic documents that fallback-mesh limitation](https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-ray-tracing-in-unreal-engine).
  It is not evidence that the current VSM symptom has that cause.
- The article spells the command `r.RayTracing.Shadows.Enabled.TwoSidedGeometry`.
  Local 5.6.1 `RayTracing/RayTracingShadows.cpp:51` registers
  `r.RayTracing.Shadows.EnableTwoSidedGeometry`. Do not copy the article's command
  literally or treat its mixed lighting suggestions as one preset.

The [Fab listing](https://www.fab.com/listings/84fda27a-c79f-49c9-8458-82401fb37cfb)
is Ultra Dynamic Sky, a developer sky/weather system with sun, moon and sky
lighting. It does not demonstrate a fix for this game's captured artifact.
Replacing the game's lighting system would introduce unrelated integration
work. No purchase, installation or asset replacement was performed.

One narrower source lead remains untested:
`VirtualShadowMaps/VirtualShadowMapProjection.cpp:48` defines
`r.Shadow.Virtual.SubsurfaceShadowMinSourceAngle` (default 5), which increases
effective directional source radius for eligible subsurface materials according
to opacity. Shader `VirtualShadowMapProjection.usf:866` applies this adjustment.
Inspect the actual material branch and live value before testing. Reducing it
would also change light diffusion, so even a positive result needs visual review
and cannot automatically become a default-on fix.

### Additional user-supplied forest-lighting article

Read the [Dre Dyson article](https://dredyson.com/lessons-from-how-come-some-games-are-seen-as-extremely-realistic-like-silent-breath-for-aaa-game-development-and-performance-the-definitive-complete-step-by-step-guide-to-resolve-shadow-and-lighting/).
Do not adopt its settings bundle. Several prescriptions conflict with the
documented renderer:

- Its instruction to set mesh Light Complexity to Surface confuses a
  [diagnostic viewport mode](https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-modes-in-unreal-engine)
  with a mesh lighting setting.
- Its baked-tree-shadow recommendation does not carry over to active Lumen GI:
  [Epic documents that precomputed static lighting is disabled](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine).
- Its blanket 512-pixel shadow-map explanation does not describe VSMs. Epic's
  VSM documentation specifies a 16k virtual map with allocated 128-pixel pages;
  the game's 512 physical-page budget is not a 512-pixel map resolution.
- Its contrast change is color grading. Local 5.6.1 `Scene.h:1441` and
  `PostProcessCombineLUTs.usf:112` place shadow contrast in tonal color
  correction, not shadow sampling or temporal denoising. Changing it may alter
  visibility of artifacts without correcting their cause.

The general advice to measure frame times and isolate variables is useful.
The article supplies no downloadable capture or reproducible project for its
performance claims. No new game settings were changed from this article.

## Subsurface and ordinary-idle controls, October 13

Live readback confirms `r.Shadow.Virtual.SubsurfaceShadowMinSourceAngle=5`.
`subsurface-angle0` changes 5 to 0 to 5. Visible noise remains on CW in Iris Out.
All sampled camera/FOV/pawn views match exactly; customization, player and
process match and the value restores. This rejects this single setting as a
fix for this reproduction, not every possible material interaction.

The source branch is narrower than a universal skin control:
`VirtualShadowMapTransmissionCommon.ush:15` loads legacy Subsurface,
Preintegrated Skin and Two Sided Foliage opacity, with a separate Substrate
branch. It does not apply the legacy adjustment to every skin shading model.
CW's authoring recipe uses shipped material parents and a skin SSS parameter;
the trial does not prove which GBuffer branch each visible pixel took.

`capture_native_shadow_control.py --native-idle` now compares the original
Genessa model with its default game idle. `--keep-outfit` runs the same idle
comparison on the current outfit. Both temporarily change the existing idle
choice and restore it, with no character or camera input.

- `native-genessa-idle1` stopped at an incorrect diagnostic assertion: transient
  `wear` defers disk saves by 30 minutes (`core.cpp:579`), so the state file still
  held the previous idle despite the acknowledged live change. The tool mistook
  that for a concurrent change. `recovery.json` confirms CW/settings restored,
  byte-identical saved state and unchanged player/process. No native crash.
- The corrected tool uses live request and animation status. In
  `native-genessa-idle2`, original Genessa looks much cleaner in the two inspected
  ordinary-idle captures than during the earlier Iris Out capture. This is a
  qualitative observation with different poses, exposed areas and materials,
  not proof that her rendering is artifact-free.
- `cw-native-idle1` retains CW and selects the ordinary game idle. Mottling remains
  visible on the exposed thigh and arm, although the large dancing-pose blotches
  are less prominent. CSS reports no engaged animation replacement, no shared
  idle, and no active facial performance. Thus Iris Out playback alone is not
  sufficient to explain the issue.

The two completed idle controls preserve every sampled camera/FOV/pawn view
exactly and restore the original outfit/customization/idle choices and
byte-identical saved state, in the same game process. CSS and its movement
choices remain installed, and neither control includes walking or combat.
These are not unmodded-game or matched-motion comparisons.

The source's standard velocity/reprojection/upscaler visualization show flags
are fixed off in Shipping (`Engine/Public/ShowFlagsValues.inl:132-138`). Do not
promise that the editor visualization commands will work in this game without
checking its custom build. A GPU capture or supported game diagnostic is still
needed to inspect actual previous-frame motion vectors. No renderer workaround
is accepted or installed; retain the softer-shadow target and investigate actual
render resolution/temporal history and material response before production work.
