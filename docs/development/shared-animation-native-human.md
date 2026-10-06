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
The original audition retains its original compression setting. The separate
native binding below uses the precise codec and now passes the Windows cook.

The default Eve retarget also matches its saved earlier result exactly:
388 tracks, 233,188 authored keys. These checks prove the numerical change and
that control, not compatibility of every clip or visible outfit.

## Canonical native binding and Windows cook

The normal authoring placeholder at the game's skeleton path has only 270
bones. It cannot author the 1,199-bone native track map. A process-local
`bwrap` mount supplies the verified diagnostic skeleton at the canonical game
path while staging and cooking. The original placeholder remains unchanged,
and the temporary skeleton is never packaged.

`BindNativeHumanClip` accepts only matching bone names, indices, parents,
reference poses, translation modes and virtual-bone definitions. It duplicates
the animation data model without rewriting rotation keys, binds the canonical
skeleton, and removes the diagnostic preview and retarget-source mesh. Every
authored key remains exactly equal before and after binding.

The first copy attempt stalled while `SetSkeleton` cancelled a pending DDC
compression request. Its main-thread stack identifies
`WaitOnExistingCompression(false)` and `FRequestOwnerShared::Cancel`.
Waiting for the duplicate's compression to finish before changing its skeleton
avoids that cancellation path. Intermediate attempts to reconstruct tracks
through the animation data controller changed rotation keys and were rejected.
The accepted path keeps exact key comparison, with no relaxed tolerance.

The staged sequence is
`/Game/CSS/Locomotion/NativeHuman/Studies/NH2/AN_BlackHole`. Its private package
contains only that sequence and the existing L5X compression settings. The
game containers alone resolve its skeleton dependency. Independent container
round-trip and import inspection find no outfit or diagnostic-mesh dependency.

The absolute-track decoder previously required embedded reference transforms
for virtual bones. Native sequences use the skeleton's default reference
instead. `RawAbsoluteTracks` now reconstructs those reference locals from
the cooked raw hierarchy and declared virtual-bone source/target relationships,
following UE 5.6.1 `FReferenceSkeleton::RebuildRefSkeleton`. Missing or invalid
dependencies still fail explicitly. Existing sequence-owned references retain
their previous path.

The full independently decoded Windows clip passes all 601 frames, 1,208
tracks and nine virtual tracks at the original 30 Hz. Maximum local errors:
0.00135491 cm position, 0.000182005 quaternion distance, and 6.04e-8 scale.
The unchanged thresholds are 0.05 cm, 0.25 degrees and 0.001 scale. Full
DumbDumb and Iris Out decoder outputs are byte-identical to the earlier
results, including their complete binary transforms.

Evidence under `work/cine-animation-port/native-binding-v6/`:

- `stage-result.json`, `stage-exit.json`: exact binding, protected inputs,
  full raw capture and unchanged authoring placeholder.
- `cook-command.json`, `cook-exit.json`: isolated native-reference Windows cook.
- `pack/verification.json`, `dependency-verification.json`: container and
  dependency checks against the game without outfit containers.
- `decoded-runtime-v3/decoded.json`, `cooked-motion-verification.json`: complete
  independent decoded-motion comparison.
- `eve-decoder-regression/verification.json`: unchanged two-dance readback.

`decoded-runtime` records the initial missing virtual-reference rejection;
`decoded-runtime-v2` records missing copied decoder dependencies. Both are
terminal failures, superseded by the successful v3 readback. The reusable
staging and packaging scripts are `stage_native_human.py` and
`package_native_human_study.py`. No game files were changed. Native playback
ownership, visible-mesh physics and live acceptance remain unfinished.

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
