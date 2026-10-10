# Peony Windows cook and cloth-buffer readback

Status: private candidate. No installation or release acceptance.

The canonical Peony normal and facial meshes, garment materials and four Astral
companions now cook with Windows UE 5.6.1. The first two attempts crashed while
building the facial mesh. Disabling cooker worker threading allowed the third
attempt to finish. This setting applies to the offline cook command only; it is
not a game configuration change or a reduction in cloth physics.

## Inputs and ownership

Evidence is under Eve's `work/peony-windows1` through `peony-windows4` and
`work/peony-cookdiagnostic1`. The inputs are the frozen `peony-cookprep1` meshes,
`peony-astral2` materials and the existing accepted cooked dependencies.

- Windows engine: 5.6.1, changelist 44394996.
- Container image: `sha256:d8b0f16dcddcf74ed4816c2151e517d37843750ca60ff8d5222318d2c224baaa`.
- 121 editor input packages: 55 new outfit assets, 10 new shared assets,
  43 reused outfit dependencies, six reused shared dependencies and seven
  editor-only inputs.
- Only the new outfit/shared assets enter the private readback container.
  Existing Eve and consolidated shared containers supply the other runtime
  dependencies. Editor stand-ins are excluded.

The cook receipt verifies protected source hashes after execution. The game
installation, saved settings and accepted Genessa cloth package are untouched.

## Failure isolation

| Check | Result |
| --- | --- |
| `peony-windows1`, ordinary cook workers | Exit 3; null read during `SK_Facial1` build |
| `peony-windows2`, `-asyncskinnedassetcompilation=0` | Same failing call chain, exit 3 |
| Linux warm-cache inspection | Render data loads; intermediate imported attributes are absent, so this does not test rebuilding them |
| Linux cold-cache inspection | Both meshes reconstruct all 15 scalar attributes, each with 153,854 finite values |
| `peony-windows3`, additionally `-nothreading` | Exit 0 in 246.85 seconds; zero errors and two synchronous asset-registry warnings |
| `peony-windows4`, same flags with the original pre-build cache | Exit 0 in 288.14 seconds; both normal and facial meshes rebuilt, zero errors |

The first crash dump records a null source pointer at `memcpy` with a 615,416-byte
copy, exactly 153,854 floats. Resolving exported addresses and checking the
matching local source identifies `FSkeletalMeshAttributeVertexBuffer::Init`,
`FSkeletalMeshVertexAttributeRenderData::AddAttribute` and
`FSkeletalMeshLODRenderData::BuildFromLODModel`. The nearest exported name alone
mislabels one internal address as Nanite; this is not evidence of a Nanite bug.

The local `FLODUtilities::UpdateLODInfoVertexAttributes` implementation launches
attribute-conversion tasks while adding entries to the same map. That is a
candidate concurrency fault, not a proven line-level cause. The serial
skinned-asset option does not disable these lower-level tasks. The successful
control disables worker threading globally in this private cooker. No engine
binary or mesh data was patched to suppress the crash.

Local source references, relative to the UE engine root:

- `Source/Developer/SkeletalMeshUtilitiesCommon/Private/LODUtilities.cpp:2651`
- `Source/Runtime/Engine/Private/Rendering/SkeletalMeshAttributeVertexBuffer.cpp:76`
- `Source/Runtime/Engine/Private/SkeletalMeshLODRenderData.cpp:500`
- `Source/Runtime/Core/Private/GenericPlatform/GenericPlatformProcess.cpp:646`
- `Source/Runtime/Core/Private/Async/Fundamental/Scheduler.cpp:226`

The public [FLODUtilities API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Developer/SkeletalMeshUtilitiesCommon/FLODUtilities)
identifies the attribute-building entry point. The exact behavior above was
checked against local 5.6.1 source, not inferred from current-version API docs.

## Independent cooked validation

`peony-windows3/readback1` contains the IoStore roundtrip, independent
AssetReadback output, dependency audit and validation receipts.

- All requested cooked exports and imports resolve against the accepted
  base-game, Eve and consolidated shared containers.
- Non-header cooked payload bytes survive the IoStore roundtrip unchanged.
- Normal and facial meshes each retain 153,854 vertices and 15 Float buffers.
- Buffers are finite and zero outside the two cloth sections. Their offsets
  agree with independently exported native render morphs; the maximum residual
  is 0.01498845 cm, within the existing 0.015 cm morph import threshold.
- Half-edge buffers account for 153,854 vertices and 612,207 triangle corners.
- Normal/facial base render digests and reference skeletons match. All 22 body
  morph digests match; the facial mesh retains 137 morphs in total.
- Four garment roots, four Astral roots and four opposite-culling instances
  contain embedded SM5 and SM6 shaders, including skin and cloth base-pass
  variants.
- Removing a correction buffer is rejected for both meshes by the validator.

This is a cook/data check. It does not prove material rendering, moving garment
contact, runtime Astral registration, final fit, gameplay or performance.
The hip-accessory walking contact remains unresolved.

The independent repeat also passes these checks. All 65 selected packages
resolve, all 82 non-header payloads match the first passing cook byte for byte,
and the full decoded mesh and shader export documents are equal. This establishes
repeatability for these inputs and commands, not a general engine-threading fix.

## Repeatable commands

From the CSS repository, use the existing .NET 10 executable and game mappings:

```sh
python3 tools/eve-fit/cook_peony_candidate.py --stage peony-windows4 --single-thread
python3 tools/eve-fit/readback_peony_candidate.py --stage peony-windows4 --dotnet "$CSS_DOTNET" --mappings "$CSS_MAPPINGS"
python3 tools/eve-fit/check_peony_candidate.py --stage peony-windows4
```

Stages refuse overwrite. The repeat used the original pre-Peony-build cache;
the log confirms normal/facial rebuilds took 5.28/18.94 seconds. Its evidence is
in `peony-windows4/readback1`, including `repeat-comparison.json`.

Disposable Wine prefixes from terminal jobs can be recreated from the pinned
image. Their removal receipts retain the reclaimed size and terminal status;
logs, crash dumps, source recipes, cooked outputs and useful DDC data remain.
The three prefixes removed after jobs 1 through 3 ended reclaimed
3,298,729,984 allocated bytes. Each job retains `prefix-cleanup.json`.
