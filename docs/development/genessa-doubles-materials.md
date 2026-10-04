# Doubles material support

October 4, 2026. Offline experiments, not installed runtime support.

The user requires Unholy Genessa, Eve and Commander White. Companion materials
belong to CSS. Outfit authoring sources and released containers stay unchanged.

## Released asset audit

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
