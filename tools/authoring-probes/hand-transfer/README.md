# Aligned hand calibration check

This is a diagnostic for [V44's finger transfer](../../../docs/development/aligned-hand-calibration.md),
not a deployed animation driver. It uses Blender 5.2.2 / Python 3.13.13 and the
existing original-Eve calibration parameters, V43/V44B2 blends and engine H2
fixtures in the workspace. No blend or game file is saved.

Set `CSS_HAND_TRANSFER_AUDIT_DIR` to a fresh direct child of
`CustomShellSystem/work/grip-grounding-v1/`, then run `probe.py` with Blender's
`--background --factory-startup --python-exit-code 1 --python` options. Use the
workspace-only sandbox command from the evidence directory. The saved probe
and command manifests are in `arm-rest-hand-transfer-v1`; completed five-sample
evidence is in its `all-h2-samples-v2` subdirectory.

`verify.py` runs with host Python 3.14. Pass the evidence directory and either
`--variant baseline` or `--variant calibrated-no-correctives`. The baseline
must exit 2 and the corrected candidate must exit 0. Both use the same exact
triangle-pair criterion, including same-finger folding. This gate does not
test weapon penetration, full graph overlays, continuity, other animations,
or live performance.

The input-domain check compares raw engine rotations with raw extracted
tracks. Compressed-versus-raw differences are reported separately. Do not
undo V43's compatible-Skeleton conversion on the game-reference H2 inputs.
