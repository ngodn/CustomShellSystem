# Eve idle expansion

Target: 14 reference animations plus the accepted Eve Default Idle, 15 choices total. The accepted six-outfit v1.2.0 release remains untouched.

## Evidence and current state

- Source inventory and archive hashes: `work/eve-idle1/inventory.json`.
- ESAP contains numbered entries 700 through 713, with additional companion entries. Actual motion must be inspected before assigning names or suitability. Asset names alone are not motion evidence.
- Retained peaceful idles 02, 03 and 04 were prepared in `work/eve-idle1/prepared` as workflow reference material. They are not substitutes for the requested reference animations.
- Preparation writes source-named JSON files. `batch.json` maps short labels to those filenames, matching the retarget reader. The earlier lookup for `Idle2.json` was incorrect; no preparation-script fix is needed.
- First reference decode: clip 700 loads as UAnimSequence (101 frames, approximately 3.333 seconds), but source-track validation rejects its skeleton map. See `work/eve-idle1/decode700-check.json` and `decode700.log`.
- Its map contains indices beyond the retained 3267-bone base skeleton. Determine the actual compatible skeleton before retargeting. Do not truncate tracks or weaken validation to make decoding pass.
- The first mount attempt omitted global containers. Corrected the source mount with read-only symlinks to the installed game containers. No game installation changed.

## Next work

Locate the matching skeleton or establish the reference map from source evidence. Decode and visually inspect each requested character clip, distinguish companion tracks, then use the accepted source-basis and retarget workflow against the current CSS skeleton. Record per-clip offline and in-game checks before packaging.

No new animation has been cooked, installed or accepted yet. Other agents' runtime edits are outside this change.

## Batch inspection

All 14 primary ESAP clips loaded successfully through metadata-only inspection. Each has 101 frames, approximately 3.333 seconds, 234 compressed tracks and indices reaching 3341. The 75 indices from 3267 through 3341 are common to all clips. Full records: `work/eve-idle1/clip-metadata.json`.

| Clip | Metadata | Track mapping | Visual review | Conversion / acceptance |
| --- | --- | --- | --- | --- |
| 700 | Loaded | 75 unresolved indices | Pending | Pending |
| 701 | Loaded | 75 unresolved indices | Pending | Pending |
| 702 | Loaded | 75 unresolved indices | Pending | Pending |
| 703 | Loaded | 75 unresolved indices | Pending | Pending |
| 704 | Loaded | 75 unresolved indices | Pending | Pending |
| 705 | Loaded | 75 unresolved indices | Pending | Pending |
| 706 | Loaded | 75 unresolved indices | Pending | Pending |
| 707 | Loaded | 75 unresolved indices | Pending | Pending |
| 708 | Loaded | 75 unresolved indices | Pending | Pending |
| 709 | Loaded | 75 unresolved indices | Pending | Pending |
| 710 | Loaded | 75 unresolved indices | Pending | Pending |
| 711 | Loaded | 75 unresolved indices | Pending | Pending |
| 712 | Loaded | 75 unresolved indices | Pending | Pending |
| 713 | Loaded | 75 unresolved indices | Pending | Pending |

A fresh read of the installed base skeleton confirms 3267 reference bones and no serialized VirtualBones property. This rules out attributing the extra indices to an available virtual-bone definition without further evidence. Epic distinguishes final reference bones from raw bones, with final bones including virtual bones: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FReferenceSkeleton .

Read-only scans of the installed third-party IoStore containers found no override at the exact referenced `CH_P_EVE_01_Skeleton.uasset` path. The scan does not cover legacy-only pak contents. Receipt: `work/eve-idle1/skeleton-candidates.json`. The ATOOL and ESAP IoStore listings also contain no matching skeleton asset. Next inspect legacy pak payloads and source track compatibility before deciding how to resolve the added indices.

## Complete track diagnostic

Added `CSS_TRACK_AUDIT=1` to MeshExport (.NET 10, SDK 10.0.401). It invokes the pinned CUE4Parse per-track decoder for every compressed track rather than iterating only resolved skeleton bones. It preserves unresolved indices without guessing names or parents. This is diagnostic output, not a relaxed production retarget path. Existing SourceTracks validation remains unchanged.

- Build succeeded with zero warnings and errors: `work/eve-idle1/audit-build.log`.
- All 234 tracks decoded for each of 700 through 713: `work/eve-idle1/audit/<clip>/track-audit.json`.
- Regression against retained Eve Default Idle: all 138 tracks match exactly across quaternion, translation, scale and all three timestamp arrays. Receipt: `work/eve-idle1/audit-regression.json`.
- Most unresolved tracks have constant approximately (0,-4,0) offsets, suggesting export helpers, but this is not confirmed bone identity. Some unresolved tracks in 707, 712 and 713 have multiple keys. Do not discard them on the static-helper assumption. Summary: `work/eve-idle1/audit-summary.json`.
- ESAP and ATOOL legacy pak listings are empty, so they do not supply a hidden skeleton override.
- Diagnostic known-joint previews for 700 through 706 rendered successfully. Inspected the first contact sheet at `work/eve-idle1/review1/joints-0.png`; poses include standing, kneeling and floor poses. This is not a skinned or full-motion acceptance check.
- The second preview batch correctly stopped on non-unit source scale. Tracks in 707, 708, 712 and 713 include non-unit leg scale. Keep this as an explicit conversion requirement; the current NumPy preview deliberately refuses it. Use an Unreal-compatible transform evaluator rather than silently stripping scale or composing a shear-producing matrix chain.

No references, installed packages, release artifacts or CSS runtime code were changed by this diagnostic work.

## Scale-aware joint review

The preview now composes positive scales separately from rotation, matching the positive-scale branch of FTransform multiplication in the checked CUE4Parse source. Negative or zero scale still fails explicitly. Reference: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/TTransform/Multiply .

Validation: a three-joint chain with non-uniform scales and two 90-degree rotations produced the expected endpoint (8,-4,0). All 211 Default Idle frames remain within 0.000016 cm of the old preview; the tiny difference comes from retaining nearly-unit source scale rather than ignoring it. Receipt: `work/eve-idle1/scale-evaluator-check.json`. The second batch now renders; inspected `review2/joints-0.png`. Full motion, skinning and in-game acceptance remain pending.

Additional compatibility evidence: the source clip SkeletonGuid is `A2B5776B-4857537B-77166983-647708BD`, whereas the freshly loaded base skeleton GUID is `2C8DEB63-499D8C56-DAA3AD80-AF949D71`. Therefore the missing mapping is not merely an exporter range check. Asked the user asynchronously for any ESAP source project, skeleton or requirements link. Known-joint previews are diagnostic assumptions based on the installed skeleton, not proof that every reference index has the author's intended bone identity.

## E4 offline candidates

A different GUID alone does not prove the common body indices are incompatible. The fresh base skeleton matches all 3267 retained names, parents and reference transforms exactly. Added indices are interleaved after existing bones, including finger endpoints. Their exact identities remain unconfirmed. The requested author requirements page was age-gated in the web tool, so no requirements claim was derived from it.

Prepared all 14 references as provisional 54-bone body/finger candidates in `work/eve-idle1/e4`, retaining complete 234-track audits separately. These candidates use the base-game mapping and must be visually validated; they are not evidence that the extra indices have been resolved. Added an explicit `--allow-positive-scale` preparation option. Default behavior still requires unit scale. Old preparation outputs for peaceful idles 02 through 04 match byte-for-byte (`prepare-regression.json`).

Source mesh import exited 0 (`work/eve26/idle-e4-source.log`). The complete retarget batch also exited 0 (`idle-e4-retarget.log`), producing 14 private `/Game/CSS/AnimLab/RT_E4_Idle700` through `RT_E4_Idle713` candidates and 101 sampled poses per clip. Result: `work/eve-idle1/e4/retarget-result.json`. Protected Black Pearl mesh and shared skeleton hashes remain unchanged.

The existing source mesh bind has 379 entries and evaluated target poses have 388 entries, including virtual bones. Before release, compare this authoring state against the exact accepted v1.2.0 cooked rig; do not infer compatibility from an old bone-count label alone. No public animation names, package manifest or installed files have been changed.
