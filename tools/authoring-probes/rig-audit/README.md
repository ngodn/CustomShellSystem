# UE 5.6.1 source animation audit

This C++20 editor module exposes commandlet-only snapshot helpers to Python.
It is authoring tooling, not a CSS runtime module.

`SnapshotAnimation` reads named source tracks through `IAnimationDataModel`,
hashing explicit transform components rather than struct padding. It records
frame/key counts, sample rate, model GUID, curve/attribute counts and retarget
reference transforms. `SnapshotSkeleton` records raw bones, parent indices,
retarget modes, reference transforms, virtual definitions and sockets.

`SnapshotMesh` waits for the requested mesh's compilation, then inspects render
weight buffers, section/bone ranges, finite positions, morph indices and deltas.
It does not edit or save the mesh. `audit_meshes.py` adds bindings/materials and
source hashes. Set `CSS_RIG_AUDIT_STAGE` to a private output directory containing
`input.json` with `meshes` entries (`package`, `source_file`, `sha256`). Mount
authoring Content read-only and retain a private writable DDC. Results distinguish
editor-built buffers from installed cooked data.

`SnapshotMeshSections` reads selected source material groups by slot name. It
records global vertex/triangle/instance IDs, positions, raw skin weights,
corner normals/colors/UV channels and source morph deltas. Missing or ambiguous
slots fail explicitly. It does not save or modify the mesh. The Genessa facial
integration uses these snapshots to establish point and corner correspondence
before editing private candidates.

`SnapshotRenderSections` captures the built position/tangent/UV/color buffers,
skin weights, section-local indices, cloth mappings and explicitly requested
morph deltas. Local indices allow comparisons when earlier sections gain
vertices. It currently requires one LOD. This is an editor-built-buffer audit,
not evidence of cooked or in-game equivalence.

`CompareMeshCloth` compares distinct meshes' clothing assets in array order.
It checks every reflected asset property, including physical mesh data and
instanced solver configurations, plus the non-reflected transition mappings
and tether arrays. It reports mismatches by property, shared-object aliases
and counts. A detached physical-data copy verifies that the comparison rejects
a changed simulation vertex. Empty clothing arrays and unsupported types fail.
The probe does not mutate or save either mesh. Matching original data is not
evidence that the original simulation behaves correctly in every pose.

Copy `Source/` into an isolated project with an editor module named
`CSSRigAudit`, then build its `CSSRigAuditEditor` target against the local
UE 5.6.1 installed engine. Enable PythonScriptPlugin and
EditorScriptingUtilities in that project. The namespace trial uses
[`migrate_namespace_trial.py`](../animations/migrate_namespace_trial.py).

Set `CSS_NAMESPACE_STAGE` to the isolated project directory. Its `input.json`
contains `assets`, each with `old_package`, `new_package`, `source_file` and
`source_sha256`. Copy the input assets and their authoring dependencies into
the private Content tree first. Optional `refresh_retarget_reference_pose`
requires a loadable source reference mesh and records the pose change
separately from the migration comparison. Never mutate source assets through
hardlinks. Use read-only mounts for the original project.

Run the script through `UnrealEditor-Cmd -run=pythonscript -script=...`.
After a successful migration, move the old redirector files out of Content
and run a fresh process with `CSS_NAMESPACE_VERIFY=1`. Cook and independently
decode the result before accepting it. Source-data equality does not establish
compressed byte identity, visual quality or game compatibility.

For the expanded shared batch, `stage_namespace_animations.py` selects pinned
sequences/blend spaces and only their explicit dependencies. Its required
native-reference receipt supplies the real game skeleton in the private tree;
the main authoring project's smaller placeholder must not be used for this cook.
`refresh_packages` limits retarget-reference refresh to the named legacy clips.

`package_namespace_trial.py` checks source/reload receipts and container payload
roundtrip. Run AssetReadback with `--animation-digests` on both the baseline and
candidate, then `compare_namespace_cook.py` to distinguish path changes from
changed compressed bytes. Differences are reported, not silently accepted.
`verify_namespace_motion.py` uses `CSS_NAMESPACE_STAGE` and `CSS_NAMESPACE_COOK`
to compare every independently decoded Eve key with raw UE poses. It writes
only to the staging project, so the cooked inputs can stay read-only. Its
`eve-decoded` input comes from `decode_shared_cooked.py --all-frames
--runtime-sampling` and the cook's explicit `catalog.json`.

The first accepted offline trial is recorded in
[`three-outfit-rig-physics-status.md`](../../../docs/development/three-outfit-rig-physics-status.md).
