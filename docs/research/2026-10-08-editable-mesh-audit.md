# Editable mesh structure and migration sources

This read-only UE 5.6.1 audit covers nine outfit meshes and seven active facial
replacement meshes. It checks editable sources and their editor-built render
buffers. It does not establish cooked equivalence, visible deformation, contact
quality or gameplay performance.

## Source coverage

All 790 planned package destinations now have editable source candidates or a
recorded alias recipe. There are 61 historical cooked animation aliases: 31
Commander White paths and 30 Unholy Genessa paths. Their recipes relocate Eve
clips; Genessa's movement mapping repair remains a separate preservation step.
These aliases are not 61 missing original animations.

The inventory hashes candidates from the main authoring project, the three
outfit projects and the Astral `visuals-clothdriver1` project. Of the 790 rows,
788 have one distinct editable content version, including identical copies in
different projects. Two have differing candidates: Eve's Hair and Tail material
instances. The selected candidates match the v1.4.0 `authored.json` hashes;
choosing the older main-project instances would undo the accepted hair update.

Evidence: [pinned source inventory](../../work/three-outfit-rig-physics/source-inventory1/editable-pinned.json),
its `sources-pinned.json` input, and
[`inventory_editable_assets.py`](../../tools/diagnostics/inventory_editable_assets.py).
This resolves candidate locations and content identities, not every
source-to-installed cook comparison or the final Blender recipe chain.

## Mesh results

| Outfit source | Render vertices | Morphs | Clothing assets | Raw mesh bones |
| --- | ---: | ---: | ---: | ---: |
| Eve Black Pearl | 558,243 | 22 | 0 | 379 |
| Eve Planet Diving | 510,980 | 22 | 1 | 379 |
| Eve Skin Suit | 444,427 | 22 | 0 | 379 |
| Eve Vacation Bikini | 569,795 | 22 | 0 | 379 |
| Eve Casual Knitwear | 616,658 | 22 | 1 | 379 |
| Eve Midsummer Alice | 631,411 | 22 | 1 | 379 |
| Unholy Genessa Eve variant | 160,794 | 22 | 6 | 383 |
| Unholy Genessa Genessa variant | 160,794 | 22 | 6 | 383 |
| Commander White C4 | 2,312,092 | 16 | 1 | 386 |

Each inspected source exposes one render LOD and an eight-influence buffer.
These are split render vertices, not necessarily unique Blender vertices.
Raw mesh bones also differ from the bound Skeleton's helper and virtual bones.
Do not replace the known 386/390 Skeleton counts with the mesh counts above.

The six Eve facial replacements retain their corresponding render vertex count,
clothing count and bindings, and expose 137 morphs each. Commander White E9
retains C4's 2,312,092 render vertices and one clothing asset, with 206 morphs.
Both Genessa meshes still require visible-face expression authoring. Their 22
existing morph names alone do not prove that the mask can animate.

Pairwise comparisons also preserve bone metadata, render-section structure,
Skeleton, Physics Asset and post-process bindings. Mesh-owned clothing objects
have different outer paths in the facial copies, as expected for duplicated
meshes. Their section mapping metadata and counts match; full clothing payload
equality is not established by this audit. The exact comparisons are recorded in
`mesh-audit1/facial-binding-comparison.json`.

Across all sixteen meshes (11,608,800 inspected render vertices):

- Every nonempty section's vertex weights sum to 65,535 through UE's uint16
  read API. No unweighted vertices or out-of-range influence indices were found.
- Section vertex ranges cover each buffer; section bone maps are valid.
- Reference poses and vertex positions are finite.
- All 1,220 morph entries have valid render-vertex indices and finite deltas.
- All material slots resolve in the authoring environment.
- Source file hashes are unchanged after inspection.

The audit does not measure influence reduction from original import data,
triangle quality, garment/body intersections, solver behavior or performance.
Commander White's vertex count is recorded for later measurement, not declared
a defect or permission to change its accepted appearance.

The commandlet completed with zero errors and 190 warnings. The log includes
registry warnings from historical cooked readback files nested under AnimLab.
Those warnings are retained; this was not a warning-free whole-project build.

Evidence: [full snapshots](../../work/three-outfit-rig-physics/mesh-audit1/meshes.json),
`summary.json`, `input.json`, the exact read-only mount command and engine logs.
The [editor audit module](../../tools/authoring-probes/rig-audit/README.md) built
against the local UE 5.6.1 installed engine. Engine weight access follows
[`FSkinWeightVertexBuffer`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSkinWeightVertexBuffer?application_version=5.6);
exact signatures and weight conversion were checked in the local 5.6.1 headers
and `SkinWeightVertexBuffer.cpp`.

## Cleanup

Removed eleven extra mesh copies from the completed namespace staging project
and its disposable DDC, totaling 2,138,157,472 logical bytes. Original sources,
the 32 migrated assets, their BlackPearl2 retarget reference mesh, cook results,
recipes and evidence remain. The initial free-space query had not yet reflected
Btrfs reclamation, so logical bytes are not presented as measured physical space
recovered. See [cleanup receipt](../../work/three-outfit-rig-physics/cleanup-migration1.json).

No installed package, core, setting or save was changed.
