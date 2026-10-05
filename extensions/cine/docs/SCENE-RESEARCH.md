# CINE scene features: research (5 October 2026)

Research for three additions: clean scene, action takes, breakdown takes. Nothing here was
run in the game. VERIFIED = read in the game's class dump, Blueprint bytecode, cooked assets
or the UE 5.6.1 source. INFERRED = needs a live test. Raw exports and tables are in
`extensions/cine/work/export-{scene,action,assets}/` (git-ignored).

Engine source note: only `reference-tools/UnrealEngine-5.6.1-installed/Engine/Source` exists
now; the `UnrealEngine-5.6.1/` copy is gone.

## 1. Clean scene (no enemies or NPCs, world keeps running)

VERIFIED
- The game has its own reversible AI pause: `BP_AICharacter_C::Pause()` / `Resume()`
  (movement tick off, `Mesh.bPauseAnims`, mesh tick off, `BrainComponent.StopLogic`). It uses
  them on every `BP_AICharacter_C` when entering and leaving a seamless dungeon.
- Its single-enemy freeze (Harros Stone Stun) adds: `HitTraceComponent.DisableHitProcessing()`,
  `VoxComponent.SetVocalizationsEnabled(false)`, `FoleyComponent.DeactivateDynamicFoley()` /
  `StopFoleyLoop()`, `Mesh.SuspendClothingSimulation()`, `GE_HealthDamageImmunity_C`.
- Spawning is triggered by the player pawn overlapping `BP_SpawnTrigger_C` /
  `BP_Spawner_Proximity_C` (not by zone change). NPCs come from `BP_NPCSpawner_C` (spawn on box
  begin overlap, destroy on end overlap). Spawners show a stand-in `UnitProxy` mesh plus
  `UnitProxyAddons` before the real enemy exists.
- Classes: every enemy, boss and NPC is a `BP_AICharacter_C`; NPCs are `BP_NPC_C`; team in
  `ASpartaCharacter.Team`; bosses have `BPC_BossAI_C` (`bIsAggroed`); player summons are not
  `BP_AICharacter_C` except a few with `Team.Player`. Registry:
  `USpartaEnemySubsystem.SpawnedEnemies`.
- Lock-on is refused for an actor with `AI.State.Invisible` / `State.Invisible`
  (`GE_AI_Invisible_C` grants both).
- Save writes happen only in death handlers, so hide-and-pause writes nothing as long as a
  hidden enemy cannot die.
- No cutscene or dialogue code in the game clears enemies; there is no global spawn switch.
- Player safety options: `GE_Invulnerability_C` (+ `GE_FallDamageImmunity_C`),
  `GE_Undetectable_C`. The Cheat Menu uses `pawn.bCanBeDamaged`, so CINE must not use that flag.

INFERRED (live tests, in order)
1. `Pause()` / `Resume()` on one enemy mid-attack and one idle.
2. Hide then collision off: no fall-through, no health bar left.
3. `GE_AI_Invisible_C` breaks and prevents lock-on; GE handles round-trip through the bridge.
4. Spawn trigger actor collision off and on: no spawn while off, no double spawn after.
5. Player GEs block a hit; `GE_Undetectable_C` stops aggro.
6. Boss fights (plan: refuse clean scene while a boss is aggroed).

Never: `DespawnEnemy()` / `K2_DestroyActor` on NPCs, collision off on `BP_NPCSpawner`,
`USpartaCheatManager::DestroyAll*`, `cs.DisableAI` except as a last fallback (process-wide).

Per enemy on enter (save each prior value, restore in reverse): `Pause()`,
`HitTraceComponent.DisableHitProcessing()`, `SetActorHiddenInGame(true)` on it and its attached
actors (weapons), `SetActorEnableCollision(false)` (only after Pause, or it falls through the
floor), apply `GE_AI_Invisible_C` and `GE_HealthDamageImmunity_C`, audio off, cloth suspended.
Restore the un-pause from saved values and call `BrainComponent.StartLogic()` only if it was
running before (some NPCs are stopped on purpose). Keep-clean poll every 0.5 s: re-query, diff,
treat newcomers, drop dead handles. About 14 bridge calls per enemy on entry, spread over ticks.

## 2. Action takes (cinematic fighting)

VERIFIED
- Lock-on target: `pawn.LockOnComponent.LockedTarget`. Aim point:
  `enemy.BPC_LockOnTarget."Get Tracking Location"(Location)` (socket `Socket_Target`, stable).
  No lock: `pawn."Targeting Component".LastSoftTargetActor`, then
  `controller."Combat Tracker Component".AllValidEnemies` (+ `bInCombat`).
- The game's camera is a priority stack of camera states (`ACSCameraManager`); its own
  cinematic cameras use `SetViewTargetWithBlend` like CINE. Only `BP_CustomCamera`
  (anim-notify cameras, tag `State.CustomCamera`) and `BP_DeathManager` take the view target.
- Slow motion: `GameplayStatics.SetGlobalTimeDilation` works in shipping (Cheat Menu uses it and
  re-asserts it). The game also writes it (level up, death 0.3, perfect block hit-stop). Melee
  hit-stop is per actor. `AWorldSettings.CinematicTimeDilation` multiplies on top.
- CINE's tick and frame clocks are real time; `MoveComponentTo` and view blends run on game
  time (scale `OverTime` by the dilation).
- Events by 10 Hz polling: target `HealthComponent.GetHealthNormalized()` / `IsDeadOrDying()`,
  player `HealthComponent.GetHealth()`, `AbilitySystemComponent.GetGameplayTagCount` for
  `State.Parry`, `State.Animation.Paired`, `State.Dodge`, `State.CustomCamera`,
  `pawn."Combo Counter".CurrentComboCount`. The bridge hook cannot observe a call today.
- Camera post-process: `CameraComponent.PostProcessSettings` (`DepthOfFieldFstop`,
  `DepthOfFieldFocalDistance`, `MotionBlurAmount`, with their `bOverride_*`); camera overrides
  beat volumes. Shakes: `ACSCameraManager.SetGlobalCameraShakeScale`.
- Spawning an opponent: spawn `BP_Spawner_RuntimeEditor_C` with `ClassToSpawn` set (the game's
  own runtime level editor does this); remove with `SetLifeSpan(0.001)` on controller and enemy,
  then destroy the spawner. A kill pays gloom and gold, which the game saves.
- HUD: hiding `WBP_Player_HUD` does not hide enemy health bars, the lock-on reticle or damage
  numbers (per-enemy widget components).

INFERRED (live tests, in order)
1. Does CINE's CameraActor win over the game's camera states during a riposte?
   (`CSCameraState_CameraActor` priority; fallback `CSCineCameraActor` with a set priority.)
2. `CinematicTimeDilation` slows the world and survives a perfect block.
3. Spring-arm rig made with `AddComponentByClass` (engine follows and smooths every frame).
4. Depth of field passes through on our camera. 5. The opponent spawn recipe. 6. Shakes.

## 3. Breakdown takes (VFX breakdown look)

VERIFIED: what does NOT exist in this shipping build
- View modes and most show flags are compiled out (Wireframe, Bones, Collision,
  VisualizeBuffer fixed off; `viewmode` needs a cheat manager; `ShowFlag.*` are cheat cvars).
- `DrawDebug*` are empty in shipping. Skeletal debug draw is compiled out.
- The engine's debug and buffer-visualization materials are not in the game's files. No
  material in the game has the Wireframe flag.

VERIFIED: what DOES work
- Shape components draw their wire shape in a shipping game when not hidden:
  `UCapsuleComponent` / `USphereComponent` / `UBoxComponent` + `SetHiddenInGame(false)`,
  `ShapeColor`, `SetLineThickness`. Depth tested. Zero assets.
- Any component can be created with `AActor::AddComponentByClass` and attached to a bone with
  `K2_AttachToComponent(Parent, BoneName, ...)`; the engine then moves it every frame.
- Skeleton data by reflection: `GetNumBones`, `GetBoneName(i)`, `GetParentBone(name)`,
  `GetBoneTransform(name, space)`, `GetRefPoseTransform(i)`. Virtual bones are in the runtime
  skeleton with the `VB ` prefix (`USkeleton.VirtualBones` gives source and target). IK bones
  are a naming convention (`ik_*`).
- Player rig: 94 to 182 bones per shell; 9 virtual bones; 7 IK bones; 16 twist bones; 82
  sockets. Physics assets keep full shape data (`SkeletalBodySetups[].AggGeom`: capsules,
  boxes, spheres; e.g. 22 bodies on the standard humanoid, 60 on Genessa).
- Meshes already in the game: `/Engine/BasicShapes/{Sphere,Cube,Cylinder,Cone,Plane}`,
  `/ControlRig/Controls/ControlRig_<Shape>_{1mm,3mm,solid}` (18 shapes; Pyramid and Diamond
  suit bones), `/Niagara/DefaultAssets/S_Gnomon`.
- Materials already in the game:
  `/Game/Developers/Zack/Materials/Mat_TranslucentColor_NDT` (unlit, see-through, `Color`,
  `Opacity`, skeletal usage), `/ControlRig/Controls/ControlRigXRayMaterial`,
  `/Game/Sparta/Cutscenes/01_Intro/TestSpheres/M_BaseTestMat` (flat lit `BaseColor`, works on
  skeletal, cloth, static, Nanite; not on meshes with active morphs),
  post-process `/Game/Sparta/FX/PostProcess/M_VFX_Outline`,
  `/Game/Sparta/Core/Player/Ability/Materials/M_PP_Sobel`,
  `/MovieRenderPipeline/Materials/MovieRenderQueue_WorldDepth`.
- Post-process materials can be added at runtime: `UCameraComponent::AddOrUpdateBlendable`
  (the game's own `BPC_Player_PostProcess` does it). Custom depth with stencil is on.
- Material swaps: `SetMaterial`, `SetOverlayMaterial` (wireframe on top of the shaded mesh;
  not under Nanite). A material with the Wireframe flag renders as wireframe in shipping.

Needs CINE's own small cooked pak
- True wireframe (overlay material with the Wireframe flag and skeletal usage).
- G-buffer passes: one post-process material showing base colour / normals / roughness /
  depth, with a wipe parameter.
- A flat material that also works on morph-target meshes.
- Cook route exists (Windows editor under Wine, inline shaders:
  `tools/shared-assets/cook-shared-inner.sh`, `CSS_UnholyGenessa_eins0fx_P/tools/cook-materials.sh`).
  Container trio `CSSX_CINE_eins0fx_P` under `~mods/`, assets under `/Game/CSSX/CINE/`
  (user's rule: a CSSX extension's assets live under `/Game/CSSX/<EXTENSION>/`, never under
  `/Game/CSS/`, which belongs to CSS packages). CSSX extension zips carry no pak today.

INFERRED (live tests)
- `SkinnedMeshComponent.bForceWireframe` + `r.SkeletalMesh.UseCachedMDCs 0` (wireframe with no
  asset). `UDynamicMeshComponent` wireframe pass. Wireframe on Nanite environment meshes
  (`SetForceDisableNanite`). G-buffer reads after TSR. A Wine-cooked material rendering in game.

## Disk

`/home` was at 97% during this research. The three export folders total about 130 MB and can
be deleted and regenerated.
