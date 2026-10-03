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
