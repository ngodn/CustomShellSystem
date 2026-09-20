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

## Captured poses and native corrective curves

- `captured.py` runs actual Blender skinning for the retained 35 corrected
  captured poses on B2. Set `CSS_CAPTURED_HAND_AUDIT_DIR` to a fresh direct child
  of the grip evidence directory. It also checks the pre-thumb-correction
  failure. This tests local hand transfer, not a complete V44 gameplay graph.
- `export_corrective_driver.py` derives the 16 local corrective formulas from
  the original rig and checks them against 13 controls and 35 captured-pose
  fixtures. Set `CSS_CORRECTIVE_DRIVER_AUDIT_DIR` similarly. The output was
  archived byte-for-byte as `left-finger-correctives-v1.json`.
- `build_corrective_rig.py` runs with the UE5.6.1 Python commandlet. It builds
  `/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftFingerCorrectivesV1` from shipped
  native units and tests 53 cases. Set `CSS_CORRECTIVE_RIG_AUDIT_DIR` similarly.
  It refuses to replace an existing graph. The graph starts disabled and
  consumes already calibrated locals; do not attach it to raw game input.

See [native finger correctives](../../../docs/development/native-finger-correctives.md)
for evidence, input conventions and the remaining integration work.

## Native calibration and combined curves

- `left-finger-calibration-v1.json` archives the existing 19-joint model. Its
  original description refers to V43 captures; the native graph explicitly
  distinguishes those inputs from game-reference inputs.
- `prepare_native_fixtures.py` runs in Blender with
  `CSS_NATIVE_HAND_FIXTURE_DIR` set to a fresh grip-evidence child directory.
  Completed fixtures are in `hand-native-calibration-fixtures-v2`.
- `native_nodes.py` authors calibration and corrective nodes. It snapshots
  inputs, guards ambiguous twist, preserves metacarpal motion and passes
  disabled/singular input through while clearing its private curves.
- `build_calibration_rig.py` runs in the UE5.6.1 commandlet with
  `CSS_NATIVE_CALIBRATION_AUDIT_DIR` set similarly. It requires the v2 fixtures,
  refuses to overwrite the graph, and saves only after 91 checks pass.
- `verify_corrective_export.py` runs with host Python and compares the B2
  six-morph and 22-morph interchange files, including native baked-value agreement.

See [native hand calibration](../../../docs/development/native-hand-calibration.md)
for successful and rejected runs. The runtime clearance stage is not yet
implemented. A fresh input pose is required on every evaluation.

## Thumb web contacts on B2

These diagnostics save no blend or production asset. All output environment
variables below must name a fresh direct child of `work/grip-grounding-v1`.
Run with the retained workspace-only Blender command manifests.

- `skin_fixture.py` evaluates B2 through the actual Blender armature and
  existing triangle-crossing audit, with source corrective curves.
- `thumb_stress.py`, `CSS_THUMB_STRESS_AUDIT_DIR`, reproduces the six retained
  stress inputs. Five still fail on B2, so exit 2 is the expected baseline.
- `thumb_ablation.py`, `CSS_THUMB_ABLATION_AUDIT_DIR`, isolates twist, distal
  extension and corrective effects. `CSS_THUMB_ABLATION_STAGE=base-axis`
  selects the second axis-isolation experiment. Exit 0 means measurements
  completed; inspect its contact results before drawing a repair conclusion.
- `verify_batch_pose.py`, `CSS_BATCH_POSE_AUDIT_DIR`, compares sequential and
  batched pose assignments on all body vertices and exact contact pairs.
- `inspect_thumb_contacts.py`, `CSS_THUMB_CONTACT_AUDIT_DIR`, records actual
  triangle weights and distances to the joints.
- `thumb_base_sweep.py`, `CSS_THUMB_SWEEP_AUDIT_DIR`, measures source-Z contact
  boundaries. Its saved per-pose results are diagnostic, not runtime overrides.
- `thumb_web_guard.py` implements the empirical model. V1 passes saved poses
  but fails a retained subframe; V2 changes only the ceiling. Neither is an
  anatomical limit or a deployed driver.
- `verify_thumb_web_guard.py`, `CSS_THUMB_WEB_AUDIT_DIR`, checks 464 paired
  poses and preservation. Select `CSS_THUMB_WEB_MODEL=thumb-web-guard-v2.json`
  explicitly; the default retains the reproducible V1 experiment.
- `verify_thumb_web_transitions.py`, `CSS_THUMB_TRANSITION_AUDIT_DIR`, checks
  105 interpolated poses in the running intervals affected by the guard.
  The same model environment variable selects V2. V1 is expected to exit 2.

See [thumb-web contact](../../../docs/development/thumb-web-contact.md) for
the fitting data, rejected variants, successful checks and remaining gates.
