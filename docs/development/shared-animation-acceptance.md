# Shared animation acceptance

CW Cinderella is the fourth prepared facial mapping. Its full 139.66667-second
track maps twelve source channels. The final `cw-cinderella-curves-v3` export
has 51 active curves, 61,727 keys and 2,547,961 bytes. It passes all 11,387
sampled poses and 11,386 separately evaluated intermediate poses, with maximum
errors of 0.009974 and 0.014629 cm under the unchanged 0.015 cm limit.
Native playback/restoration passes 20,515 checks across 393 sample times.

Only Gesture 200 is packaged. Spin, DumbDumb and Cinderella need a combined
shape representation, cooking and catalog integration. Come Closer and Iris
Out remain unfinished for CW. Iris Out's signed cheek/tongue controls now
evaluate correctly in an isolated bake, but its first combined pose curls
the tongue too high and is not accepted. Full temporal/source comparison,
live compatibility and release remain open. Nothing new was installed.
See [the mapping and verification record](shared-animation-cw-face.md).

Earlier checkpoints:

CW DumbDumb is the third prepared facial mapping, after Gesture 200 and Spin.
All twelve source channels are adapted through measured or native CW controls.
`cw-dumbdumb-curves-v2` fits the existing runtime limits: 122 active curves,
58,555 keys and 2,410,645 bytes. The final stored curves pass 5,061 original
evaluated poses and 5,060 independent intermediate poses, with maximum errors
of 0.007841 and 0.010172 cm under the unchanged 0.015 cm limit. Native sampling
passes 35,978 assertions, including baseline restoration. See the
[mapping, compression and source evidence](shared-animation-cw-face.md).
Only Gesture 200 is packaged. Spin and DumbDumb still need cooking and catalog
integration; Come Closer, Iris Out and Cinderella still need CW mappings.
Full temporal visual comparison, live compatibility and release remain open.
Nothing new is installed.

Earlier checkpoints:

Facial resource loading is now implemented and host-tested. The private
`facial-resource-trial-v1` candidate preserves every entry and all 98 cooked
exports from `cw-jaw-trial-v1`, with a 37,884-byte manifest and two separately
checked facial resources. Its combined and pack-only checks pass 1,398 and
1,386 assertions. Old inline catalogs still pass. This removes the packaging
size obstacle; it does not complete Spin cooking or the four remaining CW
mappings. Nothing new is installed. See
[the format and verification](facial-package-resources.md).

CW Spin now has a complete source-rig mapping and bounded playback curves in
`cw-spin-source-v3`, `cw-spin-basis-v3` and `cw-spin-curves-v2`. All eleven
source curves are mapped. The 401 poses and 400 independent intermediate
poses pass the unchanged 0.015 cm geometry limit; maximum holdout difference
is 0.005586 cm. This is a second prepared CW facial track, not a second
packaged or live-accepted track. Gesture 200 remains the only CW performance
in the combined candidate. Come Closer, DumbDumb, Iris Out and Cinderella
still need their CW mappings. Spin also needs asset cooking and catalog
integration. Its curve data exceeds the manifest limit and passes the new
bounded resource loader as a parser fixture.

CW's expanded performance face is now independently cooked and checked in
`cw-performance-cook-v1`: 59 added shapes, all 90 prior mesh morphs preserved,
and 144 source-control interpolation holdouts passing. This is preparation
for the remaining five CW facial performances, not five completed mappings.
The latest combined catalog is still `cw-jaw-trial-v1` below. Nothing from
the expanded face study is installed or live-accepted.

Latest facial addition: `cw-jaw-trial-v1` retains C11 and adds CW's jaw-only
Gesture 200 facial performance. Its geometry matches 32 fresh source poses
within 0.007691 cm, and native sampling/restoration and packaged loader checks
pass. All 98 cooked exports are unchanged. CW's other five facial performances
remain unfinished; nothing in this candidate is installed or live-accepted.
See [CW facial evidence](shared-animation-cw-face.md).

October 7 update: `cinderella-combined-v2` combines 63
body/movement IDs with four native locomotion alternatives, six updated Eve
faces and the verified Commander White face. It preserves all 31 photo poses
and five outfit adapters. Eve now has 115 facial shapes, 21 expressions and
six prepared facial performances. Actual combined and pack-only catalog
checks pass 1,133 and 1,121 assertions. See the
[author-head evidence](shared-animation-author-head.md). This candidate is not
installed or live-accepted. Cinderella passes full cooked-motion comparison
and 81 adapter cases across the nine requested meshes, but the full fitted
render exposed knee penetration around 61.83 seconds in C10. The C11 replacement
passes the complete visible-surface scan at 240 Hz, including knees, with a
minimum height of +0.009562 cm. The checked kneeling still now meets the floor.
C11's full render and contact measurements pass, and 28 representative poses
have been reviewed. Full temporal/source comparison, secondary physics and
live acceptance remain unfinished. C10 remains rejected. Historical
checkpoints below describe earlier candidates.

Checkpoint: 2026-10-06. This is a private candidate, not release approval.

The latest private combined catalog contains eight released idles, twenty
moving clips and thirty-one photo poses. DumbDumb and Iris Out's full L5X body
auditions are now included, with unresolved source/facial mappings still open.
The installed trial remains the older 57-sequence version. All live acceptance
entries below are pending for this shared-pack revision, even where an older
outfit release has previously worked in game.

`shared-dance-trial-v1` preserves the earlier 57 entries and all 68 cooked
exports exactly. Its 71 exports include two dances and their compression
settings. Fresh current-module evaluation passes all 18 dance/mesh pairs and
162 playback/restoration cases. Independent package readback resolves all 59
sequences with Eve, Unholy Genessa or Commander White installed alone. Host
catalog checks pass for both the combined and three isolated installations.
Evidence: `l5x-adapter-v1/corrected-current-result.json`,
`shared-dance-trial-v1/verification.json`, `shared-dance-isolated-v1/report.json`.
No excluded outfit is included; this is not live or release acceptance.

## Evidence boundaries

Both L5X full neutral fitted-mesh renders and their MP4 encodes have finished
successfully. Independent video readback counts 1,972 DumbDumb frames and 501
Iris Out frames at 30 FPS, at original speed with the final frame included.
The 60 Hz animation assets are unchanged. Sampled review sheets show no obvious
limb collapse; this does not establish complete temporal smoothness, simulated
cloth behavior or gameplay acceptance. Evidence and hashes:
`l5x-cook/full-render-review.json`. No render process remains active.

Latest Iris Out candidate: L5W's local knee correction and authored contact
pass. Its independently packaged Windows animation now passes comparison at
all 1,001 source-key times across 388 tracks, including virtual bones, using
linear ACL interpolation and an unretargeted UE reference. Maximum local
translation error is 0.009512 cm and quaternion distance is 0.00217104 (below
the existing 0.25-degree rotation limit). Evidence:
`work/cine-animation-port/l5w-cook/decoded-runtime-v1/unretargeted-motion-verification.json`.
The corrected nine-mesh adapter run subsequently passed; gameplay remains open.

DumbDumb L5U's full comparison found a 0.00313772 quaternion distance at
`ball_r`, frame 615, exceeding the unchanged 0.25-degree review limit. The
L5UPrecision2 copy preserves all 1,529,884 authored keys and uses an owned ACL
settings asset with threshold 0.001 cm instead of 0.01 cm. It now passes all
3,943 key times across 388 tracks: maximum translation error 0.005594 cm,
quaternion distance 0.000461133 and scale error 0.000000962. Its cooked animation
grows from 981,617 to 1,280,027 bytes. The package includes its settings asset
and passes exact export round-trip with all imports resolved. Evidence:
`work/cine-animation-port/l5u-precision2-cook/decoded-runtime-v2/full-motion-verification.json`
and `pack-with-settings2/verification.json`. The raw reference is independently
captured from the original L5U study with retargeting disabled; fresh native
comparison proves the precision copy's authored data is unchanged. This is
compression evidence only, not dense contact or full outfit acceptance.

Subsequent dense contact check: both corrected candidates pass nine-mesh adapter
playback (162 cases, 44,496 full-clip poses). Cooked samples at 120 Hz preserve
every original-key transform exactly but reveal between-key footwear dips.
For neutral Black Pearl material 20, Iris Out reaches -0.18258/-0.27834 cm
left/right and DumbDumb -0.42759/-0.42878 cm. Alternate hidden flat-foot sections
are reported separately and are not evidence of visible footwear penetration.
The cooked mesh's translation-retarget reconstruction matches 13 raw editor
poses within 0.000020 cm across 379 bones. Evidence: `decoded-dense-v1` in both
cook studies, `current-eve-skin-audit-v1`, and `dense-acl-regression.json`.

L5X applies localized additional sole clearance, capped below 0.47 cm, with a
two-key taper around the affected times. Fresh editor repeat passes all 388
tracks and 1,529,884/388,388 DumbDumb/Iris Out keys. Only the six leg tracks are
eligible for changes; native checks retain every other authored transform.
Its Windows cook and three-export package round-trip now pass. Every cooked
key passes the unchanged transform limits against a fresh unretargeted raw
reference. The 240 Hz neutral footwear scan covers 15,769/4,001 poses, with
worst left/right height -0.02991/-0.02948 cm for DumbDumb and
-0.02831/-0.01662 cm for Iris Out. No footwear sample crosses -0.1 cm.
The source-key cooked knee check introduces no flips. Updated adapters pass
all nine meshes, 162 cases and 44,496 full-clip poses. Full fitted-mesh renders
are running. Neither clip is installed or accepted;
other footwear choices, proportions, post-process and live checks remain.

The first full comparisons used nearest ACL sampling and retargeted contact
captures. Those diagnostic failures are preserved, but cannot be read as
runtime errors. The tools now record the ACL rounding policy and require an
explicit unretargeted reference. Native data-model virtual tracks are also
distinct from the evaluated virtual poses regenerated for cooking.

- Cooked samples: independent compressed-pose readback compared with the authored
  sequence, up to five samples per clip. This does not prove every frame or contact.
- Adapter poses: all eighteen moving clips pass on all nine meshes, including
  38 finger bones, 1,458 cases and 87,255 full-motion poses at 60 Hz. This does
  not establish the world cloth solver or contacts.
- Sampled silhouettes: all eighteen moving clips have complete sampled reviews;
  close contacts, native-speed motion and live physics remain open.
- Live acceptance: playback, interruption, restoration, cloth/wings, contacts and
  frame costs remain required in both CSS and CINE.

Receipts beneath `work/cine-animation-port/shared-v1/`: `plan.json`,
`catalog.json`, `cooked-samples-v3/motion-verification.json`,
`performance-fingers-200-v1-result.json` and the completed
`performance-fingers-all-v1-result.json`. Source means the audited pre-cook
authoring asset; `plan.json` also retains the original source paths and rate
metadata for new ports. Original archives are inventoried separately.

## Per-clip state

All source assets in the table are beneath `/Game/CSS/`.

| Clip | Canonical ID | Audited source | Cooked samples | Live |
| --- | --- | --- | ---: | --- |
| Eve Default Idle | `css.eve.idle.default` | `Eve/Anim/AN_I2_Idle` | 5 pass | Pending |
| CSS Seated Recline | `css.eve.idle.seated_recline` | `Eve/Anim/Idles/AN_Idle702` | 5 pass | Pending |
| CSS Kneeling Bow | `css.eve.idle.kneeling_bow` | `Eve/Anim/Idles/AN_Idle703` | 5 pass | Pending |
| CSS Low Lean | `css.eve.idle.low_lean` | `Eve/Anim/Idles/AN_Idle704` | 5 pass | Pending |
| CSS Deep Kneeling Bow | `css.eve.idle.deep_kneeling_bow` | `Eve/Anim/Idles/AN_Idle705` | 5 pass | Pending |
| CSS Reclining | `css.eve.idle.reclining` | `Eve/Anim/Idles/AN_Idle706` | 5 pass | Pending |
| CSS Forward Fold | `css.eve.idle.forward_fold` | `Eve/Anim/Idles/AN_Idle708` | 5 pass | Pending |
| CSS Side Kneel | `css.eve.idle.side_kneel` | `Eve/Anim/Idles/AN_Idle713` | 5 pass | Pending |
| Bass Down Low | `css.eve.dance.bassdownlow` | `AnimLab/RT_L2_S01A000` | 5 pass | Pending |
| Black Hole | `css.eve.dance.blackhole` | `AnimLab/RT_L2_S02A000` | 5 pass | Pending |
| Eve Catwalk | `css.eve.catwalk.evecatwalk` | `AnimLab/RT_L2_S03A000` | 5 pass | Pending |
| Come Closer | `css.eve.dance.comecloser` | `AnimLab/RT_L2_S04A000` | 5 pass | Pending |
| Jang Won Young Spin | `css.eve.dance.jangwonyoungspin` | `AnimLab/RT_L2_S07A000` | 5 pass | Pending |
| ATOOL Performance 1 | `css.eve.performance.atool1` | `AnimLab/RT_L2_S11A000` | 5 pass | Pending |
| ATOOL Performance 2 | `css.eve.performance.atool2` | `AnimLab/RT_L2_S11A001` | 5 pass | Pending |
| ATOOL Performance 3 | `css.eve.performance.atool3` | `AnimLab/RT_L2_S11A002` | 5 pass | Pending |
| ATOOL Performance 4 | `css.eve.performance.atool4` | `AnimLab/RT_L2_S11A003` | 5 pass | Pending |
| ATOOL Performance 5 | `css.eve.performance.atool5` | `AnimLab/RT_L2_S11A004` | 5 pass | Pending |
| ATOOL Performance 6 | `css.eve.performance.atool6` | `AnimLab/RT_L2_S11A005` | 5 pass | Pending |
| ATOOL Performance 7 | `css.eve.performance.atool7` | `AnimLab/RT_L2_S11A006` | 5 pass | Pending |
| ATOOL Performance 8 | `css.eve.performance.atool8` | `AnimLab/RT_L2_S11A007` | 5 pass | Pending |
| Bring It On | `css.eve.photo.bringiton` | `AnimLab/RT_L2_Photo000` | 5 pass | Pending |
| Charm | `css.eve.photo.charm` | `AnimLab/RT_L2_Photo001` | 2 pass | Pending |
| Clap | `css.eve.photo.clap` | `AnimLab/RT_L2_Photo002` | 5 pass | Pending |
| Confident | `css.eve.photo.confident` | `AnimLab/RT_L2_Photo003` | 5 pass | Pending |
| Cutie | `css.eve.photo.cutie` | `AnimLab/RT_L2_Photo004` | 5 pass | Pending |
| Flow | `css.eve.photo.flow` | `AnimLab/RT_L2_Photo005` | 2 pass | Pending |
| Give Me | `css.eve.photo.giveme` | `AnimLab/RT_L2_Photo006` | 5 pass | Pending |
| Hand Heart | `css.eve.photo.handheart` | `AnimLab/RT_L2_Photo007` | 5 pass | Pending |
| Hush | `css.eve.photo.hush` | `AnimLab/RT_L2_Photo008` | 5 pass | Pending |
| Kiss | `css.eve.photo.kiss` | `AnimLab/RT_L2_Photo009` | 2 pass | Pending |
| Kung Fu Salute | `css.eve.photo.kungfusalute` | `AnimLab/RT_L2_Photo010` | 5 pass | Pending |
| Natarajasana | `css.eve.photo.natarajasana` | `AnimLab/RT_L2_Photo011` | 5 pass | Pending |
| Oh My | `css.eve.photo.omg` | `AnimLab/RT_L2_Photo012` | 5 pass | Pending |
| Reach | `css.eve.photo.reach` | `AnimLab/RT_L2_Photo013` | 2 pass | Pending |
| Salute | `css.eve.photo.salute` | `AnimLab/RT_L2_Photo014` | 5 pass | Pending |
| Shutter Chance1 | `css.eve.photo.shutterchance1` | `AnimLab/RT_L2_Photo015` | 5 pass | Pending |
| Shutter Chance2 | `css.eve.photo.shutterchance2` | `AnimLab/RT_L2_Photo016` | 5 pass | Pending |
| Shutter Chance3 | `css.eve.photo.shutterchance3` | `AnimLab/RT_L2_Photo017` | 5 pass | Pending |
| Slav Squat | `css.eve.photo.slavsquart` | `AnimLab/RT_L2_Photo018` | 5 pass | Pending |
| Somersault | `css.eve.photo.somersault` | `AnimLab/RT_L2_Photo019` | 5 pass | Pending |
| Stretch | `css.eve.photo.stretch` | `AnimLab/RT_L2_Photo020` | 5 pass | Pending |
| Sweetie | `css.eve.photo.sweetie` | `AnimLab/RT_L2_Photo021` | 5 pass | Pending |
| Thumb Up | `css.eve.photo.thumbup` | `AnimLab/RT_L2_Photo022` | 5 pass | Pending |
| Toast | `css.eve.photo.toast` | `AnimLab/RT_L2_Photo023` | 5 pass | Pending |
| Victory | `css.eve.photo.victory` | `AnimLab/RT_L2_Photo024` | 5 pass | Pending |
| Walk | `css.eve.photo.walk` | `AnimLab/RT_L2_Photo025` | 5 pass | Pending |
| Warm Up | `css.eve.photo.warmup` | `AnimLab/RT_L2_Photo026` | 5 pass | Pending |
| We Did It | `css.eve.photo.wedidit` | `AnimLab/RT_L2_Photo027` | 5 pass | Pending |
| What | `css.eve.photo.what` | `AnimLab/RT_L2_Photo028` | 5 pass | Pending |
| Wish | `css.eve.photo.wish` | `AnimLab/RT_L2_Photo029` | 5 pass | Pending |
| Worry | `css.eve.photo.worry` | `AnimLab/RT_L2_Photo030` | 5 pass | Pending |
| Adam Catwalk | `css.eve.catwalk.adamcatwalk` | `AnimLab/RT_L3A_Adam` | 5 pass | Pending |
| Lily Catwalk | `css.eve.catwalk.lilycatwalk` | `AnimLab/RT_L3L_Lily` | 5 pass | Pending |
| ATOOL Gesture 200 | `css.eve.performance.example200` | `AnimLab/RT_L4S_Example200` | 5 pass | Pending |
| ATOOL Performance 801 | `css.eve.performance.example801` | `AnimLab/RT_L4D_Example801` | 5 pass | Pending |
| After Brute Battle | `css.eve.performance.afterbrutebattle` | `AnimLab/RT_L4C_CameraBody` | 5 pass | Pending |

## Unfinished source clips

| Clip | Retained source | Blocker |
| --- | --- | --- |
| DumbDumb | source-05, `HD-ATOOL_P/anim/971` | L5X passes full cooked-key comparison, nine-mesh adapters and 240 Hz neutral Black Pearl footwear contact within 0.030 cm, with no new knee flips. Full-motion review and broader/live acceptance pending. Seven extra source indices and nine facial controls remain unfinished; user confirmed permission on October 6 |
| Iris Out | source-06, `HD-ATOOL_P/anim/970` | L5X retains the local knee repair and passes full cooked-key comparison, nine-mesh adapters and 240 Hz neutral Black Pearl footwear contact within 0.029 cm, with no new knee flips. Full-motion review and broader/live acceptance pending. Seven extra source indices and two facial controls remain unidentified; user confirmed permission on October 6 |

Neither clip is silently discarded, approximated or counted among the 57 cooked
sequences. See [source audit](shared-animation-library.md) and
[facial curve audit](performance-facial-curves.md). Jang Won Young Spin has a
cooked body clip but still lacks mappings for its blink, woo and ooo facial
controls; the body table does not claim complete facial fidelity.

## Facial and runtime state

### Required outfit coverage

Required custom-rig coverage is Eve (all six outfits), Unholy Genessa (both
faces), and Commander White. Also cover representative compatible appearances
using the default game human skeleton, including native, CSS and non-CSS.
Older Seductress, GeneXXa/Nunexxa and other custom-rig projects are excluded by
the user's October 6 correction. Do not make them release prerequisites.
Each supported appearance needs playback, interruption and restoration checks
in CSS and CINE. Body success does not establish facial or world-physics support.

The two additional body auditions now also pass all nine meshes:
`work/cine-animation-port/l5p-adapter-check/l5p-shared-all-result.json` records
18 clip/mesh pairs, 162 cases and 44,496 full-motion poses at 60 Hz. Maximum
body and finger position differences are zero; maximum angular differences are
7.31e-8 and 6.67e-8 radians. The shared authoring copies were independently
verified against the source candidates. These two clips are not installed or
counted in the 57 cooked sequences above.

The isolated two-clip cook now passes export round-trip and independent ACL
readback against raw editor poses: five samples per clip, 388 tracks including
nine virtual tracks. Largest local translation error is 0.00737 cm; largest
quaternion chord distance is 0.00125. Receipt:
`work/cine-animation-port/l5p-cook/decoded-v2/motion-verification.json`.
The initial decoder attempt used an obsolete binary and rejected the reference
coverage; the current audited `exporter-v2` successfully decoded both clips.
This is sampled compression evidence, not contact or full-motion acceptance.

Read-only face-source inspection is recorded in
`work/cine-animation-port/other-face-sources-v1/compatibility.json`:

| Character | Retained source capability | Remaining work |
| --- | --- | --- |
| Six Eve outfits | Installed derived meshes contain 105 facial shapes | Live playback and face restoration on each outfit |
| Unholy Genessa, Eve skin | Same native Genessa mask as the Genessa-skin variant. The source body's 105 matching names are not mask controls; export filtering removes 18 entirely | Keep body playback independent. Facial support would need an authored mask rig or verified native controls, not the Eve expression binding |
| Unholy Genessa, Genessa skin | Native face and eyes have no shape keys; inspected rig has no deforming facial bones | Preserve the mask. No verified expression binding currently exists |
| Commander White | Eight source expressions plus jaw/blinks produce 44 shapes with saved readback and base/cloth preservation. Individual interpolation passes; combined-blink calibration still fails 9 of 172 cases at the declared limit | Refine combined control representation, map performances, then verify cooking, facial playback, restoration and world physics. Trial is not installed |

Matching names establish source availability only. They do not prove runtime
vertex correspondence or equivalent expression strength. The original outfit
sources remain unchanged.

The installed private trial has 21 static Eve expressions and Come Closer facial
curves on six verified derived Eve meshes. Gesture 200 facial curves are now
installed through the catalog-only revision recorded in
`work/cine-animation-port/live-face-motion-2/deployment.json`. Come Closer and Gesture 200 retain source timing and
signed curve values; neither has live acceptance yet. Unassigned facial actions
in 801 are not treated as active animation.

The stopping-pop correction is installed but still needs the matched gameplay
check described in [the transition investigation](eve-stopping-transition.md).
Eve, Genessa and Commander White isolated-package checks pass; those dependency
checks do not substitute for their live outfit/physics tests.

Before release, complete the unresolved source work, native-speed/contact and
live acceptance, performance measurements and the
separate asset/runtime distributions with credits, BBCode notes and checksums.

The [source requirements](shared-animation-source-requirements.md) specify the
missing rig data. The [permission audit](shared-animation-source-permissions.md)
now covers all six additional source pages as well as DumbDumb and Iris Out.
The user confirmed permission on October 6; the public-page audit is historical,
not an outstanding permission request. The installed files remain a private
trial, with no public upload made.
