# Author head controls recovered, October 7

The user supplied the optional Jail3 head and Cinderella animation archives.
The head contains all nine previously missing controls used by DumbDumb,
Iris Out and Jang Won Young Spin. This resolves their missing facial source
shapes. It does not identify the seven unresolved animation bone indices or
complete runtime acceptance.

## Source evidence

The author describes the optional custom MMD shape-key head in the
[DumbDumb posts](https://www.nexusmods.com/stellarblade/mods/2545?tab=posts).
It is the optional `Jail3_Head` download on the
[Cinderella files page](https://www.nexusmods.com/stellarblade/mods/2534?tab=files).
The archive spelling is Jail3; the contained mesh is `CH_P_EVE_Face_Jali3`.

| Input | SHA-256 |
| --- | --- |
| Jail3_Head-2534-v1-0-0-0-1764506935.7z | `3599f92ce5581cdfa9bfa1ef14eea977b95c89affc0c5f461f3543c43ca6e28c` |
| D_Cinderella_Main-2534-v1-1-0-1-1772027045.7z | `a5df5243ce63a775d0312eb50c8d76121521af98721ed4a57257f287457a0d9a` |

`work/cine-animation-port/author-head-source-v1/inventory.json` records the
safe member lists, separate extraction and extracted hashes. Both archives
remain unchanged. Their containers were mounted separately with the retail
containers and global dependency, so identical ATOOL paths cannot overwrite
one another during decoding.

The head has 24,296 render vertices, 190 morphs and 159 mesh bones. Its package
contains a mesh and physics asset, not the missing authoring Skeleton. Of its
morph names, 169 also exist in the original Eve assembly. The recovered names
are `smile`, `blink`, `lid_mid_closed_L`, `lid_mid_closed_R`, `iii`, `woo`, `eee`,
`ooo` and `brow_down_miner`.

| Performance | Duration (seconds) | Facial curves | Keys | Missing names on new head |
| --- | ---: | ---: | ---: | ---: |
| DumbDumb | 65.7 | 12 | 2,616 | 0 |
| Iris Out | 16.666666 | 23 | 1,892 | 0 |
| Jang Won Young Spin | 3.6666667 | 11 | 426 | 0 |
| Cinderella | 139.66667 | 12 | 3,897 | 0 |

Cinderella is an additional supplied animation at `/Game/Mods/HD-ATOOL_P/anim/972`,
with 8,381 samples. Indices 3269-3272 still have unidentified bindings. It has
been inventoried, decoded and initially retargeted as described below. It is
not accepted or packaged. Keep it in the unfinished count. The earlier library
counts exclude it.

## Transfer candidate

`MeshExport` now has a separate `CSS_MORPH_AUDIT=1` mode. It preserves original
render indices and all position/tangent deltas, avoiding the conversion
library's position-only glTF morph lookup at coincident vertices. A repeated
head decode is identical across the manifest and three binary payloads:
190 morphs, 580,434 delta records. Invalid mode values, conflicting modes and
an existing output are rejected. The .NET 10 exporter builds without warnings.

`capture_eve_face_correspondence.py` reads the immutable Eve assembly with
embedded scripts disabled. Its output preserves the raw facial basis,
per-corner UVs, triangle materials and the accepted fitted vertex order.
A fresh run matches every array in the earlier diagnostic capture.

`transfer_author_head_shapes.py` measures alignment from the matching eyes,
then uses position and UV correspondence. Scale is 0.9796633602; it adds an
axis-aligned offset after swapping the source horizontal axes. It directly
matches 23,824 facial vertices within 0.002 cm. At 174 welded vertices with
differing source deltas, incident triangles select the source corners and
corner angles weight their contribution. Two teeth vertices project onto
the teeth surface at 0.0176551 and 0.0006631 cm. The 44 unmatched neck-seam
vertices have no original facial deformation and remain stationary.

The candidate in `facial-author-head-export-v1` retains all 105 existing facial
shapes, all 21 expressions, existing performance entries and every fitted body
point unchanged. It adds the nine recovered controls plus Cinderella's
`surprise`, making 115 facial shapes. No nonfacial vertex receives a new delta.
The original 22 body/hand morphs are separate and remain intact, for 137 total
morphs per derived Eve mesh.

Four geometry tests cover distinct coincident surfaces, equal and unequal
welded corner angles, and rejection of an isolated corner. The neutral fitted
comparison is exactly zero. Twenty neutral/individual-shape renders and
26 selected performance renders compare source and candidate side by side.
The inspected front views show matching eyelid and mouth shapes without gross
collapse. This is sparse offline review, not full-motion or live acceptance.

Existing same-named source morphs differ locally around some mouth and eye
seams. The candidate deliberately retains the original 105 definitions;
these are not claimed to be numerically identical to the custom head. Full
performance review remains necessary, especially tongue and eye movement.
The rejected linear-combination and correspondence diagnostics are retained
as evidence, not used as accepted aliases.

## Unreal checkpoint

All six derived meshes under `/Game/CSS/AnimLab/SK_Face_Jali3V1_*` pass import
and fresh-process verification. Geometry, materials, bone binding, skeleton,
physics/post-process bindings, cloth properties and original 22 morphs are
preserved. The new shapes use the existing 0.015 cm morph filter; it was not
lowered. Black Pearl's maximum retained-delta difference is 0.000095176 cm.

Shared-path staging is under `shared-faces-jali3-v1`, using unique
`/Game/CSS/Locomotion/Faces/SK_Face_Jali3V1_*` assets. Fresh staging checks,
Windows cooking, container verification and independent cooked readback pass
for all six meshes. All 3,331,514 render-to-source vertex mappings were checked,
including retained body/hand morphs, render geometry, cloth and bindings.

The verified face catalog contains six adapters, 115 facial morphs, the same
21 expressions and five performances: Come Closer, Gesture 200, DumbDumb,
Iris Out and Spin. All 6,196 prepared facial keys retain their original source
chain; Gesture 200 retains its previously verified Bezier conversion bound.
Cinderella is not in this catalog because its body port is still pending.

The candidate is merged into the combined shared pack described below, but
is not installed. Complete performance review and live acceptance remain.
Commander White performance mapping and Unholy Genessa's rigid-mask feasibility
remain separate work. The custom head does not establish mappings for them.

The transfer's fresh replay produces a byte-identical 115-shape payload.
Three real-input rejection checks cover an existing output, an unknown shape
and an incomplete head audit. Verified duplicate payloads were removed after
comparison, with canonical copies and hashes retained in
`author-head-source-v1/duplicate-cleanup.json`.

Reproduction uses Blender 5.2.2 and the existing .NET 10 exporter. First capture
with `--source`, `--head-audit` (the audit JSON) and a fresh `--output`. Transfer
with `--head-audit` (the audit directory), `--body-samples` (the captured NPZ),
`--baseline` (the unchanged 105-shape export), repeated `--shape` options and a
fresh `--output`. Both scripts require Blender's `--` argument separator.
Run `test_head_transfer_corners.py` in background Blender. The existing
`run_facial_library.py` imports/inspects the export with `--revision Jali3V1`.

## Combined candidate and motion previews

`eve-face-jali3-trial-v2` replaces the six old Eve face exports while retaining
the other 90 exports byte-for-byte. The catalog retains 62 body/movement IDs,
31 photo poses, five outfit adapters and four native locomotion alternatives.
Commander White's 74 shapes and eight expressions plus Neutral are unchanged.
The pack remains separate from outfit releases and requires no runtime change.

The first combined manifest failed the real CSS loader: pretty-printed JSON
was 744,767 bytes, above the existing 256 KiB manifest limit. Compact JSON is
167,999 bytes with exactly the same values. The packaging helper checks that
limit before writing. The runtime limit was not raised. The rejected manifest
remains a negative control. Actual combined and pack-only loading now pass
1,131 and 1,119 checks; `host-catalog-validation.json` retains both failure and
success outputs. Seven focused catalog tests protect existing curves, saved
expressions, outfit revisions, distinct mesh bindings and body references.
All three animation/facial host test suites pass.

`author-head-source-v1/performance-motion-v1` contains full-duration 30 Hz
facial comparison videos for DumbDumb, Iris Out and Spin, with 1,972, 500 and
111 frames. Source is on the left; the fitted 115-shape interchange is on the
right. FFprobe independently verifies every encoded frame count and rate.
Iris Out's last visual sample is 16.633333 seconds, one sample before its
16.666666-second endpoint. Its full curve data is unchanged. Thirty-six evenly
spaced comparison frames were inspected: eyelids and mouth match closely,
without visible collapse in those views. This is sampled visual inspection of
full-length renders, not proof of every-frame smoothness or live acceptance.

The rejected v1 stage/readback held 192 payload files identical to v2. After
hash comparison, they were replaced with references to the retained v2 copies,
removing 1,084,314,122 duplicate bytes. The receipt is
`eve-face-jali3-trial-v1/duplicate-payload-cleanup.json`. Original archives,
outfits, release packages and the accepted base candidate were preserved.

## Cinderella body baseline

`cinderella-body-v1/Cinderella/verification.json` accounts for all 155 decoded
tracks without changing any key. The 54 required body/ancestor tracks are
projected for an isolated audition; the other 101 remain in
`preserved-tracks.json`, including unidentified indices 3269-3272. This does
not claim the unresolved tracks are unnecessary or identify their bindings.

`retarget-cinderella-c1` imports and retargets the complete 8,381-sample,
139.66667-second sequence at 60 Hz. The existing source basis and retarget
settings are unchanged. Full raw pose capture streams to compressed JSON to
avoid keeping 8,381 complete poses in memory. Fresh-process comparison of the
saved asset against a repeated conversion passes all 388 tracks and 3,251,828
authored transforms exactly. Five reloaded pose samples also match exactly.
Original target mesh and skeleton hashes remain unchanged.

The five sampled skinned views show recognizable body poses, but contact is
not accepted: the right shoe penetrates the ground by 1.408 cm at the start
and 1.697 cm at the end. Intermediate elevated feet need comparison with the
source trajectory before any correction; authored jumps must not be flattened.
`review-status.json` records sample times, measurements and evidence hashes.
Apply the established source-relative foot/contact workflow, preserve non-leg
motion, then perform dense contact, cooking and outfit checks. Cinderella
remains outside the playable catalog. All import, retarget, render and fresh
verification processes finished successfully; none installed game files.

## Cinderella contact investigation

The C2 study reuses the earlier foot-local retarget configuration after checking
that all 54 source reference bones and the target bind pose match. A fresh
conversion again matches all 3,251,828 saved authored transforms. Measuring the
neutral cooked lower-leg surfaces at all 8,381 keys finds penetration of
8.225 cm on the left and 3.975 cm on the right. Sparse screenshots alone missed
the worst frames.

The C3 per-frame sole correction preserves every non-leg local pose, but is
rejected. It reduces right-foot penetration to numerical noise and leaves
1.496 cm on the left heel-support surface. More seriously, it increases the
left calf's frame-1254 rotation step from 17.56 to 62.47 degrees. Neither C2
nor C3 is accepted, cooked or installed.

The C4 diagnostic retains full source and target leg trajectories in
`retarget-cinderella-c4/knee-worlds.npz`. The existing short knee-reversal
repair rejects two intervals because the neighboring bend directions disagree
by more than its 45-degree guard. No C4 animation was created and the guard was
not weakened. Source inspection also shows that the frame-1254 reversal exists
in the supplied motion; contact correction amplifies it when the target leg
changes from straight to bent. This needs a source-aware retarget correction,
not blanket smoothing of the performance.

Evidence is under `retarget-cinderella-c2` through `retarget-cinderella-c4`:
`fresh-verification.json`, the two `contact.json` reports,
`pose-preservation.json`, `knee-diagnostic.json` and `knee-comparison.json`.
These are full-key offline measurements of the neutral fitted mesh. They do
not establish outfit physics, body-morph or in-game acceptance.

Follow-up inspection identifies a selection error in C3: its lift used the
minimum across all alternate foot sections, including hidden flat-foot
geometry. The actual Black Pearl footwear is material 20. At left frame 1253
it is 3.000 cm above the floor, while the all-section minimum is -5.436 cm.
At right frame 7478 it is 2.206 cm above the floor versus -3.901 cm across all
sections. Correcting those hidden surfaces introduced unnecessary knee bends.
Keep all-section measurements for diagnostics, but select the active footwear
explicitly when deriving contact corrections. Alternate footwear needs its own
visibility-aware review; a single all-section minimum does not represent it.

The isolated C5 experiment copies source foot XYZ instead of Z only. It moves
the feet by a median 14.16/13.66 cm left/right, increases straight left-leg keys
from 2,305 to 3,180 and leaves sampled penetration. It is rejected. No original
retargeter was changed. C6 instead starts from C2 and derives sole lifts only
from material 20. That footwear passes all 8,381 authored keys, with minimum
left/right heights of -0.0000214/-0.0000283 cm. Fresh conversion matches all
3,251,828 authored transforms exactly; five saved/reloaded poses also match
exactly. All non-leg evaluated local poses remain unchanged.

C6 is still not motion-accepted. The left calf has a 154.53-degree step at
frame 5240, versus 135.06 degrees in C2. The source knee changes bend direction
near extension, whereas C2 is already bent about 70 degrees. Absolute source
foot height combined with different limb proportions amplifies that reversal.
The existing conservative local repair cannot resolve this safely.

C7 tests Unreal's limb-length-relative foot goal with no absolute position
blend or static foot offset. Across all keys its median knee-flexion error
against the source falls to 0.145/0.141 degrees left/right, with maxima
1.903/2.007 degrees. However, sampled visible footwear penetrates by up to
24.17/15.54 cm, so C7 is also rejected as a playable candidate. Its compact
`legs.json`, selected `sparse-motion.json`, `sparse-contact.json` and
`comparison.json` retain the evidence without another full-pose dump.

Next investigate source hip placement together with limb proportions rather
than adding further sole lifts to C2. Source thighs are parented under Spine;
target thighs are parented under pelvis. At frame 5238 the source left hip is
89.58 cm high and the C2/C7 target hip is 80.25 cm, despite the target leg being
longer. A derived hip-center motion reference is a hypothesis, not an accepted
fix. Preserve the original source, compare all affected body motion explicitly
and reject a correction that merely exchanges knee flips for foot penetration.


## Cinderella C9 pelvis-clearance candidate

C7 now has a complete raw capture and visible-footwear contact report.
C9 retains C7's limb-relative knee motion and lifts only pelvis translation
using the C8 measured clearance plan. The largest world-Z lift is 24.1723 cm;
smoothing can add up to 5.0934 cm over the instantaneous requirement. This
changes vertical body travel and still needs full-motion review.

The generic C8 track setter failed rotation/scale preservation. The new
`BakePelvisClearance` instead edits only sequencer translation channels and
checks every authored transform. C9 fresh repeat and a zero-lift control pass
exactly across 388 tracks and 3,251,828 transforms. Five reopened poses also
match exactly. All 8,381 poses clear visible material 20 within 0.000004 cm.
All nine evaluated virtual bones match their declared targets at every key.
Seven invalid-input cases are rejected without modifying protected sources.

At the former left-calf discontinuity (5240), the angular step is now
14.8697 degrees, versus C2's 135.058 and C6's 154.527. The largest calf steps
are 40.2996/37.7100 degrees left/right, close to the original source's
40.2391/37.6599. These are measured comparisons, not a smoothness guarantee.
Three 41-key motion windows were rendered and encoded at 60 FPS. Reviewed
stills show no obvious knee collapse; the render omits simulated hair/cloth.
No complete visual, cooked, adapter or live acceptance is claimed.

Cinderella's facial preparation separately preserves all 12 named curves and
3,897 keys exactly from the decoded source. Evidence is in
`facial-cinderella-v1/verification.json`. It is not added to the pack yet;
current catalog counts remain 62 body/movement IDs and five Eve performances.

C9 cook staging initially crashed in Unreal's animation thumbnail preview
lookup (`USkeleton::FindCompatibleMesh` while setting compression). This is
separate from the passing raw-pose tests. Assigning the audited Black Pearl preview mesh explicitly resolves staging.
Both Windows cooks exit zero, and the isolated two-export package round-trips
exactly. Full independent ACL readback passes all 8,381 samples and 388 tracks:
maximum position error 0.015531 cm, quaternion distance 0.000708 and scale
error 0.000000962, within the unchanged limits. Preserve both staging attempt
logs under `cinderella-c9-cook/`. Between-key contact and wider acceptance
remain pending.


The C9 cooked 240 Hz contact scan contains 33,521 poses. It retains every
60 Hz decoded key exactly, but material 20 reaches -0.458656 cm on the left
(seven samples below -0.1 cm) and -0.099207 cm on the right. Original-key
minima remain within 0.000429 cm. C9 therefore fails the existing dense contact
criterion. C10 adds a measured upper-envelope clearance to the pelvis only,
with at most 0.463657 cm additional lift and a predicted 0.005 cm floor margin.
This prediction still requires a fresh bake and independent cooked recheck.

The diagnostic ACL reader now permits four million transforms per source
clock, retaining checked arithmetic, dimension and finite-value guards; dense
sampling is still limited to four substeps. Python readers account for a
possible restored wrap endpoint. This accommodates Cinderella's 3,251,828
transforms without truncating the clip. Fourteen focused Python tests pass,
and the .NET 10.0.401 build succeeds. Existing dependency warnings remain.
Evidence: `cinderella-c9-cook/full-motion-verification.json`,
`dense-source-key-verification.json`, `footwear-contact-summary.json` and
`retarget-cinderella-c10/pelvis-clearance.json`.


## Cinderella C10 cooked contact passes

C10 is saved, freshly reconstructed and cooked as an isolated study. The
fresh-process repeat matches all 388 tracks and 3,251,828 authored transforms
exactly. Both Windows cooks exit zero; the animation and its owned compression
settings round-trip through the package unchanged. Complete independent ACL
readback again passes the existing position, rotation and scale limits.

The 240 Hz cooked scan covers 33,521 poses. Visible Black Pearl footwear stays
above the floor: left minimum 0.004485 cm and right minimum 0.004487 cm, with no
sample below -0.1 cm. Every original 60 Hz decoded key remains exactly equal
in the dense export. This is neutral fitted-footwear evidence only. It does
not establish other footwear, full-motion appearance, simulated cloth/hair or
in-game acceptance. The underlying animation remains 60 Hz and 139.667 seconds;
240 Hz is a diagnostic sampling rate, not a shipped animation or game FPS.

Evidence: `retarget-cinderella-c10/` contains the clearance recipe and creation
receipt. `cinderella-c10-cook/` contains the fresh bake/staging comparison,
Windows cook exits, exact package verification, complete raw/cooked comparison,
dense-source-key verification and footwear-contact summary. The inherited
`settings` field in the creation receipt audits the unchanged original L5P
retargeter; the bake actually uses the protected C7 limb-relative sequence.

The enlarged decoder also reproduces the earlier DumbDumb and Iris Out
binary transforms, sampled JSON and manifests byte for byte. Fourteen focused
Python tests pass. Their duplicate regression exports now link to the retained
originals. Rejected C3/C6 raw pose dumps were deleted after hashing and checking
retained native assets, capture recipes and diagnostic reports. Combined space
reclaimed is 507,747,617 bytes; see `cinderella-rejected-capture-cleanup.json`
and `cinderella-c9-cook/decoder-regression-verification.json`.

Next: full C10 motion review, requested outfit adapters and facial integration.
The combined release candidate still has 62 body/movement IDs, 31 photo poses
and five Eve facial performances. Cinderella is not cataloged or installed;
its 12 facial curves remain prepared separately. No excluded rigs were added.
