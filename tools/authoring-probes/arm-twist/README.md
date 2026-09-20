# Arm twist diagnostics

These scripts preserve the rejected V44A experiment described in the
[binding findings](../../../docs/development/arm-twist-binding.md).
They are not a production V44 builder or a shipped motion correction.

Set `CSS_ARM_TWIST_AUDIT_DIR` to a direct child of
`CustomShellSystem/work/grip-grounding-v1/`. Existing run evidence lives in
`arm-twist-binding-v1`. All application commands use the workspace-only bwrap
manifests there; builds, caches, logs and backups stay inside the workspace.

- `probe_response.py`: Blender 5.2.2, Python 3.13.13. Reads the original Eve rig
  and sampled H2 poses without saving either source blend.
- `build_candidate.py`: same Blender. Creates the isolated V44A blend and bind
  JSON, refusing to overwrite them. Splits original weights and relocates eight
  helpers; verifies unchanged geometry, morphs and unrelated weights.
- `evaluate.py`: UE5.6.1 Python commandlet. Requires the diagnostic candidate
  reference imported from `candidate.mesh.json` plus the corrected game-source
  and existing diagnostic reference assets. Writes poses and engine bind
  readback. Production asset hashes must remain unchanged.
- `validate_poses.py`: host Python 3.14. Compares 40 paired results and the
  previous foundation control using Unreal's case-insensitive bone names.

The original command manifests describe candidate reference import and both
render invocations. The renderer requires an explicit candidate blend, expected
mesh identity and the independent engine bind readback. Do not substitute a
copy of the builder's JSON for that readback.

The engine uses a diagnostic triangle to evaluate the full reference pose.
Actual candidate skinning is evaluated in Blender. No production mesh import,
cook, runtime behavior or all-weapon acceptance is established by this fixture.
It samples five H2 frames and a partial player graph. Source controls are
explicitly put in FK mode for the response probe; the saved rig uses IK.

To reproduce pose validation against the existing results:

```bash
CSS_ARM_TWIST_AUDIT_DIR="$PWD/work/grip-grounding-v1/arm-twist-binding-v1" \
  python3 tools/authoring-probes/arm-twist/validate_poses.py
```

Run from `CustomShellSystem`. The archived builder remains a rejected
experiment. Do not rebuild or deploy it as the next release.
