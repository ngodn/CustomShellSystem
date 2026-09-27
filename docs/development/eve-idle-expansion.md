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

## Released rig and first skinned review

Read Black Pearl and SKEL_Base directly from the exact v1.2.0 release containers plus base-game dependencies, without a separate SharedAssets package. MeshExport audit mode now emits `reference-skeleton.json` for skeletal meshes.

The released Black Pearl mesh has 379 bones. Every name and parent matches the E4 target. Maximum reference differences: translation 0.00000361 cm, quaternion component-vector distance 0.000000054, scale zero. The released shared skeleton has 386 bones, with the same initial 379 names in order. The extra seven are Eredrim_Diapazon, Eredrim_Shoulder_l, tiel_dagger, unrealHelmet1_M, unrealBarrel1_R, weapon_l and weapon_r. No shared skeleton replacement is needed for these body animation candidates. Evidence: `work/eve-idle1/release-rig-comparison.json` and `release-rig/`.

Rendered five samples each for all 14 candidates using the accepted fitted Black Pearl blend and the existing replay verifier. All 70 samples passed its pose-replay tolerances. Inspected frame zero for each candidate. These are solid-material offline poses, without secondary simulation. Hair sticking outward in bent poses is not evidence of live physics failure. Visible body/clothing contact at knees and hands needs review; do not reopen accepted fitting automatically from a diagnostic render.

Loop endpoint checks cover all sampled local transforms, including virtual bones: worst angle below 0.005 degrees, worst translation below 0.005 cm. This establishes endpoint continuity only, not velocity continuity or gameplay transition quality. Per-clip receipt: `work/eve-idle1/candidate-review.json`. Full-speed motion review, ground placement, interruptions, six-outfit checks and packaging remain outstanding.

## Ground-contact probe

Added `--measure-only` to the accepted Blender replay tool, reporting visible-body, footwear and combined minimum heights. Visible-body measurements use vertices referenced by remaining polygons, excluding vertices left behind after hidden wardrobe faces are removed. The initial contact700/contact701 reports predate this correction and must not be used. The obsolete probe was stopped during contact702. Corrected reports are `c2-700` through `c2-713` (21 samples per clip), summarized in `work/eve-idle1/ground-review.json`.

The raw retargets need ground correction. Some kneeling/floor poses penetrate the nominal floor by approximately 7 to 19 cm. Clip 708 has about 6.5 cm variation across the sampled contact heights, so a constant lift alone cannot keep it closely planted. Next author a conservative per-clip correction, verify its loop continuity and resample ground contact. Keep the existing per-variant mesh offsets, including Black Pearl's -3 cm, in the live comparison.

Read-only review of `native/src/walk_override.inl` confirms the existing custom idle path releases on active montages, movement and sidearm aiming and restores hidden weapons. The authoring idle overlay blends in over 0.18 seconds and exits immediately to preserve attack starts. These source checks do not replace runtime interruption tests. No runtime edits were made.

## E5 ground correction

`ground_idles.py` uses the corrected visible contact measurements. Clips with at most 3 cm contact variation receive a constant offset. Other clips use periodic interpolation and a five-frame weighted smoothing filter. Nominal target contact is 2 cm, accounting for the already accepted Black Pearl -3 cm display offset without changing it. This is a trial placement choice; the other outfits and real ground still require review.

All 1414 frames differ from E4 only in root local translation Z. Root correction endpoints match exactly. Predicted sampled contacts span 0.50 to 3.23 cm before existing variant offsets. Receipts: `work/eve-idle1/e5/grounding.json` and `root-only-check.json`.

`import_grounded_idles.py` duplicates E4 into private E5 assets, edits only root translation keys and evaluates every physical bone at every frame. All 14 completed, maximum local translation error 0.00000060 cm; original rotations/scales pass unchanged. Existing CSS asset hashes were verified unchanged. The commandlet process exited 0; log `work/eve26/idle-e5-ground.log`, receipt `work/eve-idle1/e5/import-result.json`.

A denser skinned render and contact pass is running for all E5 clips at stride 2 (15 fps samples). Outputs are `work/eve-idle1/e5-render700` through `e5-render713`; each completed clip has its own report. Do not infer completion from directory existence. Full sequence has 51 rendered samples including the duplicate loop endpoint; encode only the first 50 at 15 fps to preserve the original 3.333-second cycle.

The accepted manifest stores animation options per variant, not at outfit level. All six variants currently carry Eve Default Idle plus movement slots. Add new options to all six while preserving their existing definitions.

## Combined E5 review candidate

All 14 dense renders finished (714 total samples). Inspected five distributed samples per clip in `e5-review-a.jpg` and `e5-review-b.jpg`. Full-resolution sequences remain in `e5-render700` through `e5-render713`. Replay translation error stays below 0.001 cm and reported rotation error is zero. `e5-dense-review.json` contains per-clip contact ranges. These measurements still use Black Pearl without live secondary simulation.

The dense pass catches a contact peak missed by the earlier stride-five measurements: Deep Squat (712) rises to about 5.95 cm nominal at frames 22, 42, 62 and 82. Black Pearl's existing offset reduces this to about 2.95 cm. Check this in motion and on the other outfits before accepting it; do not describe all contacts as within the earlier predicted range. Other clips range from approximately 0.44 to 3.26 cm nominal. Review videos use the first 50 frames at 15 fps, excluding the duplicate endpoint.

`export_idle_library.py` duplicated the corrected sequences into `/Game/CSS/Eve/Anim/Idles/AN_Idle700` through `AN_Idle713`, preserving all prior authoring asset hashes. Receipt: `e5/export-result.json`. The Windows commandlet cook exited 0 with no errors; the two warnings are attempts to write editor settings outside the writable workspace. Log: `work/eve26/idle-e5-cook.log`.

`package_idle_library.py` built `work/eve-idle1/pack1/CSS_EveStellarBlade_eins0fx_P`. It extracts the exact accepted release, adds only the 14 cooked sequences, then round-trips the combined IoStore. All 323 existing asset headers and payloads match, including the released 386-bone shared skeleton. All 14 added animation payloads match their cooked inputs. Total: 337 assets. No separate shared-assets container participates in verification.

Each of the six variants now has 15 idle entries in this candidate. Existing IDs, choices, customization and other metadata remain unchanged except for the added idle entries and updated container hashes. The candidate deliberately retains the baseline version until release validation; it is not a new public release. Receipt: `pack1/verification.json`.

The game was running when packaging completed. Installation, live physics, floor placement, interruptions, weapon restoration and persistence have not been tested. Requested a convenient game closure for a backed-up installation. No installed package or release ZIP has been changed by this stage.
