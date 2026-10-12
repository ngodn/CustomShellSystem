# Peony V2 lighting response

Status: investigating the user's October 12 screenshots. No repair is installed.
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
property and reads it back. Attempt 2 reuses the same content links and cache;
it has reached Peony's material compilation. Its live handle and process IDs
are recorded in `native1/job.json`. No successful images or lighting repair
are claimed at this point.
