# Authored H2 hand graph checkpoint, 2026-09-21

The isolated engine evaluator now retains the animation's authored retarget
source, animated virtual-bone tracks, both hand-anchor CopyBone nodes and the
left TwoBoneIK node. All 60 mechanical checks pass. The visible candidate still
has unacceptable wrist, finger and arm deformation. No repair is deployed.

This supersedes the pending H2 capture and hand-IK steps in earlier checkpoints.
V43 remains installed. Preserve its user-accepted grip improvement, original Eve
proportions, hair 200/24, body settings and disabled CSSX.

## Matching live reference

`work/grip-grounding-v1/v43-full-h2-reference-v2/` is the matching SeduXtress
capture: V43 mesh, CSS Skeleton, 388 evaluated bones, Martyr's Blade, H2 idle,
left/right correction alphas 1/0 and no additive body-adjustment pose. The weapon
is attached to `Socket_Prop_R`; its actor and static mesh have identity relative
transforms. Its Steam still and offline replay were inspected. The replay's
left distal-finger median gaps are approximately 22-29 cm; right medians are
0.15-1.08 cm. These unsigned distances do not measure penetration or grip quality.

Do not infer identity from a directory name. `v43-full-h2-reference-v1` actually
captured H1 with the Genessa overlay and both hand-IK alphas zero.
`morebeaute-hand-graph-live-v1` actually captured V43 after the selection changed.
The earlier `full-pose-current-v1` captured MoreBeaute. Read each fixture's mesh,
Skeleton, weapon and locomotion fields before comparing it.

## Engine evaluation and rejected fixture

`work/grip-grounding-v1/authored-h2-hand-graph-v2/` evaluates five original H2
samples on three meshes, raw and compressed, with left IK disabled/enabled.
It restores the 1,208-transform `SK_DarkForm` authored retarget reference and
retains all 102 animation tracks, including eight virtual-bone tracks.

The reproduced graph order is component-space conversion, right-hand CopyBone,
left-hand CopyBone, then left TwoBoneIK. The copies update `VB hand_r` and
`VB hand_l` from the actual hands in world space. Left IK uses
`VB hand_l_relative_hand_r` as effector and `VB lowerarm_l` as elbow reference,
including the decoded (-12, -12, -12) offset. Right IK is omitted because its
captured alpha is zero. The full player graph is not reproduced.

The previous `authored-h2-hand-ik-v1` omitted the CopyBone anchors and failed
the working-reference contact check. Keep it as a rejected fixture.

The corrected run exits zero. All enabled IK effectors reach their targets
within 0.001 cm; disabled IK preserves the effector. Authored pelvis translation
matches within 0.000011 cm. Production mesh and Skeleton hashes are unchanged.
The five imported sample positions do not constitute a continuous original clip
or validate transitions, motion matching, sidearm aiming or attack montages.

The diagnostic helper changes are preserved in
[diagnostic-hand-ik.patch](../../tools/authoring-patches/diagnostic-hand-ik.patch).
This patch is already applied to the workspace authoring project. Two editor
builds succeeded after an initial compile failure. All six guards pass:
production source/refresh/IK are rejected, as are truncated retarget references,
invalid alpha and missing pose data. See `guard-results.json`, `validation.json`
and `source-hashes.json` in the corrected run directory.

## Visual findings and phase control

Reviewed audit views include `authored-h2-graph-control-v2`,
`authored-h2-graph-candidate-v2`, their `*-s1-v2` variants and
`v43-live-h2-full-pose-v2`, under SeduXtress `work/nextgen-audit/`.
The reference-rotation candidate improves placement but still bends the wrist
sharply and leaves poor finger contact. The user's front/rear annotations add
both arm/elbow contours and the right wrist/grip to the acceptance checks.
Zero effector error is not a visual acceptance gate.

The source idle itself changes finger closure over time. A nearest-frame search
over all 384 original samples matches MoreBeaute's 19 left-finger rotations near
frame 220 (0.0182 degree RMS), and inverse-remapped V43 near frame 182 (0.4366
degree RMS). See `h2-phase-match-v1/result.json`. These are discrete approximate
matches, not exact synchronization. A sequential read of selected animation time
does not establish the phase of a later pose snapshot.

## Next discriminating comparison

Compare an isolated game-derived Skeleton foundation with the current compact
rotation candidate, preserving Eve's mesh bind pose and proportions. Account for
reference rotations, translation modes, authored retarget sources, virtual bones
and socket metadata separately. The canonical 258-bone prefix is a Genessa mesh
reference, not the complete game Skeleton. See the
[foundation research](skeleton-foundation-research.md).

Accept only after visual checks of both hands and arms across matched idle,
heavy-weapon attacks, movement and sidearm states, including the user-tolerated
Axe & Dagger and Axatana cases. Do not treat a new skeleton version number or a
compatible-Skeleton flag as evidence that deformation is repaired.
