# Animated wardrobe preview, 2026-09-13

The user reported that the protected wardrobe froze character animation. Global game pause correctly froze gameplay, but also froze the real player's skeletal animation.

The working implementation spawns an `ASkeletalMeshActor`, copies the selected mesh, material overrides and mesh world transform, and disables collision. It plays `A_Shared_Idle_L` on the same human skeleton. The component can tick while paused; CSS advances `SetPosition` from real elapsed time with `bFireNotifies=false`. Playback rate is zero so engine time cannot advance it a second time. The real player and its attached actors are hidden with their original hidden flags saved for restoration. The preview owns no gameplay abilities, input or callbacks into the core DLL.

Only the disposable visual copy changes animation mode. Close, error cleanup and core stop restore visibility, destroy the preview, restore the camera, release the owned pause and restore controller settings. Weak object handles are resolved again before use. Asset loading is followed by pawn, component and mesh identity checks.

`A_Genessa_Idle_H` was rejected by live reflection: it uses the correct human skeleton but has `AdditiveAnimType=1`. It cannot be played alone as an absolute full-body animation. The shared light idle is a compatible non-additive sequence. Do not remove the additive and skeleton guards to force an idle to load.

Epic documents the explicit notify flag on [USkeletalMeshComponent::SetPosition](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USkeletalMeshComponent/SetPosition). The shipped UE5.6 reflection dump supplies the actual function signatures and parameter layouts; current online documentation is not an ABI reference for this binary.

## Live evidence

- [Preview lifecycle checks](../work/preview-lifecycle-checks.json): two evaluated head positions differ while `world_time` remains identical; all three outfit previews report moving bones and an active pause.
- The same checks verify that a favorite change preserves the camera actor and exact default framing. After a deliberate 30-degree orbit, a wardrobe refresh returns to the default using that same camera actor.
- Closing restores player visibility, input and pause. The gameplay animation-instance path before and after closing is identical.
- [Active preview reload](../work/preview-active-reload.log) acknowledges the new core in the same Windows game process, PID 372.
- [After reload](../work/preview-after-active-reload.json): player visible, pause false, cursor false, no CSS camera, preview or input ownership, original gameplay animation instance still present.
- [Final screenshot](../work/animated-wardrobe-final.png) and [inspection](../work/preview-final-inspection.json).
- User confirmation: “Looks right” for the animated idle and steady default camera.

The bone-motion diagnostic stops sampling once motion is observed. Detailed frame values are read only by an explicit `inspect` request, keeping animation time and camera motion out of the regularly written status file.

## Camera and UI findings

The game uses CameraStateFramework. `PlayerController.GetViewTarget` can still return the pawn after a successful custom camera change. `PlayerCameraManager.ActiveCameraActor` is the effective camera and must be checked first.

On initial open, attach the UMG root again after switching camera and pause state. Before this ordering fix the widget could report `IsInViewport=true` but remain invisible. A category, favorite or outfit refresh now rebuilds only the widgets. It retains the live camera, pause and input ownership. It replaces the preview actor only if the selected mesh changes and carries the idle phase forward.

The user's final camera preference is conditional recentering: do nothing if already at default, otherwise return to default when changing a wardrobe option. Right-stick inversion is vertical only.

## Remaining limits

The preview uses one full-body idle for both outfits. Per-outfit pose choices and independently animated cloth need further work. World pause was measured directly; arbitrary third-party real-time hooks are outside that protection. Cross-level lifecycle checks and performance profiling remain pending.
