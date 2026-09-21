# Short-path mesh and combined package

2026-09-21. The corrected heel candidate passes cooked geometry, skin, morph and complete package readback. The prepared trio is `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/short1/trio`. At this checkpoint it is not installed and V44 remains unchanged.

The runtime mesh is `/Game/CSS/SeduXtress/SK_BlackPearl2`, bound to the verified `/Game/CSS/Shared/SKEL_Base`, `PA_Body` and `ABP_Secondary`. The earlier `SK_BlackPearl` remains a retained diagnostic candidate. Neither old mesh is overwritten.

## Geometry and the rejected first cook

The independent ActorX readback preserves all 133,234 source positions exactly, including the 168 added heel positions. It retains all 193,589 oriented triangles, their material assignments and all three UV channels exactly at float32 precision. The 558,243 render vertices include duplicated triangle corners and seams; their count is not the source point count. Every render vertex has one to eight normalized skin influences.

The first cook passed those checks but did not preserve five existing hair weight assignments. Their source float weights were unchanged and tied after conversion to 16 bits. Different ordering of the tied leading influences assigned the final 8-bit normalization remainder to a different neighboring hair bone. Five unique point/weight assignments changed by one to three units out of 255. Body and hand assignments matched.

This mechanism is visible in the pinned UE 5.6.1 source:

- `Runtime/AnimationCore/Public/BoneWeights.h`: float weights convert to 16-bit integers, and the descending comparator compares weight only.
- `Runtime/Engine/Private/SkeletalMeshLODRenderData.cpp`, around lines 434–458: the 8-bit conversion truncates each weight and adds the remainder to influence zero.
- The retained CUE4Parse conversion, `Dto/MeshVertex.cs`, converts those actual integer weights to ActorX floats. It does not infer source weights.

Epic's [bone weight API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AnimationCore/FBoneWeight) provides the general type reference; the pinned local engine implementation establishes the behavior for this build.

A replay of 336 active body/hair/hand frames measured up to 0.072873825 cm of skin displacement from these five changes. That exceeded the 0.05 cm gate established before measuring. The candidate was rejected, rather than widening the gate. Evidence is retained in `work/paths/rig1/skin-differences.json`, `mesh-check.log` and `mesh-check-exit.json`.

## Correction and proof

`prepare_mesh_ties.py` creates an isolated import document. For each of the five tied pairs, it places the V44-leading influence one 16-bit unit above their mean and the other one below. Their total stays unchanged and neither crosses an 8-bit truncation boundary. The original Blender and source export remain untouched; only ten float entries in this import document change. All other source fields and weights are checked unchanged.

The fresh import uses the short mesh path above and a temporary `/Game/CSS/SeduXtress/Reference/SKEL_Import2`. `bind_mesh_ties.py` binds it to the already verified shared Skeleton and copies the accepted material/physics/post-process bindings. `short-mesh-binding.patch` limits native binding and virtual-bone refresh to this exact commandlet-only mesh/Skeleton pair. The temporary import Skeleton is not cooked or accepted by the runtime allowlist.

The second cook restores every existing unique position/skin-weight assignment exactly to V44, with zero differences. It retains the exact decoded mesh reference transforms. All 22 compressed morphs match the source within the existing engine compression/import bounds, with maximum measured delta error 0.003423 cm. The new heel weights are separately compared to their source, including quantization bounds.

`validate_mesh_cook.py` now requires exact existing skin equality. It does not permit the earlier hair-tie exception. `read_mesh_candidate.py` runs packing, container verification, independent export, geometry/UV checks and this skin/morph check as one sequence with terminal exit codes.

## Motion and complete packaging

The old and migrated rigs produce matching complete component outputs across 336 frames at 30 FPS, three overlay levels, active body/hair motion, hand toggles/resets and six public morphs. This is editor execution, not a live performance or combat certification. The first attempt selected the incomplete `hand-postprocess-motion-v1` fixture directory and failed on a missing second input. That attempt is retained in `rig1/motion1`; the corrected run requires the prior passing V2 fixture report and exits zero.

`prepare_short_package.py` combines seven runtime assets, 90 textures and 30 preserved materials. Catalog mesh references, material plans and metadata use the new short paths. Historical audit data remains outside the new metadata directory. Accepted customization recipes and hair defaults are retained. This is a 127-asset candidate; editor-only preview assets and material placeholders are excluded.

`validate_short_package.py` reads the final trio with only the base game containers present. All 127 packages decode. The mesh, six rig assets and 30 complete material documents match the independently verified candidates. All 90 textures resolve, every decoded CSS reference stays within the package, and Retoc plus CSS metadata/resource verification pass. The longest asset destination under `C:/CSS` is 87 characters. Full source/staging paths also pass the existing 240-character guard.

## Evidence and remaining work

- `work/paths/rig1/motion.json`, `motion2-exit.json`: 336 matching active-component frames.
- `work/paths/rig2/ties.json`: exact import-only adjustments and source hash.
- `rig2/import-exit.json`, `bound.json`, `bind-exit.json`, `cook-exit.json`: successful import, binding and cook.
- `rig2/geometry-validation.json`, `skin-equality.json`, `mesh-validation.json`: geometry, exact skin and compressed morph checks.
- `rig2/*-exit.json`: terminal results for the readback sequence.
- Authoring `work/short1/report.json`: assembled candidate hashes and copied-file hashes.
- `work/paths/final1/verified.json`: clean-container comparison, resolved dependencies, path budget and unchanged installed V44 trio.

Next install the backed-up candidate with the local native compatibility change, then verify actual selection, footwear appearance, motion and grounding. The local core also contains the previously compiled preview lighting controls. Live lighting/input/lifecycle, broader weapons/combat, modularity and full Next-Gen acceptance remain open. Do not equate offline package success with live acceptance.
