# Facial motion inside supplied performances

Private authoring checkpoint, 2026-10-06. The installed combined trial supports
the 21 static Eve expressions. It does not yet reproduce these performance
curves alongside the body animation.

## Source evidence

Four decoded dance sequences contain named facial curves. Their compressed name
tables agree exactly with the decoded `FloatCurves` names. All inspected keys
use linear interpolation, unweighted tangents and constant extrapolation.

| Source | Duration | Curves | Keys | Unavailable names in the accepted Eve assembly |
| --- | --- | --- | --- | --- |
| Come Closer, source-04 | 19.333334 s | 15 | 987 | None |
| DumbDumb, source-05 | 65.7 s | 12 | 2,616 | smile, blink, lid_mid_closed_L, lid_mid_closed_R, iii, woo, eee, ooo, brow_down_miner |
| Iris Out, source-06 | 16.666666 s | 23 | 1,892 | lid_mid_closed_L, lid_mid_closed_R |
| Jang Won Young Spin, source-07 | 3.6666667 s | 11 | 426 | blink, woo, ooo |

These are comparisons against all 536 shape keys in the saved V45C Eve Body,
not just the 99 shapes selected for the static expression trial. Missing names
need the source's actual morph/driver mapping. Similar-looking names are not
evidence of an equivalent control. No aliases were guessed and no missing
curve was discarded. DumbDumb and Iris Out also retain their separate unresolved
seven-bone mapping problem.

A fresh retail skeleton readback in `retail-skeleton-audit-v2` still contains
3,267 reference bones and no `VirtualBones` definitions. Its GUID is
`2C8DEB63-499D8C56-DAA3AD80-AF949D71`; it does not identify the extra source
indices 3269-3275. The `VirtualBoneGuid` property alone cannot supply their
names, parents or reference transforms. Do not drop or guess those tracks.

Come Closer contains signed weights and values above one: the full range is
-0.13211426 to 1.116883. Clamping to a conventional zero-to-one slider would
alter the source. `prepare_performance_curves.py` preserves every named key,
time, value, tangent, flag and extrapolation setting. It rejects mismatched or
duplicate names, nonfinite data, unordered/out-of-range times and interpolation
it does not support. Its diagnostic evaluator uses the original linear
intervals and constant endpoint extrapolation. Nine focused tests cover this
contract, including immutability and signed/above-one values.

## Come Closer authoring candidate

The facial exporter now accepts explicit performance metadata in addition to
the static expression set. It requires every requested shape to exist. The
new export adds six existing source shapes: `squint_left`, `tongueTip_right`,
`tongue_arch`, `tongue_flatten`, `tongue_left` and `tongue_right`.

The resulting 105-shape candidate preserves the original body points, all 99
previous facial shapes and all 21 expression definitions exactly. Its body-fit
difference is zero; the existing body/hand corrective comparison remains
0.0000775943 cm. Source hashes remain unchanged. This is a derived JSON export,
not a recooked or installed update.

The six revised UE candidates have since passed a fresh-process editor
comparison. They use separate `SK_Face_Performance_*` names, each with 105
facial shapes plus the 22 preserved body/hand shapes. Geometry, bone bindings,
materials, physics and post-process bindings passed the existing verifier.
The protected source files and facial interchange hash are unchanged. Reports:
`facial-performance-export-v2/ue-import-all.json` and `ue-verification.json`.
The same six candidates were staged under
`/Game/CSS/Locomotion/Faces/SK_Face_Performance_*` and cooked successfully.
Independent cooked readback passes for all six: source geometry, skin weights,
bindings and the 22 original body/hand morphs match. All 105 facial shapes pass
the mapped source-delta comparison, including the extra six. Evidence is in
`shared-faces-v2/cooked-readback-v1.json`, with a separate private probe
container and source vertex maps. The revision-bound `facial-rig.json` includes
the 21 static expressions and the exact Come Closer keys. No candidate has been
installed; animated live comparisons remain pending.

All 195 diagnostic facial samples across the full 19.333334 seconds were
rendered at 10 samples per second and inspected through ten sheets. Early
sections are largely neutral; the more visible mouth/tongue and squint motion
appears around 14-18 seconds. No gross detached geometry is visible in these
views. This is a face-only source-shape check, without body motion, game
materials or runtime blending. Source keys are not resampled by this review.
The renderer explicitly allows signed and above-one values and verifies that
Blender did not clamp a requested weight.

## Remaining work

The native catalog now accepts optional per-rig `performances`, keyed by the
body clip's canonical ID. Each performance declares its duration and named
linear curves as `[seconds, value]` pairs. Parsing rejects missing body clips,
movement blend spaces, unknown shapes, duplicate curves, malformed keys and
excessive counts. Old facial catalogs need no changes.

The native sampler takes the existing body clock's position and blend weight,
plus reusable baseline/output buffers. It preserves signed values and values
above one, uses constant endpoint extrapolation, and allocates nothing on a
valid sampling call. Zero blend weight returns the baseline exactly. Tests
cover shared pause, seek, speed, looping, fade and immediate interruption.
This is data-layer support, not an installed runtime facial player.

`prepare_performance_curves.py --clip <canonical-id>` emits the native entry
alongside the complete preserved source metadata. It requires every named
shape, respects case-insensitive Unreal names while emitting the declared
shape spelling, enforces runtime bounds and leaves the source keys unchanged.
The generated Come Closer entry exactly matches the native test corpus.

The real Come Closer corpus compares all source key times and interval
midpoints against the independent Python evaluator: 320 sample times, 15
curves and 987 original keys. The comparison tolerance is 0.0000002 in morph
weight. All 23 portable CSS suites and the Windows core build pass. Evidence:
`facial-performance-native-v1/comecloser.json`.

Runtime integration must account for Original face using the original outfit
mesh, which lacks these facial shapes. A performance needs a temporary derived
mesh without changing saved expression selection. Restore the previous mesh
and expression on completion, cancellation, failed startup or outfit change.
Use Appearance's existing morph owner so static expressions and performance
curves cannot overwrite each other independently.

Epic's [UE 5.6 animation-curve documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-curves-in-unreal-engine?application_version=5.6)
also describes driving morph targets directly from animation curves with
matching names and morph-target flags. That path needs an ownership check
before adoption here: in the installed 5.6.1 engine source,
`SkeletalMeshComponent.cpp` applies animation curves first and then
`UpdateMorphTargetOverrideCurves()`. Existing static expression overrides
therefore need explicit coordination even if curves are baked into sequences.
The native sampler is independently validated; the final engine write path
still needs live timing and restoration evidence.

- Trace the missing controls to their source morphs or driver graph. Keep
  their original names and curves until the mapping is measured.
- Evaluate facial shape animation in the supplied Blender examples, using
  their actual active action/NLA state. The 200 inspection includes a
  Bezier `jaw_open` action over frames 1-10. The 801 file contains several
  facial actions, so their presence alone does not establish which one plays.
- Extend derived facial meshes, catalog validation and cooked readback to
  cover accepted extra shapes. Do not overwrite the installed static trial
  while its live acceptance is pending.
- Bind performance facial evaluation to the same body playback clock,
  including pause, seek, speed, fades, interruption and restoration of a
  user's selected expression. Avoid two independent writers fighting over
  the same morphs. Validate both CSS and CINE.
- Check source-to-cooked and live playback before claiming these facial
  performances are supported.

Evidence under `work/cine-animation-port/`: `performance-curves-v1/` retains
all four source curves and their hashes; `facial-all-shapes-v1.json` records the
complete unchanged assembly's shape names; `facial-performance-export-v2/`
contains the candidate, source hashes and exact baseline comparison;
`facial-come-closer-review-v1/` and `facial-come-closer-sheets-v1/` contain the
full sampled face review. Original outfit packages and game files are unchanged.
