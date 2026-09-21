# Eve animation-camera repair

Status: repaired, cooked, packaged, installed and accepted in gameplay review on 2026-09-22.

## Report and reproduction

The user clarified that the failure happens during the attack after a successful
parry (riposte), rather than the parry impact. Trap reactions and other animated
close-ups also pan or zoom away from the character. The baseline recording
`work/parry1/before.mp4`, around 20 seconds, catches the camera framing empty
scenery during the follow-up. See `action.jpg` for the sampled frames.

## Cause and scope

The installed `/Game/CSS/Shared/SKEL_Base` sets `camera_pivot` and
`camera_target` translation retargeting to Skeleton. The stock human skeleton
sets both to Animation. Our original authoring policy exempted root and IK
helpers from Skeleton translation, but omitted the camera helpers.

The cooked game `CameraState_Animation` explicitly uses camera_pivot, with pelvis
as its look-at target. Both left-leg and right-leg bear-trap montages activate
that state. Several longer/smoother animation-camera variants also use the same
pivot. This establishes a shared asset defect rather than a riposte-only offset.

UE 5.6.1 source, Engine/Source/Runtime/Engine/Private/Animation/AnimationRuntime.cpp,
GetBoneTranslationRetargetingMode and RetargetBoneTransform, selects the target
skeleton policy when compatible-source modes are disabled. The Skeleton branch
replaces the animation translation with the mesh reference translation. Animation
preserves the authored translation. Eve's fixed reference pivot is approximately
(0, -118.549, 147.819) cm. Rotations alone cannot reproduce the cinematic path.

Only the two helper modes are changed. Keep the accepted body policy, all 379
reference bones, nine virtual bones, mesh, hand corrections, body proportions,
physics, hair settings, ground offset and movement clips intact. No camera offset
hack, camera-state override, DLL hook or per-frame work is added.

## Reproduction and regression commands

From CustomShellSystem:

```sh
python3 tools/authoring-probes/camera/verify_camera.py \
  work/parry1/decoded/SKEL_Base.json work/parry1/decoded/SKEL_Base.json
# Expected failure: the original helper modes are still Skeleton.
python3 tools/authoring-probes/camera/verify_camera.py \
  work/parry1/decoded/SKEL_Base.json work/parry1/fixed-decoded/SKEL_Base.json
# PASS: full decoded equality except the two helper modes.
```

`repair_camera.py` changes the editor asset with a backup, then verifies all
reference transforms and unrelated modes. Run its verify mode in a fresh editor.
`short-retarget-path.patch` fixes the helper's old authoring-path allowlist so
it accepts /Game/CSS/. `camera-retarget-policy.patch` updates the historical
setup/probe/validator scripts so rebuilding from them cannot reintroduce this
omission. Both patches have been applied to the workspace authoring tools.

The authoring helper module build, editor repair, fresh editor readback and
one-asset Windows cook all exit zero. Full packaged skeleton JSON comparison
passes, including sockets and virtual bones. All 239 other export/bulk payloads
remain byte-identical to the accepted alpha sample. Metadata differs only in
container sizes and hashes. ZIP CRC and extracted byte readback pass.

Evidence: work/parry1/{repair,verify}-result.json, cook-exit.json,
regression-before.log, regression-after.log, pack/verification.json,
game-cameras/*.json. Short cooked path remains /Game/CSS/Shared/SKEL_Base.
The alpha native build was checked from its original clean tagged worktree;
Ninja reports no work needed because this repair changes no runtime source.

## Release handling

The replacement Eve ZIP retains v1.0.0-alpha.1 as explicitly requested. Preserve
the original runtime tag and runtime ZIP. Record this asset correction's separate
source commit and checksums in release verification rather than moving the tag.

Candidate Eve ZIP SHA-256:
1eb327b1ce5a86327be8b95974b4dc1b1d88c4405e74c56f9b382ef076b4f3d4

Game deployment preserved all installed CSS/CSSX DLLs, selectors and state.
The user closed the game after the automatic window-close attempts did not
complete. The installer then verified the game was stopped before writing.
Receipt: work/parry1/live2/deployment.json. The user is starting the game.
No hot reload. Current standalone CSSX work belongs to a separate agent and is
not part of this alpha asset correction.

## Gameplay acceptance

The user tested the installed candidate and answered "Both look correct now"
for riposte follow-ups and trap close-ups. The two 30-second game-window recordings
under work/parry1/live2, after.mp4 and after2.mp4, show riposte close-ups, returns
to normal camera framing, movement and combat. The first also contains a CSS
menu visit and return. Reviewed sheets: riposte.jpg and after2-sheet.jpg.
Trap acceptance is the user's direct review; a distinct trap reaction was not
identified in the sampled recordings. No claim is made for every cinematic.
Twelve runtime samples have no animation or maintenance error. Installed package
hashes match the candidate ZIP; the deployment preserves CSS/CSSX runtime files.

This resolves the two reported close-up cases. The shared helper correction
also applies to other animation cameras that use these bones, but those cases
still require normal release testing.

Custom Eve idle and the wider v1.0.0 work stay paused until this urgent repair is
finished. Do not restart accepted hand, proportion or movement tuning.
