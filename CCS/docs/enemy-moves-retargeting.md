# Enemy and boss moves on the player: what CSS's rig work means for CCS

Written 27 October 2026. Research only: nothing was built, installed or run in the game. Inputs are the CSS source (`native/src/`), CSS boss-appearance research, the UE 5.6.1 engine source, the game's reflected class dump and fresh read-only CUE4Parse exports of the game's Skeleton and montage assets. Game build: `MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`.

Scripts and data for this note are in [`CCS/work/enemy-retarget/`](../work/enemy-retarget/) (see the last section).

## Short answer

The engine already retargets. Since UE 5.2, skeleton compatibility is an editor-only concern: at runtime any animation plays on any skeleton, with bones matched by name and rotations corrected for the two rest poses. Translations follow the *player's* skeleton's per-bone retarget modes. SKEL_Human_Skeleton puts pelvis, spine and limbs on AnimationScaled, so a big boss's offsets shrink to the player's lengths. Nothing in the montage play path checks skeletons. The only thing stopping a Swordman or Moth Knight swing from playing on the player today is CCS's own rule in `CCS/src/core/rig.cpp`.

CSS has already proved this engine path live, in the opposite direction. Its four boss appearances run the player's animation graph on boss Skeleton assets that keep their own bind lengths. The fix that made them work was per-bone translation modes, not new animation data.

Data from this build: 11 non-human enemy and boss rigs carry all 23 core human bone names. Together they hold **107 attack montages** that CCS discovery currently drops. Another 9 rigs miss only a few core bones (41 attacks). Only one boss (the Final Boss, 13 attacks) is humanoid under different bone names. True creatures hold 81 attacks.

The top recommendation is Option 1 below: let audited same-name rigs through the rig rule, and scale root motion by leg length. Option 4, an offline IK Retargeter bake through CSS's editor pipeline, is the quality path for the rest.

---

## 1. What CSS does with skeletons, bones, IK and retargeting

CSS never retargets animation at runtime. Its cross-rig work is either authoring-time (cooked Skeleton copies with chosen translation modes) or runtime switches on the game's own graph nodes. Paths below are under `CustomShellSystem/`. Line numbers are as of today.

### 1.1 Skeleton compatibility is an explicit allowlist

- `native/src/skeleton_compatibility.hpp:20-29` lists the four permanent boss Skeleton copies under `/Game/CSS/eins0fx/Bosses/` (Tarsus, Dwarf, Swordman, MothKnight). Its comment says "Isolated boss rigs retain native bind lengths with player translation modes".
- `skeleton_compatibility.hpp:42-61`: `equivalent_animation_skeletons` and `compatible_animation_skeletons` decide which animation assets may play on which CSS rig. Base motion maps into the wing, hair and belt extensions, never in reverse.
- `skeleton_compatibility.hpp:63-78`: `compatible_standard_skeletons` gates mesh swaps between the game's human rig, CSS bases, Merrick and the four boss rigs. Line 63 says plainly: "This allowlist is not a structural validator for arbitrary mod rigs."
- Users: mesh swap gate `native/src/engine.cpp:423-431` (`is_compatible_skeleton`, enforced at `engine.cpp:1228`). Animation library ranking `native/src/animation_library.cpp:197-204` and `:269-276` (exact rig first, then equivalent, then compatible). Performance playback `native/src/performance_player.inl:485-491` refuses additive and root-motion sequences and re-checks the declared skeleton after load. Walk override `native/src/walk_override.inl:93-100`, `:263`.

So CSS's rule is "known pairs only", the same philosophy as CCS's `rig.hpp`. It is not a bone-structure test.

### 1.2 Boss appearances: translation modes, not new animation

Source: `docs/research/2026-10-15-boss-appearances.md`, recipe `tools/authoring/bosses/configure-isolated.py`, map `packaging/boss-appearances.json`.

- First trial: allowing the *native* boss Skeletons was not enough. "Most collapsed under player animation." Native boss skeletons use Animation mode on every bone, so the player's (shorter) animated offsets replaced the boss's bind offsets.
- Fix: cook isolated Skeleton copies under `/Game/CSS/eins0fx/Bosses/<id>/`. Every bone is set to Skeleton mode, root and `ik_*` to Animation, pelvis to AnimationScaled (`configure-isolated.py:61-64`). Original bind transforms and sockets are kept. `CompatibleSkeletons` is set only while the editor samples poses, then cleared (`:66`, `:82`). The shipped assets do not need it.
- Live results: player animation graph verified on all four; short walking checked for all four plus Merrick.
- Body scale comes from native leg lengths against an 88 cm thigh-plus-calf target: Tarsus 0.7661, Droeg (Dwarf) 0.3349, Nameless Captive (Swordman) 0.4283, Sir Isaac (MothKnight) 0.6523.
- Deferred, with reasons that matter for CCS: The Lost Child (no full leg, left-arm and spine chains), Zmey (asymmetric limbs, extra tail and neck chains need "a dedicated mapping"), Hexapod (tentacles), The Monolith (19 bones), Magdalena (seated reference pose), Orrem (multipart Nanite body).

Lesson for CCS: UE's name remap plus the target's translation modes is good enough to look right on these four rigs. Where a rig lacks the human chains, CSS stopped rather than inventing a mapping.

### 1.3 Native foot IK with the worn body's leg lengths

`native/src/native_foot_ik.hpp:12-19`, `native/src/native_foot_ik.inl` (commits 1985bc4, 9f50577, 43a29f6, 3eb5b98).

- The game's foot IK is `CTRL_Human_FullbodyIK` on an `AnimNode_ControlRig` in ABP_Player. It solves with the rig asset's authored leg lengths unless `bSetRefPoseFromSkeleton` is on.
- CSS finds that node by its ControlRig class (`native_foot_ik.inl:115-140`). It snapshots the rig's initial bone transforms through the hierarchy's `GetBoneKeys` and `GetLocalTransform`. It compares calf, foot and ball lengths with the mesh's `GetRefPosePosition` (`native_foot_ik.hpp:27`). It sets the flag only past 0.5 cm, only for opted-in outfits on a CSS skeleton (`needs_mesh_lengths`, `native_foot_ik.hpp:59-62`; decision at `native_foot_ik.inl:200-211`).
- The trap that cost a release: RigUnit_PBIK caches bone lengths at init. After the mesh pose lands, it needs `RequestInit` (`native_foot_ik.inl:57-59`). Measured live: the thigh still stretched 17 to 19 mm per step until then.

Lesson for CCS: the foot IK already adapts the planted feet to whatever pose a montage produces, for the body the player wears. CCS does not need its own foot IK for retargeted moves. If CCS ever changes leg proportions or the rig's initial pose, it inherits the re-init rule.

### 1.4 Hand support and other ControlRig hooks

- `native/src/hand_support.inl:246-306` swaps the ControlRig class on the post-process instance's `AnimNode_ControlRig` (`:127`) to a CSS-cooked candidate for specific mesh bindings, then drives its controls (`ShoulderEnabled`, `ShoulderCarryAlpha`, `:31-41`). `barrier()` (`:3-9`) waits for the animation task before touching the rig.
- `native/src/engine_bridge.inl:238-290`: the CSSX bridge trial that toggled `bSetRefPoseFromSkeleton` and the ControlRig alpha live.
- Memory note `cw-fps-regression`: that hand-support barrier was one of the costs behind a Commander White frame-rate drop. A per-frame barrier is not free.

Lesson for CCS: a cooked ControlRig dropped into the post-process graph is a proven way to fix hands at runtime (Option 5).

### 1.5 Pose copying and leader pose (same skeleton only)

- Astral doubles: `native/src/astral_namespace.hpp:9-11` (`ABP_CopyPose`), `native/src/astral_visual_mesh.inl:22-23` (`AnimNode_CopyPoseFromMesh`), `:159` (`SetSkeletalMeshAsset`), `:176` (`SetLeaderPoseComponent`). Guarded by `is_compatible_skeleton` at `:106` and `:136`.
- Attachment follower: `native/src/attachment_follower.inl:82-83` (leader pose proxy).
- `native/src/mesh_animation_layers.inl:1-87`: a cross-skeleton `SetSkeletalMeshAsset` can rebuild the main anim instance and drop linked layers, so CSS snapshots and re-links them (`LinkAnimClassLayers`, `:75`).

These copy pose between components that share a skeleton. None of it retargets.

### 1.6 An enemy animation already plays on the player

`native/src/vista_rest.hpp:14-16`: CSS's feminine Vista idle is `A_CultistSpearLady_Locomotion_Idle_02`, an enemy animation on the human rig. That is the same-skeleton case CCS already supports.

---

## 2. What the engine does at runtime (UE 5.6.1 source)

Paths under `reference-tools/UnrealEngine-5.6.1-installed/Engine/Source/Runtime/Engine/`.

| Fact | Where |
| --- | --- |
| `Montage_PlayInternal` only requires the montage to have *a* skeleton. There is no compatibility test. | `Private/Animation/AnimInstance.cpp:2386-2392` |
| `IsCompatible*` are deprecated since 5.2: "Compatibility is now an editor-only concern." | `Classes/Animation/Skeleton.h:772-779` |
| `FSkeletonRemapping` matches bones by `FName` (case-insensitive). Roots always map to each other. Rotations go through `Lt = Q0 * Ls * Q1`, which carries the source's delta from its rest pose onto the target's rest pose. | `Private/Animation/SkeletonRemapping.cpp:24-84`, `:85-200` |
| The "remapping rig" (custom bone map) is commented out as a TODO. Only plain name matching exists. | `SkeletonRemapping.cpp:44-66` |
| Remaps are cached per (source, target) pair. Same skeleton means no remap. | `Private/Animation/SkeletonRemappingRegistry.cpp:67-72` |
| Pose decompression applies the remap to every track. | `Private/Animation/AnimationDecompression.cpp:53-56`, `:92-140` |
| Translation mode per bone comes from the **target** skeleton, unless the target sets `bUseRetargetModesFromCompatibleSkeleton`. | `Private/Animation/AnimationRuntime.cpp:2885-2897` |
| AnimationScaled multiplies the animated translation by target ref length over source ref length. Skeleton uses the target's ref translation. Animation copies it unscaled. | `AnimationDecompression.cpp:279-301`, `AnimationRuntime.cpp:2899-2950` |
| Root motion comes from the source root track with no remap and no scale. | `Private/Animation/AnimSequence.cpp:1590-1609` |
| Root motion becomes world motion through the mesh component's transform, including its scale. | `Private/Components/SkeletalMeshComponent.cpp:4228-4246` |

What follows:

1. A boss montage on a rig with the human bone names plays on the player today, as soon as CCS stops refusing it. Matched bones animate; unmatched bones stay at the player's rest pose. The result is always a finite pose built from the player's own bind transforms. So the "garbage poses to the GPU" worry in `rig.hpp`'s `humanoid()` comment is not what the remap does. The 28 September GPU crashes were never tied to a CCS swap (`docs/work-queue.md`, "28 September, crashes reported").
2. "Registering a compatible skeleton at runtime" would change nothing. `CompatibleSkeletons` only feeds editor asset pickers and editor sampling. CSS clears it before cooking.
3. There is no runtime hook to give the remap a custom bone map. Bone renaming has to happen in a cooked asset (Option 4).
4. Root motion is not retargeted. A Dwarf lunge moves the player the Dwarf's full distance unless something scales it (Option 3).

---

## 3. What this game build ships

From `Binaries/Win64/ue4ss/CXXHeaderDump/` and the pak file list (`work/research/list/files.txt`, 112,372 files).

- **IK Rig runtime module is compiled in.** `IKRig.hpp` has `FAnimNode_RetargetPoseFromMesh` (line 23), `FAnimNode_IKRig` (6), `UIKRetargetProcessor` (785), `UIKRetargeter` (830) and `UIKRigDefinition` (890). ControlRig, FullBodyIK, PoseSearch, MotionWarping and AnimationWarping are present too.
- **No IK Rig or IK Retargeter assets ship.** There are zero `IKR_*` or `RTG_*` files. The only "Retarget" names are baked results: `Content/Mixamo/*_UE_Retargeted*`, `Enemies/Brigands/BrigBase/Animation/MS1_Retarget/` and two `*_MerrickRetarget` memories. The developers bake retargets offline; they do not retarget at runtime.
- **SKEL_Human_Skeleton** (fresh export, 1,273 bones): 644 Animation, 545 AnimationScaled, 84 Skeleton. Root Animation. Pelvis, spine, upperarm, lowerarm, hand, calf and foot AnimationScaled. Clavicle, thigh and ball Skeleton. Fingers, `prop_*` and `ik_*` Animation.
- **The game pairs the human rig with a boss rig.** `SKEL_Human_Skeleton.CompatibleSkeletons` lists `SK_Swordman_Skeleton` and `SK_Shopkeeper_MH_LOD0_Skeleton`. `SK_Swordman_Skeleton` lists the human rig back and uses AnimationScaled on 86 of its 98 bones. This is editor-only metadata, but it shows the studio shared animation between the player rig and the Nameless Captive.
- **CSS bodies:** a decoded `SKEL_Base` (`/run/media/eins0fx/theark/CSS/CustomShellSystem-work/parry1/fixed-decoded/SKEL_Base.json`, 379 bones) is 368 Skeleton, 10 Animation (root, `ik_*`) and pelvis AnimationScaled. On a CSS body, a boss move transfers only rotations and pelvis height. The body keeps its own lengths, which is the cleanest case.
- **ABP_Player** (exported CDO, `work/enemy-retarget/abp/`): two TwoBoneIK nodes keep each hand relative to the other through virtual bones (`VB hand_l_relative_hand_r`, `VB hand_r_relative_hand_l`). Two CopyBone nodes copy the hands into `VB hand_l` and `VB hand_r`. One ControlRig node runs `CTRL_Human_FullbodyIK`. The graph does not read `ik_*` bones, so unscaled `ik_*` translations from a boss should not matter.
- **Root motion scaling at runtime:**
  - `URootMotionModifier_Scale::AddRootMotionModifierScale(MotionWarpingComp, Animation, StartTime, EndTime, Scale)` is reflected and static (`MotionWarping.hpp:220`). The player's `MotionWarpingComponent` is at `ASpartaCharacter` 0x6E8.
  - The game already ships `ANS_RMScaler` (a notify state holding a `RootMotionModifier_Scale`).
  - `ACharacter::AnimRootMotionTranslationScale` is a reflected field (`Engine.hpp:9915`), but `SetAnimRootMotionTranslationScale` is not a UFUNCTION in 5.6 (`GameFramework/Character.h:1073`).
- **Montage slots:** every enemy and boss montage uses `DefaultSlot` (a few add AdditiveReaction or AdditiveSpine tracks). The player graph has that slot, and CCS's transplant keeps the player's own montage shell in any case. No slot work is needed.

---

## 4. Which enemy and boss rigs exist

Method: `classify_rigs.py` reads all 116 Skeleton assets exported from the paks. It counts which of 23 core human bones exist by the same name: root, pelvis, spine_01-03, neck_01, head, and both clavicle, upperarm, lowerarm, hand, thigh, calf, foot and ball. It then counts the 1,292 montages authored on each skeleton: 982 enemy, 304 boss and 6 MS1. "Attacks" uses CCS's runtime discovery filter (`CCS/src/core/discovery.cpp:18-27`). "Leg ratio" is the player's thigh-plus-calf length (99.2 cm on SKEL_Human) over the rig's. It is the uniform factor that maps the rig's root motion to the player. "Rest offset" is the largest angle between the same limb segment in the two rest poses (`rest_pose_delta.py`). The remap keeps that offset for the whole move. All rigs face the same way as the player (lateral axis angle 0).

### 4.1 Group A: all 23 core names (a different USkeleton asset, same naming)

| Rig | Who | Attacks | Leg ratio | Rest offset (worst) | Notes |
| --- | --- | ---: | ---: | --- | --- |
| SK_MothKnight_Skeleton | Sir Isaac (boss) | 20 | 0.735 | 29 deg forearm | CSS boss appearance; native bind kept |
| SK_Swordman_Skeleton | Nameless Captive (boss) | 15 | 0.483 | 34 deg forearm | Game lists it compatible with the human rig |
| SK_TarGolem_Skeleton | Tar Golem (boss) | 12 | 0.813 | 22 deg forearm, 20 deg pelvis | Same rest pose as ParasiteGolem |
| SK_parasiteGolem_Skeleton | Parasite Golem (boss) | 11 | 0.625 | 22 deg forearm, 20 deg pelvis | |
| SKEL_Dwarf_Skeleton | Droeg (boss) | 10 | 0.377 | 31 deg thigh, 23 deg neck | Giant; CSS boss appearance |
| SK_LadyOfTheWood_Skeleton | Lady of the Woods (boss) | 10 | 0.803 | 83 deg thigh, 66 deg shin | Legs are posed very differently at rest; arms 41 deg |
| SK_MultiHeadedHarpy_Boss_Skeleton | Multi-headed Harpy | 7 | 0.627 | 38 deg upper arm | |
| SK_BabyBomb_Skeleton | Baby Bomb / Baby Enemy | 7 | 1.563 | 36 deg foot | Smaller than the player |
| SK_BladeSlave_Skeleton | Blade Slave | 6 | 0.888 | 32 deg clavicle | Near player size |
| SK_Babka_Skeleton | Babka | 5 | 1.983 | 59 deg thigh | Hunched, half size |
| SK_ToxicStalker_Skeleton | Toxic Stalker | 4 | 1.516 | 120 deg foot, 80 deg neck | Crawler; not a good fit |
| MS1_Tarsus_Skeleton | Tarsus (MS1 boss) | 0 | 0.864 | 25 deg forearm | Only one montage in the paks; CSS boss appearance |

That is **107 attacks** outside the human rig, 81 of them from bosses. NPC-only rigs in this group (Blacksmith, Barmen, Depraved, Shepherd, Egon, Village child, Merrick) have no attack montages.

For comparison, CSS judged player animation on Tarsus, Droeg, the Nameless Captive and Sir Isaac acceptable with rest offsets of 25 to 34 degrees, in the reverse direction. So the first eight or nine rows are the realistic candidates. Lady of the Woods, Babka and Toxic Stalker will show leg posture problems.

### 4.2 Group A2: most core names, a few missing

| Rig | Attacks | Missing core bones | Note |
| --- | ---: | --- | --- |
| SKEL_Grisha_Skeleton | 7 | ball_l, ball_r | Four arms; extra arms stay at rest; 59 deg neck offset |
| SK_ShellSnatcher_Skeleton | 6 | whole leg chains | Upper body only |
| SK_TarredVestige_Skeleton | 6 | spine, neck, head | Arms and legs map |
| SK_StoneCrab_Skeleton | 6 | spine, neck, head | Leg ratio 3.5; 122 deg arm offset; creature in practice |
| SK_ParasiteKnight_Skeleton | 5 | neck_01 | 57 deg forearm offset |
| SK_PapaRoach_Skeleton | 4 | both arm chains | |
| SK_Snowworm_Skeleton | 3 | leg chains | |
| SK_ShepherdSheep_Skeleton | 2 | ball bones | Quadruped with human names; 82 deg arm offset |
| SK_Spider_Skeleton | 2 | neck_01, head | 179 deg spine offset; creature in practice |

41 attacks. Grisha, Tarred Vestige and Parasite Knight are worth a look. The rest are creatures that happen to reuse a few names.

### 4.3 Group B: humanoid under other bone names

- **SK_FinalBoss_Skeleton** (13 attacks). Legs are `Hip1_*`, `Knee1_*`, `foot_*` instead of thigh and calf. Neck_01 is missing. There is a complete left arm chain, but the right side has only `ik_hand_r`; no right arm bone carries an arm-like name. Spine and head names match.
- The MS1 rigs (Thestus, Genessa, Vlas, Gorf) use `hip_R`, `knee_R`, `shoulder_R`, `elbow_R`, but have no attack montages in the paks.

### 4.4 Group C: creatures

Hexapod 27 attacks, Mouth Boss (HeadBoss) 12, SpiderBro 11, Monolith 9 (19 bones), The Coffin 8 (arms map, no legs), Slug 4, The Head 4 (legs map, no arms), Batman 2, plus a few others: **81 attacks** in all. These share at most an arm or leg chain. Only partial uses are possible, such as The Coffin's or Batman's arm swings on the upper body.

The live registry scan that "dropped 264 creature-rig montages" (`docs/work-queue.md`, 27 September evening) lines up with groups A, A2, B and C together (242 attack-like by this count, plus montages outside these folders).

---

## 5. Options, ranked

### Option 1: let audited same-name rigs through the rig rule

**What it needs**

- A data file, not a code list. One row per rig: skeleton object path, family, leg ratio, rest offsets and allowed slots. Write it from `rigs.json` and `rest-pose-delta.json`, and ship it like `enemy-catalog.json`.
- `rig::compatible` (`CCS/src/core/rig.cpp:16-24`) accepts a montage skeleton listed there. The same rule then serves the registry scan (`discovery.cpp:104`) and the load gate (`Combat::load_pending`, `combat.cpp:1033-1038`).
- `build_enemy_catalog.py` gains those skeletons and records each move's rig, so the menu can show "retargeted from Sir Isaac".
- Keep `humanoid()` as is. It guards the *player's* body (Harbinger, creature shells), a separate question.
- Enemy montages already play through the cleaned copy (`Combat::clone_montage`, `combat.cpp:392`; `strip_ai_notifies`, `:426`) or the feel transplant (`build_transplant`, `:465`). The transplant puts the boss's AnimSequence inside a clone of the player's own montage. The engine decodes with the *sequence's* skeleton, so the remap still happens. No change is needed there.

**Evidence it works in this build**

- Engine path (section 2): no gate, name remap, target translation modes.
- SKEL_Human's AnimationScaled limbs and pelvis (section 3) shrink giant offsets to the player's.
- CSS ran the same engine path live on four boss skeletons, with walking checked (section 1.2).
- The studio itself paired SKEL_Human with SK_Swordman.

**Risks**

- Rest-pose offsets of 20 to 38 degrees, mostly elbow bend and clavicle. They tilt the weapon arc and can make the off hand miss a two-handed grip. They do not break the pose.
- On stock shells, finger bones are Animation mode on SKEL_Human, so a giant's finger offsets copy unscaled. Fingers may look slightly stretched on Swordman (2x) and Droeg (2.6x) moves. CSS bodies (Skeleton mode) are immune.
- Body-contact attacks (stomps, grabs, `ANS_FlexibleCollider`) still use the player's weapon trace, the same as today's enemy moves.
- Moves authored for a giant's reach (a long overhead) will reach less far after scaling. That is correct for the body, but the hit timing may feel early.

**Minimal live test**

Allow only `SK_Swordman_Skeleton`. Put `A_Swordman_Attacks_Overhead_01_Montage` in L1 on a stock shell with a one-handed weapon, in Marrow Keep (see the memory note "Safe spot for live tests").

Check:
1. The `CCS swap:` log line shows the move.
2. Arms, spine and feet look intact through the swing, compared by eye with the same swing on Sir Isaac's rig.
3. Distance travelled (position before and after).
4. Wrist and fingers on a close screenshot.

Then repeat on a CSS body (SKEL_Base) to see the Skeleton-mode case.

### Option 2: "wear the boss, use the boss" (a subset of Option 1 with zero retarget error)

**What it needs**

When the player wears a CSS boss appearance, `rig::player_skeleton` returns the CSS copy (for example `/Game/CSS/eins0fx/Bosses/dwarf/.../SKEL_Dwarf_Skeleton`). Accept montages on that copy's native source skeleton. `packaging/boss-appearances.json` already maps `source_skeleton` to `skeleton` for all four.

**Evidence**

The CSS copies keep the native bind transforms (section 1.2), so the remap is identity on rotations. Pelvis is AnimationScaled at ratio 1. The CSS mesh scale (0.3349 for Droeg) then scales root motion through the component transform (`SkeletalMeshComponent.cpp:4245`). The pose is exact and the travel matches the shrunken boss.

**Risk**

Very low. It is the boss's own animation on the boss's own bind.

**Minimal live test**

Wear Droeg (CSS NPC appearances). Put `A_DwarfBoss_Attack_3Hit_Combo_01_Montage` in L1, swing once, and check the pose and that the travel is about a third of the boss's.

### Option 3: scale root motion by leg ratio

**What it needs**

- A per-move or per-rig factor. Leg ratio from section 4 is the obvious default, and it matches CSS's boss sizing rule.
- Apply it only while CCS's swapped montage plays. Preferred: in the swap path, call `URootMotionModifier_Scale::AddRootMotionModifierScale(player MotionWarpingComponent, played montage, 0, length, (r, r, 1))`. That scopes it to the one montage, and MotionWarping removes it when the window ends. Fallback: write `ACharacter::AnimRootMotionTranslationScale` and restore it when `Montage_IsPlaying` turns false. CCS already polls that for weapon meshes.
- Keep Z at 1 unless a jump attack shows it is needed.

**Evidence**

Root motion is not retargeted by the engine (section 2). The modifier API and `ANS_RMScaler` exist in this build (section 3).

**Risks**

- Interaction with the warp notifies CCS keeps (`ANS_SpartaMotionWarping_Translation` toward `WT_DesiredEndLocation`). A warp window already rescales root motion to reach its target. The scale modifier may compound or be overridden inside it. Measure, do not assume.
- A stale `AnimRootMotionTranslationScale` would make every later move short. The modifier route avoids that.

**Minimal live test**

The Option 1 Swordman swing with r = 0.483, compared with the unscaled run. Also check that one vanilla swing afterwards travels its normal distance.

### Option 4: offline bake with the IK Retargeter (quality path, and the only path for groups A2 and B)

**What it needs**

- CSS's UE 5.6.1 authoring project and cook pipeline (the boss recipe and its `CSSRetargetLibrary` editor helper show the tooling exists).
- In the editor, one IK Rig per source family and one for SKEL_Human: chains for spine, neck, head, arms and legs, with IK goals on hands and feet.
- One IK Retargeter per family with a matched retarget pose. This removes the 20 to 40 degree rest offsets that Option 1 keeps. Chain mapping covers the Final Boss's `Hip1`/`Knee1` legs and Grisha's extra arms. Root and pelvis motion ops scale travel.
- Batch-export the attack sequences onto SKEL_Human_Skeleton. Rebuild their montages with the original notifies.
- Cook into a CCS-owned pak under a CCS namespace. The asset path convention for CCS still needs a decision (`/Game/CSSX/CCS/` or `/Game/CSS/CCS/`).
- The rig rule then needs no exception at all: the baked moves are on the human rig.

**Evidence**

- This is how the studio itself retargets (the Mixamo, MS1_Retarget and Merrick assets).
- The IK Rig runtime classes are not even needed, since the bake happens in the editor.
- CSS has cooked game-derived animation and skeleton packages before (boss appearances, Vista).

**Risks**

- Asset weight and per-move authoring time.
- A release now ships a pak. Install needs the user's confirmation (memory note "Install needs confirmation").
- Baked moves must be regenerated if a patch changes the source animations. That is rare for animation, but it should be checked alongside the shader audit.

**Minimal test**

Offline first. Bake one Swordman overhead, then compare sampled hand and weapon-socket paths against the Option 1 result. CSS's `*-corrected-motion.json` sampling in `configure-isolated.py:67-80` shows the method. Then one live swing.

### Option 5: runtime hand and grip fixup, borrowing CSS's hand support pattern

**What it needs**

Only if Option 1 shows two-handed grips breaking. A small cooked ControlRig swapped into the post-process graph's `AnimNode_ControlRig`, the way `hand_support.inl:246-306` does it. It would pin the off hand to the weapon's grip socket while a retargeted move plays, with alpha on only for those moves.

**Evidence**

CSS ships this mechanism. ABP_Player's own hand TwoBoneIK works through virtual bones, which may already hold the grip (open question 2).

**Risks**

The barrier cost (memory note `cw-fps-regression`). It is also another runtime graph edit to keep alive across menu reopen and mesh swaps.

**Minimal live test**

Hammer or Martyr's Blade (two-handed) with the Swordman swing from Option 1. Watch the left hand.

### Option 6: runtime retarget with `RetargetPoseFromMesh` (last)

**What it needs**

- Cooked IK Rig and Retargeter assets (none ship).
- An anim blueprint with the node, linked as a layer or post-process.
- A hidden second skeletal mesh component carrying the boss mesh, playing the boss montage every frame.
- The real montage still has to play on the player, for notifies, hit windows and root motion, and the two must stay in sync.

**Evidence**

The classes exist in the build (section 3).

**Why last**

- Two full mesh evaluations plus a retarget solve per swing, against CCS's no-frame-cost rule.
- Hit-check notifies would fire on the hidden puppet, not the player.
- It needs the same authored assets as Option 4 anyway, and Option 4 has no runtime cost.

### Not viable

- **Runtime CompatibleSkeletons registration.** Editor-only since 5.2 (section 2). It changes nothing at runtime.
- **A runtime bone map.** The remap only matches names. The mapping-rig hook is commented out in 5.6. Renaming bones inside a live USkeleton's `FReferenceSkeleton` is native memory surgery on a shared game asset.
- **Writing translation modes on SKEL_Human at runtime.** For example, setting fingers to AnimationScaled to stop giant finger stretch. It would be a no-op for same-skeleton animation (ratio 1), but it mutates a shared game asset for every human enemy. CCS's own rule says to avoid shared asset edits. Keep this as a fallback idea only.
- **Group C creatures as whole-body moves.** No chains to map. At most an arm-only upper-body use through Option 4 with a layered blend.

---

## 6. Open questions that need the game running

1. Does a Swordman or Moth Knight swing look acceptable with the 29 to 34 degree forearm offset? Or does it need Option 4's retarget pose? This is the decision point between Options 1 and 4.
2. Do ABP_Player's hand TwoBoneIK nodes (virtual-bone targets) hold a two-handed grip during a retargeted move? Virtual bones on a foreign-skeleton animation may sit at the rest relation instead of the authored one.
3. How does `AddRootMotionModifierScale` interact with the `WT_DesiredEndLocation` warp windows CCS keeps? Does it compound, or does the warp win?
4. Does the game's foot IK stay enabled during these boss montages, or do they carry `ANS_DisableFootIK`? (The notify class exists in the dump.)
5. How visible is the finger stretch on stock shells for Swordman and Droeg moves? Is it worth anything beyond a note?
6. What does discovery's live count become with the group A allowlist? Compare it with the 107 attacks counted offline.
7. Option 2: does a CSS boss appearance's mesh scale really scale root motion the way `ConvertLocalRootMotionToWorld` says? (Measure travel while wearing Droeg.)
8. Frame cost: a remapped decode is one cached table lookup per bone. Confirm with CCS's frame profiler (`docs/frame-profiling.md`) that a retargeted swing costs the same as a human-rig swing.

---

## Side findings

- `CCS/work/enemy-catalog/build_enemy_catalog.py` uses a case-insensitive SKIP regex. "Parasite" contains "sit" and "ToxicStalker" contains "talk", so attacks from those enemies are filtered out of `enemy-catalog.json`. The runtime discovery filter (`discovery.cpp:18-27`) is case-sensitive and not affected.
- The `rig.hpp` header comment says creature rigs "share too few bones". Eleven non-human enemy and boss rigs with attacks share all 23 core names.
- The older SKEL_Human export under `CustomShellSystem-work/research/socket-adjust/` has 1,199 bones. The current paks have 1,273. Any tool that hard-codes the human bone list should re-read it.

## Work files

All under `CCS/work/enemy-retarget/`:

- `classify_rigs.py` produces `rigs.json` (groups, core coverage, leg ratio, pelvis height and modes, montage and attack counts per skeleton).
- `rest_pose_delta.py` produces `rest-pose-delta.json` (per-segment rest-pose angle against SKEL_Human).
- `scale_ratios.py` produces `scale-ratios.json` (AnimationScaled ratios per shared bone, and Animation-mode bones that copy unscaled).
- `skeletons/`: 203 read-only CUE4Parse exports (116 Skeleton assets).
- `boss-montages/`: 304 exports. `ms1-enemy-montages/`: 6 exports.
- `abp/`: the ABP_Player CDO export.
- Enemy montages: the existing 982 in `CCS/work/enemy-catalog/export/`.

Exports were made with `CustomShellSystem/work/research/GameDump-tool` (mode `export`) against the game's paks and usmap, read-only.
