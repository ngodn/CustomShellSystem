# Chest contact: soft-tissue collision against arms, hands, thighs and floor

Date: 27 Oct 2026. Research only. Nothing was built, cooked, installed or run in the game.

Problem: in idle poses (kneeling bow, forward fold, side kneel, reclining, low lean, seated recline) the chest surface passes through the character's own arms, hands, thighs and the floor. Goal: no pass-through, believable squash, spread and slide where pressed, natural settling, response that scales with real contact depth across body slider sizes, smooth onset and release, flat frame cost, and a data-driven opt-in for future animations and outfits.

Evidence labels used below:

- **[src]** verified in the local UE 5.6.1 engine source (`CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/...`) or in our own tools.
- **[game]** verified in the game's UE4SS reflected-class dump, `/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Binaries/Win64/ue4ss/CXXHeaderDump/`. The dump is dated 13 Sep 2026, so a later game patch could differ.
- **[web]** read from the cited page or repository during this pass.
- **[inf]** my inference. Not verified.

## Short answer

The pass-through in these idle poses is mostly **static**: the animation's own target pose already puts the arm or thigh inside the chest. Jiggle is not the main cause. The current rig cannot fix that by design. `controlrig_body_planes.py` assumes "at theta=0 the pose-relative constraint is feasible". `controlrig_body_contacts.py` freezes the helper when the incoming plane is already infeasible ("does not invent an outward displacement or alter body shape"). Its only colliders are two body half-spaces and the opposite breast. So the fix has to move the chest's rest pose and change its surface, not just clamp the spring. [src: both files in `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools/`]

These are the options, cheapest first. All of them fit the "cook assets, extend the DLL, no engine plugins" rule:

1. **Contact offset in the existing angular rig (recommended first step).** Once per evaluated frame, before the 120 Hz step, test each chest ellipsoid against a few bone-attached capsules (upper arm, forearm, hand, thigh) and one floor plane.
   - Turn the penetration into a rotation away from the collider about the helper pivot. Give it its own travel budget, and low-pass it with a critically damped filter so contact never pops.
   - Make that the spring's target, so jiggle and settling continue around the pushed pose.
   - Each contact also gives the existing analytic half-space projection a supporting plane, which is the safety net against fast motion.
   - This is closed-form math with RigVM units the game already ships, on two helpers. Cost should stay flat.
2. **Residual depth drives a few squash and spread morphs, through ControlRig curves.**
   - The rotation can only take up a few cm (chest skin travel is 1.5 cm at the 1.5.0 cap and 2.35 cm at 6 degrees). Deeper presses need the surface to change.
   - The rig writes curves named after a few "press" morphs per breast. Each is weighted by residual depth and by the contact normal's direction in the breast frame, the same idea as an RBF corrective.
   - UE 5.6.1 copies rig curves to the pose and turns them into morph weights when the curve has morph-target metadata on the skeleton or mesh.
   - The GPU morph buffer is rebuilt only on frames where some morph weight changes. A deadband on the press weights therefore keeps a held pose free once it settles.
3. **Slider scaling.**
   - The contact ellipsoid has to follow PBMBreastsSize. Today the rig bakes in only morph corner 0. The measured chest semiaxes are 8.25/6.4/5.15 cm at PBMBreastsSize 0 and 12.5/9.69/7.8 cm at 1, with 12.63/9.79/7.88 at all six morphs on.
   - CSS native already writes AnimInstance properties, so it can write the current slider weights, or ready-made semiaxes, as rig inputs.
   - A small chest then never reaches the arm, and a large one presses deeper. No per-size tuning is needed.
4. **Fallbacks, not recommended as the main path:**
   - AnimBP RigidBody node with a physics asset: kinematic arm and thigh capsules, simulated breast spheres. Force-based, and the game ships it.
   - A Deformer Graph (Optimus) GPU kernel that pushes vertices out of capsules: real per-vertex deformation, and the game ships OptimusCore and ComputeFramework.
   - Both are much heavier to build and validate. See the table.
5. **Not available in Mortal Shell II without shipping engine plugin code:** KawaiiPhysics, Physics Control and ML Deformer. None of their modules is in the game dump.

## Comparison table

| Approach | What collides | Resolution | Surface deforms? | Cost | Main artifacts | Usable in MS2 |
| --- | --- | --- | --- | --- | --- | --- |
| Skyrim CBPC | Breast/butt/belly bones vs hand, arm and finger spheres and capsules; optional ground | Position push of the bone, clamped, averaged with last frame | No, bone only | Cheap, parallel, spatial hash | Jitter with `collisionElastic`, hand-fitted spheres per body | Pattern only |
| HDT-SMP / FSMP | Bullet rigid bodies, per-vertex or per-triangle shapes | Bullet impulse and constraint solve | Bones only, weighted | 1 to 3 ms normal, >8 ms heavy | Puffed margins, twitching low-weight vertices | No (Skyrim plugin) |
| VaM soft physics | Soft vertex joints with colliders vs auto colliders on hands etc. | PhysX joints and contacts | Yes, locally | Heavy (unverified) | Not documented | No |
| DynamicBone (Illusion) | Particles vs sphere and capsule colliders | Position projection, Verlet | No | Cheap | Length restore can pull back inside | Pattern only |
| KawaiiPhysics | Bone particles vs sphere, capsule, tapered capsule, box, plane, physics asset | Hard position projection | No | Cheap | Possible bounce, since PrevLocation is not corrected [inf] | Not in MS2 dump |
| UE SpringBone (Better Jiggle) | Nothing | Travel clamp only | No | Very cheap | Pass-through | Yes, but no contact |
| UE AnimDynamics | Bodies vs planar and spherical limits | Velocity-level limit constraints | No | Cheap | Limited shapes | Yes [game] |
| UE RigidBody (Chaos immediate) | Physics asset bodies, simulated vs kinematic, optional world | Chaos solver, impulses | No | Moderate, scales with bodies [inf] | Jitter, tuning, teleports | Yes [game] |
| Physics Control | RigidBody plus spring drives to animation | Chaos solver plus drives | No | Moderate | Experimental plugin | Not in MS2 dump |
| Pose Driver (RBF) | Nothing (pose space) | Curves from bone pose | Via morphs | Cheap | Pose only, does not react to actual contact | Yes [game] |
| ML Deformer | Nothing (pose space) | Learned morph weights | Yes | CPU net plus morphs | Pose only, needs training data | Not in MS2 dump |
| Deformer Graph (Optimus) | Any analytic shape in a custom kernel | Per-vertex GPU push | Yes | GPU kernel per vertex | Beta, cooked shaders, garments must match | Present [game], high risk |
| **CSS rig contact offset + press morphs (proposed)** | Chest ellipsoids vs bone capsules plus floor plane | Filtered rotation offset, compliant push, plane safety net, residual to morphs | Yes, via a few morphs | Two helpers, a few capsules, closed form | Morph authoring per body and garment | Yes |

## 1. How existing game and mod solutions do it

### Skyrim CBPC (CBP Physics with Collisions, Shizof)

- **What collides:** spheres instead of vertices, evaluated every frame. Breasts, butt, belly and vagina collide with NPC or player hands and genitals, plus VR hands and weapons. [web: https://www.nexusmods.com/skyrimspecialedition/mods/21224]
- **Source:** the readable source is the MIT repo Acro748/CBPCSSE, which may lag v1.7.3. [web: https://github.com/Acro748/CBPCSSE]
- **Config sections** (`CBPCollisionConfig.txt`):
  - `[AffectedNodes]` lists breasts, butt, pelvis and spine1.
  - `[ColliderNodes]` lists upper arms, forearms, hands, five finger bones per hand and genitals.
  - `[PlayerNodes]` lists the VR wand nodes.
  - [web: https://github.com/Acro748/CBPCSSE/blob/HEAD/config/Basic/skse/plugins/CBPCollisionConfig.txt]
- **Shape syntax:** a sphere is `x0,y0,z0,radius0 | x100,y100,z100,radius100` in bone-local units. The two halves are the values at NPC weight 0 and 100, interpolated by weight (`GetPointFromPercentage`). A capsule joins two such ends with `&`, so it can taper. A `: 0.3` suffix is `scaleWeight`, which sets how far the collider follows bone scale. [web: same file; https://github.com/Acro748/CBPCSSE/blob/HEAD/CBPC/Collision.cpp, https://github.com/Acro748/CBPCSSE/blob/HEAD/CBPC/config.cpp]
- **Exclusions and ground:** exclusions are written `Node (@)` or `(@NPC L Hand [LHnd])`. Ground collision comes from adding `NPC Root [Root]` as a collider, and the ground push is limited to one collider radius. [web: https://github.com/Acro748/CBPCSSE/blob/HEAD/CBPC%20newer%20features.txt, Nexus page]
- **Resolution** (`Thing.cpp update()`, `Collision.cpp IsItColliding`):
  1. The spring step runs first.
  2. Each overlapping sphere or capsule adds `(limitDistance - currentDistance)` along the centre-to-closest-point normal.
  3. The sum is clamped per axis (`collision*Offset`).
  4. It is scaled by `1 - collisionPenetration`.
  5. It is averaged with last frame, `(v + buffer) * 0.5 * collisionMultipler`. The key really is spelled that way.
  6. The result is added to the bone's local position.
  - Next frame's velocity is multiplied by `1 - collisionFriction`. With `collisionElastic 1` the pushed position feeds back into the spring state.
  - Only bones move. [web: https://github.com/Acro748/CBPCSSE/blob/HEAD/CBPC/Thing.cpp]
- **Artifacts:** the author notes jitter with `collisionElastic`, and there is a `NoJitterFixNodes` list. Low bone weights make the push look smaller. [web: newer features.txt]
- **Lesson for CSS [inf]:** the cheap bone-push model is enough for "no pass-through" in most poses. Its weak points are the frame-to-frame averaging hack and the lack of surface change. The weight-0/weight-100 collider syntax is exactly what CSS needs for slider scaling.

### HDT-SMP / FSMP (Skyrim)

- **Bodies and shapes:** Bullet rigid bodies. `mass > 0` makes a bone dynamic, mass 0 makes it kinematic. Collision uses `per-vertex-shape` spheres or `per-triangle-shape`, with `margin` (default 1) and `penetration` (default 1). [web: https://github.com/DaymareOn/FSMP-Validator/wiki/36-‐-Collision-shapes-per-vertex-vs-per-triangle]
- **Resolution:** the Bullet solver produces impulses, not projection. Defaults are `numIterations 16`, `erp 0.2`, `maxSubSteps 4`, and restitution values multiply. [web: https://github.com/DaymareOn/FSMP-Validator/wiki/37-‐-How-collisions-and-rebound-work, https://github.com/DaymareOn/hdtSMP64/blob/HEAD/configs/configsPresets/Default.json]
- **Cost:** `budgetMs 3.0`. The project's own scale calls 1 to 3 ms "normal" and over 8 ms heavy, and 120 Hz physics costs about twice 60 Hz. [web: https://github.com/DaymareOn/hdtSMP64/wiki/03-‐-Configuration]
- **Artifacts:** a large margin looks puffed and collides in mid-air, low-weight vertices twitch, and bounce reads as jitter. [web: wiki 36/37, https://github.com/DaymareOn/hdtSMP64/wiki/05-‐-Solving-problems]

### Virt-A-Mate

- **Two layers:**
  - A main breast joint (`BreastControl`: `spring`, `damper`, `positionSpringZ`, `mass` and others).
  - "Soft physics" (`BreastPhysicsMesh`: `softVerticesSpring`, `softVerticesDamper`, `softVerticesMass`, `softVerticesColliderRadius`, `softVerticesDistanceLimit`, `softVerticesUseAutoColliderRadius`, `allowSelfCollision`), with vertex groups for outer, areola and nipple.
  - [web: https://github.com/everlasterVR/TittyMagic/blob/HEAD/src/Static/ParamName.cs, https://github.com/everlasterVR/TittyMagic/blob/HEAD/src/Handlers/SoftPhysicsHandler.cs]
- **Hard colliders:** auto colliders `lPectoral1..5` with force, radius and centre. [web: https://github.com/everlasterVR/TittyMagic/blob/HEAD/src/Handlers/HardColliderHandler.cs, https://github.com/acidbubbles/vam-collider-tuner]
- **Mechanism (decompiled, unofficial):** each soft vertex set is a Unity Rigidbody with a ConfigurableJoint and a collider, so contact is PhysX impulse based and the skin deforms locally. [web: https://github.com/chelw97/1.0_2020/blob/master/Assembly-CSharp/DAZPhysicsMesh.cs]
- **Cost and artifacts:** no primary source found.
- **Lesson [inf]:** the best looking result comes from many small soft regions with their own colliders. That is a per-vertex solver, which we can only approach with morphs or a GPU deformer.

### Illusion (Honey Select, Koikatsu) and DynamicBone

- **Collider fields:** `DynamicBoneCollider` has `m_Center`, `m_Radius` (0.5), `m_Height`, `m_Direction` and `m_Bound` (Outside/Inside). It is a sphere when `m_Height*0.5 - m_Radius <= 0`. [web: https://github.com/katboi01/UmaViewer/blob/HEAD/Assets/Scripts/DynamicBone/Scripts/DynamicBoneCollider.cs]
- **Resolution:** pure Verlet position projection, `p = c + d * (r/len)`.
  - Order per step: stiffness, collide, freeze axis, then length restore. Length restore can pull a particle back inside.
  - `m_Friction` is added to damping after a hit.
  - Defaults: `m_UpdateRate 60`, `m_Damping 0.1`, `m_Elasticity 0.1`, `m_Stiffness 0.1`.
  - [web: https://github.com/katboi01/UmaViewer/blob/HEAD/Assets/Scripts/DynamicBone/Scripts/DynamicBone.cs]
- **KK_Plugins "Colliders":** adds floor, breast, hand and skirt colliders.
  - The floor is a giant sphere: radius 100 at `(0,-100.01,0)` on `cf_j_root`.
  - Arm capsules: hand r 0.020 h 0.075, forearm 0.025/0.25. Thigh capsules: 0.095/0.32 and 0.083/0.35.
  - The breast particle radius is `0.10*BustSize` in KK and 0.6/0.8 in HS2/AI.
  - [web: https://github.com/IllusionMods/KK_Plugins/blob/HEAD/src/Colliders.Core/Core.Colliders.Constants.cs, https://github.com/IllusionMods/KK_Plugins/blob/HEAD/src/Colliders.Core/Core.Colliders.Controller.cs]
- **Lesson [inf]:** scaling the breast collision radius with the bust slider, and using a large sphere or a plane as the floor, are proven cheap tricks.

### Stellar Blade jiggle mods

- **Better Jiggle** (local copy v2.6.3) writes properties of the `AnimGraphNode_SpringBone_N` nodes on `CH_P_EVE_01_AnimBP_New` and its Studio variant. [src: `reference-tools/Better Jiggle Mod v2.6.3 .../ue4ss/Mods/JiggleUpdate/Scripts/main.lua`, lines 52 to 61]
- **Collision:** stock `FAnimNode_SpringBone` has no collision members, only `bLimitDisplacement`/`MaxDisplacement`. [src: `Engine/Source/Runtime/AnimGraphRuntime/Public/BoneControllers/AnimNode_SpringBone.h`; web: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/AnimGraphRuntime/FAnimNode_SpringBone]
- **Jiggle Control** (Nexus 3466) edits the same node family and adds impulses. No collision. [web: https://www.nexusmods.com/stellarblade/mods/3466]
- **Side finding:** Stellar Blade itself ships KawaiiPhysics. `CH_P_EVE_08_AnimBP` has 7 `FAnimNode_KawaiiPhysics` nodes. [src: `/home/eins0fx/development/mods/stellar-blade/research/CXXHeaderDump/CH_P_EVE_08_AnimBP.hpp`]

### KawaiiPhysics (UE plugin)

- **Shapes:** `SphericalLimits`, `CapsuleLimits`, `TaperedCapsuleLimits`, `BoxLimits`, `PlanarLimits`, `LimitsDataAsset` and `PhysicsAssetForLimits`. Defaults: sphere `Radius 5`, capsule `Radius 5`/`Length 10`, particle `Radius 3`, `Damping 0.1`, `Stiffness 0.05`, `TargetFramerate 60`. Release 1.21.0 has a 5.6 build. [web: https://github.com/pafuhana1213/KawaiiPhysics, https://github.com/pafuhana1213/KawaiiPhysics/releases]
- **Resolution** (`AnimNode_KawaiiPhysicsCollision.cpp`): hard projection using particle radius plus shape radius.
  - Capsule: closest point on segment, then `p = closest + dir*(Rb+Rc)`.
  - Plane: also checks the PrevLocation-to-Location segment against tunnelling.
  - No compliance.
  - [web: https://github.com/pafuhana1213/KawaiiPhysics/blob/HEAD/Plugins/KawaiiPhysics/Source/KawaiiPhysics/Private/AnimNode_KawaiiPhysicsCollision.cpp]
- **Not usable in MS2:** no KawaiiPhysics module is in the MS2 dump [game], and we cannot ship plugin code. Use it as a reference for shapes and order only.

### UE RigidBody, Physics Control, AnimDynamics

- **RigidBody node:** `FAnimNode_RigidBody` runs a Chaos immediate-mode simulation of the physics asset.
  - Bodies whose `PhysicsType` is Simulated become dynamic actors, and the others become kinematic actors that follow the animation. [src: `AnimNode_RigidBody.cpp` lines 1142 to 1143, 779]
  - Collision pairs are ignored from the physics asset `CollisionDisableTable`, from constraints with collision disabled, and optionally from `bForceDisableCollisionBetweenConstraintBodies`. So self-collision between a simulated breast body and a kinematic arm capsule of the same asset works unless the table disables it. [src: lines 1202 to 1294]
  - World geometry is off by default (`bEnableWorldGeometry false`). [src: line 199]
  - The LOD gate is `p.RigidBodyLODThreshold`. [src: line 64]
  - Epic's guide says that with default settings collision is disabled on all bodies, and it describes Component, World and Base Bone simulation spaces. [web: https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-rigid-body-in-unreal-engine]
  - The game ships the node and uses it in its own post-process AnimBPs. 11 dumped classes contain it, for example `ABP_Tiel_Post`, `ABP_Blacksmith_PostProcess` and `ABP_PapaRach_PostProcess`. [game]
- **Physics Control:** the plugin is `IsExperimentalVersion: true` in 5.6.1 [src: `Engine/Plugins/Experimental/PhysicsControl/PhysicsControl.uplugin`]. No PhysicsControl module is in the MS2 dump [game]. Not usable.
- **AnimDynamics:**
  - Planar and spherical limits are velocity-level linear limits, with target speed = distance / dt. [src: `Engine/Source/Runtime/Engine/Private/Animation/AnimPhysicsSolver.cpp` lines 769 to 817]
  - Bodies can use `CustomSphere` collision with `SphereCollisionRadius` (default 10). [src: `AnimNode_AnimDynamics.h` lines 246 to 283]
  - It is in the game. [game]

### Commercial engines and research

- **Studio talks:** none found from Naughty Dog, Santa Monica, Insomniac or Shift Up on breast or soft-tissue contact. GDC Vault was in maintenance during this pass. [web]
- **Ziva:** Unity stopped selling and supporting Ziva VFX and Ziva Real-Time on 2 Apr 2024. [web: https://unity.com/en/blog/news/update-about-ziva]
- **Real-time skin with collisions:** Projective Skinning (Komaritzan and Botsch 2018) and Fast Projective Skinning (2019) are the relevant research references, and both are GPU solvers. [web: https://cg.cs.tu-dortmund.de/publications/2018-skinning.pdf, https://cg.cs.tu-dortmund.de/publications/2019-skinning.pdf]

## 2. Corrective shapes driven by contact

- **Pose Driver** (RBF) is still marked Experimental in Epic's docs [web: https://dev.epicgames.com/documentation/en-us/unreal-engine/pose-driver-in-unreal-engine].
  - Drive source is bone rotation or translation. Output is poses or curves. Functions are Gaussian, Exponential, Linear, Cubic and Quintic.
  - The Additive solver is cheaper, and Interpolative is smoother at "a higher computational cost".
  - `FAnimNode_PoseDriver` is in the game. [game]
  - It reads pose, not contact. For a contact squash it can only work if each idle pose gets its own target. That is brittle across sliders.
- **ML Deformer** learns pose-driven correctives from bones and curves.
  - Training wants 3,000 frames minimum and 7,500 for good results. The Neural Morph model outputs morph weights on the CPU, 64 to 256 morphs recommended. [web: https://dev.epicgames.com/documentation/en-us/unreal-engine/ml-deformer-framework-in-unreal-engine]
  - The docs never mention contact.
  - Not in the MS2 dump. [game]
- **Contact-distance-driven morphs (recommended):**
  - The rig computes residual depth `r` and contact normal `n` in the breast frame, per breast.
  - Author K press morphs per breast, each a volume-preserving flatten along a direction `a_k` with a sideways spread: front, below (thigh or floor), inner side (arm), outer side.
  - Weights: `w_k = clamp(r / D_k, 0, 1) * max(0, dot(n, a_k))^p / sum_j max(0, dot(n, a_j))^p`.
  - Here `D_k` is the depth the morph reaches at weight 1. That is a normalized RBF in normal space, the PSD idea applied to contact instead of joint angle. [inf]
  - The morph surface then meets the collider at the measured depth, on any slider size, because `r` comes from the slider-scaled ellipsoid.
- **How a curve becomes a morph weight, verified in 5.6.1:**
  1. ControlRig output copies the rig's changed curve elements into the pose curves. Curves that exist only in the rig are added too. [src: `Engine/Plugins/Animation/ControlRig/Source/ControlRig/Private/Tools/ControlRigHierarchyMappings.cpp` lines 370 to 410]
  2. The post-process instance evaluates into the same output curve as the main instance. [src: `SkeletalMeshComponent.cpp` line 2507]
  3. After evaluation, curves whose flags include `MorphTarget` go into the morph-target curve map. [src: `AnimInstanceProxy.cpp` lines 3589 to 3600]
  4. That flag comes from skeleton curve metadata `bMorphtarget`, or from the mesh's `UAnimCurveMetaData` asset user data, which overrides. [src: `BoneContainer.cpp` lines 288 to 385]
  - So a ControlRig curve in a post-process AnimBP can drive a morph, if the curve name is flagged as a morph on the CSS skeleton or the outfit mesh.
- **SetMorphTarget from CSS native:**
  - It is a `TMap` write (`MorphTargetCurves`). [src: `SkeletalMeshComponent.cpp` lines 3804 to 3828]
  - It is applied after the animation curves each evaluation (`UpdateMorphTargetOverrideCurves`). On the same morph name it overwrites the animated weight. [src: lines 1937 to 1946 and 3119; `AnimationRuntime.cpp` lines 2780 to 2815]
  - The call itself costs almost nothing. What matters is when the GPU morph pass reruns.
- **GPU morph cost:**
  - The morph vertex buffer is rebuilt only when the active set or a weight changed (`IsMorphUpdateNeeded`, `ActiveMorphTargetsEqual`). [src: `SkeletalRenderGPUSkin.cpp` lines 732 to 745]
  - The rebuild passes all active weights (`UpdateMorphVertexBufferGPU` with `MorphTargetWeights`). [src: same file, around lines 1321 to 1370]
  - So an animated press weight also re-accumulates the body slider morphs on that frame. [inf from the call shape]
  - A deadband (hold the weight when the change is below about 0.002) makes a settled pose rebuild nothing. [inf]
- **Rig curves vs SetMorphTarget:**
  - Rig curves are evaluated with the pose on the animation worker, and have the same latency as the bones.
  - Native SetMorphTarget runs on the game thread from the last read pose, likely one frame behind. [inf]
  - Prefer rig curves. Keep SetMorphTarget for slider morphs, and never reuse a slider morph name for a press morph.

## 3. Collider shapes and cheap distance tests

- **Capsule** (exact): `h = clamp(dot(p-a, b-a)/dot(b-a, b-a), 0, 1)`, `d = |p - a - (b-a)h| - r`. [web: https://iquilezles.org/articles/distfunctions/]
- **Plane:** `d = dot(p, n) + h`. In the rig this is `Distance To Plane`, which returns `SignedDistance` and `ClosestPointOnPlane`. [web: same; src: `RigVMFunction_MathVector.h` line 1081; game: `RigVM.hpp`]
- **Ellipsoid:**
  - Bound: `k0 = |p/R|`, `k1 = |p/R^2|`, `d = k0 (k0 - 1) / k1`. This is a lower bound, good near the surface. [web: iquilezles]
  - For an ellipsoid against a capsule, map the capsule axis into the ellipsoid's unit-sphere frame with the existing cofactor inverse. `controlrig_body_contacts.py` already builds that frame. [src]
  - Find the closest point of the mapped segment to the origin, which is the capsule formula with `p = 0`.
  - The contact direction is that point's direction mapped back. The ellipsoid surface point along it is `c + A u/|u|`.
  - The depth is the capsule SDF at that surface point. One segment clamp, one inverse, one normalize per pair. Approximate, and exact for spheres. [inf]
- **Floor:**
  - A plane at the component origin, which is the capsule bottom on flat ground, with the up axis as normal. CSS native could instead pass a traced floor height.
  - ControlRig has `Sphere Trace By Trace Channel`, which calls `SweepSingleByChannel` from wherever the rig runs. That is a world query from the animation worker per evaluation. Avoid it, or run it native at a low rate. [src: `Engine/Plugins/Animation/ControlRig/Source/ControlRig/Private/Units/Collision/RigUnit_WorldCollision.cpp` line 134; game: `ControlRig.hpp` line 4807]
- **Engine soft collision unit (reference only):**
  - `FCRSimSoftCollision` (plane, sphere, cone) computes a falloff ratio between `MinimumDistance` and `MaximumDistance`, eases it (`EaseFloat` with a falloff type), and returns a force `direction * falloff * Coefficient`. [src: `Engine/Plugins/Animation/ControlRig/Source/ControlRig/Private/Math/Simulation/CRSimSoftCollision.cpp`]
  - It belongs to `FRigUnit_PointSimulation` (point masses), not to our angular helpers. It is in the game [game]. The falloff-plus-ease pattern is the one to copy.
- **Smooth onset:** use a band `b` (contact starts at distance `+b`). GLSL `smoothstep`: `t = clamp((x-e0)/(e1-e0),0,1)`, `t^2(3-2t)`. [web: https://registry.khronos.org/OpenGL-Refpages/gl4/html/smoothstep.xhtml]
  - The rig has the `Ease` unit (`FRigVMFunction_AnimEasing`) [src: `RigVMFunction_AnimEasing.h` line 32; game]. Remap and Clamp also exist. [src: `RigVMFunction_MathFloat.h` lines 454 to 513]
  - Penalty with no kink: `phi(x) = x^2/(2b)` for `0 < x < b`, `x - b/2` beyond. Here `x = b - d` is depth into the band. The force grows from zero slope. [inf]
  - The soft-limit work found a quadratic or cubic band still too steep when crossed in about two steps (gates 4 and 6 failed). [src: `docs/research/2026-10-19-body-motion-travel-channel.md`, pass three]
  - A **filtered offset** avoids that problem, because static contact is not crossed at speed.
- **XPBD compliant contact:**
  - `alpha~ = alpha/dt^2`, `dlambda = (-C - alpha~ lambda) / (grad C M^-1 grad C^T + alpha~)`, `dx = M^-1 grad C^T dlambda`.
  - The paper assumes zero compliance for contact. A soft contact is the same constraint with `alpha > 0`. [web: https://matthias-research.github.io/pages/publications/XPBD.pdf]
  - For rotations, the generalized inverse mass is `w = 1/m + (r x n)^T I^-1 (r x n)`. [web: https://matthias-research.github.io/pages/publications/PBDBodies.pdf]
  - With one angular body and a fixed pivot, the per-step correction fraction is `s = w / (w + alpha~)`. That is unconditionally stable, unlike an explicit penalty force at 120 Hz. [inf]

### Proposed contact step (design sketch) [inf]

1. **Per evaluation, at the animated target:** for each chest helper `i` and collider `j`, get the depth `p_ij = max(0, b - d_ij)` and the normal `n_ij`.
   - The target offset rotation is the sum of `(r x n_ij) / |r|^2 * phi(p_ij)`, with `r` the contact point minus the pivot.
   - Clamp it to the contact budget `C_i` (degrees).
   - The residual is `r_i = max over j of (p_ij - b - skin moved by the offset along n_ij)`.
2. **Filter the offset** with a critically damped second-order filter, time constant about 40 to 60 ms. That gives onset and release without pops. Then add it to the spring target. Inertia, gravity and jiggle stay as they are.
3. **Per 120 Hz step:** give each active contact a supporting plane to the existing half-space projection. Keep it hard as the safety net, and count the engagements as `ClampContacts` already does.
4. **Curves:** `Press_<k>_L/R = w_k(r_i, n_i)`, filtered the same way, with the deadband.
5. **Friction or slide:** the offset only rotates about `r x n`, so the tangential motion of the chest slides along the collider. To let a held press stay put, low-pass the tangential part of the offset harder than the normal part. [inf]

## 4. UE 5.6.1 and MS2 constraints

| Fact | Status |
| --- | --- |
| Game dump contains no PhysicsControl, KawaiiPhysics, MLDeformer, NeuralMorph or NearestNeighbor module | [game] (`ls` of the dump, 0 matches) |
| Game dump contains `OptimusCore`, `OptimusSettings`, `ComputeFramework`, `UOptimusDeformer`, skinned-mesh read/write and morph data interfaces | [game] `OptimusCore.hpp` lines 952, 1094, 1423, 1460 |
| Deformer Graph is `IsBetaVersion: true`, off by default, gated by `r.DeformerGraph.Enable` and `ComputeFramework::IsSupported` | [src] `Engine/Plugins/Animation/DeformerGraph/*.uplugin`, `OptimusSettings.cpp` lines 17 to 32 |
| ControlRig units in the game: `FRigUnit_SetCurveValue`, `FRigUnit_PointSimulation`, `FCRSimSoftCollision`, `FRigUnit_SphereTraceByTraceChannel`; RigVM `Distance To Plane`, `Get Distance to Box`, `Ease` | [game] `ControlRig.hpp`, `RigVM.hpp`; [src] headers above |
| No dedicated segment, capsule or ellipsoid distance unit in RigVM; capsule tests are built from dot, clamp and length nodes | [src] grep of DisplayNames in RigVM and ControlRig `Public/` (only Distance, Distance To Plane, Get Distance to Box, Intersect Plane/Ray, Find Closest Item) |
| ControlRig curve output reaches morph targets in a post-process AnimBP when the curve is flagged as a morph | [src] see section 2 |
| `FAnimNode_RigidBody`, `FAnimNode_AnimDynamics`, `FAnimNode_PoseDriver`, `FAnimNode_SpringBone` exist in the game; RigidBody runs in 11 of the game's own AnimBPs, mostly post-process | [game] |
| RigidBody cost per body or contact in MS2 | Not measured. Unknown. |

## Numbers worth reusing

| Item | Value | Source |
| --- | --- | --- |
| CSS chest ellipsoid semiaxes, PBMBreastsSize 0 | 8.25 / 6.4 / 5.15 cm | [src] `CSS_SeduXtress_eins0fx/work/nextgen-audit/body-contact-ellipsoids-v2.json`, region brust001, mask 0 |
| Same, PBMBreastsSize 1 | 12.5 / 9.69 / 7.8 cm (x1.52) | same, mask 16 |
| Same, all six morphs on | 12.63 / 9.79 / 7.88 cm | same, mask 63 |
| Morphs behind those corners | PBMWaistWidth, PBMHipSize, PBMThighsTone, PBMGlutesSize, PBMBreastsSize, FBMBodyTone (64 corners) | same file |
| Rig uses only corner 0 | `corner = region['morph_ellipsoids'][0]` | [src] `controlrig_body_contacts.py` |
| Rig contact point radius | 0.25 cm per region | [src] `controlrig_body_contacts.py` (`ContactPointRadii`) |
| Chest weighted support radius | 22.41 / 22.48 cm | [src] travel-channel doc |
| Chest effective cap 1.5.0 | 3.83 degrees = 1.5 cm skin; 6 degrees = 2.35 cm | [src] travel-channel doc |
| Stable stiffness cap at 120 Hz | 1.5/dt = 180 rad/s | [src] travel-channel doc |
| Rig inertial filter | 25 ms; acceleration cap 5000 cm/s^2 times owner scale | [src] `controlrig_body_step.py` |
| CBPC breast sphere | `0.0,4.0,-4.0,3.4 \| 0.0,5.0,-4.5,4.0` (Skyrim units, weight 0 \| 100), scaleWeight 0.3 | [web] CBPCollisionConfig.txt |
| CBPC upper arm capsule, hand, finger | `-3.5,0.0,1,3.5 & -2,0.5,16,2.8`; hand r 2.5; fingers r 2 | same |
| CBPC breast spring | stiffness 0.05, stiffness2 0.01, damping 0.08, max/min offset +/-5, timetick 15, collisionFriction 0.2, collisionPenetration 0, collisionMultipler 1, collisionElastic 0 | [web] CBPConfig.txt |
| HDT-SMP solver | 16 iterations, erp 0.2, maxSubSteps 4, margin 1, budget 3 ms | [web] FSMP Default.json, wiki |
| DynamicBone defaults | rate 60, damping 0.1, elasticity 0.1, stiffness 0.1 | [web] DynamicBone.cs |
| KK colliders (Unity m) | hand r 0.020 h 0.075; forearm 0.025/0.25; thigh 0.095/0.32; floor sphere r 100 at y -100.01; breast r 0.10*BustSize | [web] KK_Plugins Constants.cs |
| KawaiiPhysics defaults | particle radius 3 cm, sphere 5, capsule 5/10, damping 0.1, stiffness 0.05, 60 Hz | [web] KawaiiPhysics |
| AnimDynamics sphere radius | 10 (default, min 1) | [src] `AnimNode_AnimDynamics.h` line 247 |
| Starting points for the proposed step | band b 0.5 to 1.0 cm; offset filter 40 to 60 ms; contact budget 8 to 12 degrees (3 to 4.7 cm skin at R 22.4); morph deadband 0.002; at most 8 capsules plus 1 plane | [inf], to be measured |

**Data-driven profile sketch [inf].** Put a `contact` block in the package recipe, beside the body controls:

- **Colliders:** bone, two endpoints in bone space and a radius, each given at slider 0 and slider 1, like CBPC's `| 100` form.
- **Floor:** an on/off switch.
- **Per region:** band, budget, filter time, and press morph names with their `D_k` and directions.
- **Animations:** an optional allow-list of animation tags. Most idle poses on flat ground need nothing tag-specific.

Native code writes the arrays to fixed-size AnimBP inputs, as it does for the travel and soft-zone pairs. A rig without the inputs keeps today's behaviour.

## Open risks

- **Garments:** every outfit mesh that covers the chest needs the same press morph names, or the body squashes under an unchanged top and the garment clips. Whether CSS morph transfer covers new morphs automatically is unverified.
- **Collider fit:** arm and thigh capsules also change with body sliders. Their radii need the same per-corner measurement the chest ellipsoids got.
- **Ellipsoid accuracy:** the ellipsoid-vs-capsule test is approximate. Exact only for spheres. Check it against the mesh in the worst poses before tuning.
- **Large offsets:** a big contact offset (8 to 12 degrees) can push the chest through the garment from the inside. Run the existing clearance audit (`audit_body_motion_clearance.py`) at the budget angle.
- **Cost:** the frame cost of the extra rig nodes is unmeasured (`measure_body_graph_cost.py` exists). The GPU morph rebuild runs on every frame a press weight changes, so the deadband matters.
- **Floor height:** on slopes or stairs a plane at the component origin is wrong. A native trace at a low rate fixes it, but the rig then lags by a frame on fast terrain changes. Idle poses do not move, so this matters little there.
- **Fallback paths:** RigidBody would replace or fight the angular rig on the same bones, and needs a physics asset per body. Deformer Graph needs custom kernel shaders cooked for MS2's shader platforms, which brings back the shader-hash risk recorded for earlier cooks, and it changes the skinning path for that mesh.
- **Animation edge cases:** some idle animations may push the arm deep enough that no plausible squash hides it. Those may still need an animation-side fix (a small arm offset through IK), which is outside this research.
