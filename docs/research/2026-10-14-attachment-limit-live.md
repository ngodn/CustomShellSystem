# Live attachment check above 128 children

Peony completes three Iris Out facial-mesh activation and restoration cycles
with **129 direct mesh children and 166 player-owned components**. All 73
temporary components are removed afterward. The original 56 direct children
and 93 owned components return with identical handles, parents and sockets.
Saved CSS state, CINE model, appearance, idle override and player/camera state
restore exactly. A separate live read confirms those counts and settings with
both menus closed. No core or asset installation was needed.

## Method and provenance

The previous repeated-switch run tested 30 cases at 56 direct children. This
check specifically crosses the old 128-child snapshot limit. It uses the same
running process, PID 1944353, with core SHA-256
`7004a5a913f6c8ce31fb26b64e6b055134b684269074faa7eb42079b286b3dc1`.

`tools/diagnostics/check_attachment_limit_live.py` creates 73 empty
`USceneComponent` objects on the current player through reflected
`AddComponentByClass`, then attaches them to the body mesh with no socket.
These are not renderable mesh or collision components. Each returned handle
is recorded before attachment, and cleanup destroys only those recorded
components after checking their owner. No gameplay input or save-game edit is
sent. Player, camera and appearance guards run throughout the test.

The implementation follows the project's existing component creation and
cleanup calls in `native/src/attachment_follower.inl`. Before mutation, live
reflection confirmed the signatures. Local UE 5.6.1 sources establish creation,
registration and owner-authorized destruction:

- `Engine/Source/Runtime/Engine/Private/ActorConstruction.cpp`,
  `AActor::AddComponentByClass` and `FinishAddComponent`.
- `Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp`,
  `UActorComponent::K2_DestroyComponent`.

Epic's [AddComponentByClass documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/AddComponentByClass)
also describes ownership and manual attachment. The installed branch and live
metadata, rather than the current online documentation's engine version,
determine the actual calls used here.

Each cycle starts Iris Out at two seconds, pauses after blending in, verifies
`SK_PeonyV2_Face` and the enabled full-weight performance adapter, then stops
only its owned performance token. `SK_PeonyV2` and the prior post-process class
return. All 129 child handles, parents and sockets remain identical throughout
the sampled facial/normal transitions. The shared manifest independently
confirms the exact facial mapping and `hide_weapons=true`, which exercises the
production attachment snapshot in performance startup. Normal accessory passes
also enumerate the expanded array.

## Evidence

Retained under `work/three-outfit-rig-physics/attachment-limit-live-20261014/`:

- `recipe.py`, `requests.jsonl`, `result.json`: executed recipe, reflected
  requests/results, component ownership ledger and three passing cycles.
- `state-before.json`, `cine-before.json`: restoration baselines.
- `verify.py`, `verified.json`: independent checks of component sets, parent/
  socket records, exact manifest mapping and restoration evidence.
- `live-after.json`: fresh post-cleanup live counts and unchanged appearance/state.
- `SHA256SUMS`: retained evidence hashes.

Python 3.14.7 compiles the diagnostic. The live runner and independent archive
verification both pass. No new production code was required. Only small
diagnostic evidence was generated; no full project copies, cooks or packages.
Before and after free space from `df -h`: root 97G, theark 60G. No cleanup of
unrelated files was needed.

## Scope

This verifies the removed snapshot cap and Peony facial/attachment restoration
with a real registered child array above 128. It does not simulate 129 rendered
accessories, 129 missing-bone follower transfers or 129 collision-offset records.
Empty components also do not establish cloth/rendering cost. Long-session
lifetime, world/save/form changes, action/weapon transitions and the broader
release matrix remain separate checks. The existing production snapshot host
fixture still supplies malformed-array and larger-count coverage through 4,097.
