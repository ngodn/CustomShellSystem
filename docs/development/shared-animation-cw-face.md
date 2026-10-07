# Commander White facial library checkpoint

October 7, 2026. Private candidate, not installed or accepted in game.

The combined shared trial is `work/cine-animation-port/cw-face-trial-v1`, a
symlink to secondary-drive scratch storage. It retains all 62 body/movement
identities, the four native locomotion alternatives and five outfit animation
adapters. The 95 previous exports remain unchanged. One additional facial mesh
brings the package to 96 exports and two facial rigs.

Commander White adds eight expressions (Afraid, Angry, Flirting, Frown, Shock,
Smile, Open Smile and Surprised) plus Neutral. These use her source controls,
not Eve's facial deltas. The 74 new morphs comprise 44 control samples and 30
compressed corrections. All 16 original body/shoulder morphs remain intact.
Static expressions use inverse storage gains so Unreal's existing morph filter
does not erase their small corrections. Her performance curves remain pending.

## Cooked evidence

`cw-expression-cook-v3` has successful Windows cook, container conversion and
independent CUE4Parse readback receipts. The source comparison checks the
released Commander White package, not only another copy of the new candidate.
It verifies:

- All 2,312,092 rendered vertices, normals, UVs, skin weights and topology.
- Original skeleton, physics, post-process graph, material slots and sockets.
- Render sections, active/required bones and cloth index mappings.
- All 16 original cooked morphs and all 74 added facial morphs.
- 25,405 mapped facial render vertices against their sparse source positions,
  with zero neutral-position difference.
- All eight expressions plus Neutral. The largest decoded deformation
  difference is 0.0030196444 cm (Surprised), within its measured shape bound.

The extended decoder also passes the existing six Eve faces and their source
comparisons (`cw-expression-map-v2/eve-regression-readback.json`, exit 0).
The Commander White rig passes 765 host checks. Combined and pack-only catalog
checks pass 1,127 and 1,115 assertions, respectively. The three focused host
facial/library suites pass. This proves offline data and catalog behavior,
not live facial appearance, gameplay restoration or runtime performance.

## Material repair

The first cooked comparison correctly rejected E3: 37 hair-material references
were empty. The isolated authoring process had mounted CommanderWhite and
CWTest but omitted the original CWMaterial/CXMaterial folders. An editor
comparison against that same incompletely loaded source could not detect the
loss. The released cooked package exposed it.

E4 loads those original folders read-only, verifies all 70 released material
identities and restores them on a new derived mesh. Both fresh creation and
reload pass in `cw-expression-shared-v3`. E4 then passes the complete cooked
comparison against the released package. Original materials and source assets
are unchanged. Neither E3 nor E4 has been installed in the game.

## Remaining work

The full goal remains active. Commander White's facial performance mappings,
seven unknown source-bone mappings,
feasible Unholy Genessa facial coverage, final live checks across the three
requested outfits, native locomotion acceptance and release delivery remain.
No old Seductress, GeneXXa or native dance expansion is authorized.

Build tooling remains .NET 10.0.401, UE 5.6.1 C++/Python and host Python 3.14.
The shared candidate can be reproduced with `package_cw_facial_trial.py` after
the hashed cook/readback inputs pass. Runtime CSS and CINE continue to consume
the same facial catalog. No runtime DLL changed in this checkpoint.

## Additional performance controls sampled

`work/cine-animation-port/cw-performance-controls-v1` samples 65 additional
controls from the immutable source rig, covering brow, jaw, eyes, tongue and
phoneme controls. Four levels per control produce 260 deformed states; neutral
and restoration checks bring the total to 326 recorded states. All controls
restore the sampled geometry exactly, and the source Blender hash is unchanged.

57 controls affect the sampled geometry. Eight `facs_ctrl_EyeLook*` directional
controls have no measured effect on these parts. Other joint/combined eye
controls do deform them, so directional names alone are not reliable mappings.
The four tongue controls produce only about 0.0011 cm of visible displacement
in the sampled body, eyelashes and tear meshes and need closer source review.

This is calibration only. It does not add expressions or prove cross-character
performance equivalence. Mapping must also account for negative and overshoot
source values; these samples only cover zero through one. The existing cooked
eight expressions plus Neutral remain the accepted offline CW baseline.

The newer Eve face work is recorded in `shared-animation-author-head.md`:
the optional head recovered the missing controls, and the Cinderella combined
candidate now contains six prepared Eve facial performances. Their live
acceptance remains separate from this Commander White work.

The front/quarter source review contains 32 images. Compared with the original
Jali3 shape previews, its `smile` also closes the eyes; CW's `MouthSmile` does
not. A direct name-based alias would lose that part of the expression. The four
sampled phonemes show distinct lip shapes, but their combined behavior and
cross-character strength still need measured mapping.

An isolated-control compression study covers all 228 nonzero sample states and
5,918 affected points. Keeping residual below 0.001 cm requires 83 basis shapes
(measured maximum 0.000464 cm). Simulating the existing morph filter with bounded
storage gains gives 0.002025 cm maximum residual. The initial 54-shape budget
fails. Adding all 83 to the existing 74 would exceed the current 128-shape facial
catalog limit, so this is not a shippable representation. Retain the limit and
investigate a joint representation or only the controls required by verified
performance mappings. Evidence: `basis.json`, `basis.npz` and the rejected
54-shape receipt. No runtime, cooked mesh or original source changed.

## Tongue controls recovered from the source rig

The four nearly inactive tongue-joint controls were being overridden by the
source Rigify tongue IK mode. This was not missing tongue geometry. In the
same in-memory source scene, `MhaTongueIk=1` limits their measured displacement
to about 0.001 cm. With `MhaTongueIk=0`, the left/right, tip-bend and up controls
move the tongue by 2.4954, 2.5080, 1.4567 and 2.6752 cm, respectively. The
TongueOut shape remains active in both modes, at 2.3634 cm. The audit records
the actual copy-transform and stretch constraints that implement the source
chain. Blender's [IK documentation](https://docs.blender.org/manual/en/4.4/animation/constraints/tracking/ik_solver.html)
explains the general constraint behavior; these values come from the local rig.

`sample_cw_facial_controls.py --tongue-fk` disables only that source control in
memory for sampling. It captures both neutral modes, restores the original
mode and geometry, and verifies the source hash. The default sampling path
retains the saved mode. Nine controls at four weights produce 36 deformed
states and 48 recorded states including neutral/restoration checks. All pass.
The IK/FK neutral position shift is 0.0002043 cm on the body and zero on the
eyelashes and tear mesh; preserve that measured difference when preparing a
fitted transfer instead of treating both neutral arrays as identical.

Evidence: `cw-tongue-control-audit-v1` and `cw-tongue-fk-samples-v1`.
Ten additional jaw-open combinations and their restoration checks pass in
`cw-tongue-fk-combinations-v1`; its 20 front/quarter diagnostic renders are
complete. The inspected left-tongue and tongue-out views show the expected
different mouth geometry. This resolves source-control availability, not
cross-character performance mapping or cooked/live acceptance. Original
sources and the installed mod are unchanged.

## Required-control representation fits the existing limit

A joint study of all 65 exploratory controls plus expressions still exceeds
128 shapes at the existing 0.001 cm fitting target. Its failed result remains
in `cw-unified-basis-v1`. The pack does not need unused phonemes or controls
that measured no motion, so the next study selects 27 candidate controls for
the supplied performances, together with the recovered nine tongue controls
and existing expression/blink samples. This is a provisional source-control
selection, not an accepted cross-character curve map.

`cw-required-basis-v1` fits 700 source states at 5,918 affected points using
115 basis shapes. Maximum fitting residual is 0.000452 cm. Simulation of the
existing morph filter, using storage gains bounded at 32, gives 0.003636 cm.
Projection of 204 separate expression/blink samples gives 0.002445 cm before
filtering and 0.003333 cm after it; unrepresented points have zero error.
All 914 input files still match their recorded hashes. The first launcher had
an indentation error; its failure receipt is retained separately.

These results establish that the sampled geometry can fit the current
128-shape catalog limit. They do not validate runtime coefficients: the
holdout coefficients are direct geometric projections, not interpolation of
source controls. Curve mapping, negative/overshoot values, combined tongue
controls, fitted transfer, bounded catalog size, cooking and live behavior
remain to be implemented and checked. The accepted cooked CW face still has
eight expressions plus Neutral and no facial performances.

## First mapped performance: ATOOL Gesture 200

`cw-jaw-trial-v1` adds Commander White's first mapped facial performance using
the four already cooked jaw shapes. Gesture 200 contains only `jaw_open`, so
this does not omit any source curve. Its original 0.36-second timing and body
clip remain unchanged. The other five facial performances remain unfinished
for Commander White.

`compile_facial_control.py` converts the measured cubic control interpolation
to the runtime's linear curves. It splits at spline boundaries, bounds the
interpolation error from the second derivative, and reduces keys using the
actual morph displacement. Float32 conversion is included in the bound.
Three tests cover a known cubic trajectory with reversals, constant motion
and invalid inputs. Existing limits are unchanged.

The 275 source keys become 65 keys for each of four morphs. Conversion from
the cubic control model is bounded by 0.000498 cm. A separate Blender run
evaluates 32 new source jaw poses. After the existing Unreal morph filter,
the mapped geometry differs by at most 0.007691 cm, below the unchanged
0.015 cm source-comparison limit. This is within-character geometry evidence;
Commander White retains her own jaw shape and proportions.

The private package changes only the catalog. All 98 cooked exports and every
other catalog entry remain unchanged. Its manifest is 244,391 bytes, below
the existing 256 KiB limit. Combined and pack-only loader checks pass 1,398
and 1,386 assertions. Native facial checks pass 220 assertions, including all
32 reference samples, entry blending, clearing the prior smile and restoring
it afterward. The previous C11 pack still passes 1,133 checks, and the ordinary
facial suite passes 56.

Evidence: `cw-gesture200-curves-v2`, `cw-gesture200-source-v1`, and
`cw-jaw-trial-v1/host-validation.json`. Reproduction uses
`prepare_cw_jaw_performance.py` and `package_cw_jaw_trial.py`. The first package
preflight used a string where a Path was required and failed before creating
output; the corrected package passes metadata readback and source hashes.
No runtime DLL, original source or installed outfit changed. Live facial
appearance and gameplay acceptance remain pending.

## Expanded performance controls, preserving the current expressions

The joint 115-shape study above is no longer the preferred representation.
`cw-retained-basis-v1` keeps the 12 shapes actually used by the current catalog
and eight blink samples. It adds 59 residual shapes for the 27 candidate face
controls and nine recovered tongue controls. This makes 79 catalog controls,
below the unchanged limit of 128. The original expression IDs, their weights
and the existing jaw performance do not need to change. Unused calibration
shapes remain in the mesh; this does not delete source geometry or morphs.

`prepare_cw_performance_shapes.py` checks all 5,918 affected source points
against the fitted assembly, including the recorded 547-point skirt insertion.
Maximum correspondence error is 0.000019444 cm. All 144 isolated source poses
reconstruct within 0.002996 cm after stored-float conversion and the existing
0.015 cm morph filter. Original files still match their hashes.

`cw-performance-holdout-v1` and `cw-tongue-holdout-v1` independently evaluate
four intermediate strengths for all 36 controls. The interpolation check
uses the actual retained-plus-residual weights, rather than fitting new
weights to each holdout. All 144 cases pass the unchanged 0.015 cm comparison
limit; maximum error is 0.009657 cm. Evidence:
`cw-performance-fitted-v1/holdout-v2.json`. This does not validate semantic
equivalence to another character or simultaneous control combinations.

E5 appends the 59 controls to the existing E4 face. Native creation and fresh
reload both pass in `cw-performance-native-v1`: all 90 existing morphs,
799,989,630 bytes of non-added mesh-description data, 69 built sections,
cloth and all 70 material bindings are retained. There are now 149 total
mesh morphs. The 128-control catalog limit concerns exposed facial controls,
not the number of stored mesh morphs.

E6 stages the same result under `/Game/CSS/Locomotion/Faces/`. Creation and
fresh readback pass in `cw-performance-shared-v1`. Set
`CSS_CW_FACE_STAGE_REVISION=E6` with `stage_cw_facial_library.py`; its default
E4 path is unchanged. No runtime DLL or installed game file changed. The
five remaining CW performance mappings, combined/negative control behavior,
catalog size and live acceptance remain unfinished.

The Windows cook and independent container readback now also pass in
`cw-performance-cook-v1`. The decoder compares E6 against the already verified
E4 packaged face, preserving all 90 old cooked morphs, 2,312,092 render vertices,
materials, skeleton/socket bindings and cloth data. All 59 added shapes pass
the existing morph-filter and GPU precision bounds. The 36 full-strength
residual-control combinations plus Neutral differ from their authored deltas
by at most 0.002999 cm. These combinations check the added residuals only;
the complete source-control interpolation evidence is the separate 144-pose
holdout above. They are not yet mapped dance performances.

The first cook preflight found sandbox-only material paths and stopped before
cooking. Resolving those paths through the recorded read-only mounts restored
host hash verification. Both receipts are retained. The final cook, package
and readback jobs all exited successfully. Identical staging copies were
replaced with verified hard links, reclaiming approximately 191 MiB without
changing any package bytes; see `duplicate-stage-cleanup.json`.

## Spin performance: signed gaze and combined controls

The source head uses separate left, right, up and down eye shapes. Measured
rotations of its moving eye surface reach 40 degrees horizontally, about
18.382 degrees upward and 26.644 degrees downward. CW's original controls
rotate about 30.023 and 19.996 degrees per unit. Her imported rig also has
asymmetric eye constraints and intermediate driven properties limited to
[-1, 1]. Increasing the visible control alone therefore does not produce the
requested gaze. See `cw-eye-constraint-audit-v1` and `cw-signed-eyes-v1`.

`sample_cw_facial_controls.py --eye-gaze-range` changes only those two
horizontal eye constraints and eight intermediate eye-property ranges in
memory, for the derived facial bake. It retains the eyelid constraints,
restores the original settings and neutral geometry, and verifies the source
file hash. No original Blender file is saved. The first range trial exposed
the intermediate property clamp and remains rejected.

`cw-gaze-range-v2` verifies 32 signed/extended isolated poses and 16 combined
poses. The measured order is horizontal then vertical for both eyes.
`cw_eye_mapping.py` uses the rotation of the actual blended source eye shape,
then solves CW's controls for that gaze. It does not assume that a fraction
of a morph is the same fraction of its full rotation angle. The imported
Bezier identity drivers round tiny values near zero; the sampler records
their deviation and rejects differences above 0.0001 control units. A later
independent vertex check validates the actual resulting gaze as well.

`prepare_cw_gaze_performance.py` maps all eleven curves of the complete
3.6666667-second Spin performance. Blink uses CW's own bilateral blink;
`woo` and `ooo` use her native UW and OW phonemes. They retain CW's lip
proportions. The source head and five sampled CW frontal views were inspected;
these neutral-material views are not game-material or temporal acceptance.

The first 327-pose fit passed key poses but failed three intermediate checks,
peaking at 0.063030 cm during a rapid eye turn. Adaptive sampling produced
401 poses and 400 separately evaluated intermediate poses. The accepted
source batches are `cw-spin-source-v3` and `cw-spin-holdout-v3`; both exited
successfully with source restoration verified. Across both batches, 1,602
eye checks give maximum rigid-fit error 0.000023325 cm and gaze-direction
vector error 0.000020383. Evidence: `gaze-verification.json` in the source
batch. The original sources remain unchanged.

`cw-spin-basis-v3` retains the 20 existing expression/jaw/blink controls and
adds 56 sampled correction shapes, for 76 exposed controls. These are morph
shapes, not 56 extra expressions. All 5,911 affected points belong to the
previously verified fitted transfer. Stored float32 deltas and the unchanged
morph filter give maximum key-pose error 0.002101 cm and intermediate error
0.005586 cm, below the existing 0.015 cm limit. The existing expressions and
Gesture 200 do not need new weights. This representation can be appended to
E4; it does not require exposing E6's 59 exploratory controls in the catalog.

`compile_sampled_face.py` converts the sampled coefficients to runtime float
precision and reduces them using the actual combined geometric displacement.
It checks both original and rounded knot sets, including timestamp collisions.
`cw-spin-curves-v2` has 76 curves and 205 keys per curve. Maximum conversion
error is 0.000994 cm; the source holdout plus conversion bound is below
0.006579 cm. Four gaze-math tests and three sampled-curve tests pass, including
reflection/degeneracy rejection, signed rotations, cancelling controls,
reversals and motion that cannot survive float timestamp conversion.
The default sampler still matches four prior neutral/blink/restored snapshots
exactly (`cw-sampler-default-regression-v1`). The existing facial-basis and
interpolation test group passes all 15 tests; the curve-compiler group passes
six tests. No runtime code changed in this checkpoint.

The 657,779-byte performance exceeds the existing 256 KiB manifest limit.
The [facial resource loader](facial-package-resources.md) now accepts this
full performance as a parser fixture while preserving the manifest limit.
Inline-curve compatibility and shared CSS/CINE catalog resolution remain
intact. The manifest limit is unchanged. Cooking these new shapes,
catalog integration and live acceptance are still pending. Gesture 200 is
still the only packaged CW facial performance; Spin is the second prepared
mapping. Come Closer, DumbDumb, Iris Out and Cinderella remain unmapped for CW.

Removed 512 unused samples from the two failed partial batches, reclaiming
144,078,677 bytes. Kept their logs, plans, neutral poses and endpoint evidence.
The successful batches and all original assets remain intact. Receipt:
`cw-spin-rejected-sample-cleanup.json`. No game installation occurred.

## DumbDumb: full source mapping and bounded playback

`prepare_cw_dumbdumb_performance.py` adapts all twelve channels of the complete
65.7-second clip. Jaw, frown, smile and IY/UW/EE/OW use CW's native actions.
The author's smile also closes the eyelids. Its measured displacement against
the author's blink drives the corresponding CW eyelids, as do the separate
half-lid actions. The weaker brow action uses a measured 0.425126 scale of
the stronger brow action. These are character-specific action adaptations,
not claims that different heads have identical geometry. Original meshes,
archives and animation keys remain unchanged.

The plan records the selected source vertices, fitted gains and residuals.
It retains all 1,256 source key times. Eight distinct diagnostic poses were
visually inspected, including smile, blink, one-sided lids, frown and phonemes.
Evidence is in `cw-dumbdumb-preflight-v1/visual-review.json`; these views do
not replace full-motion or game-material acceptance.

`sample_cw_performance_batches.py` evaluates at most 512 poses per Blender
process, checking source restoration and neutral geometry across every batch.
The optional omission of repeated bone-matrix records leaves evaluated mesh
geometry unchanged: seventeen preflight poses match the batched output
array-exactly (`cw-dumbdumb-sampling-regression.json`). Refinement reuses only
exact time/control matches with identical source, sampler and bake settings.
The second source batch reused 4,417 poses and evaluated 644 new ones. Its
intermediate batch reused 3,860 and evaluated 1,200 new ones. Hard links avoid
duplicating those successful source samples.

The first fit failed intermediate geometry checks at 0.044941 cm. Its refined
replacement, `cw-dumbdumb-basis-v2`, passes 5,061 source poses and 5,060 separately
evaluated intermediate poses. It preserves the twenty original controls and
all fifty-six prepared Spin shapes, adding forty-nine correction shapes.
The resulting 125 controls have maximum fitting errors of 0.001627 cm at
source samples and 0.009588 cm at intermediates. The existing 0.015 cm limit
is unchanged. The optional greedy basis method checks every point after
each projection and independently verifies reconstruction; existing SVD
behavior remains the default.

The first shared-key curve export exceeded the native 65,536-key limit.
It is rejected for playback despite passing its geometry-only check.
`compile_sampled_face.py` now rejects that case before writing an accepted
output. The separate `compile_sparse_sampled_face.py` reduces each control
independently, checks combined geometric error, then compares the final
float-precision playback against every original sampled and intermediate
pose. Per-control tolerances are not treated as a combined error guarantee.
Final acceptance still uses the same 0.015 cm source-geometry limit. Neither
the native key bounds nor the facial-resource size bounds were increased.

`cw-dumbdumb-curves-v2` contains 122 active curves, 58,555 keys and 2,410,645
bytes. Maximum final errors against evaluated source geometry are 0.007841 cm
at sampled poses and 0.010172 cm at intermediates. It remains under both
runtime limits and the existing geometry limit. Its native fixture samples
292 times, covering regular intervals, per-curve extrema, the worst geometric
checks and endpoint clamping. All 35,978 native checks pass; the existing
jaw fixture passes 220 and facial compatibility passes 86. The eighteen CW
basis/mapping tests and seven shared/sparse curve tests pass. Recompiling Spin
produces byte-identical performance data. Evidence includes `report.json`,
`native-validation.json` and `implementation-validation.json` in the accepted
curve folder, plus `cw-spin-curves-regression-v1/comparison.json`.

DumbDumb is prepared, not cooked, integrated or live-accepted. Gesture 200
remains the only packaged CW facial performance. Come Closer, Iris Out and
Cinderella mappings remain unfinished. A joint-basis probe can represent the
prepared Spin/DumbDumb geometry with 83 correction shapes, but that probe has
not passed fitted transfer or independent source checks and is not accepted.
Complete the remaining mappings before choosing the final combined shape
representation and cooking it. Full source-motion and in-game comparison
remain required for all prepared tracks.

Removed only the rejected first-fit payload/basis and oversized first curve
export, reclaiming 44,736,680 bytes. Their failure reports, plans and source
samples remain. Receipt: `cw-dumbdumb-rejected-output-cleanup.json`.
No new assets or runtime were installed.

## Cinderella mapping and signed mouth preparation, October 7

`prepare_cw_cinderella_performance.py` retains all twelve source channels and
all 2,580 source key times over 139.66667 seconds. The author's `surprise`
shape is exactly the sum of bilateral upper-lid opening, verified at every
source vertex. It maps to CW's widened lids. `upLid_down_L` is a full left
wink. The other actions use native jaw, brow and phoneme controls plus the
previously measured smile/half-lid closure. Seven extreme combined poses
were inspected; fourteen preflight snapshots match the full sampling exactly.

The initial fit failed fifteen intermediate poses. Refinement reused 14,977
completed poses through hard links and evaluated 7,796 additional poses.
`cw-cinderella-samples-v3` and `cw-cinderella-holdouts-v3` complete successfully
with source restoration checks. `cw-cinderella-basis-v2` retains the twenty
original controls and adds 51 shapes, fitting the full performance separately
from Spin/DumbDumb. Its sampled/intermediate errors are 0.003859/0.011810 cm.
This separate basis is preparation for a combined representation, not permission
to concatenate all prepared shapes beyond the runtime control limit.

Independent scalar reduction alone either exceeded 65,536 keys or exceeded the
conversion error budget. The optional geometry refinement restores the curve
contributing most to each offending vertex. The first bounded export then
failed two actual source intermediates. Restoring original keys on both sides
of those intervals passes the full source comparison. Every refinement retains
the existing key, resource and geometry limits; default compilation is unchanged.

Accepted output: `cw-cinderella-curves-v3`, 51 active curves, 61,727 keys,
2,547,961 bytes. Maximum error against 11,387 actual sampled poses is
0.009973763 cm; against 11,386 separately evaluated intermediates it is
0.014629129 cm, below 0.015 cm. All 20,515 native checks pass at 393 times,
including extrema, worst source comparisons, clamping and expression restoration.
The old jaw fixture passes 220 checks and facial compatibility passes 86.
Eighteen CW tests and ten curve tests pass. The existing DumbDumb curve file
recompiles byte-identically (`cw-sparse-refinement-regression.json`).

Iris Out remains unfinished. Its negative cheek and tongue-width actions were
clamped by intermediate property and shape ranges. The optional sampler flag
`--signed-mouth-range` adjusts only those audited ranges in memory, tags their
data blocks for evaluation, and restores them afterward. Existing drivers were
already linear; no driver extrapolation change was needed. The default sampler
matches all 37 prior snapshots exactly, and the enabled implementation matches
all 37 signed prototype snapshots. Reports are in
`cw-irisout-controls-default-regression` and `cw-irisout-controls-signed-v3`.
The first implementation check rejected two tear-mesh ranges that were already
signed; the successful replacement preserves those original ranges as well.

`cw-irisout-preflight-v1` evaluates eleven combined source poses. Four front
views were inspected. The tongue at 1.5 seconds curls too high compared with
the source, so this mapping is not accepted. Refine its arch/tip extension
before full sampling; do not count the prototype as a completed performance.
`cw-irisout-mapping-v1/negative-control-findings.json` and the preflight visual
review preserve the evidence. The original CW blend and supplied archives
remain unchanged.

Cinderella is prepared, not cooked or installed. CW now has four prepared
mappings (Gesture 200, Spin, DumbDumb, Cinderella), with only Gesture 200
packaged. Come Closer and Iris Out remain. Keep the original scope for Eve,
Unholy Genessa, Commander White and the four native human locomotion choices.
Full-motion, combined asset, live and release acceptance remain open.

Removed 21,566,423 bytes of rejected first-fit Cinderella payloads, retaining
its failure report, plans and all original samples. Receipt:
`cw-cinderella-rejected-output-cleanup.json`. No other session's work was changed.

## Iris Out: fifth prepared CW facial performance

The reviewed replacement reduces the excessive tongue curl at 1.5 seconds,
keeps the tongue extended, and reduces lower-lip movement. The six-pose trial
and selected `mix_tongue_03` are recorded in `cw-irisout-tongue-v2`, including
the source and review hashes. This is a cross-character action adaptation,
not a claim that differently shaped faces have identical vertices.

`prepare_cw_irisout_performance.py` maps all 23 source channels. The full
`cw-irisout-source-v1` plan retains original key times and the 60 Hz grid.
`cw-irisout-samples-v1` evaluates 1,419 poses; `cw-irisout-holdouts-v1` evaluates
1,418 separate intermediate poses. Original rig settings and source files
are restored and checked in every batch.

The first fit correctly rejected a 0.000204233 cm neutral mismatch caused by
temporarily disabling tongue IK. The fix uses the verified original-mode
neutral for both fitted correspondence and sampled deltas. It includes that
small mode shift in the deltas instead of loosening the 0.0002 cm check.
The accepted `cw-irisout-basis-v2` has zero fitted correspondence error,
20 preserved controls and 39 new shapes. Its training and intermediate
errors are 0.003641258 and 0.009148127 cm.

`cw-irisout-curves-v1` has 39 active curves, 5,685 keys and 236,541 bytes.
Actual float-precision playback passes every evaluated pose, with maximum
sample/intermediate errors of 0.003641258/0.008819266 cm under the unchanged
0.015 cm limit. Native playback and restoration pass 15,359 checks at 382
times. The existing jaw and facial compatibility checks pass 220 and 86.
This is prepared data, not a cooked or installed performance.

Fully reused source batches now retain a hashed original-mode neutral.
The all-reuse regression reuses 1,419 poses with zero Blender batches and
produces array-identical basis data and byte-identical fitted output.
`cw-irisout-reuse-regression/verification.json` records that comparison.
Removed only the duplicate regression basis and payload, 17,759,394 bytes,
with receipt `cw-irisout-duplicate-cleanup.json`. Accepted output, source
samples and failure reports remain.

## Come Closer: reviewed source controls

Come Closer needs independent tongue-tip rotation and extension, plus signed
mouth actions. `cw_performance_controls.py` adds three temporary controls to
the audited source tongue drivers and permits the measured signed/overshoot
range in memory. The default sampler does not enable them. Existing driver
targets, type, identity keys and expression are checked before changes;
driver state, property ranges, shape ranges and neutral geometry are restored
after sampling. The original blend is never saved.

The driver type matters: the tip rotation uses `SUM`, while tongue scale uses
a scripted expression. Blender documents these separately in its
[Driver API](https://docs.blender.org/api/5.3/bpy.types.Driver.html).
The original variables target armature data, not the similarly named rig
object. An initial strict-target check rejected the wrong ID assumption;
`cw-comecloser-controls-v5` is the successful implementation.

The first reviewed wink was too open. The updated mapping retains native
squint creases and adds the measured missing upper-lid closure. Calibration
is in `cw-comecloser-controls-v3/eyelid-calibration.json`. Front views at
16.7 and 18.2 seconds now show the intended squint/tongue action and left
wink. Full temporal comparison remains separate.

All 54 implementation snapshots match the reviewed prototype exactly, and
its original-mode neutral matches the untouched source. The default sampler
also matches four earlier snapshots; the signed Iris sampler matches all
37 earlier snapshots. Evidence is in `cw-comecloser-controls-v5/regression.json`,
`cw-sampler-default-regression-v2` and `cw-sampler-signed-regression-v4`.
The full Come Closer mapping preserves all fifteen source channels, including
negative values and small overshoots; it does not clamp them to zero or one.

`cw-comecloser-source-v1` retains the full 19.333334-second track and all
original source key times. Its 1,316 poses and 1,315 independently evaluated
intermediates pass in `cw-comecloser-basis-v1`: 20 preserved controls plus
38 new shapes, with maximum training/intermediate errors of
0.002672234/0.006280341 cm.

The accepted `cw-comecloser-curves-v1` has 38 active curves, 3,072 keys and
130,960 bytes. Final float-precision source comparisons peak at
0.002668794/0.006224979 cm, below the unchanged 0.015 cm limit. Native parsing,
sampling and restoration pass 14,860 checks at 379 times. Existing jaw and
compatibility fixtures pass 220 and 86. Eighteen CW tests and ten curve
tests pass after these changes.

All six CW facial mappings are prepared: Gesture 200, Spin, DumbDumb,
Cinderella, Iris Out and Come Closer. Only Gesture 200 is currently packaged.
The latest other five mappings use separate shape bases. Do not concatenate
them beyond the runtime limits. The final combined representation must also
preserve the eight expression presets and Neutral, then pass source, fitted,
cooked and native checks. Full temporal review, in-game compatibility and
release are unfinished. Nothing new was installed.

The joint sizing probe samples 1,177 representative prepared poses from the
five new performances. Both direct and retained-control fits exceed 108 new
shapes at a 0.001 cm internal compression target. The third probe, projecting
onto the retained controls first, uses 105 new shapes at 0.001977013 cm.
That would expose 125 controls including the twenty retained shapes.
Evidence: `probe-cw-all-performances-v3.{py,json,npz,log}`; failed v1/v2 logs
remain. This only measures a possible representation against previously fitted
poses. It has not passed all original source poses, intermediate poses, final
curve/key limits, cooking or live playback. The final 0.015 cm source limit
is unchanged. Do not package the probe as accepted geometry.

## Combined face cooked and packaged, October 7

`cw-combined-basis-v2` fits all 19,584 evaluated source poses and checks all
19,579 independent intermediate poses. It keeps the original expression/jaw
controls used by the catalog and anchors Cinderella's 51 accepted shapes and
coefficients. The other four performances add 65 shared shapes. This gives
12 original controls plus 116 added shapes, exactly 128 exposed controls.

All 90 original mesh morphs remain in the derived face. The eight previously
retained blink calibration shapes remain on the mesh but are not exposed as
unused catalog controls. Blink motion in the performances is included in the
source fit. The existing eight expression IDs, Neutral and Gesture 200 remain
unchanged. No original asset or released outfit package was modified.

The first combined fit passed geometry at 126 controls, but its Cinderella
curves could not fit the unchanged 65,536-key limit. The accepted fit preserves
Cinderella's independent basis and produces byte-identical performance data.
The first DumbDumb curve reduction failed the conversion bound after source
interval restoration. A tighter 0.002 cm per-curve reduction passes without
changing the final 0.015 cm source-geometry limit or any runtime bound.

| Track | Active curves | Keys | Bytes | Source max cm | Intermediate max cm |
| --- | ---: | ---: | ---: | ---: | ---: |
| Spin | 128 | 2,649 | 112,199 | 0.009319 | 0.010059 |
| DumbDumb | 128 | 38,666 | 1,603,173 | 0.009769 | 0.011798 |
| Cinderella | 51 | 61,727 | 2,547,961 | 0.009974 | 0.014630 |
| Iris Out | 128 | 5,971 | 251,794 | 0.009214 | 0.013246 |
| Come Closer | 128 | 3,276 | 141,505 | 0.009095 | 0.010959 |

Curve directories are `cw-combined-curves-v2/{spin,cinderella,irisout,comecloser}`
and `cw-combined-curves-v3/dumbdumb`. Their native playback/restoration checks
pass 50,498, 20,572, 51,530, 51,401 and 53,852 assertions respectively.
Each run also passes the old 220-check jaw and 86-check compatibility fixtures.
The six CW tracks total 112,549 keys, including Gesture 200. Eighteen basis
and ten curve tests pass. `cw-combined-validation-v1` records these checks.

`cw-combined-native-v2` creates isolated AnimLab face E7 from verified E4.
It preserves all 90 prior morphs, 70 material slots, 69 render sections,
the cloth asset, skeleton, bind pose, physics and post-process bindings.
Fresh-process verification passes. `cw-combined-shared-v1` stages and verifies
E8 under `/Game/CSS/Locomotion/Faces/SK_Face_CWCompact/`.

`cw-combined-cook-v1` cooks Windows data and independently decodes it with
FacialReadback. All 116 new morphs pass the stored-delta comparison; all 90
previous morphs, topology, weights, materials, sockets and cloth remain equal
to E4. The decoder checks 2,312,092 render vertices, including 29,440 mapped
facial vertices. Source-position error is at most 0.000015693 cm. Editor and
cooked proofs are separate from visual or live acceptance.

Two authoring attempts were rejected before acceptance. The initial native
creation used the shared namespace, which the append tool correctly refuses;
the successful run uses AnimLab then stages it. The first probe-package
attempt used editor-only material mount paths. All 70 protected inputs were
resolved through the recorded read-only bind mappings and hash-checked before
host packaging. Original failure logs and receipts remain.

`cw-combined-trial-v1` points to secondary-drive storage. It replaces E4 with
E8 in the verified jaw/C11 candidate and adds the five new CW performances.
It preserves all 63 animation entries, five adapters, four native locomotion
alternatives and six Eve facial performances. Of 98 cooked exports, 97 remain
unchanged. External facial resources keep the manifest at 38,968 bytes.
The resource round-trip reproduces the complete inline authoring catalog.

The actual CSS loader passes 114,262 checks with the three source outfits and
114,250 with the shared pack alone. The test compares every packaged CW key
against the verified curve files and retains the existing outfit/native/Eve
checks. Old inline and earlier external-resource catalogs still pass. The
first test attempt hit the generic 1 MiB test-fixture reader; the dedicated
performance-fixture reader uses the same 16 MiB bound as facial resources.
No production parser or resource limit changed.

Removed only the superseded first combined basis and its hard-linked fitted
payload: 81,650,382 unique bytes. Original inputs, successful individual fits,
accepted combined data and failure reports remain. Receipt:
`cw-combined-rejected-cleanup-v1.json`.

This completes combined CW fitting, cooking and private pack integration.
Nothing was installed. Full source/temporal review, unresolved source indices
3269..3275, all requested outfit/native live checks, CSS/CINE cancellation and
restoration, performance acceptance and release remain unfinished.
