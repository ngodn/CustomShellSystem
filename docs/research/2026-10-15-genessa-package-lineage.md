# Genessa installed package lineage

The installed Genessa trio matches `hair-solver-pack3`, including its retained
container bytes. Its input chain preserves the namespace stage, the accepted
four-mesh cloth overlay and the later two-graph hair-default change. This is a
read-only audit of retained artifacts, not a fresh cook or package rebuild.

Evidence: `work/three-outfit-rig-physics/genessa-package-lineage-20261015-pass2/`.
Recipe: `tools/diagnostics/verify_genessa_package_lineage.py`, Python 3.14.7.
The run exits zero and rechecks all 1,769 unique protected files before reporting
success. No game input, installed-file writes, settings changes or large output
copies are involved. Root has 96 GB available and theark 49 GB after the audit.

## Verified stages

| Stage | Checks performed now |
| --- | --- |
| `genessa-face-namespace-pack2` | All 282 recorded protected inputs and 265 staged payload hashes match. The retained verification has no differences. |
| `genessa-cloth-final-editor1` / `genessa-cloth-final-cook1` | All 160 recorded editor sources match. Successful cook, six-package finalization and fresh-reload receipts remain; both editor receipts report unchanged source assets. |
| `genessa-cloth-assembly1` | All 725 recorded inputs and 265 outfit payloads match. Reconstructing the payload table from the namespace stage plus exactly four cooked mesh pairs produces the same table. |
| `hair-solver-cook1` / `hair-solver-pack3` | Both recorded editor inputs, all 281 assembly inputs and all 265 final payloads match. Only the two intended graph pairs change from the cloth stage; the other 261 files remain identical. |
| Installed Genessa | Each of the three installed files matches the retained package and its recorded hash. All 22 archived outfit metadata readbacks match the staged metadata; manifest container hashes and sizes agree. |

The namespace package is retained under
`/mnt/eins0fxE/CSSAuthoringWork/three-outfit-rig-physics/genessa-face-namespace-pack2`.
The cloth stages are under the CSS goal workspace. Hair stages are under
`CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/work`.
These are required reproduction inputs, not superseded scratch copies.

The final graph comparison rereads the original and candidate decoded exports.
It permits only the `CSSEnabled` false default, including the omitted native-bool
serialization, and the CDO's `RF_Transactional` flag. All other decoded fields
must match. It does not rely only on the earlier `passed` value.

| Installed file | SHA-256 |
| --- | --- |
| `CSS_UnholyGenessa_eins0fx_P.pak` | `656ebf55536a05411fc93f1e42e204869e1a4dd9953234e728d4283ac93d81ef` |
| `CSS_UnholyGenessa_eins0fx_P.utoc` | `059391f3b4966d56d0852cea564d6a7b2ab5bac2ed77d04a03125fd200dc6715` |
| `CSS_UnholyGenessa_eins0fx_P.ucas` | `04c76e215b675171686d6210726cba98c3c67378a217dd2eeb3d6e4742e3d531` |

The initial verifier attempt compared the namespace report's empty differences
list with numeric zero and stopped before hashing inputs. Its recipe and failure
remain in `genessa-package-lineage-20261015/`. The corrected comparison requires
an empty list. No asset defect was found or bypassed.

## Scope

Together with the [facial-source replay](2026-10-15-genessa-facial-source-replay.md),
this identifies the accepted facial inputs and the retained stages that produced
the current outfit. It resolves the apparent mismatch with the earlier cloth
container: the later hair update changed the container while preserving its
meshes and materials.

The new audit does not rerun UE cooking, the container converter or extraction.
The graph and metadata comparisons use retained readbacks. Full outfit Blender
reproduction, remaining rig/material authoring provenance, current shared-package
coordination, gameplay and performance requirements remain separate. Historical
shared files in the hair-stage records are not asserted to be today's installed
shared package. No final release version or release readiness is claimed.
