# Sidearm aim after changing appearance

## Report and reproduction

crimsonmoon93 reported incorrect gun orientation with CSS beta.8 and Eve v1.3.0. Removing Tarstones did not help. Vanilla-to-vanilla and Eve-to-Eve changes worked; crossing from vanilla to Eve triggered the problem.

On 2026-10-03 the author reproduced the same visible fault locally with the existing beta.9 runtime and Eve v1.4.0. Returning to vanilla Genessa did not recover the aim.

Local evidence, excluded from Git: `work/crimsonmoon93-sidearm/`. The supplied clip and logs remain in `dist/v1.0.0-beta.8/bug-reports/`.

The live capture established:

- A native-human to CSS-base skeleton switch recreated `ABP_Player`.
- The pawn and mesh both referenced the new instance. A stale main instance pointer was not the cause in this run.
- Drawing the machine gun still linked `ABPL_Locomotion_ADS_Trebuchaxe`, its authored locomotion layer. Its absence during stowing was expected, not the fault.
- `WP_MachineGun.OnEquip_AnimationLayers` names `ABPL_Aim_MachineGun`, but the new player's Aiming node used `ABPL_Aim_Default` instead. Returning to vanilla created another instance with the same missing override.
- The weapon was attached to its in-hand socket without an unexpected relative rotation. It still fired.

The game applies the weapon layer on equip through `WP_WeaponBase.ApplyAnimationLayers`. A cosmetic mesh switch does not equip the weapon again. `SetSkeletalMeshAsset` calls `SetSkeletalMesh(NewMesh, false)` in the installed UE 5.6.1 header; preserving the pose flag does not preserve the recreated instance's dynamic layer assignments.

## Fix

`mesh_animation_layers.inl` captures reflected linked-layer assignments before CSS replaces a skeletal mesh. Classes different from the animation blueprint's defaults are retained across the call and relinked only when missing afterward. A read-back checks that the previous external layer assignments survived. Nothing runs on the per-frame path, and no weapon equip events, gameplay effects, input, ammunition or ability state are changed.

The logic covers both applying a custom appearance and restoring the original. An unchanged mesh returns immediately. Existing valid assignments are left alone. Reflected field types, sizes and bounds are checked before access. Classes are kept alive through the swap; no old animation-instance pointer is reused afterward.

## Verification

- C++23 production core built with `CSS_INVENTORY_DEV=OFF` and `CSS_TRANSITION_TESTS=OFF`.
- Rebuilt host animation, animation-runtime, recovery, skeleton-compatibility and socket-fit suites: 5/5 passed. These do not exercise Unreal's live layer replacement.
- `tools/diagnostics/sidearm-transition/check.py` rejects the captured broken state: the equipped machine gun has `ABPL_Aim_Default` instead of `ABPL_Aim_MachineGun`.
- Candidate installed with SHA-256 recorded in `work/crimsonmoon93-sidearm/install.json`. Original core selector backed up in `install-before/`.
- After relaunching with the candidate, the author reported "ok seems fixed" on 2026-10-03. This is acceptance of the reported aiming fix, not exhaustive coverage of every weapon or gameplay transition.
- The accepted live capture passed the same check that rejected the old build: vanilla Genessa retained `ABPL_Aim_MachineGun` with the machine gun equipped. The loader identified the tested production DLL, SHA-256 `d028cec65b4e3d0c481e05ef14b2525a786fa8bac392a0484e488778805aa335`.

The diagnostic Lua snapshot is on-demand and read-only. It is excluded from the shipped runtime. It ran through the existing MS2AttackProbe file trigger; the original `RepairPrologue.lua` was restored after the accepted capture. Do not repeat the soft-class-array Lua trial: the process exited during that attempt, before the repair call logged. Production handles reflected layer nodes in C++ instead.

## October 4: forced Harbinger transitions

The reporter says short/long jump gates and air-current jumps still break aim.
The user requested investigation from the game dump because locating a gate is
inconvenient. This follow-up uses the CL93241 cooked Blueprint bytecode and SDK
dump; it has not yet been reproduced locally in a gate.

`work/sidearm-traversal/` contains AssetReadback JSON and extraction lists:

- `GA_Traversal_BoneGate_Far` and `GA_ShellTraversalBase` call the controller's
  `SwitchToDarkFormMesh` and `SwitchToShellMesh` functions.
- High and long shell throws inherit `GA_Traversal_ShellThrow`.
- `BP_PlayerController.SwitchCharacterMesh` calls `SaveAnimationInstanceState`,
  then `SetSkinnedAssetAndUpdate`, then `LoadAnimationInstanceState`. It does not
  call `LinkAnimClassLayers` or `ApplyAnimationLayers`.
- The SDK's `FCSAnimationInstanceState` contains only `ActiveMontage`. There is
  no linked-layer map in that saved state.
- All seven shipped sidearm definitions name one `OnEquip_AnimationLayers`
  class derived from `ABPL_Aim_Default`: Ballistazooka, Crossbow, CursedChild,
  MachineGun, NailShotgun, ParasiteGun and Trebuchaxe.

This exposes a gap in the previous repair: a game-owned mesh swap can discard
the weapon layer before CSS takes its snapshot. Preserving that later snapshot
only preserves the already-default aiming graph.

The follow-up reads the currently equipped sidearm definition when the live
graph has returned to `ABPL_Aim_Default`. It links only the weapon's derived
aim class, then checks that the desired instance exists and the default is gone.
An existing non-default aim layer is left alone. No equip events, abilities,
ammunition, attachment transforms or traversal locomotion layers are replayed.
Soft references are copied through reflected parameters, not legacy layouts.
Weak identities are rechecked after loading before any write.

The check runs inside existing 150 ms maintenance, with no new hooks or Lua
runtime dependency. Healthy aim returns after the default-layer lookup. Weapons
without a supported override are remembered by weak instance/weapon/default
identities. Failures use the existing one-second maintenance backoff.
Traversal and ordinary appearance-readiness guards defer mutation. The traversal
guard now includes `GA_ShellTraversalBase` and its authored subclasses, which
were missing from the old `GA_Traversal_` prefix check.

Validation so far:

- C++23 production build passes with developer and transition-test flags off.
- Animation, animation-runtime, recovery, skeleton and socket-fit host suites
  pass, including the new traversal-family regression cases.
- `tools/diagnostics/sidearm-transition/audit_traversal.py` audits the extracted
  transition call sequence and seven sidearm definitions. This is static
  evidence, not an in-game traversal test.
- Candidate installation/hash and previous selector are recorded in
  `work/sidearm-traversal/install.json` and `install-before/`.
- Public ZIP unchanged. Live acceptance remains pending, including repeated
  short/long gates, air currents, cancellation and a subsequent weapon change.
- The original diagnostic entry was restored when switching to offline work;
  no capture loop or game control automation is running.
