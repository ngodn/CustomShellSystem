# CSS skeleton foundation and hand retargeting

Research checkpoint: 2026-09-21. Sources are Epic documentation requested for
UE 5.6, the installed UE 5.6.1 source, and the recorded CSS diagnostic outputs.
No production asset or runtime behavior changed during this research.

The established problem is that CSS used a character mesh's reference skeleton
as its shared animation Skeleton reference. Compatible-Skeleton remapping then
interprets game animation through different reference orientations. Our isolated
rotation correction already improves the pose, but it does not establish that
every wrist, finger, elbow or clipping defect has the same cause. A foundation
built from the actual game Skeleton is a sensible controlled comparison, not a
proven complete repair. See the [engine candidate results](game-reference-import.md).

## What the current foundation actually contains

The original [builder](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools/build_skel_css_base.py)
loads a reference file named `SKEL_Human_Skeleton.refskel.json`, copies its base
records, then appends extensions. The earlier provenance check identifies that
258-bone reference as the Genessa mesh bind reference, rather than the complete
game Skeleton. The filename and old compatibility wording were misleading;
this was not a skeleton invented from scratch. [Recorded provenance and comparison](game-reference-import.md).

The [decoded game Skeleton](../../work/grip-grounding-v1/live-source-skeleton-v1/decoded/SKEL_Human_Skeleton.json)
contains 1199 raw bones, nine virtual-bone definitions, 82 sockets and 21 blend
profiles. The [current CSS reference](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/authoring/reference/SKEL_CSS_Base.refskel.json)
contains 379 raw bones: 182 names overlap the game reference and 197 do not.
A literal union would contain 1396 raw bones before virtual bones. This is an
inventory calculation, not a requirement to skin Eve to 1396 bones or evidence
that such an asset has been built successfully.

## Why compatible animations can still deform badly

**A Skeleton asset and a mesh's bind reference have different jobs.** Epic permits
multiple meshes to share a Skeleton and peripheral bones to be added without
changing the original hierarchy. The Skeleton also carries retarget sources,
slots, curves and blend data. A mesh may omit bones present in that shared
Skeleton. Therefore, fewer mesh bones are not themselves a bug, and copying
all game bones into Eve's mesh is not the objective. [Epic: Skeletons](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletons-in-unreal-engine?application_version=5.6).

**Compatibility does not certify deformation.** Epic describes compatible
Skeletons as a way to share animation assets, with nearly identical hierarchy
and naming, and similar proportions for ideal results. This supports using
compatibility as an asset-routing check, not accepting a character because an
animation plays without an error. [Epic: Compatible Skeletons](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletons-in-unreal-engine?application_version=5.6).

**Matching bone names does not make rotation frames identical.** UE 5.6.1
`FSkeletonRemapping::GenerateMapping` matches names and derives rotation
corrections from both Skeleton reference poses, including parent orientations.
Its ordinary local-rotation mapping is `Lt = Q0 * Ls * Q1`; additive animation
has a different formula. This is the mechanism implicated by the CSS
reference-rotation experiment. It is not proof that Unreal's mapping is faulty.
[SkeletonRemapping.cpp, GenerateMapping](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/Engine/Private/Animation/SkeletonRemapping.cpp).

**Translation retargeting is a separate stage.** Epic's traditional-retargeting
page describes `Animation`, `Skeleton` and `AnimationScaled`, and recommends
different treatment for pelvis, deform bones, and IK/weapon markers. Its
statement that this retargeting changes translation should not be read as a
claim that compatible-Skeleton evaluation never changes rotation. Local
`AnimationRuntime.cpp` applies compatible reference remapping before selecting
the translation mode. `Skeleton` uses the required mesh pose translation;
`AnimationScaled` uses the target length divided by the animation's authored
source length. [Epic: Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-retargeting-in-unreal-engine?application_version=5.6),
[AnimationRuntime.cpp, RetargetBoneTransform](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/Engine/Private/Animation/AnimationRuntime.cpp).

Mode tables alone do not establish effective runtime behavior. The target
Skeleton's `bUseRetargetModesFromCompatibleSkeleton` chooses source or target
modes. Its in-class default is **false**; both raw evaluation and compressed
decompression consult it. An absent property in a decoded export is not a
readback of the current live CSS object. Capture this flag before interpreting
the previously measured differences between CSS and game mode tables.
[Skeleton.h](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/Engine/Classes/Animation/Skeleton.h),
[AnimationDecompression.cpp](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/Engine/Private/Animation/AnimationDecompression.cpp).

**The animation's authored source matters.** Epic's retarget-source workflow
associates an animation with the mesh proportions it was authored for. The
game H2 fixture uses `SK_DarkForm`; replacing that with a generic Skeleton pose
changes the premise of a scale-retarget test. The current diagnostic restores
the authored reference and verifies its pelvis ratio.
[Epic: Retarget Manager](https://dev.epicgames.com/documentation/en-us/unreal-engine/retarget-manager-in-unreal-engine?application_version=5.6),
[current validation](../../work/grip-grounding-v1/authored-h2-hand-graph-v2/validation.json).

## Why a hand reaching its target can still look broken

Virtual bones provide animation-dependent reference frames for IK. Epic uses
them to control unwanted movement from blending without permanently pinning
hands to a weapon. Their definitions, animated tracks and graph evaluation
must all survive the diagnostic. [Epic: Virtual Bones](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-bones-in-unreal-engine?application_version=5.6).

The **game-specific** H2 subset copies right and left hand transforms into
virtual anchors before left Two Bone IK. This order is not a universal Epic
rule; it is part of this game's decoded graph. Our first fixture omitted those
copies and is rejected for contact assessment. The replacement fixture executes
the copies and actual engine IK, and passes 60 mechanical cases. Its scope
explicitly excludes the complete gameplay graph and all-weapon visual approval.
[Evaluation script](../../work/grip-grounding-v1/authored-h2-hand-graph-v2/evaluate.py),
[validation](../../work/grip-grounding-v1/authored-h2-hand-graph-v2/validation.json).

Two Bone IK controls a three-joint limb, with separate elbow, stretch, twist
and end-rotation choices. It does not solve finger closure or guarantee a
natural wrist contour. The local implementation emits upper-arm, forearm and
hand transforms only. Thus a tiny effector error is not a grip-quality test.
Remaining skin weights, joint placement, wrist orientation and pose phase are
investigation candidates, not established causes for every visible defect.
[Epic: Two Bone IK](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-two-bone-ik-in-unreal-engine?application_version=5.6),
[AnimNode_TwoBoneIK.cpp](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/AnimGraphRuntime/Private/BoneControllers/AnimNode_TwoBoneIK.cpp).

## What a v2 comparison must preserve and prove

Use the actual game Skeleton as the animation reference, then add audited CSS
extensions without inserting parents into existing game chains. Keep Eve's
mesh geometry, bind transforms, weights and morphs fixed for the first
comparison. UE skinning combines animated transforms with inverse bind
matrices, so changing bind data while retaining old weights/geometry is not a
neutral foundation change. [SkeletalRender.cpp, UpdateRefToLocalMatricesInner](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/Engine/Private/SkeletalRender.cpp).

Carry Skeleton metadata deliberately. A socket is a parent-relative attachment
object; a similarly named bone is not automatically an equivalent replacement.
Audit CSS names such as `Socket_Prop_R` against the real socket and mesh-socket
resolution before merging. Preserve authored retarget sources, virtual bones,
slots, curves and blend profiles as well as hierarchy and transforms.
[Epic: Sockets](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-sockets-in-unreal-engine?application_version=5.6),
[game metadata](../../work/grip-grounding-v1/live-source-skeleton-v1/decoded/SKEL_Human_Skeleton.json).

Acceptance gates for the isolated comparison:

1. Record source/target asset identity, bone-name and parent mapping, reference
   transforms, effective retarget modes, authored source, virtual definitions,
   socket resolution and metadata. Do not accept equal bone counts as identity.
2. Verify unchanged Eve bind geometry, mesh transforms, morphs and production
   hashes. Keep extra-bone index migration explicit and check missing tracks.
3. Recover the working MoreBeaute control first. Evaluate V43 and each candidate
   at matching animation times, with original virtual tracks and graph order;
   compare raw and compressed results. Existing five-sample fixtures establish
   mechanics, not continuous-motion correctness.
4. Review rendered wrists, arm contours, fingers, weapon contact and clipping
   alongside measured transforms. Compare corresponding grip/release phases;
   an intentionally open source hand is not automatically a CSS failure.
5. Validate cooked assets in game across heavy weapons, sidearm aim, idle,
   movement, turns and attacks, then confirm Axe & Dagger and Axatana do not
   regress. Preserve accepted hair/body settings and measure frame time.

If the corrected foundation still needs substantial proportion-aware contact
correction, Epic's IK Retargeter supports different bone counts and orientations
with optional hand/foot goals. That is an alternative to evaluate, not evidence
the shipped game already invokes it. The documentation URL requests 5.6, but
specific UI/features still require checking against installed 5.6.1 before use.
[Epic: IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.6).

The v2 test does not establish a repair for hit-through/parry, grounding, missing
heel geometry or unrelated preview behavior. Those need their own evidence.
