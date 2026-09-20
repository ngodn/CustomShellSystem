# Arm rest-pose alignment, 2026-09-21

V44B improves arm/wrist alignment in the bounded heavy-weapon comparison while
preserving Eve's anatomical segment lengths. Left fingers remain unacceptable.
The attachment-preserving V44B2 derivative is the next authoring candidate,
not an installed repair. V43 remains installed and the full Next-Gen goal stays
active. The preceding [twist-only experiment](arm-twist-binding.md) is rejected.

## Coordinate and morph preservation

The builder aligns upper-arm and forearm directions with the working MoreBeaute
mesh reference, using projected palm normals to define rotation frames. Hands
and fingers use the corresponding measured V43 anatomical frames. It applies
rotations and hierarchical translations, with no bone scale or segment-length
adjustment. The 44 primary arm/hand parent distances differ by at most
0.00000673 cm. This changes the authoring pose, not Eve's intended limb lengths.

Per-vertex linear skin transforms are applied consistently to all stored mesh
coordinates and every shape key. Blender's independent linear-skin evaluation
agrees within 0.0000415 cm. After reopening both saved blends, all 5,503 shape
keys across 47 bound parts inverse-map to their original coordinates within
0.00000646 cm. Names, values, relative keys, ranges, weights, topology, UVs,
materials and object transforms are preserved. Unaffected vertices remain
exactly unchanged. Invertibility alone does not prove every posed contour.

The body's minimum blended-transform determinant is about 0.892. Individual
bone transforms are rigid, but linear blending can compress joint regions;
this remains a skinning consideration, not permission to claim every local
surface distance is invariant. No source blend or canonical reference is saved.

V44B inadvertently reoriented five unweighted leaf attachment bones along with
their limbs. V44B2 restores their original local transforms, verified against
the canonical reference: `Socket_Prop_InHand_Hook_R`,
`Socket_Prop_InHand_Lowerarm_L_01`, `Socket_Prop_InHand_Hook_L`,
`Socket_Prop_Upperarm_L_01` and `Socket_ShoulderBash`. All mesh and morph
coordinates remain identical to V44B. Quaternion roundoff below 0.000001 is
snapped to the exact canonical values, and the obsolete hand-frame exception
metadata is removed. V44B2 passes the ordinary protected-bind policy without
the experimental hand-rotation exception. No policy tolerance was weakened.

The full V44B2 Schema 1 export also completes with that ordinary policy:
379 bones, 133,066 points, 193,261 triangles, 30 materials and the same six
runtime morphs. All influences, fit values and material names match V43.
Vertex/UV/color associations are preserved. Ten reposed quads choose a
different triangle diagonal; their point sets, materials and oriented outer
boundaries are unchanged. Saved polygon topology is unchanged. This JSON
export is not yet a full-mesh Unreal import, cook or deployment.

## Engine and visual evidence

V44B's diagnostic reference import and the corrected H2 evaluator exit zero.
Eighty cases cover baseline/candidate, existing CSS/game-rotation references,
five original samples, raw/compressed data and left IK off/on. The largest
enabled IK error is below 0.001 cm. The test preserves authored source data,
animated virtual bones and the established CopyBone/TwoBoneIK subset.

In matched sample 95, right-arm direction differences from the working control
change from 7.6798 to 0.000638 degrees for the upper arm and 39.9420 to 0.000796
degrees for the forearm. Left upper-arm difference decreases from 31.36 to
18.08 degrees; left forearm difference increases from 20.54 to 24.19 degrees.
The source proportions differ and left IK adapts to the target. Neither arm
is stretched in this reviewed sample. These measurements support corrected
right-arm orientation, not universal pose acceptance.

Front/rear review shows cleaner arm/wrist contours with the game-rotation
reference. The old CSS reference still produces the wrong weapon pose.
Close views show unresolved left-finger folding and contact. MoreBeaute's
H2 idle also uses a relatively open supporting hand; do not force every idle
finger into a closed fist or treat its claw geometry as Eve's skinning target.

During review, the renderer's all-zero shape policy was found inconsistent
with the export. It is corrected and independently verified in
[fitted pose rendering](fitted-pose-rendering.md). The fitted candidate/baseline
views confirm the arm/wrist improvement and unresolved fingers. Earlier basis
renders remain diagnostic history, not final contact evidence.

## Artifacts and next steps

Evidence: `work/grip-grounding-v1/arm-rest-alignment-v1/`. Read `candidate.json`,
`saved-validation.json`, `attachments.json`, `canonical-policy-validation.json`,
`full-export-validation.json`, `evaluation.json`, `arm-direction-comparison.json`, `visual-review.json` and the
process result manifests. The earlier attachment derivative before canonical
quaternion snapping is preserved under `attachment-pre-canonical-snap/`.

Authoring blends are SeduXtress `work/CSS_SeduXtress_ArmRestV44B.blend` and
`CSS_SeduXtress_ArmRestV44B2.blend`, with adjacent bind JSON. Relevant reviewed
render folders are `arm-rest-v44b-game_rotations-s1-v1`,
`arm-rest-v44b-game-rotations-hands-s1-v1`, `arm-rest-more-control-hands-s1-v1`,
`arm-rest-candidate-fitted-hands-s1-v1` and
`arm-rest-baseline-fitted-hands-s1-v1`, under SeduXtress `work/nextgen-audit/`.

Reproducible sources are in [arm-rest probes](../../tools/authoring-probes/arm-rest/README.md).
Next use the fitted aligned candidate for the remaining finger articulation
and contact work, then expand to the requested weapon/default-animation matrix.
Preserve tolerable Axe & Dagger/Axatana behavior. A production asset must carry
the verified animation-reference correction and audited Skeleton metadata;
do not deploy the diagnostic triangle or silently replace the shared Skeleton.

The engine evidence currently uses V44B's diagnostic reference and Blender's
actual skinning. V44B2's five attachment leaves cannot influence the measured
arm chain, but its full mesh, cooked payload and live behavior still need
independent verification. Accepted hair 200/24, body dynamics, modularity,
original proportions, disabled CSSX and all other Next-Gen work remain in scope.
