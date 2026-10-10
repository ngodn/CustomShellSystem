# Peony cloth body-shape binding

Private work: Eve's `work/peony-clothmorph1`. Nothing is installed. This adds
Peony-specific data for the existing shared correction; it does not change
Genessa's accepted world correction or the CSS runtime.

## Implementation

The authoring helper now accepts the exact normal and facial Peony source
paths, requires the canonical `DG_ClothMorphBody` and `MDC_ClothMorphBody`
assets, and writes fresh `_BasisStudy3` duplicates. The previous Genessa study
inputs and destinations retain their existing rules.

Peony's `Peony_leg_panels_Lower_2_L` and `Peony_leg_panels_Lower_2_R` slots
receive the five body-shape bases used by the shared graph: glutes, hips,
thighs, waist and body tone. Each basis has three Float vertex attributes.
There are no source vertices shared between these cloth slots and other
sections. No garment weights, collision, simulation parameters or facial
controls are changed by this step.

The shared graph and collection are copied from the accepted Genessa final
authoring project without modifying them. The graph still belongs to
`CSS_SharedAssets_P`; these mesh attributes belong to Eve. The shader and
runtime binding contract are described in
[cloth morph deformer binding](../development/cloth-morph-deformer-binding.md).

The implementation uses the installed UE 5.6.1 headers for
`FSkeletalMeshVertexAttributeInfo`, `FSkeletalMeshVertexAttributeRenderData`
and `FSkeletalMeshAttributeVertexBuffer::GetCPUData`. Epic's
[Deformer Graph documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/deformer-graph-in-unreal-engine?application_version=5.6)
describes the mesh-deformer pipeline; the exact attribute and cloth behavior
here is checked against the local engine and these assets.

## Validation

The C++20 editor module builds against UE 5.6.1. The isolated commandlet
hashes 120 original inputs and all private input copies, with original paths
mounted read-only. `author_peony_cloth_basis.py` compares the source and rebuilt
meshes using the native rig, source-section, render-section, morph and cloth
snapshots. Only the duplicate asset's identity is normalized for comparison.

Both authoring results preserve all existing data in those comparisons:

| Mesh | Preserved morphs | Preserved cloth sections | New Float buffers |
| --- | ---: | ---: | ---: |
| Peony | 22 | 2 | 15 |
| Peony facial | 137 | 2 | 15 |

`CheckClothBasisBuffers` reads the actual built CPU render buffers. It checks
the format, vertex count, finite values, exact zero outside the selected cloth
sections and agreement with the corresponding render morph. The maximum
per-axis difference is 0.01486969 cm. The existing import filters small render
morph deltas at 0.015 cm, so that comparison allows 0.02 cm; source-data and
existing-render-data preservation are separate exact comparisons. Each buffer
also receives a value digest for fresh-process comparison.

The check rejects the original mesh without basis buffers, an unrelated slot
and duplicate slot requests. Both cloth assets are independently duplicated,
not aliased to their originals. Every added attribute is enabled for rendering
and stored as Float. The shared graph snapshot and source file hashes remain
unchanged.

The first attempt stopped on the source/duplicate path difference, before
saving any mesh. Its report showed no other data difference. The corrected
authoring run exited successfully. A fresh Unreal process reloaded both saved
meshes and repeated the full checks successfully. Its snapshots and all buffer
value digests match the authoring run exactly. Both commandlets exited with
code 0, and all 120 original file hashes remain unchanged.

Receipts: `author/result.json`, `reload/result.json`, `author2-exit.json` and
`reload-exit.json`. The detailed per-mesh snapshots are next to each result.
After both commandlets exited, 459,593,531 bytes of isolated build/cache files
were removed. Source, binaries, assets and reports remain; `cleanup.json`
records the removed files, and `validation-receipt.json` hashes the retained
186 files plus the authoring script.

## Remaining work

Test GPU output with body sliders and cloth enabled/disabled. Integrate the
accepted mesh paths, Windows cook, independent
cooked readback, facial adapters and Astral material support. This authoring
result is not gameplay, contact or performance acceptance. The existing
accessory/walking-arm contact defect remains open.
