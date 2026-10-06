# Shared animation outfit coverage

October 6, 2026. This records inspected assets, not release acceptance.

Support includes the original eins0fx-authored CSS collection, excluding
port-only mods, and compatible default-human-skeleton appearances from other
authors. Neither an author name nor a CSS manifest should be a requirement for
the eventual native-human playback path. The initial nine meshes are not the
whole collection.

## Additional collection inventory

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
Windows cooking must account for the released legacy skeleton dependency;
compression/contact, package size, full physics and live acceptance remain.
