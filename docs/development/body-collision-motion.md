# Body collision during animation and simulation

The isolated V44 collision asset follows animation, but a reference-pose fit
alone missed curled fingers. The moving skin audit identified only hand-region
gaps, up to 2.642 cm left and 3.847 cm right. This is a collision-volume defect,
separate from the user's weapon contact and finger-deformation defects.

V43 remains installed. Nothing in this checkpoint proves live damage, parry,
weapon handling, ragdoll recovery or the complete Next-Gen architecture.

## Reproduction and scope

`tools/authoring-probes/hand-transfer/probe_body_physics_motion.py` uses UE 5.6.1
and `CSSPhysicsProbeLibrary::ProbeBodyMotion` from the retained
`tools/authoring-patches/b2-body-physics-probes.patch`. The editor library is
C++20. The Blender renderer/fitter use the pinned 5.2.2 Python environment.

The animation fixture authors a temporary two-second AnimSequence from five
retained game-derived H2 poses. It uses normal single-node animation and the
candidate post-process graph, with component translation and rotation. It does
not write evaluated pose buffers. This preserves real animation-to-body updates,
but the diagnostic timing and five samples do not cover every game animation.

The fall fixture starts 100 cm above the reference origin, enables all 22 human
bodies, and steps the real Chaos scene against a static floor. It records body
transforms, speeds and separation of all 21 constraint anchors. It does not tick
the full game world or execute the game's skeletal physics blend/recovery.

The render audit skins 24,480 main-body/modular-foot vertices using the exported
weights, measured component poses and actual active morph weights. It checks
the union of all measured collision shapes. For fall images only, unsimulated
joints inherit their reference local transforms from simulated parents. These
are diagnostic reconstructions, not gameplay recordings.

## Retained evidence

All artifact paths below are under `work/grip-grounding-v1`.

| Evidence | Result and limit |
| --- | --- |
| `b2-body-motion-engine-v1` | Editor build exits 0. |
| `b2-body-motion-engine-v2` | Retained build failure: morph-array entries are TObjectPtr, not raw pointers. |
| `b2-body-motion-engine-v3` | Corrected iteration with `.Get()` and normalized quaternion comparison builds with exit 0. |
| `b2-body-motion-v1` | V2 asset, 60 FPS: 121 animation frames and 241 fall frames. Position error 0; maximum joint gap 1.4332 cm during fall. |
| `b2-body-motion-v2` | V2 asset, 30 FPS: 61 animation frames and 121 fall frames. Position error 0; normalized rotation error at most 0.0000054 degrees. |
| `b2-body-motion-render-v1` | First skin audit finds hand gaps; fall camera crops part of an arm. Retained, not accepted as a complete visual check. |
| `b2-body-motion-render-v2` | Camera covers the measured trajectory. Region audit locates every uncovered vertex in the two hands. Includes diagnostic animation/fall MP4 files. |
| `b2-body-motion-audit-v1` | Same measured-pose audit without image rendering; records hand-local point clouds for the fit. |

Both update rates produce a 184.610 cm pelvis drop, maximum speed 756.237 cm/s,
and maximum anchor gap 1.4332 cm, below the retained 5 cm projection tolerance.
Minimum body-pivot height is 2.747 cm at 60 FPS and 3.015 cm at 30 FPS. These are
body-pivot and joint checks, not assertions that every skinned triangle remains
above the floor. The 60 FPS rotation metric predates quaternion normalization;
use the corrected 30 FPS result for angular accuracy.

Reviewed stills include V2 animation frame 15 and fall frames 15/60 from the
corrected framing. The body remains connected and settles near the test floor;
there is no visible large-scale limb separation in these views. The bent torso
and reconstructed skin require actual game ragdoll/blend validation before any
quality claim about final death/recovery animation.

## Hand-volume correction

`fit_body_physics_hand_motion.py` uses 48,639 measured points per hand across 31
poses. It changes only each hand box's center and dimensions. It keeps the
orientation and entire old box volume, so earlier reference-pose morph coverage
cannot be lost. All other bodies, constraints, solver properties and collision
exclusion pairs remain equal to the prior candidate.

`b2-body-hand-fit-v1` records zero remaining point gaps and a 0.35 cm fitting
margin. Box volumes grow from 1,098.6 to 2,516.4 cubic cm on the left and from
1,098.7 to 1,957.3 on the right. Those are primitive volumes, not anatomical hand
volume. The extra space covers open and curled fingers with a fixed box; it
must still be checked for unwanted contacts in gameplay.

`b2-body-hand-query-v1` saves the isolated
`/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V3` and passes 1,688 real
component ray observations plus 608 intended-body distance observations across
null/apply/remove/reapply. Its SHA256 is
`5a6e1a7cec42781d81031f3f9c786e0e3fc483e35ef6f403cea0df1c44a8183a`.
The mesh's Physics Asset remains null, because this probe applies the candidate
only to its temporary component.

## Corrected motion checkpoint

`b2-body-hand-query-v2` reloads the saved asset in a new editor process and
repeats all 1,688 ray and 608 distance observations successfully. Its file hash
is unchanged. `b2-body-hand-motion-v1`, in that same fresh process, retains
zero kinematic position error and at most 0.0000054 degrees rotation error.
The revised fall has maximum joint separation 1.3266 cm, maximum speed
734.895 cm/s, minimum body-pivot height 4.870 cm and pelvis drop 189.212 cm.
The commandlet exits 0. This revision was tested at 30 FPS; the earlier 60 FPS
result belongs to the previous hand boxes.

`b2-body-hand-motion-render-v1` audits all 31 measured animation poses and now
finds **zero uncovered vertices** among the 24,480 selected skin vertices.
Both hands pass with measured transforms and active morph weights. The three
reviewed stills (animation frame 15, fall frames 15/60) show connected limbs
and a body reaching the floor. Fixed enlarged hand boxes visibly leave space
around curled fingers and change the final settling pose. That tradeoff needs
gameplay contact validation. `visual-review.json` records the review scope;
`animation.mp4` and `fall.mp4` retain the diagnostic sequences. Rendering and
both encodes exit 0. Original source mesh/bind and retained inputs remain
hash-identical.

## Remaining gates

The [saved binding and cooked-reference checks](body-collision-binding.md) now
pass for an isolated V44 mesh.
Actual game damage/parry, animation-to-ragdoll-to-animation recovery, all weapon
and sidearm poses, collision filtering, performance and visual contact remain
open. Preserve source proportions, 379 authored bones plus nine virtual bones,
accepted hair 200/24, body motion, modular controls and disabled CSSX. Full
Next-Gen runtime/UI/profile/lifecycle/distribution acceptance remains active.
