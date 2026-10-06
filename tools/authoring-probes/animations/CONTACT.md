# Measured sole-contact bake

This is an offline UE 5.6.1 diagnostic for the fitted Eve and older Seductress
targets. It is not a runtime foot solver or proof of outfit compatibility.

Build `CSSContactLibrary.h` and `.cpp` in the existing CSSAuthoring editor module
with C++20 and an explicit `AnimationCore` dependency in `CSSAuthoring.Build.cs`.
Keep other sessions' authoring helpers intact when staging these two files.

1. Export every raw source key using the target skeletal mesh. Preserve the
   frame rate, source hash, fitted bind and source asset path in the work receipt.
2. Run `render_eve.py` with `--stride 1 --measure-only --footwear-only`.
   Left/right vertex membership follows leg weights, including crossed poses.
   This mode reports no body-contact measurement. Use the ordinary mode for
   full visible-body and garment review.
3. Check the measurement's motion hash, complete ordered frame coverage and
   sample rate before passing the two per-foot height arrays to
   `CSSContactLibrary.bake_sole_contact`. The current audition uses 0.1 cm
   smoothing. Use a new `/Game/CSS/AnimLab/RT_` output package.
4. Save, reopen and independently evaluate the result. Re-measure each foot,
   inspect motion around contact and knee transitions, and compare cooked poses.
   Preserve original clips and failed candidates as separate evidence.

The helper duplicates the animation and edits only thigh, calf and foot tracks.
It uses the engine's two-bone IK without stretching, preserves foot rotation and
horizontal placement, and retains every other authored track exactly. Timing
and track coverage are checked before returning the candidate. A C1 positive
part smooths contact onset, with at most 0.025 cm additional lift at zero depth
for the 0.1 cm setting. Straight legs borrow the nearest measured knee plane in
hip-local space; no usable plane or an unreachable goal fails the bake.

The initial target has hand virtual bones. Rigs with virtual bones depending
on the corrected legs are rejected because those tracks need a separate bake.
Curves, attributes and animation notifies also require separate handling.
Source hashes, full-clip skinned review, other outfit/footwear checks, cooked
readback and live acceptance remain mandatory before release.

## Source knee plane and older Seductress trials

`PreserveSourceKneePlane` is an opt-in correction to the pole calculation.
Project the source knee onto the plane perpendicular to its original hip-foot
axis first, then project that direction onto the corrected goal plane. Using
the original knee position directly can reverse the bend when a raised goal
crosses a nearly straight leg. Existing callers retain the old behavior until
their trials have been repeated with this option.

`bake_seductress_contact.py` reads the audited material visibility contract and
per-material full-weight skin measurements. `CSS_CONTACT_MODE=create|verify`,
`CSS_CONTACT_REVISION=S4|S5` and `CSS_CONTACT_KNEE_PLANE=goal|source` select an
isolated run under `CSS_ANIM_WORK`. Verification repeats the bake in a fresh
editor, compares every authored track, and captures saved playback at twice
the key rate. Unsaved shared-path copies only satisfy the comparator's path
guard; they are never catalog entries.

S5 removes S4's newly introduced knee-plane flips at authored keys while keeping
all non-leg tracks exact. The six saved/repeated clips pass, but their dense
surface scan still finds up to 1.227 cm penetration in Come Closer. Catwalk's
worst sampled footwear penetration is 0.082 cm. These are diagnostic results,
not accepted contact or rendered-motion results. Evidence is under
`work/cine-animation-port/retarget-seductress-s5/`.

`measure_skin_contact.py` accepts plain JSON and gzip captures. Its scope is the
poses supplied by the caller, which may include interpolation samples. It does
not apply material visibility; use the audited visible sections when reading
the report. Foot-weighted skirts must not be interpreted as shoe soles.

## Dense refinement

`refine_seductress_contact.py` uses the S5 dense measurements for an isolated
S6 trial. Set `CSS_ANIM_WORK`, `CSS_CONTACT_MODE=create|verify` and optionally
`CSS_CONTACT_LABELS` (comma-separated audited clip/footwear labels). It imports
S5's measured 60 Hz playback into an unsaved duplicate, retaining duration,
then corrects contact at those keys. Explicit population notification updates
the sequence's cached sampling rate; changing the model frame rate alone left
the cached key count at 581 in the first failed trials. Those receipts remain
in the work directory. Original source packages are never overwritten.

Come Closer Flat passes the fresh-process comparison at all 388 tracks and
1,161 keys. Its 2,321-pose 120 Hz surface scan finds -0.412 cm left and -0.408 cm
right, down from -1.227 and -1.197 cm at S5's 60 Hz scan. The 60 Hz authored
keys themselves clear the visible footwear within 0.000018 cm. No greater-than-
90-degree knee-plane steps are introduced at the matched 60 Hz samples, but
maximum knee travel per sample increases. Contact and rendered-motion review
remain unfinished. This trial is not a 60/120 FPS game-performance claim.

S6 verification streams gzip captures. Plain and compressed Catwalk captures
produce exactly identical surface measurements over all 721 poses; see
`retarget-seductress-s5/gzip-equivalence.json`. S6's create/verify exit codes,
saved comparison, surface and preservation receipts are in
`retarget-seductress-s6/`. Further refinement must preserve playback duration,
upper-body motion and the actual footwear visibility contract.

## Stored-key verification and rendered surfaces

The refinement tool also accepts `CSS_CONTACT_BASE` and
`CSS_CONTACT_REVISION` for subsequent isolated passes. The next bake uses the
measured capture's rate, with the original duration retained. Stored keys are
read directly through `ReadAuthoredTrack`, avoiding float-time interpolation
during the import check.

UE 5.6.1's `AnimationSequencerDataModel` stores rotation as float Euler channels.
`UAnimSequencerController::SetBoneTrackKeys` converts input quaternions to
`FQuat4f`; `SetBoneCurveKeys` writes their Euler angles. This explains a
0.002581-degree clavicle difference in the S7 import. The independent
`SequencerRotationReference` calculation exactly reproduces that worst stored
rotation. The check retains a 0.00003 normalized quaternion-component bound
only for this model with a matching conversion reference. Position remains
bounded by 0.0001 cm and scale by 0.000001. Other models keep the tighter
rotation bound. Failed probes and the exact source hashes remain in
`retarget-seductress-s7/native-key-readback-audit.json` and its adjacent logs.

S7 Come Closer Flat passes the fresh saved/repeated comparison for 388 tracks
and 2,321 keys at 120 Hz. Its 4,641-pose 240 Hz scan finds 0.1444 cm left and
0.1012 cm right penetration, each with one sample below -0.1 cm. Authored keys
clear within 0.000010 cm. No new knee-plane flips occur at matched 120 Hz
samples, but peak knee travel increases slightly. This remains a contact
trial, not accepted gameplay motion.

`render_skin_contact.py` builds selected cooked surfaces directly from the
full-influence audit and captured poses. It leaves assets unchanged and can
render still indices or stream a full clip to FFmpeg. The S6 and S7
`bare-skin-motion/` directories contain 60 Hz lower-body videos and sampled
review sheets. These use sections 3, 23 and 24, without stockings, to isolate
leg deformation. Including the untextured stocking surface over skin produced
overlap artifacts, which disappeared in the bare-surface control. The renders
rebuild normals and omit material masks, morphs, cloth and post-process, so
they do not establish full-outfit appearance or gameplay acceptance.


S8 Come Closer Flat uses 4,641 authored keys at 240 Hz. Fresh-process repeat
matches all 1,800,708 bone keys exactly. A 9,281-pose 480 Hz scan finds worst
visible-foot penetration of 0.02607 cm left and 0.01731 cm right, below the
0.1 cm diagnostic limit at every sampled pose. Matched 240 Hz comparisons
introduce no greater-than-90-degree knee-plane steps. Maximum horizontal foot
drift is 0.00317 cm and orientation drift is below 0.000043 radians. Peak knee
travel still rises slightly. Non-leg resampling differences remain below
0.000064 per component. These results are in `retarget-seductress-s8/`.

The S8 lower-body video has 1,161 frames at 60 FPS (19.35 seconds with the
inclusive end sample). The animation itself remains 19.333334 seconds. Sampled
review-sheet images show no obvious new knee collapse or ankle distortion;
this is not a continuous visual or full-outfit acceptance. The same candidate
passes 54 compressed component/adapter cases on all six older Seductress
meshes: 15,606 evaluated poses, including 6,966 full-clip samples and 38 finger
bones. The single-clip evaluator uses the independently verified S3 Black Hole
as its distinct base to test stopping and restoration.

Do not raise sampling density again without cooked evidence. The S8 raw
package is 30,739,803 bytes; cooked size, compression error and live frame cost
remain unmeasured. The existing cook commandlet rejects transitive legacy
`/Game/CSSAuthoring/` dependencies, including this released rig's skeleton.
Resolve that with audited external package dependencies while preserving
released paths and original packages, not by disabling the path guard or
pretending this rig is the current Eve skeleton. Compressed editor-component
passes do not replace the independent Windows cook and surface readback.
