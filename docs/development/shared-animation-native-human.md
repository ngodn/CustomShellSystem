# Default human animation playback

## Current scope, October 8

The [three-outfit rig goal](three-outfit-rig-physics-goal.md#follow-up-stage-locomotion-review-and-native-human-conversion)
supersedes the earlier four-choice limit below. Convert the remaining 59 entries
in the existing 63-entry library for the default game human skeleton. Preserve
the four working gait choices, all custom-rig assets and public IDs. Missing
native facial controls must be reported separately from body compatibility.
Canonical new assets belong under `/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/`.

The October 8 `catalog-namespace3` inventory resolves every remaining body source
to its migrated editor asset in `migration-editor8`. The existing native
reference and retargeting evidence below remain the starting point. New retarget,
cook, visible-pose and gameplay results are required; asset availability alone
is not conversion acceptance.

## October 8 conversion batch

`work/three-outfit-rig-physics/native-library1` now contains 59 new native
sequence candidates, 20,059 source keys and about 412.18 seconds of motion.
`pilot3` authored three clips and `batch1` authored the remaining 56; both
commandlets exited successfully. All retain source key counts, sample rates
and durations. Binding to the native skeleton and renaming into the author
namespace preserve every authored key. Protected inputs retain their hashes.

The isolated editor initially could not load `ChaosClothAsset` because the
copied authoring module's relative library paths resolved from its new location.
The recorded successful command supplies exact engine plugin library directories
to this process only. No system library configuration or original binary changed.

`verify1` independently reloads Seated Recline, Black Hole and Bring It On,
compares saved authored hashes, then checks raw versus compressed local poses
at every one of their 709 keys and 1,208 bones. All pass the existing bounds:
position below 0.01 cm, quaternion distance below 0.001, scale below 0.00001.
The largest observed position error is 0.0014463 cm; scale error is zero.
These editor checks do not prove Windows cooking or visible/gameplay acceptance.

The seven looping idles now have constant native carriers for CSS's existing
idle path. `carriers4` and `carrier-readback1` pass creation/reload with all
1,092 interpolation queries unchanged. Existing-output, missing-source,
mismatched-skeleton, nonconstant-source and wrong-namespace inputs are rejected.
The other 52 entries use the existing performance playback path. No new UI
section or animation IDs are introduced.

`verify2` reloads all 59 authored sequences and compares every key across 1,208
bones through the C++ checker. The three pilot comparisons match the independent
Python checker within double rounding, including worst frame/bone identities;
see `compression-checker-parity.json`. All 59 saved authored snapshots match.
58 compressed comparisons pass. Cinderella's 8,381-key candidate fails the
position bound: 0.01983049 cm at frame 7746, `VB hand_r_relative_hand_l`.
Its maximum quaternion distance is 0.000871955 and scale error is zero.

`precision1` creates a separate C12 candidate and codec at 0.0001 error threshold,
without changing the original C11 candidate or any authored key. Fresh
`precision-verify1` still fails at the same virtual-hand frame (0.01968521 cm);
the maximum quaternion error is unchanged. This does not establish compression
quantization as the cause. Inspect long-clip evaluation time precision and raw
versus compressed virtual-bone construction before another codec trial. Do not
relax the acceptance bound or mark the whole batch passed. C12 remains rejected.

The staging tool still refuses the rejected precision replacement. It needs to
consume the resolved verification evidence before catalog integration. Do not
ship C12 just because its codec setting is tighter.

The Windows cook and independent game-only container round-trip now pass for
all 67 new packages (59 sequences, seven carriers and the original codec).
Evidence: `native-library-cook1/{exit.json,pack/verification.json}`. All protected
inputs retain their hashes; this is a private study, not an installed pack.

Fresh raw-pose exports cover all 20,059 source keys and 1,208 bones. Independent
cooked ACL playback with runtime interpolation agrees with the editor result:
58 of 59 pass the strict comparison. Cinderella still differs by 0.01983049 cm
at the same virtual-hand sample. See `native-library-cook1/runtime-verification.json`
and `native-library1/raw-keys1`. Quaternion and scale comparisons pass for all
59. The decoder's old fixed transform-count guard rejected this long clip;
it now checks actual buffer bytes against a 1 GiB limit before allocation.

The first independent decode used nearest-key sampling and failed three clips.
That is not the game's linear playback policy: it skips interpolation across
stripped keys. `decoded-runtime` uses the existing runtime helper's `none`
policy. The new comparator requires that policy and rejects the earlier
`decoded-keys` capture instead of treating it as gameplay evidence.

The targeted time probe explains much of Cinderella's discrepancy. At frame
7746, nominal double time is 129.1 seconds, while ACL receives float time
129.10000610351562. Matching that float clock reduces the position difference
to 0.00642633 cm; matching the ACL sample-index arithmetic reduces it to
0.00451787 cm. The raw evaluator accepts double time, whereas the installed
5.6.1 `ACLDecompressionImpl.h` converts evaluation time to float. Evidence:
`native-library1/time-probe1/result.json` and `probe_native_sample_time.py`.
This is a targeted diagnostic, not a full-clip clock-aligned pass. Keep the
strict failure visible until its disposition is recorded. The older cooked
motion checker uses a separate 0.05 cm review limit; do not silently substitute
that limit for the strict 0.01 cm comparison.

Catalog integration, visible-pose review and gameplay acceptance remain pending.

The C++ checker and carrier helper compile with UE 5.6.1/C++20 in
`native-compression-module1`. The first two Python carrier attempts exposed a
read-only Skeleton setter; the successful helper clones the existing native
constant carrier and changes only its sample sequence references. All work is
private. Neither installed containers nor the installed CSS DLL changed.


## Historical October 6 correction: four native locomotion choices only

For appearances using the default game human skeleton, implement only
**Eve Default Idle, Eve Walk, Eve Jog and Eve Sprint**, where technically
compatible. This applies to native, CSS and non-CSS appearances using that
skeleton. Validate their selection, smooth starting/stopping, restoration
and gameplay compatibility.

Do not expand native support to dances, photo poses, other idles or facial
expressions. Native Black Hole and montage experiments are retained as
historical evidence only; they are not release requirements or part of the
requested native selection. No remaining native dance batch should run.

The full requested animation library and feasible facial support remain in
scope for **Eve's six outfits, Unholy Genessa and Commander White**. Old
Seductress and GeneXXa remain excluded. This correction supersedes broader
native-coverage requirements and historical next steps below.

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

## Owned native playback candidate, October 6

The runtime now has a native route for the exact game human skeleton and a
player animation instance derived from the game's `ABP_Player`. It creates a
transient `DefaultSlot` montage, preserves the existing post-process graph,
and uses the shared performance clock for sequence position. The owned
montage has zero automatic play rate and automatic blend-out disabled;
CSS requests a 0.18-second Hermite blend when stopping. Source clips with
gameplay notifies, additive motion or root-motion extraction are rejected.

CSS and CINE can request this route without a CSS outfit manifest. Pending
menu playback records the actual mesh as well as the player, appearance and
catalog revisions. A mesh change cancels that request. Existing custom-rig
adapters still use their original performance fields and facial callbacks.
The native route has no facial mapping yet and does not claim one.

Only the owned montage is stopped. Existing gameplay montages block a new
take; a competing montage interrupts the current take. Mesh, animation
instance, post-process instance and gameplay character changes also interrupt
it. Visibility restoration retains its retry state if cleanup fails after
the owned montage was stopped. No source animation, game montage or native
post-process class is edited.

`native-montage-v2/result.json` passes 13 actual UE 5.6.1 lifecycle checks:
creation, ownership, blend-in, zero-rate position, seeking, held endpoint,
competition, stopping only the owned montage, cleanup, replay and blend-out.
It also checks the reflected parameter names used by the runtime calls.
Both diagnostic assets remain unchanged and the commandlet exits zero.
This uses a plain preview animation instance and the three-vertex diagnostic
mesh. It does not evaluate the game's slot graph, a visible outfit or cloth.

The first fixture failed cleanup because `TickAnimation` alone does not run
the component's bone refresh and queued animation-event dispatch. UE removes
terminated montage instances in `UAnimInstance::DispatchQueuedAnimEvents`.
The corrected fixture completes that component update; it does not manually
delete montage instances or relax the cleanup assertion. The v1 failure is
retained. Native blend weights remain engine-driven, while sequence time uses
the CSS clock; pause during blending still needs explicit live review.

`native-performance-trial-v1` contains 59 stable clip identities and 72 cooked
exports. All 71 previous exports are unchanged. Only Black Hole currently has
an additional native-human asset alternative. The five requested-outfit
adapters, six Eve face meshes and existing facial metadata remain unchanged.
The first metadata audit rejected a package-only skeleton path; the corrected
metadata uses its full object path. That rejected manifest and test output
are retained alongside the corrected package.

The final host catalog checks pass 1,093 assertions with all three requested
outfits and 1,082 with the shared pack alone. They verify native resolution
without an outfit, preserve the custom-rig alternative and reject unknown
skeletons. The performance clock/session/library tests and CINE performance
consumer test pass. The Windows core builds successfully.

This candidate is not installed. Remaining work includes the other native
clip alternatives, actual game-graph and visible-mesh/physics review, live
interruption/restoration and performance acceptance, plus the other facial
and source-accounting work in the main goal. No excluded rigs were added.

## Sources

Epic's [IK retargeting documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
describes transferring animations between rigs with different hierarchies.
Version-specific behavior above comes from the installed 5.6.1 source:
`IKRetargetProcessor.cpp`, `FResolvedRetargetPoseSet::AddOrUpdateRetargetPose`,
and `AnimInstance.cpp`, `PlaySlotAnimationAsDynamicMontage_WithBlendSettings`
and `Montage_PlayInternal`. The web documentation currently defaults to a
newer engine version and does not establish this game's compatibility.

## October 6: four native locomotion choices, asset checks passed

The native Idle, Walk, Jog and Sprint candidates now have 20 retargeted
sequences and four carriers under `/Game/CSS/Locomotion/NativeHuman/Studies/NL2`.
The walk/jog/sprint carriers retain all 36 samples each; idle retains four.
All 624 blend queries match the original Eve setup, including direction
wrapping, clamped speed, weights and per-sample rates. Fresh-process readback
passes all four carriers and all 754 sequence frames.

The Windows cook and independent game-only package round-trip pass for 25
packages (20 sequences, four carriers, one codec). Complete decoded motion
passes all 754 frames and 1,208 tracks per frame. Maximum differences are
0.001314966 cm position, 0.000178365 quaternion distance and 6.04e-8 scale,
within the existing unchanged comparison limits. Dependency inspection finds
only the owned animation assets, codec and canonical game skeleton. No outfit
or diagnostic skeleton/mesh ships in this study. Protected inputs and the
ordinary authoring skeleton placeholder remain unchanged.

Evidence:

- `native-locomotion-v4`: 20 sequences, exact binding checks, successful exit.
- `native-locomotion-blends-v3`: four carriers, 624 matching blend queries.
- `native-locomotion-readback-v1`: fresh reload and full raw captures.
- `native-locomotion-cook-v1`: successful cook, `pack/verification.json`,
  `decoded/decoded.json`, `full-motion-verification.json` and
  `dependency-verification.json`.

The first bind retained an editor FK Control Rig bound to the diagnostic
skeleton. Rebuilding the model introduced a tiny Euler round-trip difference
and was rejected. The accepted helper instead duplicates the model, gives its
private FK rig a new binding to the final skeleton, and retains exact authored
key checks. The failed attempts remain recorded; their assets are not used.
The blend-script retries corrected Python editor-property access and explicit
axis-value comparison, without changing the interpolation checks.

Next: integrate the four choices with CSS selection/profile persistence and
native idle playback, merge the accepted assets into the shared candidate,
then check real-mesh contact, transitions, physics and live restoration.
These are asset checks, not proof of finished native gameplay support. No game
installation occurred. Full animation/facial work on the three requested
outfits and the unresolved source mappings remain active; do not resume
excluded rigs or expand native performance scope.

## October 6: native locomotion runtime and combined candidate

The four native choices now use shared catalog resolution and independent
profile settings without requiring a wearable outfit manifest. Idle uses an
explicit validated BlendSpace carrier when the native post-process graph has
no CSS idle layer. Movement retains the existing blend ownership and gait
selector. The native graph is not replaced. Missing-pack choices persist,
Default remains selectable, and incompatible library entries are filtered.

`native-locomotion-trial-v1` contains 62 stable body/movement identities, 95
exports and four native alternatives. All 71 prior export payloads and five
requested-outfit adapters remain unchanged. The package passes container and
metadata readback; the combined catalog passes 1,125 checks and the pack alone
passes 1,114. The five focused data, profile, animation, library and runtime
tests pass. The Windows C++23 core builds. These checks do not establish
real-mesh contact, live graph behavior, smooth transitions or physics.

Nothing was installed. Continue native gameplay verification and the remaining
facial/source work for Eve, Unholy Genessa and Commander White. The full goal
and exclusions remain unchanged.
