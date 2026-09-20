# Isolated game-foundation diagnostics

These probes preserve the 2026-09-21 comparison. They are authoring diagnostics,
not shipped runtime behavior or a released skeleton v2. Read
[the results and limitations](../../../docs/development/game-foundation-comparison.md).

`prepare.py --output <new workspace directory>` runs with host Python 3.14. It
uses existing independently decoded game, MoreBeaute and CSS references. It
creates diagnostic Schema 1 inputs, explicit index mappings and input hashes.
The complete 1,199-record game prefix remains unchanged. Missing parents or
changed common-bone parent names fail before importing anything.

The diagnostic imports use the pinned UE5.6.1 `CSSImportMesh` commandlet with
`-AllowMirroredReferenceScale` because the game reference includes signed-scale
rope bones. All output assets must remain in `DiagnosticReferences`. Existing
command manifests under `work/grip-grounding-v1/game-foundation-v42-bind-v2/`
record the exact sandbox, caches, project and importer/evaluator commands.
Do not replace production Skeleton assets or repeat an import without checking
whether its diagnostic asset already exists and matches the input.

Set `CSS_FOUNDATION_AUDIT_DIR` to an output directory directly inside
`CustomShellSystem/work/grip-grounding-v1/` when running these archived probes:

- `evaluate.py`: UE Python commandlet, requiring both prepared JSON inputs and
  the previously imported diagnostic assets. Runs original sampled H2 tracks,
  authored source, hand copies and left IK on MoreBeaute, V43 and V42.
- `binding_guards.py`: UE Python commandlet, checks incomplete-source rejection,
  production-path rejection and accepted complete binding without reference edits.
- `audit_arm_twist.py`: Blender 5.2.2 background Python, reads the original Eve
  and V43 bindings and records twist-group coverage without saving either blend.

The editor helper patches are already applied in this workspace. The latest
`diagnostic-binding-coverage.patch` follows `diagnostic-game-reference.patch`;
`diagnostic-hand-ik.patch` supplies the graph/source helpers. Check reverse patch
application before changing existing source. Build after a helper change and
wait for every editor process to exit before rebuilding its module.

The source reference intentionally includes all 76 mesh-only MoreBeaute bones
before restoring virtual bones. Omitting them lets editor linkup mutate the
Skeleton and can crash asynchronous compression. A count check before the first
evaluation is insufficient; this probe verifies the reference after each pose.

Rendered V42 comparison uses the existing V40 source blend, explicit expected
mesh name `SK_SeduXtress_BodyRigV42`, and independently read `v42-bind.json`.
The renderer checks that bind before accepting an alternate blend. Neither the
render nor its unsigned finger distances establishes a successful grip repair.
