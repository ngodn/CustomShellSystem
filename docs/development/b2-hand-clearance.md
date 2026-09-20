# B2 directional hand clearance

The geometry model has been refitted to V44B2 and replayed from articulated
inputs. All 464 saved poses have no new hand-skin intersections: 411 synthetic
animation/overlay poses, 35 captured poses, 13 source controls and five H2
samples. The process exits zero. This is an offline hand result, not weapon,
full-arm, motion, morph-range or game acceptance.

## Required input order

Calibration must be followed by the original-Eve articulation adaptation before
directional clearance. Ten distal joints retain their source X hinge; four
non-thumb proximal joints use experimental swing gain 0.125. Independent curls,
thumb-base motion and metacarpals are retained. The gain is not a final accepted
setting. The complete intended sequence is:

1. Game-input calibration.
2. Source-axis articulation.
3. Four-finger directional clearance.
4. Thumb-tip directional clearance.
5. Thumb-web protection.
6. Original corrective curves.

`hand-clearance-b2-replay-v3` omitted step 2 by selecting pre-articulation input
directories. Its 209 failures do not reject the B2 geometry fit. V4 uses the
saved articulated inputs and recomputes all three clearance stages, rather than
reusing saved clearance outputs. The 13 source controls and five calibrated H2
samples are separate groups that enter directly; they do not establish a full
fresh calibration/articulation evaluation.

The earlier v1/v2 harness attempts stopped on bone-name case and an unrelated
thigh scale assertion. Canonical names are now compared case-insensitively and
all original scales are retained, rather than assuming the whole pose is unit
scaled. These failed attempts are retained as diagnostic evidence.

## Geometry and checks

`hand-clearance-b2-fit-v1/model.json` fits directional segment bounds to the
neutral fitted B2 skin. Mixed thumb-web tissue is intentionally outside these
bounds. Each segment stores local radial samples, its reduced convex hull and
the corresponding skeletal indices. The source blend hash remains unchanged.

The hull reduces 1,089 radial points to 193. Checking 722 directions per segment
gives a maximum support difference of 0.0000000399 cm. The solver uses B2 local
translations with fixture rotations/scales. Finite differences evaluate only
the selected contact pair. Correction limits and iteration counts retain the
existing solver settings.

`hand-clearance-b2-replay-v4/report.json` records all 464 actual Blender skin
checks, exact preservation of unrelated local transforms and all translations
and scales, and unsaturated web correction. Forty-seven cases also compare the
full radial set against the reduced hull through the entire clearance sequence;
the maximum resulting rotation difference is 0.00000513 degrees.

The cylinder proxy remains conservative: 117 finger cases do not satisfy its
clearance threshold after the bounded iterations, although the tested actual
skin has no new triangle crossings. Do not describe these as 464 solved proxy
constraints. The triangle check also does not cover containment, all coplanar
contacts or weapon geometry.

## Differential rounding investigation

`hand-clearance-port-check-v3` compares six retained poses against saved V43
solver outputs, independently of the B2 refit. The optimized local-bound version
differs by up to 0.001284 degrees in finger rotation and 0.000223 degrees in
thumb rotation. Its strict 0.001-degree equivalence gate still exits 2.

One-variable tests restore the old palm quaternion composition and the old
world-space radial matrix transport separately, then together. Restoring both
reduces the maximum finger difference to 0.00000483 degrees without changing
the solver equations or iteration limits. This identifies operation-order
rounding amplified by finite differences, not missing articulation in this
corrected differential test. The strict gate has not been loosened or relabeled
as passing. B2 acceptance evidence is the separate actual-skin replay above.

## Remaining integration

Implement articulation and directional stages natively, then test the complete
fresh-input chain and transitions, including measured native skin outputs and
cost. Complete full B2 import/cook and weapon, sidearm and live-game checks.
V43 remains installed. Original body proportions, accepted hair/body dynamics,
disabled CSSX and the complete modular Next-Gen scope remain preserved.

Evidence directories are under `work/grip-grounding-v1/`. Reproduction scripts
are in [hand-transfer probes](../../tools/authoring-probes/hand-transfer/README.md).
