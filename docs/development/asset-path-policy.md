# CSS asset paths and Windows limits

Updated 2026-09-21 after the user's path correction.

## Required for new candidates

- Unreal asset packages belong under `/Game/CSS/`, meaning `Content/CSS/` on disk. Use readable names such as `/Game/CSS/SeduXtress/SK_Body` and `/Game/CSS/Shared/SKEL_Base`. These are naming examples, not already migrated assets or audited runtime skeletons.
- No UUID, hash-padding or random folder/file names for new authoring outputs. Use short purpose names and sequences such as `work/v45/cook`, `verify-0001` and `packages-0001`. Keep hashes in evidence JSON.
- Package paths have a 96-character budget, with at most 48 characters per component. Generated asset components use letters, digits and underscores. Reject Windows reserved names and long hexadecimal identifiers.
- Check the complete source, cook, stage and intended Windows extraction paths against a conservative 240-character budget. Do not assume the user's application and Windows configuration support long paths. Require `-WindowsRoot=C:/...` for the commandlet and `--windows-root C:/...` for the SeduXtress packager. A different destination root must be checked again.
- Preserve V44 files and their recorded hashes. Do relocation in an isolated candidate. Do not rename accepted assets in place or globally replace strings in cooked binaries.

The editor project is still named `CSSAuthoring`. Its cook layout therefore includes `CSSAuthoring/Content`, which packaging maps to `MortalShell2/Content`. That project directory is distinct from the obsolete asset namespace `/Game/CSSAuthoring/`; the latter must disappear from new candidate assets and their CSS dependencies.

## What was found and changed

The installed V44 plan stages 129 distinct packages including preserved materials. Five use `/Game/CSSAuthoring/`; 125 fail the new naming rules overall. The heel candidate is also still authored at an old diagnostic path. This is an outstanding migration, not a completed rename.

`stage_nextgen_materials.py` generated long SHAKE-derived folder names to keep replacement references exactly the original byte length. Shortening those names breaks that patcher's assumptions. Its alias generator now refuses new work rather than producing more hash paths. The older general conversion tools and historical evidence remain available as references; do not reuse their fixed-length relocation for this new candidate.

The distributed and local editor cook commandlets now reject old namespaces, hash folders and paths outside the budget. They inspect hard and soft CSS package dependencies recursively, so changing only a mesh's own name cannot pass. The existing supported mesh, texture, material, skeleton, physics and animation asset classes are retained.

The actual SeduXtress packager now requires a Windows root and checks the package list, preserved material package names, catalog/recipe object paths, staging paths and output paths before creating or replacing directories. The retired default build route is rejected. Reproducible patches for external authoring tools are in `tools/authoring-patches/short-*-paths.patch`.

`css_package.py` uses workspace scratch directories and atomically reserved readable sequences for verification, release staging and backups. Read-only verification of existing packages is still supported. This does not rename already released packages or change the active native core.

## Validation

Evidence: `work/paths/`.

- Editor C++20 module build completed with exit 0 (`build-exit.json`).
- The actual rebuilt cook commandlet rejected the old V44 package list, exited nonzero and created no cook output (`reject-result.json`).
- The actual SeduXtress packager rejected the old candidate before creating either staging or candidate directories (`packager-reject.json`).
- Three path tests pass, covering old roots, padded hashes, reserved names, traversal, a long destination with a short filename, and collision-safe scratch allocation.
- Four existing package tests pass, including corrupt bulk data, blocked live installation, recipe preservation and failed release copy cleanup.
- `audit.json` records every failed package name. No newly named production cook or dependency migration has passed yet.

## Next step before the footwear cook

1. Create an explicit readable relocation map for the heel mesh and its complete skeleton, physics, animation, rig, material and texture dependencies. Rebuild serialized material references with offset-aware tooling and independent decode checks, replacing the retired equal-length patching assumption.
2. Update isolated catalog, recipes and material plans. Remove obsolete references and redirectors from the candidate closure. Confirm the accepted mesh bind, 379 real bones, nine virtual bones, 22 morphs, materials, hand graph, physics and accepted 200/24 hair settings survive.
3. Add exact relocated skeleton paths to native compatibility only after their structure is audited. Keep V44 compatibility for rollback. The illustrative new paths above are not yet allowlisted.
4. Run the guarded cook with a short work root, inspect cooked imports and dependency closure, rebuild containers and decode them independently. Only then proceed to the previously queued footwear deployment and visual checks.

The user's original proportions, modularity, body/hair physics, preview lighting and full Next-Gen acceptance scope remain active.

## References

[Microsoft's Windows path limits](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation) document classic MAX_PATH and the required long-path opt-ins. Our 240-character limit is a project budget, not an OS guarantee for arbitrary extraction roots.

[Epic's asset redirector documentation](https://dev.epicgames.com/documentation/unreal-engine/asset-redirectors-in-unreal-engine) explains why moving assets requires reference fixup. The dependency checks also use the pinned UE 5.6.1 `IAssetRegistry::GetDependencies` package-category API.
