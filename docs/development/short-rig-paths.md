# Short rig and mesh paths

2026-09-21. The heel candidate's editor dependency closure now loads under short `/Game/CSS/` paths. Fresh editor comparisons, the seven-asset cook and independent rig readback pass. Cooked mesh geometry, combined packaging and live footwear checks remain. V44 remains installed and accepted.

## Copy and reference mapping

`tools/asset-paths/inspect_closure.py` records hard and soft package dependencies from `SK_HeelSupportsV45C`. Its closure contains 44 packages: 30 material placeholders, five meshes, three Skeletons, three Control Rig Blueprints, two Animation Blueprints and one Physics Asset. Historical preview references are copied too, rather than silently discarded.

The complete map contains 134 entries, extending the [120 material and texture paths](short-material-paths.md). Main runtime identities are:

| Asset | New package |
| --- | --- |
| Heel candidate mesh | `/Game/CSS/SeduXtress/SK_BlackPearl` |
| Accepted B2 animation Skeleton | `/Game/CSS/Shared/SKEL_Base` |
| Fitted collision | `/Game/CSS/SeduXtress/PA_Body` |
| Post-process animation | `/Game/CSS/SeduXtress/ABP_Secondary` |
| Hand, hair and body rigs | `/Game/CSS/SeduXtress/CR_Hand`, `CR_Hair`, `CR_Body` |

Preview meshes and their older Skeletons live in explicit `Reference` folders. Those Skeletons are not new compatibility targets for the native runtime. After cooked verification, the local runtime allowlist includes the exact new B2 path, retaining the old paths for V44 and restoration. Host checks explicitly reject the reference Skeletons and lookalike paths. This change is not deployed.

`CSSCopyAssetsCommandlet` uses UE 5.6.1's own `FAssetHeaderPatcher`. Its context includes all redirects but writes only the 44 requested new packages. Previously migrated textures must already exist and are reference-only. Source/destination overlap, duplicate destinations, existing copy targets, invalid names and excessive full paths are rejected. Dependency gathering is disabled so it cannot silently add writes.

The implementation is pinned to the installed UE 5.6.1 source, specifically `Engine/Source/Developer/AssetTools/Internal/AssetHeaderPatcher.h` and its private implementation. This internal API requires a version-specific audit if the engine changes. No engine headers were modified.

## Fresh verification

`tools/asset-paths/verify_editor_copy.py` loads original and migrated assets in a separate editor process. Every registry dependency equals its explicitly mapped original. Mesh reference transforms, Skeleton metadata, virtual bones, material slots and native UV densities, physics bodies/constraints/exclusions, animation defaults and morph names match after path substitution.

The primary candidate retains 379 raw bones, nine virtual bones, 30 material slots and 22 morphs. Hair stiffness/damping remain 200/24. The hand driver remains enabled with the accepted input mode.

Five retained H2 snapshots each execute seven hand states against both original and migrated assets. All 35 complete results match with maximum numeric difference zero. Actual bound collision queries pass with 22 valid bodies in both initial and recreated physics states. All 268 protected asset hashes remain unchanged after inspection.

These checks do not certify all weapons, moving hair/body behavior, cooked geometry or live damage/parry. They establish editor relocation without changes to the compared bindings and behavior. No game assets or runtime state were modified.

## Failures retained for future work

- The first module build lacked `CoreRedirectsContext.h`; add the pinned CoreUObject internal include directory.
- UE 5.6.1 places an internal-API attribute where this Clang build rejects it. The module uses the engine-supported `bValidateInternalApi = false` opt-out, with the dependency documented in `CSSAuthoring.Build.cs`.
- An initial probe-guard edit touched a configuration function without a mesh argument. That edit was reverted; only the evaluator's exact new mesh/Blueprint pair is allowed. The rebuilt module passes.
- Python does not expose imported material slot names as direct attributes or the native UV-density fields. Two verification attempts failed explicitly. `mesh-material-reader.patch` adds a native JSON reader for the actual fields; no fields are omitted or converted to pointer-bearing display strings.

## Evidence and reproduction

Evidence lives in `work/paths/rig1`:

- `closure.json`, `map.json`, `request.json`: original closure and explicit copy plan.
- `build3-exit.json`, `copy-exit.json`, `copied.json`: successful module build and 44 copies with protected source hashes.
- `probe-build2-exit.json`, `reader-build-exit.json`: corrected probe and native reader builds, both exit zero.
- `verify-exit.json`, `verify2-exit.json`: retained Python inspection failures.
- `verify3-exit.json`, `bindings.json`, `verified.json`: successful fresh verification and final scope.
- `hand-old0.json` through `hand-new4.json`, `queries.json`: complete evaluated observations.

Command manifests and logs are retained beside these reports. Editor scripts use `CSS_CLOSURE_WORK` to select the workspace evidence directory. The copy commandlet takes `-Input=<request.json>`. Do not rerun a successful copy into existing destinations. Keep assets read-only during verification and wait for the editor process's terminal exit, including its normal shutdown delay.

The physical project folder remains `CSSAuthoring/Content`; it does not define the asset namespace. Newly authored package names use `/Game/CSS/`, and packaging maps the physical project folder to `MortalShell2/Content`. Continue enforcing the full Windows destination budget before cooking and staging.

## Cooked rig readback

The guarded cook emits exactly seven runtime packages and exits zero. Retoc produces and verifies `ShortRig_P.pak/.utoc/.ucas`. A fresh CUE4Parse process decodes those seven plus the six corresponding installed V44 rig packages. The installed V44 trio matches its pinned hashes before comparison.

`tools/asset-paths/validate_rig_cook.py` compares the complete decoded Skeleton, Physics Asset, Animation Blueprint and three Control Rig documents. Only the declared package and generated-symbol renames are normalized, including the generated function-map key. No property or payload field in these documents is excluded. All six match. This validates decoded content, not byte identity of renamed packages.

The new mesh's Skeleton, Physics Asset and post-process bindings match the intended short paths. Thirty materials and 22 morph references survive cooking. Every decoded CSS package reference resolves within the seven runtime assets and 30 migrated materials; editor-only preview dependencies do not enter these decoded runtime references. All 268 protected editor hashes remain unchanged.

The initial decoder attempt exits 134 because a staging symlink omitted the installed package subdirectory. Correcting only that workspace link makes the second attempt exit zero. Preserve `decode.log` and `decode-exit.json` as the failed setup result, alongside `decode2-exit.json`, `assets.json` and `cooked-validation.json` as the successful readback.

The new heel mesh has not yet passed cooked geometry/skin/morph payload comparison. The local compatibility change does not establish live execution. Complete those checks, metadata/recipe migration and combined container validation before installation.

The focused host `css_skeleton_compatibility` test passes, including restoration to old audited paths and rejection of both new reference-only Skeletons. `cmake --build build/windows --target css_core -j 2` exits zero; its command, log and terminal result are retained as `native-build*` under `work/paths/rig1`. The resulting local core also contains earlier uninstalled preview-light work. Do not treat it as the installed V44 core or deploy it without the combined validation step.
