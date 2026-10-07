# Three-outfit implementation status

Updated 2026-10-08. [Goal](three-outfit-rig-physics-goal.md) is active. Installed baseline, namespace parser support and the initial asset/reference migration map are recorded below.

## Verified baseline

- Installed core selector points to `css_core-idle-handoff-db5207d4722ca6af.dll`; its full SHA256 matches the previous deployment receipt.
- User confirmed the current Iris Out gameplay-interruption test: attack, aim/fire, Genessa doubles and MISC Genessa Form work.
- All three installed outfit payload pairs match the historical audited manifests: Eve 1.4.0, Unholy Genessa 0.2.0 and Commander White's recorded `0.0.6-dev` package metadata. Version labels here come from the manifests, not inferred release names.
- Six installed containers inventoried: the three outfits plus legacy Shared, Astral and Locomotion. Export counts are respectively 331, 131, 197, 28, 30 and 103.
- There are 28 overlapping export paths among these containers. All nineteen differing movement animations were decoded independently: the legacy copies use indices 379-387 for the final nine tracks, while the installed skeleton has 386 raw bones. Eve's copies use 386-394. All other decoded fields agree; this does not establish compressed byte identity. The skeleton exports themselves agree in indexed hash across their four providers. Do not let consolidation reintroduce the stale track maps.
- Installed locomotion metadata contains 63 entries: eight idles, seven dances, three catwalks, eleven entries internally categorized as performance, thirty-one photo poses and three gait entries. These internal categories do not authorize a new CSS Performances menu. Four entries declare native-human assets and **59 have no native-human asset declaration**. This is catalog coverage, not live validation. Two facial rigs are advertised, Eve and Commander White; no Unholy Genessa facial rig is present.
- Existing source evidence identifies the visible Genessa mask and eyes as separate meshes without expression shapes. Retained Eve body shape names do not establish visible Genessa facial support.

Evidence: [baseline files](../../work/three-outfit-rig-physics/baseline-1), including `baseline.json`, independent `decode-*/decoded.json`, command/exit logs, `legacy-track-conflicts.json`, `native-coverage.json` and `installed-locomotion-manifest.json`. Reproducible inventory tool: [three_outfit_baseline.py](../../tools/diagnostics/three_outfit_baseline.py). The audit did not modify installed files, settings or saves.

## Work remaining

| Workstream | Current state |
| --- | --- |
| Nine-variant provenance | Installed hashes pinned; final source recipes and active facial replacement provenance still being mapped |
| Mesh/weights/contacts | Historical evidence located; fresh deformation checks pending |
| Unholy facial support | Missing visible-face controls confirmed in prior source audit; implementation pending |
| Shared organization | CSS parser accepts new and legacy roots with 23 host suites passing. All 790 unique installed packages have collision-free author-owned destinations; 172 catalog path references classified. First 32 assets renamed, reloaded without redirectors, cooked and decoded in isolation. Remaining migration and runtime compatibility pending; not installed |
| Locomotion | 63-entry installed catalog captured; four native declarations, 59 missing; conversion and motion-quality review pending |
| Gameplay regressions | Current interruption baseline accepted by user; candidate regression tests pending |
| Performance | Current package sizes recorded; matched gameplay captures pending |
| Trebuchaxe report | Low priority, queued and unverified |

Next: extend the isolated migration to the remaining dependencies and consumers, finish editable-source provenance and prepare the Genessa facial candidate. Package destinations, the nineteen stale mappings and per-entry native coverage are recorded; do not repeat those investigations without changed inputs. Preserve existing gameplay while these assets are prepared.

## Namespace parser change

[Organization convention](../asset-organization.md) documents the author/project roots. The first supporting CSS change accepts shared animations/carriers/adapters under `/Game/CSS/<author>/Shared/Locomotion/` and character facial replacements under `/Game/CSS/<author>/<project>/Faces/`, retaining the legacy roots. Tests cover a second author, malformed namespace boundaries, native idle resolution and unchanged revision checks. The C++23 host build and all 23 host suites pass. This is catalog validation coverage, not migrated asset or live-game acceptance. No new DLL has been installed.

## Storage and cleanup

Initial free space was approximately 12 GB on the system drive and 13 GB on the game drive. Decode mounts use symlinks. The namespace trial now has isolated editable copies and two small 32-package cooks. Eve reference dependencies were copied with Btrfs reflinks, not mutable hardlinks. Prior video research removed only its temporary model and Python environment; source videos and evidence remain intact.

## First cooked namespace trial

The updated [migration map](../../work/three-outfit-rig-physics/namespace-2/asset-and-catalog-map.json) keeps the private wing skeleton in `UnholyGenessa/Skeletons`. Ownership is now 305 Eve, 130 Genessa, 170 Commander White and 185 Shared packages. It does not move Genessa-specific wing bones into the shared foundation.

An isolated UE 5.6.1 editor project migrated the common skeleton, 27 Eve animation sequences and four blend spaces. The original source tree was mounted read-only. Animation-data snapshots cover 566,868 source bone transforms, frame counts/rates, model GUIDs, curve/attribute counts and root-motion settings. Skeleton snapshots cover 386 raw bones, nine virtual-bone definitions and 82 sockets. A fresh process reloaded the saved candidates with old redirector files removed and verified the new skeleton and blend-sample references.

The first minimal cook was rejected: saving animations without their reference mesh cleared `RetargetSourceAssetReferencePose`. The second trial includes an independent editable copy of the reference dependency and explicitly refreshes this pose before renaming. Twenty clips had 388 transforms; the current skeleton requires 395. The refresh preserves the first 379 body transforms and last nine virtual transforms, inserting seven identity helper transforms. Seven newer idles already had 395 transforms and remain unchanged. Source animation keys did not change. The effect of the old reference-pose mismatch in gameplay has not been established.

The corrected 32-package cook passed an IoStore conversion/readback with identical export payload bytes. All 27 decoded sequences retain 395 reference transforms and virtual track indices 386-394. The nineteen movement sequences independently compared against the installed Eve decode differ only in package/skeleton references and the reference-pose refresh. The other eight clips have source-snapshot and cooked structural checks, not independent installed decoded comparisons.

Evidence: [editor trial](../../work/three-outfit-rig-physics/migration-editor3) (`migration.json`, `reload.json`, `retarget-validation.json`); [corrected cook](../../work/three-outfit-rig-physics/migration-cook2) (`comparison.json`, `pack/roundtrip.json`); [rejected cook](../../work/three-outfit-rig-physics/migration-cook1/comparison.json). Reusable probe: [migrate_namespace_trial.py](../../tools/authoring-probes/animations/migrate_namespace_trial.py) with the [editor audit module](../../tools/authoring-probes/rig-audit/README.md).

This is an offline migration check, not a release or game acceptance. No installed DLL, outfit, pack, setting or save was changed. Exact runtime skeleton compatibility, Astral adapter paths/material identities, catalog dependencies and legacy outfit behavior still need coherent migration and verification.
