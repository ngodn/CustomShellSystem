# UE 5.6.1 source animation audit

This C++20 editor module exposes two commandlet-only snapshot helpers to Python.
It is authoring tooling, not a CSS runtime module.

`SnapshotAnimation` reads named source tracks through `IAnimationDataModel`,
hashing explicit transform components rather than struct padding. It records
frame/key counts, sample rate, model GUID, curve/attribute counts and retarget
reference transforms. `SnapshotSkeleton` records raw bones, parent indices,
retarget modes, reference transforms, virtual definitions and sockets.

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

The first accepted offline trial is recorded in
[`three-outfit-rig-physics-status.md`](../../../docs/development/three-outfit-rig-physics-status.md).
