# Shared animation outfit coverage

## Latest user correction: four native locomotion choices only

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

October 6, 2026. This records inspected assets, not release acceptance.

Default-human progress: the exact native reference now has a complete
Black Hole retarget audition with its mirrored accessory scales preserved.
See [native support evidence](shared-animation-native-human.md). This does not
yet establish native runtime, visible-mesh or physics compatibility.

## Authoritative user scope correction, October 6

Support exactly these authored outfits: **Eve (all six outfits), Unholy Genessa
(both faces), and Commander White**. Also support appearances using the
**default game human skeleton** where technically compatible, whether native,
CSS or non-CSS. This does not authorize adapters for other custom rigs.

**Older Seductress, GeneXXa/Nunexxa and other unrelated custom-rig projects are
out of scope.** Do not work on, integrate or make them release prerequisites.
Do not use their unfinished evidence as a reason to delay this animation pack.
The old Seductress project is not the requested Eve `CSS_SeduXtress_eins0fx`.
Preserve existing evidence and source files; no further jobs or installation
for those excluded projects. The prepared Seductress S9 batch was never run.
This explicit correction overrides all earlier broad collection wording and
historical next-step notes below. All requested clips, photo poses, feasible
facial support, CSS/CINE integration and acceptance for the actual scope remain.

## October 7: cleanup and Commander White facial checkpoint

Removed 3,247 full-resolution PNG intermediates from three rejected L5P
motion-review directories, totaling 1,985,384,005 bytes (1.85 GiB). Kept
58 representative frames, source data, commands and reports. The hashed
removal receipt is `work/cine-animation-port/cleanup-20261007-rejected-frames.json`.
Original assets, accepted candidates, installed mods and release archives were
not deleted. Large new facial trial assets and Unreal temporary save files
now use `/mnt/eins0fxE/CSS-authoring-scratch/cw-face-e2` through explicit binds.
Continue removing only inspected, obsolete intermediates as work proceeds.

Commander White's 512 expression/blink calibration samples now fit 30
corrective morphs, alongside 44 original control samples. These are 74 new
morph shapes for eight expressions, not 74 expressions. Compression residual
is below 0.001 cm. All 172 original comparisons and 204 fresh holdout checks
pass the unchanged 0.015 cm source comparison limit. The latter peak is
0.008444912 cm, including asymmetric blinks.

Unreal's existing morph filter would remove small corrections. Scaling stored
deltas and inversely scaling playback weights passes the filter simulation;
the original build threshold remains unchanged. The E3 candidate imports and
reloads with 90 total morphs, retaining the original 16 and the original
geometry, binding and cloth data. `cw-expression-native-v4` has successful
creation and fresh verification receipts. The prior v3 attempt failed during
saving because the system drive was full and is not accepted.

Fresh shared-path verification now passes for all 90 morphs, bindings and
2,312,092 render-to-source vertex mappings (`cw-expression-map-v2`, exit 0).
The first map check rejected this unchanged mesh solely because its diagnostic
limit was two million vertices. The bounded limit is now four million, with
exact map length and every source index still checked. Earlier failures remain
recorded. Independent cooked readback, performance-curve integration and live
acceptance are unfinished. Other evidence: `cw-expression-basis-holdout-v1`,
`cw-expression-compact-v2` and `cw-expression-native-v4`. Eleven focused Python
tests and the UE editor helper build pass. Nothing was installed.

## October 7: Commander White face cooked and merged

See `docs/development/shared-animation-cw-face.md`. E4 restores all 70 released
material bindings after independent decoding caught 37 missing hair references
in E3. Fresh staging, Windows cooking and complete cooked readback now pass:
2,312,092 render vertices, original cloth/bindings and 16 existing morphs are
preserved; 74 added shapes and eight expressions plus Neutral are verified.
The six-face Eve decoder regression also passes.

`cw-face-trial-v1` retains all 62 body/movement IDs and 95 earlier exports, adds
one CW face (96 exports total), and passes 1,127 combined / 1,115 pack-only host
catalog checks. It remains uninstalled. No runtime DLL change was required.
Continue the unresolved facial/source work and complete live acceptance and
delivery for the original scope. No excluded-rig work.

## Historical collection inventory, not release prerequisites

Metadata was read from each actual PAK. TOC hashes and bulk sizes were compared
with its manifest. These checks do not claim complete bulk integrity,
editor/cooked graph equivalence or playback acceptance.

| Project | Package evidence | Animation work remaining |
| --- | --- | --- |
| GeneXXa / Nunexxa | Two v1.0.0 meshes; shared base reference and nine virtual-bone definitions match Eve exactly | Fixed-physics adapter passes 20 moving clips on both meshes and independent cooked graph comparison; shared packaging and isolated dependency/catalog checks pass; live acceptance pending |
| Seductress | Six packaged variants: Midas, Argent, Noir, Violette, Regalia, Flora | BodyHairV42 adapter passes creation/reload; cooked mesh binds match authoring; distinct reference still needs animation conversion and playback checks |
| Seductress v2.0.6 | Manifest lists 18 variants, but local build TOC hash and bulk size differ from that manifest | Locate a consistent original package before using it as a fixture; do not alter this build to make the audit pass |
| Seductress Next-Gen Showcase | One packaged variant using the native human skeleton; mesh has no post-process graph | Native-human sequence and playback path needed |
| Protocol Echelon Proxima | Authoring project exists; design goal remains paused | Preserve paused project; do not treat it as a released animation fixture |
| KDA Takedown / Marci | Reference material only in the inspected project roots | No authored runtime package found in those roots; references are not playable variants |

Records include Seductress's `docs/handover-2026-09-21.md`, GeneXXa's
`claude-work/docs/plan.md`, and package manifests. The Showcase README has an
outdated engine claim; package and decoded-asset evidence take precedence.

Readbacks: `work/cine-animation-port/expanded-outfit-inventory/`. The inventory
records exact package paths, IDs, versions, mesh paths and TOC hashes. Each
eligible package was mounted separately with game containers, preventing one
outfit's shared assets from masking another's. Original sources, packages and
installed files were not modified.

## Unholy Genessa facial capability correction

The two Unholy variants are skin-material variants, not separate Eve and
Genessa faces. The original project's `authoring/variant-binding-notes.md`
records that they share geometry and differ in nine body material paths.
`authoring/skin-transfer-notes.md` explicitly excludes the original Eve
face/mouth geometry from the body export. Both retain the native Genessa mask.

The fresh Blender 5.2.2 audit in `unholy-facial-v2/export-report.json` follows
the original export's vertex mask, triangle filtering and point ordering.
Of 105 matching source shape names, 18 have no deltas after filtering. The
remaining 87 affect retained body/head-backing vertices, not the separate
`UG_Face` or `UG_Eyes` objects. Those have 1,303 and 124 vertices respectively,
and neither has shape keys. For example, `Jaw_Left` moves 3,279 retained
`UG_genessa_Face` material vertices by up to 0.973 cm, but cannot articulate
the separate mask. Adding these body shapes would not prove visible facial
support and could deform the backing beneath a stationary mask.

`export_unholy_facial_shapes.py` retains the complete source expression and
performance definitions in diagnostic-only interchange, with absent controls
explicitly listed. No candidate was imported, cooked, catalog-bound or installed.
The original Blender file, template and export helper retain their recorded
hashes. Previous name-only facial compatibility claims are superseded.

Continue body playback on both skins independently of facial capability.
Expression support requires an authored mask deformation rig or verified
native facial controls; neither currently exists in the inspected source.
Do not substitute the Eve face or guess mappings to make the capability pass.

## Commander White facial driver recovery

`cw-facial-drivers-v1` reads the preserved `cw-default-c1.blend` without
executing embedded scripts or saving. Unlike the rebound CSS mesh, this source
retains both the DAZ and Rigify facial driver definitions. The audit includes
armature-data drivers, which the earlier source inspector did not capture.
Both rigs route `facs_cbs_JawOpen_div2(fin)` from `facs_jnt_JawOpen(fin)`;
the latter also drives the jaw joint. The left blink corrective similarly
follows `facs_jnt_EyeBlinkLeft(fin)` while eyelid joints have their own drivers.

This proves that simply exporting the similarly named corrective shapes would
omit part of the original expression. Continue by evaluating each verified
source control, baking the combined joint and corrective displacement, and
checking correspondence against the fitted B9/current released mesh. Preserve
the source and fitted body, eyelashes, eyes, mouth and existing customization.
Do not treat a source control name or static dependency trace as expression
acceptance. `source-drivers.json`, `jaw-blink-evidence.json`, the source hash
and zero-exit command receipt record this finding. No derived face is installed.

### Evaluated jaw and blink trial

`cw-facial-samples-v1` evaluates jaw open and left/right blink at 0.25, 0.5,
0.75 and 1.0, including the body, eyelashes and tear surface. Each source
control returns to neutral within 0.00001 cm. `cw-facial-bake-v2` checks the
affected points against B9 and the actual C4 assembly. The recorded S7 skirt
insertion shifts later points, including eyelashes, by 547. Every affected
point matches the current assembly within 0.000007645 cm. The initial attempt
without that offset was rejected by the native point check and never saved.

The isolated `/Game/CSS/AnimLab/SK_Face_CWJ1` retains 16 original morphs and adds
12 quarter-weight shapes. Creation and fresh reload pass in
`cw-facial-native-v3`, including 76,484 built deltas, filtering bounded by the
original 0.015 cm build threshold, and unchanged material, skeleton, physics
and post-process bindings. Endpoint and half-strength front/quarter views were
reviewed in `cw-facial-preview-v1`; this is not full-motion or gameplay review.

`cw-facial-preservation-v2` also compares all non-expression mesh-description
bytes (294,193,348), 69 built sections (382,233,853 comparison bytes), topology,
bone maps and persistent properties of the one cloth asset. The new morph
attributes are removed only from in-memory comparison copies. Everything else
in those comparisons matches exactly. The same check passes all six existing
Eve facial candidates, including their three cloth-bearing meshes, in
`cw-facial-preservation-legacy-v1`. This proves stored-data preservation, not
the behavior or cost of the live cloth solver.

The strict parser rejects fractional/string/boolean/null schema values and
non-boolean or enabled diagnostic-only flags. Nine negative cases pass on
each schema, while valid saved assets still read back. Schema 1 remains the
existing Eve format; schema 2 supplies explicit sparse source point indices.
The earlier string-to-boolean coercion failure is retained in
`cw-facial-strict-v1`, with its correction passing in `cw-facial-strict-v2`.

`cw-facial-interpolation-v1` independently samples strengths 0.125, 0.375,
0.625 and 0.875. Twelve individual and four combined evaluations stay within
0.010966 cm of the quarter-weight linear approximation, below the declared
0.015 cm source-approximation limit. Combined jaw/blink interaction is at most
0.000179 cm at those strengths. This does not test all unequal combinations,
cooked filtering, expression-to-control mappings or runtime blending.

Next expand verified controls and expressions, cook the derived mesh, and
validate facial playback/restoration and full outfit behavior. No CW facial
trial is catalog-bound, installed or accepted for release. Other characters'
facial capability remains separate from body animation compatibility.

### Eight source expressions and combined-blink study

`cw-expression-sampling-v1` extends the original control evaluation with
Afraid, Angry, Flirting, Frown, Shock, SmileFullFace, SmileOpenFullFace and
Surprised. These are actual CW source controls, not assumed equivalents of
Eve's 21 expressions. All return to neutral without changing the source file.
`cw-expression-bake-v1` maps their quarter-strength samples to 5,201 affected
points in the current assembly. Thirty-four front/quarter source views are
complete, and the nine-panel front review was inspected. Individual expressions
have distinct intended features with no obvious collapsed mouth or eyelids
in those views. This is static source-geometry review, not world rendering.

`/Game/CSS/AnimLab/SK_Face_CWE1` retains all 16 original morphs and adds 44
shapes. Creation and fresh verification in `cw-expression-native-v1` pass
667,587 built deltas with retained error 0.000089593 cm. The same 294,193,348
base-description bytes, 69 built sections and one cloth asset still match the
original through the preservation gate. Materials, bind pose, skeleton,
physics and post-process bindings remain unchanged. The original asset and
source/interchange hashes also remain unchanged.

Independent intermediate samples expose a separate composition issue:
adding an expression to a blink is not the original rig's combined pose.
`cw-expression-interpolation-v1` records a 0.145608 cm maximum error without
corrections. A single full-strength corrective per pair improves six of the
eight tested combinations but still fails Frown and Flirting. The original
driver `facs_ctrl_EyesBlink(rst):01` adds 0.1 times Flirting and 0.15 times
Frown; evaluated blink shapes cap at 1. The records and immutable driver hash
are in `cw-expression-correctives-v3/blink-bias.json` and the sample reports.

Sampling correction grids against the remaining eyelid closure, instead of
interpolating across the cap, brings that small test within 0.011880 cm.
The wider `cw-expression-blend-grid-v1` then checks 44 individual samples and
128 combined cases at independent eighth strengths. The latest quarter-grid
calibration (`cw-expression-correctives-v5`) still fails 9 of 172 total cases
against 0.015 cm, reaching 0.025027 cm on strong Afraid at intermediate blink.
Earlier sparse and half-grid failures remain recorded. Do not describe the
combined-blink work as finished based on the smaller passing test.

The correction model remains an offline NumPy experiment. It is not included
in CWE1, cooked assets, the catalog or runtime. Its 256 calibration corrections
also exceed the existing 128-morph limit if exported individually. Further
work must improve the failed combinations and represent the corrections within
the intended runtime cost, with independently checked reconstruction. The
eight-expression candidate itself remains an offline trial. Full performance
mapping, cooking, runtime expression/restoration and world physics stay open.

## Native human path

`work/cine-animation-port/native-human-compatibility/assets.json` independently
decodes the current game skeleton and four representative native meshes.
The skeleton has 1,199 raw bones and nine virtual bones. It shares 189 named
bones with the inspected Eve skeleton; common parents match, but 87 local
reference transforms differ. This requires retarget validation, not just adding
the skeleton path to `compatible_animation_skeletons`.

ThornBoi, Gragu, Harros and Sester Genessa each bind a different native
post-process graph. Replacing those with an Eve graph would not preserve their
existing processing. Native support needs upstream playback or verified
adapters that retain that processing.

Current runtime restrictions still needing work:

- `PerformancePlayer` resolves adapters through exact outfit container revisions.
  Catalog-defined bound layers now replace the fixed five-class list. Audited
  fixed-physics graphs may have zero CSS controls; graphs with controls still
  require matching layouts in both directions before binding.
- `Core::customize_api` and pending playback require an active CSS appearance
  and selection. Native/non-CSS appearances need an equivalent transient owner
  and restoration path.
- Animation compatibility recognizes exact skeletons and the audited
  base-to-wing mapping. Native human and older references need verified
  sequence alternatives or remapping.

The catalog lookup and rejection tests pass, and the Windows C++23 core builds.
This runtime change is not installed or live-accepted. The native/non-CSS
ownership and retarget restrictions above remain unresolved.

## GeneXXa authoring extension

`CSSPerformanceLibrary` now inserts explicit-time playback at the linked input
of the audited GeneXXa graph, which has fixed physics nodes and no CSS idle
controls. Existing idle graphs still insert after the idle branch. Incomplete
idle contracts and ambiguous/missing inputs are rejected.

The C++20 helper builds with UE 5.6.1. Creation and fresh-process reload preserve
all 32 original GeneXXa nodes and their pin defaults, keep the original skeleton,
and start inactive. The five existing adapters also pass structural comparison.
`stage_genexxa_performance.py` provides create, verify and a 20-clip/two-mesh
evaluated-pose check. Runtime integration is not yet installed.

That evaluation completed: 40 clip/mesh pairs, 360 cases and 29,278 full-motion
poses at 60 Hz. Body and finger position differences are zero; all comparison
cases pass. `genexxa-evaluate.json` contains the measurements and protected
asset hashes. This validates preview-world evaluation, not gameplay cloth,
native cancellation or performance cost.

The isolated Windows cook now passes container verification and exact export
round-trip. Independent readback matches the released GeneXXa graph's original
nodes, defaults and skeleton, with one playback insertion at the local-to-component
input. The five existing adapter comparisons also still pass. Deliberate bad
inputs, changed spring settings, wrong skeleton, active defaults and disabled
blend reset are rejected. The revision-bound catalog entry and receipts are in
`expanded-outfit-inventory/genexxa-cooked-probe-v1/verified-binding.json`.
The staged `shared-v1/pack-with-genexxa-v1` now includes that binding and its
adapter. All 68 pre-existing export payloads and all earlier catalog fields
remain unchanged. The pack has 57 sequences, six body adapters and six facial
meshes. Independent readback resolves it with each of the four outfit packages
installed alone; all four supply the same decoded shared skeleton. Host catalog
checks pass for the combined package set and each isolated outfit. Missing
outfits do not acquire adapters; GeneXXa has body playback metadata without
being incorrectly assigned an Eve facial adapter.

Evidence: `previous-pack-comparison.json`, `host-catalog-validation.json` and
`verification.json` in the staged pack; isolated decode results are in
`isolated-dependencies-genexxa-v1/report.json`. This is not installed and has no
new gameplay acceptance. The installed private trial still has five adapters.

Facial capability remains separate. Missing expression controls must not
prevent compatible body animation playback.

## Older Seductress adapter and bind poses

`stage_seductress_performance.py` creates an isolated `ABP_Seductress` adapter
from the original BodyHairV42 graph. UE 5.6.1 creation and fresh-process reload
both preserve all 27 CSS controls, 31 original graph nodes and the original
skeleton. Playback defaults remain inactive. The six original meshes still
reference their original graph; no source mesh, skeleton or released container
was changed. This is not yet a catalog binding or installation.

All six authoring meshes have identical 379-bone bind poses. Independent
MeshExport readback from their released containers confirms every name,
parent and local transform: maximum position difference is 0.00000361 cm,
maximum quaternion distance is 0.0000000555, and scales match exactly.
This distinguishes the actual mesh binding from the earlier skeleton-asset
comparison. Against current Black Pearl's fitted mesh, 50 local references
differ, with wrist translation differences around 17.73 cm and wrist rotations
around 50.34 degrees. Names and parent relationships match. Simply marking
these rigs compatible would not establish correct hand or foot motion.

Evidence under `work/cine-animation-port/expanded-outfit-inventory/`:
`seductress-create.json`, `seductress-verify.json`,
`seductress-mesh-reference-comparison.json`, and
`seductress-cooked-bind-verification.json`. Build, create and verify exit codes
are zero. The adapter still needs independent cooked graph comparison, clips
converted for this reference, full-motion checks and live acceptance.

Epic's [IK retargeting documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
describes explicit retarget poses and exported target sequences. That is the
appropriate next investigation here; the measured differences above are local
asset evidence, not a claim derived from matching skeleton labels.

### Direct Seductress motion auditions and cooked surface checks

Black Hole (601 keys), Catwalk (361 keys) and Come Closer (581 keys) now have
direct conversions from the decoded L2 source to the older Midas reference.
S2 uses FK; S3 adds source-height foot IK with offsets derived from the actual
source and target bind heights. Each retains the full original duration and
nominal 30 Hz sampling. Fresh-process saved and repeated conversions match at
every key, with zero position, quaternion and scale differences. The first S1
probe failed at the endpoint because float-derived sample times exceeded the
clip duration; subsequent probes use exact frame indices.

S3 also passes the performance insertion checks on all six original meshes:
18 clip/mesh pairs, 162 cases and 44,418 evaluated frames, including 18,498
full-clip frames at 60 Hz. All 38 finger bones pass, as do inactive, seek,
held-end, stop, missing-clip, restoration and zero-weight cases. These checks
compare the adapter against the original graph. They do not approve the motion
itself, customization extremes or world physics. The three isolated copies in
`/Game/CSS/Locomotion/Studies/SeductressS3/` have no runtime catalog binding.

A direct readback of the released Midas mesh preserves 1,777,508 vertices and
all eight possible influences. Two exports have identical geometry and weight
hashes. Neutral bind reconstruction passes; an independent Blender matrix
evaluation agrees with the twelve reported lowest vertices within 0.000904 cm.
The scan covers every exact source key and reports material sections separately.
It includes hidden alternatives, without morphs or post-process evaluation.

The S3 surface results still fail contact review. Worst flat-foot material
heights are -4.39 cm (Black Hole), -7.57 cm (Catwalk) and -4.18 cm (Come Closer).
Heel-foot material minima are -0.115, +1.740 and +0.239 cm respectively. Those
are different footwear sections, not interchangeable acceptance results.
Midas underskirt/overskirt sections have much larger excursions because they
also carry foot weights. Keep their garment investigation separate from sole
correction; no released weights have been edited.

Evidence: `retarget-seductress-s2/verify-result.json` and
`retarget-seductress-s3/{verify-result,adapter-evaluation,foot-height-comparison,
surface-contact-comparison,skin-matrix-crosscheck}.json`, plus
`retarget-seductress-s3/midas-skin-audit-v2/skin-audit.json` and its overwrite
guard receipt. The per-material full-frame result is
`retarget-seductress-s3/surface-contact-by-material.json`.
All paths are below `work/cine-animation-port/`.

Next: evaluate visible footwear choices through their original controls,
correct measured contacts without altering weights or performance, and inspect
the full rendered motion. Cooked adapter validation, remaining clips, catalog
integration and gameplay acceptance are still required.

### Visible footwear and knee-plane correction

The released six-outfit manifest and original BodyHairV42 graph now have direct
readback under `seductress-footwear-controls/` and `retarget-seductress-s5/`.
Heels change section visibility only. Neither original ControlRig output filter
contains calf, foot or ball bones. The visible heel-mode sections are 20, 25,
26 and 28; flat feet with stockings use 23, 24 and 27. All six variants share
that contract. Skirt sections 32 and 33 remain separate garment concerns.

S4 measured contact correction cleared visible footwear at authored keys, but
introduced knee-plane flips in Come Closer. S5 derives the pole direction from
the source hip-foot plane before projecting onto the corrected goal plane.
The six 30 Hz trials pass fresh-process saved/repeated comparison across all
388 tracks. Non-leg tracks remain exact and world foot XY/orientation remain
preserved within measurement precision. The newly introduced greater-than-90
degree knee-plane steps disappear; the original right-leg frame-24 step stays.
This does not establish smoothness or visual acceptance.

The 6,166-pose scan at twice the original key rate still finds interpolation
penetration: worst visible-footwear values are -0.336 cm for Black Hole,
-0.082 cm for Catwalk and -1.227 cm for Come Closer. These full-weight scans
remain neutral, without morphs, cloth or post-process. The next S6 trial
refines between-key contact at 60 Hz with the original playback speed, then
checks saved playback at 120 Hz. It is not installed or catalog-bound.

Evidence below `work/cine-animation-port/retarget-seductress-s5/`:
`footwear-contract.json`, `result.json`, `verify-result.json`,
`pose-preservation.json`, `dense-surface-contact.json`, and zero-exit build,
create and verify receipts. S4 failures remain in their original work folder.

S6 Come Closer Flat is now built and reopened successfully: 388 tracks and
1,161 keys match the repeated bake exactly. The original 19.333334-second
duration is unchanged. Its 2,321-pose 120 Hz scan reduces worst visible-footwear
penetration to 0.412 cm. Authored keys clear within 0.000018 cm; the remaining
error is between keys. Matched 60 Hz samples show no additional greater-than-
90-degree knee-plane steps. Non-leg pose differences from resampling stay below
0.001 per transform component, with the saved repeat itself exact. Knee travel
increases, so these measurements are not visual acceptance. Continue correction
and full-motion review. Evidence: `retarget-seductress-s6/{create-result,
verify-result,dense-surface-contact,visible-contact-summary,pose-preservation}.json`.

S7 continues this same isolated flat-foot trial at 120 Hz. Fresh readback and
repeat match all 900,548 authored bone keys. Its 4,641-pose 240 Hz scan reduces
the worst dip to 0.1444 cm; exactly one sample per foot remains below -0.1 cm.
No new knee-plane flips occur at matched 120 Hz samples. S6/S7 lower-body
60 Hz renders and sampled sheets use the original cooked weights with bare
leg/foot sections, without material masks or secondary motion. They support
the deformation investigation but do not approve the whole outfit. All
candidates remain uninstalled.

The denser import exposed the engine's float Euler storage conversion.
An independent native calculation exactly explains the worst observed rotation
difference (0.002581 degrees). See `CONTACT.md` for the bounded import check and
`retarget-seductress-s7/native-key-readback-audit.json` for engine source hashes.

S8 Come Closer Flat now passes fresh comparison for 4,641 keys at 240 Hz,
with original duration retained. All 9,281 poses in the 480 Hz scan remain
within the 0.1 cm contact diagnostic limit: worst dip 0.02607 cm left and
0.01731 cm right. Matched 240 Hz samples introduce no knee-plane flips.
The neutral lower-body render and sampled sheet show no obvious new collapse.
Its compressed component execution also passes all six original outfit graphs,
54 cases and 15,606 poses, including 38 fingers and distinct-base restoration.
See `retarget-seductress-s8/{verify-result,visible-contact-summary,
pose-preservation,adapter-evaluation}.json` and `bare-skin-motion/`.
This covers one corrected flat-foot clip, not all clips or footwear modes.
Windows cooking now accounts for the released legacy skeleton and retarget
source mesh through an explicit hashed external-rig manifest. The isolated
S8 shared-path candidate preserves the original authored tracks, removes only
its editor preview mesh and owns its compression settings. Fresh readback,
both Windows cooks, two-export container round-trip and independent decoding
of all 4,641 keys / 388 tracks pass. The private trio totals 1,645,446 bytes;
41 protected authoring inputs and released outfit containers are unchanged.
Evidence: `s8-shared-cook-v3/acceptance-checkpoint.json`. Subsequent full
raw/cooked comparison passes all 4,641 keys, maximum position error
0.003582 cm. Independent 480 Hz cooked sampling passes visible-foot contact:
worst penetration 0.025908 cm left and 0.017154 cm right. See
`s8-shared-cook-v3/{full-motion-comparison,cooked-visible-contact-summary}.json`.
The BodyHairV42 adapter also passes Windows cooking and independent comparison
against the released outfit graph, preserving the original secondary-motion
nodes and 23 serialized CSS defaults. Three legacy rig dependencies are hashed
external boundaries, not recooked assets. Evidence:
`seductress-adapter-cook-v2/probe/verified-binding.json`.
The broader clip set, footwear, full physics and live acceptance remain.
This isolated study has not been installed or added to the playable catalog.
