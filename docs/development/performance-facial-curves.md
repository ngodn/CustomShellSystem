# Facial motion inside supplied performances

Private authoring checkpoint, 2026-10-06. The installed combined trial supports
the 21 static Eve expressions and source-timed Come Closer facial playback.
The runtime and packaged data pass offline checks. Live facial playback,
restoration and timing still need acceptance.

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
with the subsequent cook and installation recorded below.

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
the 21 static expressions and the exact Come Closer keys. The combined candidate
is installed; animated live comparisons remain pending.

All 195 diagnostic facial samples across the full 19.333334 seconds were
rendered at 10 samples per second and inspected through ten sheets. Early
sections are largely neutral; the more visible mouth/tongue and squint motion
appears around 14-18 seconds. No gross detached geometry is visible in these
views. This is a face-only source-shape check, without body motion, game
materials or runtime blending. Source keys are not resampled by this review.
The renderer explicitly allows signed and above-one values and verifies that
Blender did not clamp a requested weight.

## Runtime and installation

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
The installed runtime supplies that clock to the facial sampler from the same
body pose update used by CSS and CINE.

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

Original face uses the original outfit mesh, which lacks these facial shapes.
Starting a supported performance temporarily applies the verified derived
mesh through Appearance and retains the previous mesh against collection.
The saved expression remains unchanged. Completion, cancellation and failed
startup schedule ordinary appearance reconciliation, using the current
selection in case an outfit, expression or gameplay transition occurred.

The facial performance captures the live expression weights as its baseline.
Static world-face writes pause during playback; menu preview writes remain
independent. Entry and exit use the body's blend weight. Shapes absent from
the performance fade out so a previously selected expression cannot distort
the authored face. Cleanup restores only weights still owned by this player.
The morph owner skips unchanged values, and sampling reuses its buffers.
The loaded body duration must match the facial duration before playback starts.

The Windows core builds and all 23 host suites pass. Actual data checks pass:
88 facial-catalog checks, 5,177 source-curve comparisons and assertions, and
85 packaged-catalog checks. The latter resolve 57 sequences, five post-process
adapters, all three source outfit revisions and all six derived Eve faces.
Eight installer tests cover rollback, closed-game races, shape-set validation
and preserving the installed CINE files.

`shared-v1/pack-with-face-motion-v1/` contains the 68-package candidate and
independent readback from that complete pack. It was installed with the game
closed. `live-face-motion-1/deployment.json` records the files and rollback;
`post-install-check.json` independently confirms all 25 checked installed or
protected files and both DLL selectors. CSS core SHA-256:
`7962d601cb9efb123f90b212c081a7d551ea987a923f1b3b16be9053bdda6210`.
The installed CINE DLL/menu, saved settings and source outfits were preserved.

Epic's [UE 5.6 animation-curve documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-curves-in-unreal-engine?application_version=5.6)
also describes driving morph targets directly from animation curves with
matching names and morph-target flags. That path needs an ownership check
before adoption here: in the installed 5.6.1 engine source,
`SkeletalMeshComponent.cpp` applies animation curves first and then
`UpdateMorphTargetOverrideCurves()`. Existing static expression overrides
therefore need explicit coordination even if curves are baked into sequences.
The native sampler is independently validated; the final engine write path
still needs live timing and restoration evidence.

## Remaining work

- Trace the missing controls to their source morphs or driver graph. Keep
  their original names and curves until the mapping is measured.
- Add the verified Gesture 200 facial candidate below to the next packaged
  catalog, with source evidence checked by the catalog builder. It is not in
  the currently installed trial.
- Extend derived facial meshes and cooked readback if further source controls
  can be resolved.
- Validate installed playback in CSS and CINE, including pause, seek, speed,
  fades, interruption, outfit changes and restoration of Original face or a
  selected expression. Measure live frame cost.
- Check source-to-cooked and live playback before claiming these facial
  performances are supported.

Evidence under `work/cine-animation-port/`: `performance-curves-v1/` retains
all four source curves and their hashes; `facial-all-shapes-v1.json` records the
complete unchanged assembly's shape names; `facial-performance-export-v2/`
contains the candidate, source hashes and exact baseline comparison;
`facial-come-closer-review-v1/` and `facial-come-closer-sheets-v1/` contain the
full sampled face review. Original outfit packages and game files are unchanged.

## Supplied Blender facial actions

Fresh Blender 5.2.2 inspection now records the mesh's shape-key animation
binding, action slot and NLA settings. An action stored in a file is not
necessarily assigned to the face. Blender documents these bindings in its
[AnimData API](https://docs.blender.org/api/5.1/bpy.types.AnimData.html) and
[action-slot design](https://developer.blender.org/docs/features/animation/animation_system/layered/).
The actual checks use the installed 5.2.2 evaluator with embedded scripts
disabled and source hashes unchanged.

- `200.blend`: one active `k200` NLA strip, frames 1-10 at 25 Hz, matches
  the existing 0.36-second body clip. It drives only `jaw_open`, from 0 to 1
  with Bezier easing. All 169 shapes were evaluated at 19 times, including
  half-frames; all other weights remain zero.
- `801.blend`: no assigned face action, no NLA strips and no face drivers.
  All 169 shapes remain zero across 2,263 evaluated times covering the full
  18.85-second body clip. `ak1`, `k0`, `k1` and the TakeItem facial action are
  unassigned library data. Do not invent a facial performance from them.

`audit_blender_face.py` ties evaluation to the verified body sample's source
hash, scene range and frame rate. A separate Blender fixture verifies NLA
retiming, half-frame evaluation, signed weights, exclusion of an unassigned
expression and source immutability.

Gesture 200 has uniform time handles at frames 1, 4, 7 and 10, with values
0, 0, 1 and 1. `prepare_blender_face.py` requires this supported kind of
binding and retains the original controls in its audit. It converts the cubic
to 275 linear keys using the second-derivative interpolation bound. Maximum
calculated error is 0.00000998988 in morph weight; the analytic source curve
differs from Blender's evaluated samples by at most 0.0000000949259. Four
portable tests check the bound between keys, signed/above-one weights,
rejection of unsupported bindings and source preservation. This is authoring
evidence, not packaged or live acceptance.

Receipts: `blender-facial-inspection-v1/{200,801}.json`,
`{200,801}-evaluated-controls.json` and `prepared-example200.json` under the
private animation work directory. The runtime trial installed above remains
unchanged by this audit.
