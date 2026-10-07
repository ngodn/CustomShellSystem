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
| Nine-variant provenance | Installed hashes and 16 editable mesh identities pinned; all 790 migration paths have source candidates or alias recipes. Final Blender-to-cook recipe comparisons remain |
| Mesh/weights/contacts | Fresh checks of all 16 editor-built meshes pass weight sums, section/bone/morph indices and finite-data checks. Deformation/contact checks and cooked comparison remain |
| Unholy facial support | Both W3 candidates cook with 137 morphs; geometry and morph indices match independent readback within stored compression precision. All six cloth assets per variant match source data. Native SM6 base-pass UV3 checks pass. Oral assets and mesh bindings resolve without Eve mounted. Remaining shader/material review, CSS/CINE binding and game acceptance pending |
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

## Editable source audit

The [current mesh/source report](../research/2026-10-08-editable-mesh-audit.md) records all nine outfit meshes plus seven active facial replacements. The read-only editor audit checked 11,608,800 render vertices and 1,220 morph entries without basic weight/index/nonfinite failures. It preserves the distinction between mesh bones and Skeleton helpers. Eye, cloth, material and post-process identities are recorded per mesh; visual behavior still needs validation.

All 790 migration destinations now have editable candidates or alias recipes. Sixty-one are historical cooked animation aliases. The two differing source candidates are Eve's hair instances, resolved against their v1.4.0 authoring receipt. This avoids reverting its accepted hair while reorganizing assets. Final source-to-installed cook equivalence is still pending outside the first movement trial.

Cleanup removed 2.14 GB of reproducible extra namespace-trial mesh copies and cache (logical size), preserving original sources and the accepted candidate. See `work/three-outfit-rig-physics/cleanup-migration1.json`. The new mesh audit used read-only content mounts and did not install anything.

## Genessa facial candidate

Character-specific implementation lives in the [Unholy Genessa authoring notes](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/facial-controls-notes.md), with tools in that mod's `tools/` directory. The sealed mask mouth and poor eyelid transfer were reproduced visually. The current candidate adds seven lip-seam vertices, an oral surface and controls fitted to Genessa's eyelids while retaining the original neutral mask points. The head backing also needs matching expression motion; leaving it neutral reproduced mouth obstruction and cheek breakthrough.

The retained `face-articulation5`, `face-backing2` and `face-patch1` outputs cover 115 control names. The interchange preserves original face UVs, corner data and head weights. Sparse indices, finite geometry and neutral triangles pass structural checks. All six current facial performances have matching control names. Iris Out's 23 existing curves were rendered for 501 frames at 30 fps, with inspected neutral and expression samples. This is offline clay-head evidence, not an installed or fully validated face.

The editor integration below follows this head-only candidate. Original Genessa source hashes remain unchanged. Nothing was installed. Initial cleanup removed 17.9 MB of rejected intermediate files (logical size), with reports and useful visual evidence retained.

### W3 editor integration progress

Both W3 source descriptions match the facial patch: 1,303 mask points and all
face UVs/normals match exactly; 8,976 backing points map within 0.000007643 cm.
Original polygon checks account for 68 alternate quad diagonals and four tiny
exporter-omitted triangles. The backing retains its native triangulation.

The outfit-owned C++20 module now builds private facial replacements under
`/Game/CSS/eins0fx/UnholyGenessa/Faces/`. Both candidates add 115 expression
controls and retain all 22 original morphs and their delta counts. Skeleton,
physics-asset and post-process identities agree; weight and morph index checks
pass. This does not yet prove complete cloth or body equivalence.

Intermediate evidence is Genessa `work/face-integration4`, whose editor exited 0.
It has 172,151 render vertices per variant, compared with 160,794 originally.
The oral section still has more render splits than its source points/corners
justify. The source uses MikkTSpace tangent recomputation, so authored tangents
alone do not resolve this. Next: inspect the final render basis, verify full
source/render/cloth equivalence, review materials and expressions, then cook
and wire CSS/CINE. Original assets and installed game files remain untouched.

The subsequent `face-integration6` resolves the excessive oral splitting by
ordering mesh instances to match UE 5.6.1's tangent writer/reader indexing.
Both variants now contain 162,939 render vertices, including 2,138 oral vertices
and seven independently controlled lip duplicates. All 24 unchanged render
sections match the originals exactly, including their cloth mappings and body
morph deltas. All 115 mask controls pass correspondence checks subject to the
original 0.015 cm pruning threshold; retained delta error is below 0.000047 cm.

The lip duplicates use the mask's previously zero UV3 channel to prevent render
welding. Native/Astral material use of this channel is still unverified. This is
an explicit remaining acceptance gate, alongside full physical cloth-asset
comparison, expression/material review, cooking and runtime integration. No
candidate has been installed. The complete goal remains active.

The next read-only check closes the physical cloth-data comparison for both W3
candidates. `face-integration6/cloth-comparison.json` matches all six clothing
assets per variant: reflected physical data/configuration, collision references,
bone mappings, GUIDs, LOD settings, non-reflected tethers and transition arrays.
The candidate owns separate clothing objects. A detached-copy negative check
rejects an altered simulation vertex. `cloth-audit-exit3.json` records exit 0.
The first two diagnostic attempts failed on an unavailable Python property and
an incorrectly named probe morph; neither saved or installed assets.

The probe rebuild also passes on UE 5.6.1. Its finite-data guards were exercised
on the rebuilt mask/mouth with `jaw_open`. Cleanup compressed older render
snapshots with hash-verified roundtrips and removed superseded private caches
and candidate copies, reducing logical size by 1,215,207,053 bytes. The retained
receipt is Genessa `work/face-cleanup3.json`.

Native face shader evidence is in Genessa `work/face-material-audit1`. The
readback pins `MI_Genessa_top`'s SM6 shader map
`4C3FB7ABD0DFE5893189D38F886434F002E4A8FB`. Its four GPU-skin/cloth base-pass
vertex permutations declare only UV0 (`ATTRIBUTE5.xy`); there is no UV3 input.
Its two base-pass pixel variants use only `TEXCOORD0.xy` for material UVs.
The fifth (local/manual-fetch) permutation fetches only UV0: SSA 327-333 indexes
`NumTexcoords * (VertexOffset + VertexId) + min(0, NumTexcoords - 1)`, and writes
that pair to `TEXCOORD0.xy`. `uv-review.json` pins all five disassemblies.
This supports the seam tag for those permutations. It does not establish all
render passes, SM5 or Astral shader behavior. SM5 extraction
succeeded, but DXC cannot disassemble those containers. Material review and
cooked/runtime acceptance remain open. The pixel extractor's previous outputs
remain identical after adding explicit vertex extraction.

Both candidate Windows cooks now pass independent geometry/morph readback and
an IoStore export-payload roundtrip. Genessa `work/face-cook1/morph-verification.json`
records unchanged neutral positions, primary UVs, triangle/morph indices and
compression errors within the stored precision for all 137 controls per mesh.
The backing and mouth expression checks also pass. Extra ornament/hood entries
are tangent-only changes, not positional displacement; visual shading review
remains required.

The subsequent `work/face-cook3` trial includes the outfit-owned oral material
and its three textures in Genessa's `Faces/Mouth` directory. With Eve absent
from the decode mount, both mesh material bindings resolve and all four oral
assets match original decoded fields after namespace changes. Material and
texture payloads remain byte-identical, and all five direct game dependencies
decode. A stale binding in cook2 was caught and corrected before this trial;
the final verifier rejects that previous candidate. Remaining original W3
dependencies still use legacy paths, pending the wider migration. The candidate
has not been installed or accepted in the game.
