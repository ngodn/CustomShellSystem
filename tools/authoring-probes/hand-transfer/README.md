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

## Native thumb-tip stage

`prepare_thumb_clearance_fixtures.py` prepares all 464 retained B2 poses after
the measured native finger outputs. Set `CSS_NATIVE_CLEARANCE_FIXTURES` to a
fresh direct child of the grip work directory. It defaults to the accepted
`hand-native-finger-clearance-one-v3` report; `CSS_NATIVE_FINGER_RESULT_DIR`
can select another passed, terminal native report. The source hashes are
checked and carried into the fixture manifest. Thumb defaults remain the
fitted model's 12-iteration bound and 0.05-degree derivative interval.

`build_clearance_rig.py` reads `stage` and model settings from that manifest.
It builds either the four-finger or thumb-tip graph, checks computed math
separately from engine hierarchy writes, and saves only after strict gates
pass. `verify_native_clearance_skin.py` consumes the recorded native stage:
after native thumb output it runs only the offline web/corrective stages,
without solving the thumb a second time.

See [native thumb-tip clearance](../../../docs/development/native-thumb-tip-clearance.md)
for evidence, the exact 934-case finger regression after sharing graph code,
and remaining combined/cooked/live acceptance. The diagnostic package version
is V2 to preserve the previous saved finger probe; it is not a CSS skeleton
version or a deployed SeduXtress release.

## Independent corrective source check

`verify_corrective_source_deltas.py` compares the complete B2 exported hand
correctives against Blender-evaluated key-zero/key-one geometry. Set
`CSS_CORRECTIVE_SOURCE_AUDIT` to a fresh direct child of the grip work folder.
It independently maps neutral body points and checks all 16 deltas per point,
including omitted deltas below the exporter threshold. It preserves source
files and does not substitute for Unreal import/cook or GPU morph verification.

## Complete native hand sequence

`native_hand.py` composes all stages, isolates solver members and restores the
incoming hand if a stage rejects its input. The graph requires fresh poses.

- `prepare_combined_fixtures.py`, `CSS_COMBINED_HAND_FIXTURES`, reconstructs
  411 original decoded/overlay inputs and includes the 43 retained calibration
  fixtures. `verify_combined_inputs.py` uses the same output directory and
  `calibration.py` to compare all inputs with earlier calibration evidence.
- `build_combined_hand_rig.py` consumes that fixture directory and writes
  `CSS_COMBINED_HAND_DIR`. It compares a complete graph with the separately
  saved native stages, checks preservation and saves only after the gates pass.
  Graph construction may take several minutes while RigVM validates links.
  A live process or observation timeout is not a failed build.
- `verify_combined_hand_skin.py` consumes a passed terminal native report via
  `CSS_COMBINED_HAND_DIR` and writes `CSS_COMBINED_HAND_SKIN_DIR`. Its mesh
  replay uses native rotations and curves directly, with no offline repairs.
- `probe_combined_lifecycle.py`, `CSS_COMBINED_LIFECYCLE_DIR`, loads the saved
  combined asset and checks stale private curves, warm disable, invalid-input
  recovery and instance replacement without rebuilding the graph.

Output directories must be fresh direct children of the grip evidence folder.
See [combined native hand](../../../docs/development/combined-native-hand.md)
for measured evidence and pending full animation, asset and game acceptance.


## Sampled native motion

`prepare_native_motion_fixtures.py` writes `CSS_NATIVE_HAND_MOTION_FIXTURES`
from the original running-attack inputs, including retained regression
intervals. `probe_native_hand_motion.py` consumes that directory and writes
`CSS_NATIVE_HAND_MOTION_DIR`, using the saved complete rig without rebuilding.
After a terminal exit zero, `verify_combined_hand_skin.py` accepts that native
report and replays all 759 measured rotations and curve sets on B2 skin.
Output directories must be fresh direct children of the grip evidence folder.
The added-step measurement is descriptive, not a gameplay acceptance gate.


## Full B2 import and cooked readback

`verify_b2_import.py` checks the saved isolated B2 mesh/Skeleton in a fresh
UE editor process. It preserves exact translation/scale equality and bounds
only the double rounding introduced by Skeleton quaternion normalization.
`read_b2_cook.py` packs the explicit terminal-successful three-asset cook and
independently decodes it, without deployment. Run the existing geometry
verifier with `--check-uv`, then `verify_b2_cook.py` in Blender for all 22
morphs, float32 bind equivalence and cooked VM structure. These fixed evidence
folders are single-use. See [B2 cooked hand assets](../../../docs/development/b2-cooked-hand-assets.md)
for retained failure diagnoses, measurements and remaining runtime checks.


## Hand stage in the body/hair post-process

`probe_hand_postprocess.py` appends the saved hand rig to an isolated copy of
the accepted two-rig blueprint and evaluates actual components. Set
`CSS_HAND_POSTPROCESS_DIR` to a fresh grip evidence directory. The original
probe detects missing morph-driver metadata; `CSS_HAND_METADATA=1` performs
the controlled metadata-copy experiment with a retained negative control.
`verify_imported_morph_metadata.py` checks the importer fix after a fresh
process load. The editor changes are retained in
`../../authoring-patches/hand-postprocess-integration.patch`.

`read_b2_cook.py` also accepts `CSS_B2_COOK_DIR` and
`CSS_B2_COOK_READBACK_DIR`, both direct children of the grip evidence folder,
for explicit later cooks. `verify_hand_postprocess_cook.py` checks the cooked
mesh flags, unchanged ActorX data, 7/29/19-bone filters and inherited control
mappings. See [hand post-process integration](../../../docs/development/hand-postprocess-integration.md)
for the reproduced failure, evidence and remaining production/live work.


`probe_hand_postprocess_motion.py` adds 336 moving component samples at three
overlay weights, six animated public morphs, controlled head/pelvis/travel
motion, hand toggles, physics reset and instance replacement. Use a fresh
`CSS_HAND_POSTPROCESS_MOTION_DIR`. Its public-curve oracle is the actual
upstream compressed evaluation; source compression differences are recorded
separately. Apply the editor motion patch after the integration patch. This
checks transfer and coexistence, not complete combat playback or morph-driven
collision geometry.


## B2 reference binding

`probe_b2_animation_binding.py` checks the full B2 mesh against the retained
H2 retarget/IK evidence and raw hand convention. Set `CSS_B2_BINDING_DIR` to a
fresh grip evidence child; optionally set `CSS_B2_REFERENCE_SKELETON` to an
isolated saved reference candidate. `prepare_b2_reference_candidate.py` builds
the metadata-preserving copy and accepted defaults. Its fixed package names
are single-use. `verify_b2_reference_candidate.py` checks a fresh process and
35 actual component evaluations. The editor helpers are already applied from
`../../authoring-patches/b2-reference-metadata.patch`, after integration/motion.
See [binding evidence](../../../docs/development/b2-reference-binding.md) for
failed runs, the null Physics Asset, weapon review and remaining release gates.
