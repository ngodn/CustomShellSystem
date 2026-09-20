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

## Native web stage

- `prepare_web_fixtures.py` runs in Blender with `CSS_NATIVE_WEB_FIXTURE_DIR`.
  It retains all 569 pre-web inputs and expected curve/rotation values. Inputs
  already contain calibration, hinge/proximal articulation and both clearance
  stages. The isolated web graph does not implement those preceding stages.
- `build_web_rig.py` runs in the UE5.6.1 commandlet with
  `CSS_NATIVE_WEB_AUDIT_DIR`. It authors the isolated V3 graph, verifies 1,143
  cases, measures paired editor execution cost, and saves only after checks
  pass. It refuses to overwrite an existing graph.
- `verify_native_web_skin.py` runs in Blender with `CSS_NATIVE_WEB_SKIN_DIR`.
  It checks all 569 measured native outputs on actual B2 skin.

These output variables also require fresh direct grip-evidence children.
See [native thumb web](../../../docs/development/native-thumb-web.md). Native
finger/tip clearance is still absent; its output is supplied by the fixtures.


## B2 directional clearance

- `fit_clearance_model.py`, `CSS_CLEARANCE_FIT_DIR`, fits the B2 neutral model
  and compares full radial support with its reduced hull.
- `directional_clearance.py` recomputes four-finger and thumb-tip clearance.
  Its optional legacy transport exists only for differential diagnosis.
- `verify_b2_clearance.py`, `CSS_B2_CLEARANCE_AUDIT_DIR`, uses saved articulated
  synthetic/captured inputs and actual B2 skin. Set
  `CSS_B2_CLEARANCE_SYNTHETIC=1` for all 464 cases. The successful run is v4.
- `verify_clearance_port.py`, `CSS_CLEARANCE_PORT_AUDIT_DIR`, compares six
  saved V43 outputs and isolates quaternion/matrix operation-order rounding.
  Its strict optimized equivalence gate still exits 2; inspect the separate
  rounding variants and actual-skin replay rather than treating this as green.

See [B2 hand clearance](../../../docs/development/b2-hand-clearance.md).


## Native articulation between calibration and clearance

- `articulation.py` implements the original distal hinges and experimental
  proximal swing gain, with whole-stage bypass for ambiguous X twist.
- `prepare_articulation_fixtures.py`, `CSS_ARTICULATION_FIXTURE_DIR`, compares
  446 saved full-matrix results and adds a singular-input control.
- `build_articulation_rig.py`, `CSS_NATIVE_ARTICULATION_AUDIT_DIR`, verifies
  899 native cases before saving the isolated, disabled-by-default graph.
  The verifier distinguishes computed rotations from the hierarchy's
  component-tolerance rule for skipping tiny writes.
- `verify_b2_clearance.py` accepts `CSS_B2_NATIVE_ARTICULATION_DIR` pointing to
  a completed native run. It reconstructs pre-articulation inputs with measured
  native output, then checks offline clearance and actual skin. The successful
  run is `hand-native-articulation-skin-v1`.

See [native hand articulation](../../../docs/development/native-hand-articulation.md).
The complete graph still needs native directional stages and fresh-chain,
weapon, motion and live verification.


## Native finger clearance and cost

The current candidate uses one correction step and 1-degree central-difference
sampling. Do not reintegrate the rejected 12-step port because its skin passes.
It was too expensive and failed the strict numerical comparison.

- `prepare_clearance_fixtures.py`, `CSS_NATIVE_CLEARANCE_FIXTURES`, prepares
  464 hashed inputs and expected rotations. The defaults are
  `CSS_FINGER_ITERATION_LIMIT=1` and `CSS_CLEARANCE_GRADIENT_DEGREES=1`.
- `native_clearance.py` builds bounded loops from shipped nodes. It caches
  geometry, prunes pairs conservatively and bypasses unsupported hand scales.
- `build_clearance_rig.py`, `CSS_NATIVE_CLEARANCE_DIR`, checks native execution,
  disabled/re-enabled instances, antipodes, scaled-input bypass and preservation
  before saving. `CSS_NATIVE_CLEARANCE_FIXTURES` selects an existing fixture
  directory; its default is `hand-native-clearance-fixtures-one-v3`.
- `verify_native_clearance_skin.py` takes `CSS_NATIVE_CLEARANCE_RESULT_DIR`
  and writes a fresh `CSS_NATIVE_CLEARANCE_SKIN_DIR`. It inserts measured native
  finger output before running the offline thumb stages and actual B2 skin.
  It can investigate complete rejected reports without promoting them.
- `verify_fresh_hand_transitions.py`, `CSS_FRESH_HAND_TRANSITION_DIR`, checks
  423 interpolated inputs across the running clip and known thumb regressions.
  It recomputes articulation and all clearance stages, with a default sampling
  step of 1 degree. It does not interpolate already-cleared output poses.
- `verify_b2_clearance.py` accepts `CSS_FINGER_ITERATION_LIMIT` for the retained
  iteration-budget comparisons. Its default remains the original 12-step
  offline baseline; it is not a production-runtime selector.

The output variables require fresh direct children of the grip-evidence
folder. Reuse the saved workspace-only command manifests. Native execution is
still isolated from the full animation pipeline; see
[native finger clearance](../../../docs/development/native-finger-clearance.md)
for performance, numerical failures, measured skin and remaining acceptance.
