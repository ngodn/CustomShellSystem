# Hand targets and corrective consumers

The pinned Eve and Genessa editor snapshots contain the hand rig's literal
targets, even though the cooked reader currently skips RigVM property-bag values.
This closes the source-target inventory gap. It does not establish the values
executed by the installed VM or prove good grip/contact during gameplay.

## Findings

Both source graphs have identical transform-writer and curve-writer records
after removing their package prefixes:

- 115 `FRigUnit_SetTransform` nodes target 19 distinct left-hand bones: four
  metacarpals and three joints of each of the five fingers.
- Target, space, initial-pose, propagation and weight pins have no incoming
  links, including parent `Item` links. All use local space, current pose,
  child propagation enabled and weight 1.
- Against each cooked skeleton hierarchy, targets plus descendants remain
  those 19 bones. Neither wrist nor right hand is in that set. Body/hair
  `BoneNames` arrays do not overlap it.
- 96 curve-write nodes refer to 16 distinct left-hand corrective curves. All
  16 names exist as cooked morph exports on the 12 active normal/facial Eve
  meshes and four active Genessa meshes. Eve's ten auxiliary mesh exports also
  contain them. Existence does not prove useful deltas or runtime activation.
- Commander White's decoded graph has body and hair rigs, not this adapter.

The source recipe explains why 115 nodes do not mean 115 sequential corrections
every frame. `tools/authoring-probes/hand-transfer/native_hand.py` captures incoming
locals, applies calibration, articulation, finger clearance, thumb clearance and
web/corrective stages, and contains restoration writes on rejected branches.
Disabled/rejected paths clear private curves. Branch activation and installed VM
behavior remain separate checks.

`native_clearance.py` builds posed finger segments and radial hull samples from
authored arrays. This hand self-clearance consumer does not replace a weapon
socket, Physics Asset or garment collider. These findings provide no reason to
change a weapon attachment or enlarge a general-purpose collider.

## Evidence and reproduction

Reports are under `work/three-outfit-rig-physics`:

| Report | Source snapshot and asset | Cooked hierarchy |
| --- | --- | --- |
| `eve-hand-targets1.json` | `eve-rig-editor2/before.json`, `/Game/CSS/SeduXtress/CR_Hand` | `eve-final-container1/decoded.json`, `/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base` |
| `genessa-hand-targets1.json` | `genessa-rig-editor2/before.json`, `/Game/CSS/UnholyGenessa/CR_Hand` | `genessa-face-namespace-pack2/decoded.json`, `/Game/CSS/eins0fx/UnholyGenessa/Skeletons/SKEL_Wings2` |

Each target report records input SHA256 hashes, inspected writers/pins, incoming
links, targets and potentially affected descendants. Run
`tools/diagnostics/inspect_hand_targets.py` with `--snapshot`, `--asset`,
`--readback`, `--skeleton` and a fresh `--output` using the table above.
`eve-hand-consumers1.json` and `genessa-hand-consumers1.json` record cooked
body/hair arrays and per-mesh missing-curve results.

`python3 tests/hand_targets_tests.py` passes six tests covering linked parent or
target fields, unrelated inputs, missing bones, invalid parents and descendant
propagation without siblings/ancestors. Linked settings remain unresolved.

No rig, mesh, package, installed setting or runtime DLL changed in this pass.
Next is posed deformation using the active mesh and actual animation input.
Cooked literal identity and runtime scheduling remain unverified.
