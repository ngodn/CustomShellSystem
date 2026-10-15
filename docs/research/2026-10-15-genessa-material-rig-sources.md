# Genessa material and rig sources

The current material writer reproduces all 33 preserved Genessa material
instances exactly. Retained editor and cook records also account for seven
authored parent materials, 48 textures and 13 original rig/mesh packages.
All 224 material payloads remain identical in the current installed package's
verified assembly. No game files or settings changed.

Evidence: `work/three-outfit-rig-physics/genessa-material-rig-sources-20261015/`.
Recipe: `tools/diagnostics/verify_genessa_material_rig_sources.py`, Python 3.14.7.
The completed run exits zero, verifies 641 protected files and rechecks their
hashes before recording success.

## Preserved native instances

The original `.pak`, `.utoc` and `.ucas` hashes in the material extraction record
match the corresponding entries streamed directly from the published
`dist/v0.2.0/CSS_UnholyGenessa_eins0fx_P-v0.2.0.zip` in the authoring project.
The archive matches its retained checksum. No archive extraction or copy is
needed. All 66 original header/export source files match the migration record.

The current writer's source matches its retained build checkout. Its executable
hash is `64aaebcabc565dc5046c8e05590decadb4573d1ab21fc5c438da5e69c89cfdde`.
Using the original 101-entry namespace map and 33 jobs, it reproduces every
accepted header and export byte. Each job passes unchanged and inverse header
roundtrips. The results also match the final installed lineage's payload table.

This preserves the native skin and eye implementation. Their editable binding
proxies are not substituted for these cooked sources. The distinction and
earlier shader failure remain documented in
[outfit material migration](../development/outfit-material-namespace.md).

## Authored materials and textures

All 55 original source hashes and migrated editor hashes match the authoring
and cook records. These cover seven Material assets and 48 Texture2D assets.
The successful fresh editor reload and cook receipts remain hash-pinned.
Combining their retained cooked outputs with the 33 preserved instance pairs
reconstructs the complete 224-file material table, with no overlapping source.
Every entry matches the rig assembly and final hair-update assembly.

The cook records UE 5.6.1 and pins the original renderer configuration, including
unlimited bone influences, threshold 8, 16-bit bone indices and skin-cache shader
compilation. Those settings belong to the accepted shader recipe. This check
does not recook textures, rebuild shaders or claim a new shading fix.

## Rig and mesh sources

All 13 outfit packages in the original rig cook list have matching original
editable sources, migrated sources and cooked header/export files. The recorded
command mounts that editor content read-only. Fresh-reload and source-validation
receipts pass. The separate shared base skeleton is outside this 13-package count.

Nine packages retain their original migrated payloads in the final assembly:
the idle and secondary graphs, three Control Rigs, three Physics Assets and the
wing skeleton. The two normal W3 meshes and two remaining graphs were replaced
later. They are explicitly classified as changed, with their accepted cloth and
hair-update overlays covered by the [package-lineage audit](2026-10-15-genessa-package-lineage.md).
They are not mistaken for preserved original payloads.

## Limits and cleanup

This extends retained source/cook provenance and performs an actual material
writer replay. It does not perform a fresh UE cook, whole-outfit Blender rebuild,
new extraction or runtime/performance validation. The newer oral material and
facial integration sources belong to their separate facial records; this audit
covers the original 88 material/texture packages and 13 rig/mesh packages.

The first attempt incorrectly treated the historical container table as a
path-to-hash map. It stopped before running the writer. Its recipe and failure
remain in `genessa-material-rig-sources-20261015-attempt1/`. The corrected recipe
checks the actual table schema and verifies the original containers in the
published archive, since the old installed paths now contain the new build.

After exact comparison, the 66 duplicate replay outputs were removed under the
[prewritten cleanup list](../development/cleanup-2026-10-15-genessa-material-replay.md).
The cleanup receipt records 2,037,270 logical bytes. `df -h` before and after
reports root 96 GB available and theark 49 GB; no physical recovery is inferred
from those rounded readings. Requests, hashes, commands, reports, source files,
accepted cooks and rollback are retained.
