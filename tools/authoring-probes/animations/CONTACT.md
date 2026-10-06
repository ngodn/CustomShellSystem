# Measured sole-contact bake

This is an offline UE 5.6.1 diagnostic for the fitted Eve target. It is not a
runtime foot solver or proof of compatibility with other outfit proportions.

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
