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
intervals and constant endpoint extrapolation. Six focused tests cover this
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

All 195 diagnostic facial samples across the full 19.333334 seconds were
rendered at 10 samples per second and inspected through ten sheets. Early
sections are largely neutral; the more visible mouth/tongue and squint motion
appears around 14-18 seconds. No gross detached geometry is visible in these
views. This is a face-only source-shape check, without body motion, game
materials or runtime blending. Source keys are not resampled by this review.
The renderer explicitly allows signed and above-one values and verifies that
Blender did not clamp a requested weight.

## Remaining work

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
