# Peony V2 lighting response

Status: accepted for now by the user on October 12. After reopening the unchanged
build, they report "so far now its fine". Retain the current materials and resume
the remaining Peony checks. This is not evidence of a repaired cause or a
completed comparison across outfits. No lighting variant or diagnostic core was
installed. Keep the earlier observations below for later polish if they recur.
The accepted V2 fit, flowing trim, Iris Out facial playback/interruption and
current-form doubles remain accepted within the limits in the
[gameplay record](2026-10-12-peony-v2-gameplay.md).

## Evidence and first checks

The rear screenshot at 14:56:08 shows dark fabric under backlighting; the front
screenshot at 14:55:53 shows bright fabric and skin with reduced visible detail.
These are different views, not an exposure-controlled comparison. They establish
the user's appearance concern but do not establish its cause.

The V2 authoring recipe feeds ORM green to Roughness and blue to Metallic.
Base color imports use sRGB; masks and normals do not. The floral layer replaces
base color inside its mask, while roughness, metallic and the baked normal pass
through from the source textures. The recipe does not reconstruct every original
RoyalSilk layer. This limitation was already recorded in the material study.

An initial source-texture check on the default palette found upper floral-mask
texels averaging roughly 0.999 roughness and 0 metallic; lower floral-mask texels
averaged roughly 0.939 roughness and 0.0001 metallic. These are texture-space
samples, not UV-weighted measurements of visible triangles or a live GPU capture.
They do not support treating all the floral fabric as an accidental metal or
making an arbitrary global roughness increase.

`work/three-outfit-rig-physics/peony-v2-lighting1/source-samples.json` repeats
the check against the hash-verified manifest actually selected by
`peony-v2-materials2/material-input.json`. It selects each recipe's fabric-mask
channel, thresholds at 240/255 and uses nearest-neighbor resizing where the mask
and ORM resolutions differ. Under that sampling rule the upper means are
roughness 0.9987/metallic 0.0002, and lower means 0.9348/0.0005. These rounded
differences from the preliminary sample do not change the next diagnostic step.

`tools/eve-fit/audit_peony_lighting_normals.py` also checks the hash-pinned cooked
position, index and surface streams. `cooked-normals.json` in the same evidence
folder records all sections. The decoded triangle winding gives negative raw
dot products on both the accepted body and garment, so the report calibrates
its sign against Eve Body, Legs and Arms rather than assuming a winding convention.
Across 26,090 calf, thigh, jacket and corset triangles, no face-average normal
is strongly reversed relative to that body convention. Median alignment is
0.9873, 0.9980, 0.9955 and 0.9940 respectively; none of those sections has a
degenerate face in this calculation. This descriptive reference-pose check does
not validate tangent handedness, sampled normal maps or deformed normals.

The passive runtime snapshot after the report selects `black_white_floral`, with
Trim glow 17.25, Flow speed about 0.25 and Glow coverage 0. No fabric color override
appears in its customization values. This is not a synchronized capture of the
screenshots. Bright trim may affect the result, but that remains a hypothesis.

Epic's [UE 5.6 physically based material guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine?application_version=5.6)
distinguishes base color, roughness, metallic and specular. Roughness controls the
spread of reflections; a nonmetal uses zero metallic. Use those inputs according
to the surface rather than compensating through unrelated global settings.

## Comparison needed before a repair

Pin the same mesh, pose, palette, camera and exposure in a native UE comparison.
Render the installed graph under front light, backlight with weak fill, and soft
fill. Retain the lighting values, exposure and graph hashes with the images.
The screenshot symptom is not yet reproduced in such a controlled scene.

Test one change at a time:

1. Disable only trim emission. A large improvement implicates emission/bloom or
   exposure interaction; no change weakens that explanation.
2. Compare fixed and automatic exposure under the same lighting. Changes shared
   by skin and fabric suggest a scene/exposure contribution rather than a
   fabric-only defect.
3. Inspect the native base-color, roughness, metallic and normal buffers. Incorrect
   material values or normals must be corrected before adding a fabric lobe.
4. Compare the simplified fabric against a masked fabric-response candidate,
   preserving the original pattern and trim. Improvement must hold in both
   bright light and shadow before accepting an adaptation of the source layers.

No automated player/camera input, global lighting change, blanket fabric emission
or geometry refit is part of this initial investigation. Preserve working
stocking opacity and ghost controls if material parents change. A cook or shader
compile alone cannot close this visual issue. Record any candidate's controlled
comparison and subsequent native game result before calling it fixed.

Do not assume that selecting Cloth instead of Default Lit repairs the reported
shadow problem. Epic's [UE 5.6 shading-model documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/shading-models-in-unreal-engine?application_version=5.6)
describes Cloth's additional surface fuzz response. The local 5.6.1
`Engine/Shaders/Private/ShadingModels.ush`, `ClothBxDF` at line 730, still
multiplies direct diffuse and fuzz lighting by the incident-light cosine and
sets transmission to zero. That implementation is evidence against treating
the shading-model switch as an automatic source of shadow fill. It does not
rule out a measured sheen improvement or establish the game's indirect light.

## Controlled native capture setup

`tools/eve-fit/render_peony_lighting.py` uses the existing UE 5.6.1 preview-world
and render-flush helpers. The private stage is
`work/three-outfit-rig-physics/peony-v2-lighting1/native1`. Content and binaries
are linked to the existing character2 and clothgpu4 inputs, which the process
mounts read-only. No full content copy is made. `inputs.json` pins 172 assets and
libraries; `render-command.json` records the isolated offscreen invocation.

The initial matrix uses front and rear cameras, three directional-light pairs
(3/0.15, 0.15/3 and 1/1 lux relative to each camera), fixed manual exposure and
trim glow 0/17.25. Bloom is disabled to separate surface response from bloom.
The body and hair are hidden because their editor stand-ins cannot establish
game skin quality. The matrix tests the current garment materials in reference
pose, not the game's environment, skin shading, automatic exposure or DX12.
Shader compilation and capture completion must be inspected before using it as
visual evidence; staging alone is not a successful render.

The first run completed global shader compilation but failed before rendering:
UE Python exposes `disable_post_process_blueprint` as an editor property, not
the C++ setter name. `native1/attempt1` retains the exact failed script, log,
exit code 255 and empty capture directory. The corrected script sets the
property and reads it back. All four attempts are now terminal. Attempt 2
completed, but its display encoding and reversed view labels made it unsuitable
for the calibrated comparison. Attempt 3 corrected those details, but its gray
control was absent because the cube's render resources were not ready. Both
attempts retain their recipes and review notes under `native1`.

Attempt 4, retained at the stage root, exits 0 with 24 captures: front/rear,
three light pairs, glow off/on, each as a display PNG and linear HDR EXR.
All 172 pinned inputs remain unchanged. The cube is flushed after loading and
its transform is read back. `tools/eve-fit/check_peony_lighting.py` validates
image hashes, dimensions, finite pixels and the gray control. The control's
raw value is 0.180054 for an authored 0.18; its display patch is identical
across all lighting/glow cases within each view. The same checker rejects
attempt 3 with `Gray control is missing or incorrect`. `verified2.json` and
`receipt.json` record the current measurements and recipe hashes.

In a fixed corset crop with glow off, reducing the camera-facing light from
3 to 0.15 lux reduces mean HDR values from 0.4422 to 0.02231 at the front and
0.4493 to 0.02247 at the rear. That is approximately the same 20-fold reduction
as the light itself. The crop includes narrow trim, so glow adds brightness
at some pixels; no crop channel gets darker when glow is enabled in this
fixed-exposure, bloom-free fixture. This does not test the game's automatic
exposure or bloom interaction.

The comparison establishes a working capture fixture, not a repair. It does
not support blaming broad metallic assignment, reversed geometric normals or
direct darkening by the trim graph. Normal-map/tangent behavior and the game's
environment, exposure and skin response remain unverified. Epic's
[exposure documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine?application_version=5.6)
describes scene-dependent exposure and local highlight/shadow adjustment;
that is a reason to measure the game, not evidence that its settings are wrong.

A subsequent read-only `css_probe` player request timed out without a matching
acknowledgement, and the process check found no Mortal Shell or Unreal Editor
process. No live exposure measurement was obtained. No material, gameplay
setting or installed package was changed. Do not ask the user to repeat the
accepted fit/flow tests on this unchanged build. The remaining comparison must
address shadow and highlight detail together before selecting a material trial.

## Normal-map exclusion and source functions

`native1/flat_normal` repeats the 24-image matrix with the garment parents'
Normal inputs replaced by tangent-space (0, 0, 1). The parents and palette
instances are private duplicates; base color, roughness, metallic and emissive
connections are checked for preservation. The original 172 files remain
unchanged. Compilation, capture and the gray-control checks pass.

Removing the map changes the fixed corset crop's mean HDR brightness by only
about -2.8% at the front and -1.2% at the rear. The weak-fill view remains dark
and loses surface detail. Reject normal removal as a repair. This comparison
does not validate every tangent under animation, but the map is not responsible
for the broad weak-fill darkness in this fixture. `flat_normal/comparison.json`
pins the measurements, recipe and command. No variant was installed.

The inherited source chain was exported separately in `source-parent1`:
`MI_CH_P_EVE_33_Upper_Black` -> `MI_CH_P_EVE_33_Upper` -> `MA_CH_P_EVE_33`.
The master's cached function list references `MF_RoyalSilk`, `MF_ClothStandard`
and `MF_ClothShading`. Direct exports of those functions retain identifiers but
no expression graphs. `LoadedMaterialResources` is empty in the master export,
and no explicit shading-model property is emitted. These files establish the
dependencies, not the original shader equations or its effective shading model.
Do not infer an exact silk reconstruction from the parameter names alone.

The exporter commands, mapping path, raw JSON and logs are retained. The first
parent invocation omitted the mappings argument and exited -6; the corrected
parent, master and function exports exit 0. The 26 generated texture copies were
removed under the cleanup record after retaining their hashes; the original
palette exports and source containers are intact.

The next private comparison used UE's Cloth response, masked to the existing
fabric region, with base color supplying its fuzz color. It is an adaptation
experiment, not an exact RoyalSilk port. Normal, base color, roughness, metallic
and trim emission remain the baseline inputs. Three authoring attempts stopped
before images: the Saturate input is unnamed, and Python cannot read the
protected ClearCoat input even after resolving its reflected spelling.
`cloth-attempt1` through `cloth-attempt3` retain those terminal failures.

The private C++20 `CSSLighting` editor module exposes just the missing cloth
weight connection. Its UE 5.6.1 build succeeds, with BuildId 43139311 matching
the existing helper modules. The fixture now owns its binary directory, with
symlinks to the unchanged existing libraries and the new helper. The accepted
source project's binary directory is untouched. The native run checks rejection
of a null expression and an expression owned by a different material, then
reads back the valid connection. Normal and the four original surface/emission
connections are also checked for preservation.

Attempt 4 exits 0 and passes all 24 image/control checks. The fixed corset crop
gets approximately 13.5-13.6% brighter at the front and 16.8% at the rear, in
both strong and weak light. The weak-fill image remains nearly black and the
strong/weak brightness ratio remains approximately 20. Reject this as a
standalone repair: it does not recover shadow detail while preserving bright
detail. `native1/cloth/comparison.json` records the metrics and pins the helper,
project descriptor, module map, recipe and output hashes. No trial is installed.

The source-map and cloth-response comparisons have now answered their narrow
questions. Repeating them is not the next step. Obtain a comparison that includes
the game's environment and exposure, with the same view and appearance, before
selecting another shader change. The captured empty preview world cannot prove
the cause of the user's skin and fabric appearance in the actual level.

## Prepared runtime settings capture

The user is reopening the accepted build to compare Peony with another outfit
in the same location and camera direction. Their comparison is pending. No
material candidate or replacement core has been installed for this step.

The development bridge now has a read-only `get.member` query for individual
fields inside reflected structs. This avoids decoding all of the camera's
large post-process structure. Paths are limited to 12 fields and may descend
through structs only, with field and structure bounds checked at each step.
The C++23 Windows core builds successfully. It remains staged in
`work/three-outfit-rig-physics/peony-v2-lighting1/runtime-probe1/css_core.dll`,
SHA-256 `bb131280a2872047e731bf8fc2ef58b4468d25c5e580a06d866d8cc988b78db4`.
Live reflection behavior is not yet verified.

`tools/diagnostics/capture_lighting_settings.py` prepares a capture of camera
settings, up to eight post-process volumes, and selected project defaults.
It records missing fields and before/after camera, player and appearance
state. An unavailable camera field makes stability unknown, not passed.
These inputs do not establish volume containment, effective blending or the
final GPU exposure. The capture is not synchronized to a screenshot.

Python syntax and CLI checks pass. With the accepted game build running, the
staged core hash is rejected before any probe request is sent; the request
file remains unchanged and no capture is written. `runtime-probe1/receipt.json`
records these checks and the source hashes. Do not run a settings capture
until that diagnostic core is installed during a later agreed restart:

```sh
python3 tools/diagnostics/capture_lighting_settings.py \
  --core-sha256 bb131280a2872047e731bf8fc2ef58b4468d25c5e580a06d866d8cc988b78db4 \
  --output work/three-outfit-rig-physics/peony-v2-lighting1/runtime-probe1/live-settings.json
```

The accepted installed core remains
`94bb1d08bc7d6dd4181e21d4605a3b87cfb7f6046c90f35194f0e090291c423b`.
The comparison the user is doing now does not require the new diagnostic core.

The subsequent attempt to use the old `player` probe while the game was running
timed out after 20 seconds. Fresh status acknowledges that request ID and keeps
reporting Peony, but the probe output remains from October 11. The installed
DLL contains neither the `css_probe` command nor the `css-probe.json` output
path, both guarded by `CSS_INVENTORY_DEV` in the source. This build therefore
cannot serve that diagnostic request. Do not retry it expecting a camera
capture. `runtime-probe1/installed-probe-unavailable.json` records the IDs,
core hash and observations. No gameplay, camera or lighting setting was written.

The prepared capture now also reads console variables as strings before reading
their numeric values. UE 5.6.1 `KismetSystemLibrary.cpp` returns an empty string
when lookup fails, whereas the numeric helpers return zero. Missing variables
are recorded as unavailable instead of being interpreted as disabled settings.
The reads remain sequential. Unsupported reflected fields also make camera
stability unknown. The updated script passes syntax and wrong-core rejection
checks with no new request or output file; live capture remains unverified.

Before the user accepted the current lighting, a check of the separately loaded
CSSX 1.3.1 development core found another available transport. Its existing
`engine` request successfully returned the live Genessa player and controller.
CSS's missing development probe does not mean that all read-only game queries
require a restart. The CSSX bridge still has the same 256-field struct decode
limit, so this does not validate the prepared nested settings capture. No CSSX
binary, extension or setting was changed. Further lighting capture is deferred
following the user's acceptance above.
