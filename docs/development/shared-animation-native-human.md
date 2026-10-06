# Default human animation playback

October 6, 2026. Native support is still in development, not installed or
accepted. This work covers the default game human skeleton. It does not add
support for excluded custom-rig projects.

## Native reference and retargeting

The independently decoded game skeleton has 1,199 raw bones and nine virtual
bones. The existing three-vertex diagnostic reference matches every name,
parent, translation and scale. Its largest quaternion distance from the game
dump is 5.55e-8. Translation retargeting modes and all nine virtual-bone
definitions also match. The diagnostic mesh is a numerical fixture, not a
representative visible outfit.

Six rope/string bones have mirrored reference scales. UE 5.6.1 strips these
scales while constructing its IK retarget pose, before our unit-scale guard.
The first experiment expected a rejection and disproved that hypothesis.
The measured baseline instead creates a clip with the six mirrored scales
lost. Do not treat the initial failed assertion as an engine failure.

`RetargetClip` now has an explicit `PreserveScaledReferenceBranches` option,
off by default. It requires attachment preservation and accepts only unmapped
terminal branches below an animated ancestor. It restores complete reference
locals on scaled branches and their descendants. Mapped/intermediate bones,
singular scales and invalid transforms remain rejected. Existing callers keep
their original behavior.

The native Black Hole audition keeps the complete source performance:
601 keys, 30 Hz, 20 seconds. Across every raw frame, the six affected bones
recover the native reference (exact translations/scales, quaternion distance
below 3.73e-8). The other 1,193 raw bones exactly match the unchanged retarget
control. Original source, diagnostic and production skeleton files retain
their hashes. The new candidate is isolated at
`/Game/CSS/AnimLab/AN_NH1_BlackHole`.

Evidence under `work/cine-animation-port/`:

- `native-retarget-baseline-v1`: original, disproved rejection hypothesis.
- `native-retarget-baseline-v2`: successful full-clip reproduction of scale loss.
- `native-retarget-build-v1`: successful UE 5.6.1 editor-module build.
- `native-retarget-v1`: corrected full-clip comparison and protected hashes.

Reusable authoring probes are `retarget_native_human.py` and
`verify_native_human.py` under `tools/authoring-probes/animations/`. The latter
checks fresh saved/compressed poses and the unchanged Eve retarget control.
The default compression check failed the rotation bound (0.00235781 quaternion
distance versus the 0.001 limit). Repeating with the existing L5X ACL settings
(0.001 error threshold), on an unsaved duplicate only, passes all 601 frames
and 1,208 raw/virtual bones: maximum position error 0.00135491 cm, quaternion
distance 0.000182005, exact scale preservation. The worst errors are on virtual
hand/forearm bones. No acceptance threshold was relaxed. See
`native-retarget-v1/precise-verification.json` and `precise-verify-exit.json`.
The saved audition still has its original compression setting; its future
staging step must bind the precise codec and validate the Windows cook.

The default Eve retarget also matches its saved earlier result exactly:
388 tracks, 233,188 authored keys. These checks prove the numerical change and
that control, not compatibility of every clip or visible outfit.

## Native animation graph and playback ownership

The decoded `ABP_Player` has `FullBody` and `DefaultSlot` montage slots.
`FullBody` feeds a Boolean blend that can bypass it. `DefaultSlot` is later in
the graph. A native montage may let the current appearance keep its own
post-process physics graph. This is a candidate route, not verified playback:
aiming, hand correction, additive pose offsets and full-body IK still run
downstream and could alter the performance.

Graph evidence is
`work/grip-grounding-v1/player-hand-graph/decoded/ABP_Player.json`, with the
native mesh/skeleton exports in
`work/cine-animation-port/native-human-compatibility/assets.json`.

The pinned 5.6.1 engine source also shows that
`PlaySlotAnimationAsDynamicMontage_WithBlendSettings` calls `Montage_Play`
with its default stop-other-montages behavior. `StopSlotAnimation` can stop
unrelated transient montages in that slot. A CSS implementation needs an
owned montage pointer, competition checks and precise cancellation. The
current performance guard rejects any active montage, so it must distinguish
its own montage from gameplay interruption for this route to work.

Do not remove the outfit restriction alone or replace every native
post-process graph with an Eve graph. CSS and CINE still need a verified
native animation binding, playback ownership, interruption/restoration,
actual skinned-mesh/physics checks and live acceptance. Missing facial
controls must not prevent compatible body playback.

## Sources

Epic's [IK retargeting documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
describes transferring animations between rigs with different hierarchies.
Version-specific behavior above comes from the installed 5.6.1 source:
`IKRetargetProcessor.cpp`, `FResolvedRetargetPoseSet::AddOrUpdateRetargetPose`,
and `AnimInstance.cpp`, `PlaySlotAnimationAsDynamicMontage_WithBlendSettings`
and `Montage_PlayInternal`. The web documentation currently defaults to a
newer engine version and does not establish this game's compatibility.
