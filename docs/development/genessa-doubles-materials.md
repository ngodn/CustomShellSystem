# Doubles material support

October 4, 2026. Offline experiments, not installed runtime support.

The user requires Unholy Genessa, Eve and Commander White. Companion materials
belong to CSS. Outfit authoring sources and released containers stay unchanged.

## Genessa eye and smoke reconstruction, October 5

`native-eye-response.hlsl` reconstructs the two shipped eye materials before
the engine's fog/output stages. The opaque/unlit eye surface uses facing raised
to `Fresnel Power`, `Color`, `Intensity` and eye adaptation. The translucent
smoke uses UV0, vertex red for coverage and vertex green for noise phase. Three
red-channel samples are desaturated and raised to a UV-dependent exponent.
Scrolling uses the native period and speed.

Smoke literal texture index 1 addresses **T_noise_0082** in
`CachedExpressionData.ReferencedTextures`, not T_noise_0109 in the differently
ordered top-level list. Extraction receipt:
`work/genessa-doubles/eye-noise1/export-results.json`. The preview imports its
512-square PNG with sRGB and wrap addressing. Production must reference the
game's original texture and compressed mip chain. Samples use
`View.MaterialTextureMipBias`, confirmed by the native DXIL and UE 5.6.1
`HLSLMaterialTranslator.cpp` lines 7818-7823.

`check-native-eye-response.py` evaluates the required DXIL instructions from
hash-checked eye/smoke shaders independently of the HLSL reconstruction.
`work/genessa-doubles/native-eye-response2/result.json` passes 3,096 arithmetic
and scrolling-UV cases, including boundaries and negative time. Maximum
absolute error is 0.000005; DXC compilation also passes. Native shader hashes:

- Eye `376BF936FA4BD614F125AF91C1D196D4DA98589D-6`:
  `7318e1e8da3dc7bc5b489a7ec304e5ea`.
- Smoke `4AE4A640F38D9B7CDAC4A36B69E91638D9AB59FC-10`:
  `9f2fbcea9ceb66e8b7682e7b4c3a0a0a`.

All four private parents in `AstralEyes1` compile in UE 5.6.1 VULKAN_SM5 with
ready shader maps and no material errors (`native-eye-compiled.json`). The
`eye-render1` process exits zero and captures 40 HDR EXRs on SK_EveW3's actual
eye/smoke sections. `check-native-eye-renders.py` passes four kind/form groups:
native tint, custom color/intensity response, independently animated clocks,
partial fade, exact empty zero fade and smoke opacity zero. The comparison
image was visually inspected. This omits face occlusion and does not prove
native rendered equivalence, DX12 or live integration. Commander White's
different eye/refraction materials remain separate work.

Receipts and images are under
`/mnt/eins0fxE/CSS-work/genessa-doubles/material-outfits1/`, including
`native-eye-renders-1/hdr-check.json`. Protected sources remain unchanged.
Head placement and section visibility use the
[UE 5.6 skinned-component API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SkinnedMeshComponent?application_version=5.6).
No installed outfit or game state changed.

## Lit surface composition experiment

`create-ghost-surfaces.py` created seven additional private parents in
`material-outfits1`, under `AstralSurface1`. They retain the original base
color, metallic, specular, roughness, normal, ambient occlusion and world-position
offset connections. Original emission is added to the reconstructed native
ghost emission; coverage multiplies the native fade as in the earlier coverage
experiment. All seven source parents use Default Lit. The script rejects other
shading models except Unlit rather than changing their behavior implicitly.

The copies use translucent surface forward shading. This keeps per-pixel
lighting available for the retained surface inputs, as described in
[Epic's lit translucency documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/lit-translucency-in-unreal-engine?application_version=5.6).
It still needs visual and performance validation. Keeping graph connections
does not prove that an opaque source and translucent companion look identical.
The native ghost contribution also intentionally changes the final color.

Creation completed with exit zero on UE 5.6.1; the source hashes and retained
connections passed their checks. `ghost-surfaces.json` records the seven
parents, original emission nodes and available color/glow parameters. No
source material or installed outfit was changed.

The first render attempt (`surface-render1`) spent several minutes repeatedly
compiling intermediate duplicated graphs before any captures. It was stopped
deliberately, not reported as a shader failure or completed test. SIGTERM did
not finish the commandlet, so its editor and owned workers were killed; the
wrapper returned 137. No image acceptance result exists for that run.

`prepare-ghost-surfaces.py` now builds and saves the final comparison fixtures
under NullRHI before `render-ghost-surfaces.py` loads them. Preparation run 2
completed with exit zero, producing 57 interfaces for seven families. This
avoids editing and recompiling every intermediate graph during the render run.
The fixtures compare the lit source-only term, ghost-only term and their sum;
palette edits, authored glow, animation, half fade, zero fade and removed
geometry are separate states. `check-ghost-surfaces.py` reads half-float EXRs,
checks file hashes and requires complete groups. Full characters, DX12, body
and eye adaptation, live ownership and summon performance are outside this
fixture's scope.

Completed run 6 (`ghost-surface-renders-6`) contains 156 EXRs. All 14 groups
pass the composition and control checks, including palette edits, authored
glow response, animated effects, partial fade and zero matching removed
geometry. Fabric visibility and half-opacity checks also pass. The final
tint test fails all seven Stray groups and Commander White's Faithful hair.
The additive lit composition is rejected for production. This test is not a
native-render equivalence check, and preserved input connections alone were
insufficient evidence of the required visual result.

The fabric fixture uses the authored garment and its actual vertex masks.
The engine cube's white vertex colors mask out the cloth pass, so the earlier
run 2 gave two empty fabric groups. Run 3 stopped on a missing garment asset;
read-only source symlinks and a SHA-256 receipt now supply its dependencies.
Run 4 rendered successfully. Its cloth-glow expectation was wrong: the cloth
pass excludes the glowing metal. Run 6 compares glow changes against a separate
authored source-glow render, including the legitimate zero cloth response.

Run 5 stalled waiting for GPU completion. Its isolated editor backtrace is
`surface-render5-backtrace.txt`: the render thread waits in Vulkan submission,
the game thread waits for a render fence, and the RHI thread is idle. Shader
compilation had finished. After TERM failed to finish, the editor was killed;
that run returned 137 and its incomplete captures are not accepted. Run 6
used the diagnostic-only `CSS_ASTRAL_SERIAL_RHI=1` switch (`-norhithread`) and
completed with exit zero. One successful run does not establish the cause of
the stall or a general driver fix. No game or desktop setting was changed.

## Native-filtered source detail candidate

After rejecting lit addition, the private `native_filter` variant uses the
original base-color and emissive graphs to modulate reconstructed native ghost
RGB. Its opacity is still original coverage times native alpha. For nonnegative
source emission `E`, its detail factor is:

```text
factor = 0.5 * (1 + saturate(base_color) + max(E.r, E.g, E.b) + E)
emission = native_ghost_rgb * factor
```

White with zero emission leaves native RGB unchanged. Black has half its
brightness. The source color and glow can still change the result, with a
maximum 2:1 channel ratio in this factor. That bound is not a proof of native
hue equivalence at every ramp value. The check evaluates mean tint for the
tested baseline, red/blue palettes and glow states, not every possible pixel
or user setting. The native noise, fade and timing kernel is unchanged.

Creation (`filtered-create1`) and fixture preparation (`filtered-prepare1`)
both exited zero. Render `filtered-render1`, using the diagnostic serial-RHI
flag, exited zero and captured 156 EXRs under
`material-outfits1/ghost-surface-renders-filtered1`. All 14 groups pass both
composition and tint checks. Original sources and the native kernel retain
their protected hashes. No installed files changed.

For the product check, the independent captures contain `G*a`, `F*a`, and
`100*a`; the checker compares `(G*F*a)*(100*a)` with `(G*a)*(F*a)*100`, allowing
half-float rounding. This avoids dividing by almost-zero coverage. It is valid
for the single surface fixtures and constant-detail overlapping cloth here,
not arbitrary overlapping surfaces with different detail factors. The glow
check applies the same relation to the independently rendered source change.
Visibility, holes, partial fade, zero versus removed, fabric visibility and
fabric half-opacity also pass.

`comparison.png` in that directory was visually inspected. It uses identical
exposure and display mapping for the ghost-only reference, rejected lit sum
and filtered candidate, showing metal, authored cloth and Commander White
hair in both forms. Its "native" column is the reconstructed ghost-only
fixture, not an image of the running game's original double. The filtered
images retain blue/red where the lit images become white or brown.

This is a candidate, not a complete adapter. Unlit shading intentionally does
not reproduce the original lit roughness, metallic, specular or normal response.
Required full-character detail/customization fidelity still needs evaluation;
the stored graph connections do not establish that response. Native body and
eye families, actual hair geometry, arbitrary overlapping surfaces, DX12,
runtime ownership and performance remain pending.

Reproduce in the existing isolated stage, using an unused fixture label:

```sh
CSS_ASTRAL_COMPOSITION=native_filter CSS_ASTRAL_SURFACE_RUN=filtered2 \
  bash tools/diagnostics/genessa-doubles/run-material-test.sh \
  "$engine" "$stage" surface-prepare filtered-prepare2
CSS_ASTRAL_SERIAL_RHI=1 CSS_ASTRAL_SURFACE_RUN=filtered2 \
  bash tools/diagnostics/genessa-doubles/run-material-test.sh \
  "$engine" "$stage" surface-render filtered-render2
python3 tools/diagnostics/genessa-doubles/check-ghost-surfaces.py \
  "$stage/ghost-surface-renders-filtered2"
```

The existing seven parents are recorded in `ghost-filtered.json`. On a new
staged project, create them first with `CSS_ASTRAL_COMPOSITION=native_filter`
and runner mode `surface-create`. Creation refuses to overwrite them.

## Released asset audit

### Effective inheritance audit

`tools/diagnostics/genessa-doubles/resolve-material-state.py` resolves the cooked
root defaults and enabled instance overrides. Stored values with disabled
override flags do not change inheritance. The resolver preserves explicit zero
and false values and parameter association/index, and records each value's
source. It rejects missing defaults, conflicting readbacks and parent cycles.
Material-layer structures are retained, not flattened. Live MID overrides still
need copying at summon time.

The current seven-mesh Eve/Commander White inventory resolves 102 distinct
interfaces and 240 material/overlay references:

| Family | Effective mode | Interfaces | Slot references |
| --- | --- | ---: | ---: |
| Native `M_Uber` | Opaque | 24 | 86 |
| Native `M_Uber` | Masked | 67 | 133 |
| Native `M_refraction_01` | Translucent | 3 | 3 |
| Eve hair | Translucent | 2 | 12 |
| Commander White hair | Translucent | 6 | 6 |

Both native Uber groups use `Opacity from Color Map=true`,
`Use Opacity Dither=false`, and `USE VIRTUAL TEXTURES=false`; `USE OPACITY`
distinguishes the groups. These switches describe this inventory, not all
native appearances. Do not select an opaque companion by root name alone.

Reproduce from the existing readbacks, choosing a new output filename:

```sh
python3 tools/diagnostics/genessa-doubles/resolve-material-state.py \
  work/genessa-doubles/eve-cw-inventory.json \
  work/genessa-doubles/effective-materials-01.json \
  work/genessa-doubles/eve-cw-materials.json \
  work/genessa-doubles/eve-cw-parents.json \
  work/genessa-doubles/eve-cw-roots.json
python3 tests/astral_material_state_test.py
```

All seven inheritance tests pass. They cover disabled base overrides, omitted
constructor values, false static overrides, zero values, layer identity,
shading-model inheritance, the translucent enum alias and invalid ancestry or
default counts. They do not test rendering or runtime reflection.

UE 5.6.1 source references: `Material.cpp:SetInitialValues`,
`MaterialShared.cpp:FMaterialInstanceBasePropertyOverrides`,
`MaterialInstance.cpp:UpdateOverridableBaseProperties`, and
`MaterialTypes.h:EMaterialParameterType`. The material's clip default is
0.3333; the override struct's constructor uses 0.333333. The resolver preserves
that distinction. `MSM_FromMaterialExpression` does not replace an inherited
instance shading model. `BLEND_TranslucentGreyTransmittance` aliases Translucent.

### Native surface shader extraction

October 5: texture registers are now resolved from each shader's serialized
`FShaderResourceTable`, matched by layout hash to the material uniform
expression set. `read-material-bindings.py` verifies extraction/container
hashes, bounds, token offsets, active buffer bits and a byte-identical header
round trip. All five extracted SM6 shaders parsed successfully, yielding 13
material texture bindings. Seven parser tests cover a known binding, a
single-byte register change, truncated headers, negative counts, malformed
streams, inactive buffers and wrong container boundaries.

| Shader | Material buffer | Confirmed material texture registers |
| --- | --- | --- |
| Opaque ShellKeeper body | 2 | t4 `NormalMap non VT`, t5 literal index 25, t6 `BaseColorMap  non VT`, t7 `BRM non VT`, t8 literal cube index 10 |
| Masked Eve eye mesh | 3 | Same t4-t7, t8 literal index 23, t9 literal cube index 10 |
| Native eye refraction | 4 | t19 `ReflectionMap` (cube) |
| Genessa eyes | 1 | No material texture bindings in this shader |
| Genessa eye smoke | 2 | t3 literal texture index 1 |

The masked shader's `%4` handle is t6. Its `%253` annotation feeds sample
`%255`, whose alpha is `%259`. This closes the earlier identity gap: the
discard calculation uses the actual `BaseColorMap  non VT` alpha. Preserve
the parameter's two spaces when matching or copying values.

Texture indices resolve through `CachedExpressionData.ReferencedTextures`,
not the material export's top-level `ReferencedTextures` list. They differ
in this native root. The cached Uber list resolves literal 25 to `T_Sphere_A`,
23 to `/Engine/MaterialTemplates/Textures/T_Noise01`, and 10 to the cube render
target `RT_MetallicReflectionBoost`. The top-level list would misidentify
index 10 as a 2D blood texture. Named parameters still use effective live
overrides before that fallback; this table is not a replacement for copying
the currently bound textures.

Exact UE 5.6.1 source references: `RHIResources.h:788` (serialization),
`RHIDefinitions.h:716` (8/16/8-bit token layout),
`ShaderCompilerCommon.cpp:51` (offset table),
`MaterialUniformExpressions.cpp:473` (texture/sampler member order),
`MaterialUniformExpressions.h:431` and `MaterialInterface.cpp:360` (cached
texture lookup). Raw reports are `work/genessa-doubles/opaque-bindings1.json`
and `surface-bindings1.json`. The script handles standard 2D/cube groups only
and rejects unsupported texture layouts. This establishes binding identities,
not a complete body/eye graph or rendered equivalence.

Five additional SM6 base-pass pixel shaders were extracted successfully and
disassembled with DXC. The extraction selects `TGPUSkinVertexFactoryDefault`
and `TBasePassPSFNoLightMapPolicy` from the full readbacks. The selected JSON
files are extraction inputs, not complete shader-map representations.

| Material | Map hash | Resource |
| --- | --- | ---: |
| Eve `MI_ShellKeeper_Hair_01` (masked Uber) | `B0E0C59654BAAF957577C5C003F6827A6912E6AA` | 184 |
| Native `MI_ShellKeeper_Body_03` (opaque Uber) | `B5F422B6BCDF7BD56CBC9686FE62EC52C87BF82D` | 109 |
| `M_refraction_01` | `2CF1A2C5FF20EC86B9FBCE98AEA8D1AC7F06C2A3` | 6 |
| `M_Genessa_Eyes` | `376BF936FA4BD614F125AF91C1D196D4DA98589D` | 6 |
| `M_Genessa_Eyes_Smoke` | `4AE4A640F38D9B7CDAC4A36B69E91638D9AB59FC` | 10 |

Local evidence: `work/genessa-doubles/surface-shader-maps.json`,
`opaque-shader-maps.json`, `surface-bytecode1/manifest.json` and
`opaque-bytecode1/manifest.json`. The manifests retain container provenance and
SHA-256 hashes; adjacent `.dxbc.ll` files contain disassembly. Commander White's
`MI_Layer_00` has no own shader map, so the opaque extraction uses its native
parent rather than treating an absent map as unsupported rendering.

In the masked shader, SSA `%259` is sampled alpha. `%621` takes its replicated
RGBA dot product with `OpacityMask_Channel`, `%624` multiplies by
`Opacity Strength`, and `%625` subtracts the compiled clip threshold. `%626`
discards when negative. Preshader fields at float offsets 20 and 24 map directly
to numeric parameters 15 and 14 through opcode bytes `03 0f 00` and
`03 0e 00`. Both one-hot channel choices in this inventory therefore use alpha,
not the selected RGB texture channel. The October 5 binding trace above now
identifies this as the base-color texture; a rendered companion comparison
still needs verification. The opaque sample has
no discard instruction.

The refraction shader saturates its `Opacity` scalar for ordinary output alpha.
This alone does not prove that setting opacity to zero removes all rendering:
reflection/emissive color and the separate distortion path still need tracing.
Do not drop Commander White's eye layers based only on their zero opacity.
Genessa's two eye shaders are extracted but their expressions remain untraced.

No game files, outfit assets or installed runtime were changed by this audit.

The intended delivery is CSS runtime code plus CSS-owned cooked companion
materials, rather than an update to each outfit. Individual outfit changes are
conditional on evidence that the runtime and companions cannot handle an asset.
No outfit update or installed material replacement has been made for this work.

Release packaging correction, October 4: the user wants **two separate ZIPs**,
one for the CSS runtime and one for the cooked shared assets. Runtime stays in
`Binaries/Win64/ue4ss/Mods/CustomShellSystem/`; shared assets go in
`Content/Paks/~mods/`, relative to `MortalShell2/`. This supersedes the earlier
proposal to include both in one ZIP. The shared package name and version are
not finalized. Record compatibility and update requirements in the install
notes. Individual outfits should not duplicate the shared package.

Read back Eve v1.4.0's six meshes and Commander White v0.0.6-dev's
`SK_CommanderC4` from their release containers, alongside the game's global and
base containers. Resolve each material's entire parent chain rather than
classifying by filenames such as `MI_ShellKeeper_Hair_01`, which is also used
for clothing and eyes.

| Root graph | Slot references across seven meshes | Required treatment |
| --- | ---: | --- |
| Native UberShaderV2/M_Uber | 219 | Skin, clothing and masked surfaces; inspect instance switches and overrides |
| Native Eyes/Shared/M_refraction_01 | 3 | Eye surface, opacity and refraction |
| CSS/EveHair/M_Hair1 | 12 | Two current hair instances shared across six outfits |
| CSS/CommanderWhite/MaterialY/M_Hair | 6 | Six distinct hair layers |

These are authored slots, including parts that may be hidden. They are not draw
counts. The inventory resolves 102 directly referenced interfaces and 112 total
interfaces including parents. Runtime customization can override textures and
parameters, so the cooked defaults are only the starting point.

Root identity is insufficient for a native Uber adapter. Many descendants
override blend mode to Masked; body instances override it to Opaque. Reparenting
an unchanged instance can therefore override a companion's Translucent mode.
Resolve effective instance properties and preserve the original coverage while
deliberately setting the companion's fade permutation. Do not interpret a
root's `UseGhost` switch as proof that a descendant compiled that branch.

## Hair graphs

Both new hair graphs use `BaseColorMap  non VT` (two spaces), `BRM non VT` and
`NormalMap non VT`. Base-color alpha supplies continuous opacity. There is no
threshold or density multiplier to add.

| Property | Eve v1.4.0 | Commander White v0.0.6-dev |
| --- | --- | --- |
| BRM red | Blood mask, not connected to AO | Connected to ambient occlusion |
| Specular default | 0.5 | 1.0 |
| BRM green / blue | Roughness / metallic | Roughness / metallic |
| Normal input | Normal texture RGB | Normal texture RGB |

Use separate companions for these current graphs. Copying one shader over both
would alter shading. Later consolidation needs explicit control over AO and
defaults, with equivalent renders first.

`stage-materials.py` accepts the exact source receipts from Eve's
`work/hair-v14-material2` and Commander White's `work/cw270/runtime5`. It checks
every copied package against those hashes. `create-fade-materials.py` duplicates
the original graphs and multiplies existing opacity by `CSS_AstralOpacity`.
The combined stage contains 57 unchanged source packages and seven companions,
including Unholy Genessa's five garment parents. Creation completed on UE
5.6.1 CL44394996 with all source hashes intact.

## Fade evidence

Unholy Genessa's ten original/full/half/zero/removed renders completed with exit
code zero under Vulkan. Zero-opacity garments are pixel-identical to removed
garments in both views. Full and half opacity differ. Full opacity differs from
the original render too, so this establishes fade behavior, not visual identity
or native ghost-effect fidelity. Shader-map checks passed; source hashes match.

The body stayed as a fixed reference and native eyes were excluded. Neither
received ghost support in this test. Native blue/red shading, DX12 cooking,
runtime parameter transfer, pooled doubles and performance remain unverified.

The hair test compares all eight material instances on flat UV cards, using
their original textures, at original/full/half/zero/removed states. The first
run completed three card sets, then stalled exporting the fourth card's full
image. A debugger found the game thread in `ExportRenderTarget`/`ReadPixels`
and the render thread waiting in the NVIDIA Vulkan driver. Compilation workers
had finished. The process did not respond to SIGTERM and was stopped with
SIGKILL; partial images and both backtraces remain. This is not a passed batch.

`CSS_ASTRAL_HAIR_INDEX=0..7` selects one card per editor process. It also uses a
distinct output directory and fixture namespace. The bounded index-3 retry
adds explicit component-update flushing. The game was consuming 6632 MiB and
total GPU utilization measured 96 percent during that retry. Contention is a
hypothesis, not an established cause. The retry timed out with exit 124 after
three images and an empty zero-opacity file. No completed capture report was
written, and the test process is gone. The user was asked to close the game for
a comparison. Do not silently restart a hung editor or treat partial captures
as a completed visual test.

The subsequent `hair-zero-first.log` run used index 3 with
`CSS_ASTRAL_HAIR_RUN=zero_first` and
`CSS_ASTRAL_HAIR_STATES=zero,removed,original,full,half`. Shader compilation
finished, then the first zero-opacity export stalled. The bounded process
exited 124 and left a zero-byte `00-zero.png`, with no captures report.
This rules out preceding exports as a necessary condition, not GPU contention
or a driver issue. No test editor remains running. Run labels and per-state
begin/end logs now make future comparisons distinguishable.

An opaque backdrop was then added as a changed capture condition. The index-3
`hair-backdrop.log` run completed with exit zero and passed all five states.
The full `hair-all-backdrop.log` batch wrote all 40 images and a complete
manifest. All eight groups passed: zero equals removed, half differs from both
endpoints, and full equals original in RGB. Compilation and source-hash checks
also passed. These are dim flat-card captures; Eve full and Commander White
half were visually inspected. Full hairstyles and masks under the bright
native ghost effect still require validation.

The full batch's shell wrapper exited 127 even though UE logged successful
capture and normal shutdown. Its console identifies `h: command not found` at
the runner's last line. The runner was edited to add `native-render` while Bash
was waiting for its child, shifting the remaining script text. This was a
diagnostic orchestration error, not an image or shader failure. Do not edit a
running shell script. The later native render used the finished runner and
exited zero. No render process remains running.

The backdrop result is consistent with an empty-capture issue, but does not
establish the driver cause. Preserve the failed runs and use the tested backdrop
condition for further controlled comparisons.

## Local evidence

The runtime copy boundary now has live evidence. This game's header dump exposes
`K2_CopyMaterialInstanceParameters(Source, bQuickParametersOnly)`, although
`CopyMaterialUniformParameters` itself is not reflected. Exact UE 5.6.1 source
routes the `true` flag to `CopyMaterialUniformParametersInternal`. That walks the
hierarchy from the root through successive overrides and clears the destination
first. Set ghost/fade-specific values after copying. `CopyInterpParameters`
alone does not include inherited defaults. Static switches are not copied by
the uniform path and still require a compatible compiled companion.

`live-material-copy-03.log` checks five current player material families using
unattached transient MIDs: native Uber skin, Mat10 trim, Fabric02 silk, Mat12
metal and native eye smoke. All tested explicit scalar/vector values matched.
The skin source had no direct texture overrides, but its ten inherited texture
references became ten explicit overrides on the copy and matched the source's
effective values. Each copy's first scalar was changed, and the original value
and component binding remained unchanged. The callback was restored afterward.

This proves the tested copy semantics on the live game. It does not prove all
parameter associations, shader compatibility, double lifecycle, performance or
complete ghost composition. The first copy capture tested three skin instances;
the second widened coverage to five families; the third added inherited textures.

- `work/genessa-doubles/eve-cw-{meshes,materials,parents,roots}.json`: cooked readback.
- `work/genessa-doubles/eve-cw-inventory.json`: complete resolved parent chains and properties.
- `/mnt/eins0fxE/CSS-work/genessa-doubles/material-prototype1/fade-renders/`: garment images and compilation results.
- `/mnt/eins0fxE/CSS-work/genessa-doubles/material-outfits1/`: combined source hashes, companions and hair test.

The test runner uses a read-only filesystem sandbox except for its stage and
has no network. The installed engine's `InstalledNoZenLocalFallback` graph
reads `Compressed.ddp`. Its local node accepts an environment override, not the
command-line override used by `NoZenLocalFallback`. On Linux, UE replaces
hyphens in environment names with underscores, so set `UE_LocalDataCachePath`.
Two failed startup logs establish this; the corrected creation run exited zero.

API references: [MaterialEditingLibrary](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialEditingLibrary?application_version=5.6),
[StaticMeshComponent](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/StaticMeshComponent?application_version=5.6).
Local exact-version evidence: `BaseEngine.ini` sections `NoZenLocalFallback` and
`InstalledNoZenLocalFallback`, `UnixPlatformMisc.cpp` lines 289-308, and
`MaterialExpressions.cpp`'s `UMaterialExpressionStep::Compile`.
The copy path is in `MaterialInstanceDynamic.cpp` lines 459-474 and
`MaterialInstance.cpp` starting at line 5503; game declarations are in
`ue4ss/CXXHeaderDump/Engine.hpp` lines 20718-20721.

## Combined ghost coverage experiment

`coverage-create1` exited zero and created seven private parents under
`/Game/CSS/UnholyGenessa/AstralCoverage1/Parents`. This namespace satisfies the
existing editor helper's restriction; it is not an Unholy Genessa release
change. The shared `native_ghost_graph.py` builds the same reconstructed native
response used in the standalone test. Original opacity or thresholded coverage
multiplies native ghost alpha. Source color/lighting composition is not included.
All protected source hashes remained unchanged.

`ghost-hair1` completed 112 captures with exit zero: eight hair instances, both
forms, source-alpha reference, uncut ghost, two times, half fade, zero and removal.
The independent checker **failed**. Eve and CW layer 31 show animated colors and
fade, but CW layers 29, 30, 32, 35 and 36 have blank combined ghost captures.
There are too few detectable uncovered pixels for a useful cutout assertion.
Zero matches removal in all groups; that alone does not establish support.

`ghost-hair-manual1` repeated with manual exposure and physical camera exposure
disabled. It also exited zero; the reported pixel results are unchanged. The
reference images are bright while the ghost images are extremely dim. Do not
treat this as a fixed exposure issue or a passed shader integration.
The setting follows [Epic's exposure documentation](https://dev.epicgames.com/documentation/unreal-engine/auto-exposure-in-unreal-engine)
and UE 5.6.1 `Scene.h`'s exposure fields.

`coverage-inspect1` exited zero. `ghost-coverage-wiring.json` confirms both hair
parents originally use BaseColorMap alpha, and the companion Multiply inputs
still read output A. This excludes an accidental RGB connection at that edge;
it does not establish the effective GPU texture bindings or fragment values.
Next, inspect those values or change the fixture to distinguish sampling,
bounds/view-dependent ghost response and capture precision. Do not repeat the
same LDR setup or weaken the checker to label blank hair successful.

Receipts are in `material-outfits1/ghost-coverage.json`,
`ghost-hair-renders/captures.json`, `ghost-hair-renders/pixel-check.json`,
`ghost-hair-renders-manual/captures.json` and its `pixel-check.json` on the
secondary-drive stage. No live-game process was contacted.

## HDR coverage verification

The follow-up resolved the blank-layer observation in the test, without
changing either hair graph. UE 5.6.1 Vulkan's `RHIReadSurfaceData` overload for
`TArray<FLinearColor>` first reads `FColor` and then converts that 8-bit result
back to linear color (`VulkanRenderTarget.cpp`, lines 233-243). Consequently
`RenderingLibrary.read_render_target_raw(normalize=False)` does not preserve
HDR precision on this path, even with a 32-bit float render target. The first
float attempt, `ghost-hair-float1`, completed but its samples were quantized and
clamped. They are not suitable for checking the linear coverage product.

`RTF_RGBA16F` plus `export_render_target(... .exr)` instead reaches
`ImageUtils.cpp:GetRenderTargetImage`'s `RGBA16F` branch, which calls
`ReadFloat16Pixels` without those conversions. The completed `ghost-hair-exr1`
run uses that path, SceneColor HDR, a black backdrop and single-sided cube
faces. All source hashes are unchanged. Exit code is zero.

`check-ghost-hair-hdr.py` passes all 16 groups (eight materials, both forms).
It checks the actual linear relation:

`combined ghost RGB = uncut ghost RGB * source coverage`

Source coverage comes from a separate render of the untouched opacity graph
with white emissive 100. The check allows half-float rounding (0.3% relative
plus 0.000002 absolute), rejects nonfinite pixels and clamped reference captures,
verifies color ordering, two animation times, partial fade and exact zero/removal
equality. CW layers with zero-coverage holes have no ghost pixels in those holes.
Other layers have continuous alpha in the tested region; the product comparison
verifies their attenuation without pretending they contain fully empty holes.

Receipts: `material-outfits1/ghost-hair-renders-exr1/captures.json` and
`hdr-check.json`. The test uses Python 3.14.7, OpenEXR 3.4.15 and NumPy 2.5.3;
the [OpenEXR Python API](https://openexr.com/en/latest/python.html) documents
the channel-array readback. This establishes coverage composition for these
textures, not full hairstyles, source palette composition, DX12 or live doubles.
