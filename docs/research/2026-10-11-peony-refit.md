# Peony fit correction after the October 11 gameplay review

The user rejected the installed trial's oversized upper garment, lower-front
clipping and distorted world-space hanging panels. The screenshots are linked
in the goal. Peony correction takes priority over continuing Slipstream binding.
No game package, core, save, profile or settings were changed in this work.

## Reused work and identified gap

The earlier Eve tools `fit_nextgen_garments.py`, `sync_garment_morphs.py` and
`fit_morph_clearance.py` separate fabric fitting, morph correspondence and
triangle-interior clearance. `fit_peony_upper.py` only expanded intersecting
fabric. It did not shrink an oversized exterior. Its chest follow-up was also
limited to nearby breast-morph support. Thus those checks did not establish
the close silhouette requested by the user.

The new authoring tool `refit_peony_upper.py` operates on the retained
`new-outfits-flow-uv1.blend`, using the accepted body and its six shape controls.
The jacket/corset main fabric receives inward and outward fitting; separate
trim follows fabric displacement. Small trim components translate together.
The body and skeleton remain unchanged. These are offline Blender 5.2.2 changes.

## Candidates and limitations

All paths below are under `CSS_SeduXtress_eins0fx/work`.

- `peony-closefit1`: the corset's neutral sampled vertex-to-body gap had a
  95th percentile of about 5.70 cm before refitting. The first inward fit reduced
  it to about 1.07 cm, but review renders exposed neckline/chest intersections.
  This candidate is rejected for installation.
- `peony-closefit2`: direct triangle-clearance pushes clear the sampled contacts
  but compress small folds and turn some normals by over 90 degrees. This is
  rejected, not a shipping repair.
- `peony-closefit3`: spatially smooth clearance corrections replace individual
  pushes. All seven recorded shape cases reach zero failures in their selected
  upper-fabric clearance samples. Small compressed folds and normal rotations
  remain, so this is not evidence of complete geometric or gameplay acceptance.
- `peony-closefit4`: adds the requested sweetheart neckline with a 2.5 cm
  authoring depth parameter. Front/side/back review images are retained.
  Lower-front fitting is being investigated separately. Three lower projection
  variants exceed the 8 cm deformation guard and are rejected; their logs and
  recipes remain. Restricting the body material region or fitting fabric before
  its trim does not resolve the failure. Do not import their output or loosen
  the guard.

- `peony-closefit5`: responds to the user's request for more cleavage with a
  4 cm neckline-depth parameter and 6 cm Gaussian width parameter. These are
  authoring parameters, not measurements of the final opening. Neutral review
  shows the deeper sweetheart shape. The breast-size endpoint keeps the center
  opening, but its underbust fabric and trim still need repair. All seven
  recorded shape cases clear the fitter's selected upper samples; this does not
  establish clean topology, trim fit or gameplay acceptance. Scripts, hashes,
  neutral renders and breast-size endpoint renders are retained in its
  `work-receipt.json`. This candidate is not cooked or installed.

## Lower-front cause and recovery

The initial lower diagnostic localized outward drift to vertex 141329. Its
initial body gap was 0.524 cm, yet clearance iterations moved it over 20 cm.
The neighboring triangle interiors really passed through the thigh: full-body
solid-angle winding measured approximately 1 at those interior samples.
The donor was not a hand, and restricting the body region did not fix it.

`work/peony-lower-diagnostic1/topology.log` compares the preserved source and
bound geometry at corresponding vertices. The old `CSS_LowerClearance` radial
fit stretched ordinary 1–2 cm fabric edges into 16–20 cm edges across the leg.
These are visible garment faces, not internal closure caps. The underwear had
258 edges stretched more than threefold relative to joint-aligned geometry,
with a maximum ratio of 23.07. Pushing their corners farther outward was the
wrong repair. This establishes an authoring defect, not a CSS runtime cause.

`refit_peony_lower_surface.py` now recovers the underwear and its trim from
the retained `CSS_JointAlignment` shape before that radial fitting step. It
builds fresh body-morph correspondences, fits the fabric with a smooth
displacement field and triangle-clearance checks, and transfers body skin
weights. Original source, accepted body, other garment parts and cloth points
remain unchanged. Unresolved selected contact samples or fabric triangles
turning over 90 degrees reject the candidate.

`peony-lower-recovery1` exposed two small front holes in its review render.
The full-body comparison identified an omitted body section, `Eve Genitals`.
`peony-lower-recovery2` includes that section in its body surface:

- All seven individual cases finish with zero selected fabric-contact failures
  and zero over-90-degree fabric normal rotations. Minimum triangle area ratio
  is 0.239 relative to the recovered aligned fabric.
- Maximum neutral fabric edge length is 3.51 cm, down from 20.17 cm. Maximum
  edge stretch relative to aligned source is 1.52, with no edges over threefold.
- `preservation.json` verifies unchanged body, other parts, skeleton, topology,
  UVs and unrelated morph deltas. Transferred weights are normalized; at most
  floating-point noise was discarded by the eight-influence export check.
- Neutral, combined lower-body sliders at 1, and recorded walk frame 36 have
  retained front/back/side/quarter review renders. The repaired front fabric
  stays intact in these views. The first walking render used nonexistent index
  145 of a 145-frame capture and stopped; frame 36 was then rendered successfully.

These are offline checks, not continuous gameplay acceptance. The review uses
neutral opaque materials and does not reproduce the garment's alpha masks.
Visible overlap in other sections must be checked with their actual materials.
Separate trim has internal contacts not covered by the primary-fabric pass;
do not describe every garment triangle as collision-free.

The two main hanging-panel components do not exhibit the same extreme edge
stretch. Their geometry was not changed by the underwear recovery. The world
report still needs comparison with the new package before attributing all of
the reported ragged appearance to cloth simulation.

## Upper trim follow-up

`peony-closefit6` carries the deeper neckline and recovered lower garment. Upper
trim now uses fixed neutral fabric donors across body shapes and transports its
surface frame; small hardware rotates with the fabric rather than only moving
its center. Neutral and maximum-breast renders are retained. The large-bust
underbust result is still not accepted: a targeted body-contact probe finds
small contacts on components 70/71, and trim-to-fabric appearance needs work.

The later isolated diffuse renders identify components 70/71 as ordinary round
decorative buttons, not robotic ornaments. Their measured contacts include
embedded backs. The maximum-body monochrome preview shows much of the apparent
underbust break is trim occluded by the larger cup shape. This narrows the
remaining review; it does not establish continuous-motion clearance. A denser
primary-fabric probe also includes internal caps excluded by the original
fitter, so its raw contact count must not be treated as a visible-defect count.

`cloth-sprint78` replays the retained two-panel geometry composite against the
repaired assembly. The source composite uses matching body poses from separate
full-component runs, not a simultaneous game render. It preserves a useful
cloth/garment comparison without proving current gameplay behavior.

## Remaining mechanical fittings and native rebuild

The isolated textured component renders in `peony-decoration-review1` identify
additional mechanical rods and clips already merged into the two panel slots.
`strip_peony_panel_hardware.py` removes these source components:

- Arm: 26, 27.
- Lower 1: 35, 41, 46, 50, 62, 67.
- Lower 2: 7, 8, 25, 26, 27, 28.

`peony-strip1/assembly` combines the closefit6 upper, recovered lower garment
and this removal. Its mesh SHA-256 is
`ec30879a114e39170178982c1ceb251fe7bd2af090dcf8d88bb36d662ff41577`.
The exact comparison in `preservation.json` verifies removal of 692 triangles
from panel material slots 34/38, with every other interchange field unchanged.
This preserves the actual fabric, point IDs, skeleton, UVs, weights and morphs.
It is additional to the earlier removal of 11,430 ornament/pocket triangles.

The regenerated wardrobe fragment has 20 controls and five palettes. Its
comparison matches the prior fragment exactly after removing the two ornament
toggles and color bindings to their four empty slots. The initial comparison
omitted the expected empty-slot binding removal and failed; the subsequent
comparison accounts for those specific changes, not arbitrary differences.

`peony-refitnative1` stages this mesh with 115 hash-verified dependencies from
the retained native project. Import and material/skeleton/physics binding,
cloth rebuilding and facial assembly have completed. Native verification shows:

- Each panel loses exactly 346 reviewed hardware triangles, leaving 2,319
  triangles and 1,578 render vertices. The retained oriented triangles and
  combined position/weight/mapping/morph records match the previous build.
- Simulation particles, maps, settings, physics references and the 115 staged
  dependencies are unchanged. New cloth GUIDs and buffer offsets are expected.
- Normal and facial cloth inspections match, excluding the mesh asset name.
- All 115 facial shapes and 22 authored morphs survive fresh loading. The
  authored morph check reads 97,990 built deltas on each mesh; deliberately
  changing an expected delta by 0.1 cm is rejected on both.

`native-verification.json` and `study-receipt.json` record the scope and hashes.
`peony-clothmorph2` regenerates the repaired meshes' cloth body-morph buffers
using the retained shared deformer. Authoring and fresh reload both pass: each
mesh has two cloth sections and 15 enabled buffer attributes, with all existing
mesh/rig/morph/cloth data preserved. The shared graph and 146 protected inputs
are unchanged. Its `validation-receipt.json` freezes the results and recipes.
GPU validation, Windows cooking, material review and gameplay remain pending.
No replacement is installed.

## Rendering and cook preparation

`peony-clothgpu2` stages the repaired cloth-buffer meshes for the established
44-capture comparison (11 body/cloth cases on each of the normal and facial
meshes, stock and trial deformers on the same held simulation). All 44 captures
and 22 comparisons pass, including two negative controls. Maximum cloth-offset
residual is 0.0150 cm. The completed 408-file validation receipt freezes this
evidence. This stage predates the stocking repair and does not validate that
newer mesh; its refreshed native/GPU build is still required.

`peony-cookprep2` copies the checked meshes to their canonical Peony and facial
asset paths. Copying and fresh reload preserve the inspected deformation data.
Dependency closure matches the retained candidate exactly, with only the two
repaired mesh package hashes changed in the ownership plan. The Windows cook tool now
has a separate `peony-windows5` candidate that requires these new preparation
and GPU results. The retained earlier candidates keep their original inputs.
No Windows cook has been started for the repaired mesh yet.

The repaired Windows candidate seeds its private DDC from the last successful
Peony Windows cook after checking the engine version and completion receipt.
The cache is copied locally before starting the new cook; retained cache files
are not modified. This keeps derived-data reuse separate from asset validation,
consistent with [Epic's UE 5.6 DDC documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine?application_version=5.6).

Four rejected/superseded fitting candidates, `peony-closefit1` through `4`,
were archived under `/run/media/eins0fx/theark/CSS/authoring-rejected/peony-20261011`.
Every file was hash-checked before removing its local copy. Original work paths
remain as symlinks, preserving references. This freed 523,540,893 bytes on the
system drive. `work/peony-refit-archive-20261011.json` records the copies and hashes.

`assemble_peony_refit.py` applies only jacket/corset coordinates and their six
morph endpoints to the retained `peony-hipfit11` interchange. The collar stays
unchanged. It removes 11,430 faces belonging to both hip-accessory and arm-ornament
groups. Body, hair, heels, gloves, skin weights and cloth point identities remain
unchanged by that assembly step. The lower-fit tool has a separate, unaccepted
weight-transfer experiment; do not confuse the two scopes.

## Customization controls

`build_peony_customize.py` recognizes either the original complete wardrobe or
the complete removal of the four pocket/arm-ornament sections. Partial removals
are rejected. Removed parts no longer produce visibility controls or parameter
bindings. The original recipe regenerates exactly unchanged; the candidate has
20 wardrobe controls instead of 22, retaining five palettes and identical
glove, collar, boot and leg-panel visibility definitions. The existing body/hair
controls are merged separately as before.

Verification: `peony-closefit3/control-check.json` records the original-recipe
comparison, removed control IDs, preserved controls and partial-removal negative
case. This verifies the builder only, not a rebuilt catalog or runtime package.

The repaired merged catalog is now built in `peony-catalog5` from the stripped
mesh and regenerated wardrobe fragment. Its 38 controls, nine dye surfaces,
five palettes and 106 dye resources retain the six existing Eve variants
unchanged. The native catalog check passes 512 visibility/boot-height
combinations and 2,560 palette/profile cases, including all 17 inherited body
and physics controls. The old 40-control catalog also still passes its 2,048
visibility and 10,240 palette/profile cases. A deliberately partial removal of
the ornament controls is rejected. `native-checks.json` records the results,
catalog hashes, checker source and linked data library.

The private assembly tool now has an explicit repair mode. It verifies the
installed combined3 container identities, replaces Peony's definition, updates
Eve's container revision and retains the existing Peony facial mapping rather
than adding a duplicate. Readback and deformation checks select the repaired
GPU reference for `peony-windows5`. These paths are prepared and syntax-checked;
the repaired cook and combined package have not run yet.

## Remaining work

The user's later reminder explicitly rejects visible gaps at the breasts and
buttocks. Installed hashes still match `peony-combined3`, including its unchanged
core; none of the refitting candidates below has been installed.

The closefit6 upper was screened at all-half, all-max, breast-plus-waist and
breast-plus-tone settings. Each case samples 55,181 exterior-fabric locations
using the fitter's original donor regions and internal-cap exclusions. Two to
four remaining contacts lie behind another garment surface on all four tested
cardinal rays. The matching breast-plus-tone renders and maximum-body palette
preview do not show skin breaking through at those witnesses. This is limited
reference-pose evidence, not continuous-motion clearance or final fit approval.
See `peony-closefit6/combined-contact-rays.json` and
`peony-strip1/breasts-tone`.

A separate visible jagged thigh patch was confirmed as skin in front of the
stocking by 0.0192642 cm, at the recorded front-view pixel `(230, 810)`.
The intersected material is `Peony_stockings_Stockings`, not the recovered
underwear or simulated panels. Two earlier nearby pixel probes hit covered
stocking surface, so they did not diagnose that hole.

`peony-stockingfit6` is now the latest interchange candidate. It corrects only
the exterior upper-stocking region, transfers its six body morphs and animation
weights from the corrected neutral body's surface correspondence, and updates
the affected corner normals. Original deep internal caps are excluded from the
exterior screen. Initial direct corrections distorted triangles; a smooth field
with projection onto the remaining clearance constraints avoided that result.
The fourth attempt exposed a glute-endpoint mismatch in the old stocking morph,
which the matched body-surface morph transfer addresses.

Its `verification.json` records 2,541 vertices in the edited region and 15,836
normal corners. Body/hair/other garment and cloth points, skeleton, topology,
UVs, materials and unrelated morph/weight data remain unchanged. Positions and
morphs exactly match the fifth candidate, whose independent nine-case screen
has no penetration on the fixed exterior face set; its smallest measured gap
is 0.0586017 cm at all-max. The remainder of the stocking screen still includes
internal caps and is not reported as globally intersection-free. The corrected
front render removes the witnessed jagged patch. A retained sprint-pose/cloth
composite is available, but it is not an in-game or full-range motion test.

The earlier native authoring and completed GPU job described above target
`peony-strip1`, before this additional stocking correction. They must not be
used as proof that stockingfit6 has been imported or cooked. Rebuild both normal
and facial meshes from the latest interchange, retain the verified cloth data,
and refresh the corresponding native/GPU/cooked evidence. Then verify the final
catalog/package and perform gameplay comparison. Keep all
previously working Eve variants, Genessa, Commander White, action interruption,
facial playback, doubles and saved selections in the regression scope.

### Latest stocking repair imported

`peony-refitnative2` now imports the exact `peony-stockingfit6` export
(`30393a258b744cca662e108ccb6bbb3790dbc0978afc15fc7a68ca7cc861e605`).
Its rebuilt panel geometry, weights, mappings, morphs and simulation data match
`peony-refitnative1`; only newly generated clothing GUIDs differ. Both GUIDs are
valid and distinct. Normal/facial base data match, and fresh expression reload
retains all 115 expressions alongside the 22 authored shapes.

Fresh native readback verifies 97,992 built authored deltas on both meshes,
with maximum retained error 0.000007704 cm. Both altered-delta negative controls
are rejected. `source-receipt.json` ties the import to the exact fitting inputs;
`native-verification.json` and the 169-file `study-receipt.json` freeze this
result. Native success is not cooked or gameplay acceptance.

`peony-catalog6` is generated against the latest mesh. Its catalog is identical
to the checked 38-control catalog5, and its native checker passes the same 512
visibility/height and 2,560 palette/profile cases. The accepted body, six prior
variants, nine dye surfaces and five Peony palettes remain intact.

The refreshed `peony-clothmorph3` passes authoring and fresh reload on both
meshes: 15 enabled render attributes per mesh, two cloth sections and unchanged
existing data, protected dependencies and shared graph. Its 165-file receipt
freezes those checks. The refreshed `peony-clothgpu3` now passes all 44 captures
and 22 comparisons, including both negative controls; its largest residual is
0.0149969 cm and its receipt freezes 408 files. The copied shader cache avoided
repeating the earlier cold-cache startup. Canonical preparation passes in `peony-cookprep3`, with the same dependency
closure and only the normal/facial mesh package hashes changed.
`peony-windows5` uses these refreshed stages, preserving the exact latest
stocking geometry through its verified native source. The prior completed GPU cache is copied into
the new private stage for reuse, separately from asset validation.

### Replacement trial installed

The first cook preflight rejected a changed GPU log. All other evidence was
unchanged. An exact prefix-hash check proves the original 190,210 log bytes were
unchanged; UnrealTraceServer appended 774 bytes of startup/shutdown text after
the commandlet exited. `stabilize_receipts.py` preserves the exact receipt-time
log bytes as immutable snapshots, archives the previous receipts, and records
the appended text. Canonical preparation logs are snapshotted too. No mesh,
readback, capture or comparison evidence was replaced to bypass this check.

The repeated `peony-windows5` preflight passes. Windows UE 5.6.1, changelist
44394996, completes the cook in 74.57 seconds with all protected inputs unchanged.
Cooked readback passes for both meshes and 12 normal/Astral shader materials,
including SM5/SM6 skin and cloth shaders. Both missing-buffer negative controls
are rejected; maximum cooked cloth-offset residual is 0.0149884 cm. The stocking
repair does not increase render vertex count or the skin-weight limit: both
prior and new meshes have 144,936 render vertices and at most eight influences.
This is a mesh-cost observation, not a frame-time claim.

`CustomShellSystem/work/three-outfit-rig-physics/peony-refit1` points to the
assembled trial on E:. All 642 legacy assets round-trip, 657 package checks have
zero dependency failures, and 1,535 native animation-library checks pass. The
six prior Eve variants, existing CW/Genessa adapters and facial performance
resources remain unchanged. No additional Peony adapter is appended.

The game was verified closed. The new Eve/shared package trios were installed
and checked against their staged hashes. The existing core remains
`css_core-peony-6d751cbdab2cab4c.dll`. Rollback copies are under
`/run/media/eins0fx/theark/CSS/peony-before-refit1`; `deployment.json` and
`installation-check.json` record the actual installed/backup identities.
The user has been asked to relaunch and check chest/hip fit, moving panels,
customization and Iris Out interruption. Player acceptance, full gameplay
regressions and performance remain pending. This installation does not complete
the Peony repair or the larger three-outfit goal.


Cleanup after the completed GPU runs removed 4,528 byte-identical derived-data
files from the superseded GPU2 cache, retaining each verified copy in GPU3.
This removed 134,191,539 logical bytes, with zero immediate available-space
change observed. The process check found no active user of the old cache.
`work/peony-gpu-cache-duplicates-20261011.json` retains the exact paths, hashes
and measurement. Source assets, captures, packages and rollback were untouched.


## Installed refit rejected: lower gap and world distortion

The user rejected `peony-refit1` after testing. Rear/side screenshots show a
large gap beneath the black lower straps, and a separate world screenshot shows
a folded/distorted lower panel during movement. The existing acceptance remains
open. The attachment-cap change is authorized if valid outfits require it, but
is queued behind this fit repair.

`peony-lower-gap1/audit.json` measures the unchanged installed-source geometry.
Outward-facing upper-stocking triangle centers reach 5.943 cm from the retained
body. The previous `peony-stockingfit6` was a local outward clearance repair and
did not address these oversized regions. Its local clearance result must not be
used as evidence of a fitted overall silhouette.

The first inward candidate is rejected: it closes much of the neutral gap but
its nine-case screen detects exterior contacts, including a neutral depth of
0.799 cm. Nothing from that candidate is installed.

The preserved source comparison also identifies damage from the earlier radial
fit in the stocking, not only the underwear recovered previously. For example,
face 186508 originally has edges of 0.988, 0.881 and 0.529 cm. Two edges in the
first inward candidate remain roughly 8.8 and 8.6 cm. Source and fitted vertex
positions are retained in `peony-lower-gap1/source-check.json`. Recovery must
start before that radial distortion. The loose-panel simulation still needs a
separate runtime diagnosis; this finding does not establish its cause.

A read-only player probe timed out, and the following process check found no
running game process. No live settings or character controls were changed.


The user clarified that the lower garment is detached while stationary as well
as during movement. No further user animation comparison is needed to establish
that symptom. The neutral proxy audit confirms that the panel's existing fixed
particles are roughly 0.54..3.05 cm from the body (`panel-anchor-audit.json`).

The new private study is `peony-lower-gap1`. Candidate 4 recovers the stockings
before the distorted radial fit, closes their oversized regions, transfers body
morphs/weights and resolves local surface intersections. Its independent screen
samples about 59,000 points in each of nine body cases: no sampled stocking
penetration, minimum 0.0888 cm in the combined maximum case. Crotch bridging
still contributes to the raw maximum surface distance; this is not proof of
an all-angle textured fit. Candidate 8 fits the lower straps as a separate layer;
its nine-case exterior screen passes, while buried trim backs retain roughly
0.2 cm contacts. Those contacts are not silently counted as passed.

The upper-panel frame-transport trial (candidate 5) is rejected because it
introduces visibly jagged trim. Candidates 1, 2, 5 and 6 are archived with
per-file hash verification under theark's `CSS/authoring-rejected/peony-lower-gap1`.
Original work paths are symlinks. Logical archived size is 536,180,531 bytes.

Candidate 10 combines the fitted lower surfaces with a closer upper-panel rest
shape, body-matched attachment weights/morphs and a tapered attachment band.
Pins increase from 16 per panel to 25 left and 24 right, with the lower panels
still simulated. This addresses the fixed/free transition that failed the prior
`peony-panelattachmentfit2` experiment; native motion must validate it, so it is
not accepted yet. The selected old rejected panel fit was not copied into this
candidate. Exact interchange verification preserves the body, skeleton, other
garments, topology and unrelated data. Floating-point noise in untouched morph
rows was restored to the original values before staging.

`peony-refitnative3` stages 115 verified dependencies and is the native check for
this candidate. Do not install it or claim gameplay success before its native
cloth, morph, facial, cook and package checks pass. The installed game remains
on rejected `peony-refit1` during this work.

### Closer attachment exposes an oversized hip collider

Native import, binding, cloth rebuilding and inspection complete successfully
for `peony-refitnative3`. All six saved-setting motion screens fail near the
new attachment band, so this candidate is not ready to install. Failed frame
counts are 111/41/25 for left walk/jog/sprint and 130/27/13 for the right.

The left walking controls use the same saved mesh and restore temporary
settings after each capture. Disabling body collision gives zero gross-strain
frames; disabling self-collision leaves 114, and raising iterations to 12
leaves 35. These are diagnostic controls only. Neither collision removal nor
the higher iteration count is adopted.

The native contact trace at walking frame 31 identifies the upper-thigh tapered
capsule. Pinned particle 64 is 1.906 cm inside it. Its free neighbor 65 has a
skinned target 2.630 cm inside it but gets pushed to 0.306 cm outside. Their
1.648 cm rest edge stretches to about 3.76 cm. This directly explains the
fixed/free conflict in this trial.

`fit_hip_collision.py` bounds only the two upper-thigh endpoint radii against
the retained neutral body; unrelated collision geometry is preserved. The
existing radii are approximately 12.15 cm, while the measured nearest body
surfaces are 7.26 and 7.07 cm from their centers. Separate `PA_LeftRefitHip` and
`PA_RightRefitHip` candidates use 6.91 and 6.72 cm radii, including a 0.35 cm
margin for the 0.3 cm collision thickness. This is an endpoint containment
measurement, not proof of full animated coverage. The six-gait collision
override trial must pass and its mapped garment appearance must be reviewed
before binding these assets into a new saved candidate. Full body and
self-collision remain enabled in that trial. Installed files remain unchanged.

The fitted-hip override passes five of six independently verified gait screens.
Right sprint fails one frame (46), at free lower-hem particle 466 rather than
the hip attachment. A repeat with contact tracing does not reproduce this
single-frame failure. Eight iterations also pass that comparison; CCD fails
six frames and is rejected. Timing from these captures overlaps Blender work
and must not be used for a performance comparison.

The mapped two-panel reviews at walk frame 31 and sprint frame 78 show the
closer hip fit, but the loose fabric still bunches excessively. A strain pass
alone would not make that appearance acceptable. `peony-refitnative4` copies
the exact candidate mesh and dependencies, binds the fitted hip collision, and
tests bending stiffness 0.65 against the previous 0.15. Four iterations, two
substeps, animation drive and body/self-collision remain unchanged. This private
fabric comparison is not an installed fix. Its saved-settings motion and visual
checks are pending.

`peony-refitnative4` preserves inspected geometry, maps, morphs, rig bindings
and unrelated staged files exactly. Its firmer fabric reduces the bunching in
the mapped walking view, but the four-iteration right sprint still fails one
frame (142). The eight-iteration trial then passes all eight anchor/strain
checks: six gaits plus two extra right-sprint runs, each 145 frames at 60 Hz.
Body collision and self-collision stay enabled. These results are retained in
`iter8-summary.json`; they are not a game-performance or textured-fit claim.

The editor command's `iter8.exit.json` records return code zero. The outer tool
session later reports 143 while inherited UnrealTraceServer output reports its
normal sponsor shutdown. Keep both facts; do not replace or rewrite the exit
evidence. The completed captures independently pass their verifiers.

`peony-refitnative5` now saves eight iterations with the fitted collision and
firmer fabric. Its saved-setting checks run without diagnostic overrides.
`check_preservation.py` checks the intentional iteration change, and
`build_face.py` is prepared to rebuild/reload 115 expressions plus all 22
authored morphs only after the saved motion screen passes. That facial step
has not run yet. Cloth-buffer authoring, GPU checks, Windows cooking, packaging
and installation remain after native acceptance. The installed game files and
CSS core are still unchanged.

The saved `peony-refitnative5` candidate passes all six 145-frame gait screens
without overrides. Its preservation check passes for 123 protected files and
the retained geometry, maps, morphs and rig data. Fresh editor readback confirms
both panels use bending 0.65, eight iterations, two substeps, self-collision and
their fitted hip physics assets. The first settings probe used Python aliases
unavailable on the base-class wrapper and failed before changing assets.
`author_face.failed1.py` and its log are retained. The retry uses the exact
reflected UE property names, as supported by local 5.6.1 `PyWrapperObject.cpp`.
It passes and has authored the facial duplicate. Fresh facial reload and all
22 authored-morph readbacks are still running through `build_face.py`; inspect
`face-verification.json` before treating that phase as finished.

The current orchestration is `peony-refitnative5/build_face.py`, logging to
`/tmp/css-peony-native5-face-retry.log`. The mapped saved-candidate reviews are
generated by `review.py`. Do not redo the successful fitting or collision
isolation. After native acceptance, continue with the existing cloth-buffer,
GPU, Windows cook/readback and assembly pipeline using new output directories.
No candidate from this follow-up has been installed.

The native phase is now complete: `face-verification.json` passes, fresh reload
retains all 137 morphs, all 22 authored shapes pass on both meshes, changed-delta
negative controls are rejected, and normal/facial cloth inspection is equal.
All successful commandlets exit zero. `finalize.py` freezes
`native-verification.json` and a 200-file `study-receipt.json`; raw logs remain
separate because UnrealTraceServer can append after editor completion.

For the next cloth-buffer/cook stages, explicitly include the new dependencies
`/Game/CSS/eins0fx/EveSB/Peony/Physics/PA_LeftRefitHip` and `PA_RightRefitHip`.
The older clothmorph staging input list does not contain these new assets.
Continue from native5, keeping the accepted shared graph and catalog controls.
Do not reuse the old GPU/cooked deformation proof for changed panel geometry.
The two mapped saved-candidate views are diagnostic, without final materials;
they are not player acceptance. All follow-up installation remains pending.

The native5 follow-up now passes cloth-basis authoring and fresh reload in
`peony-clothmorph4`. `peony-clothgpu4` passes 44 captures, 22 comparisons
and two negative controls, with maximum residual 0.0149969 cm. Its frozen
receipt covers 408 files. Canonical preparation uses `peony-cookprep4` and
explicitly replaces the two old hip physics dependencies with the fitted
left/right assets; no runtime or catalog change is required. Raw editor logs
are retained separately from immutable snapshots to avoid late trace-server
appends invalidating receipts. Windows6 and refit2 support is prepared in the
existing package tools. No follow-up candidate is installed at this point.

Canonical preparation passes with exactly the expected four changed packages
and unchanged ownership counts. A matched four-run cloth timing comparison
(4, 8, 8, 4 iterations; 240 measured frames after 30 warmup frames per run)
measures medians 1.521/2.068/2.098/1.514 ms and p95 values
2.039/2.595/2.743/2.062 ms. The mean-of-run-medians increase is 0.566 ms.
These measure the complete component clothing tick plus its completion wait
in the Linux editor, excluding animation refresh and rendering. No other
authoring editor or Blender process ran during these captures. They do not
measure game frame time or establish an FPS guarantee. The bounded additional
cloth cost is acceptable for the private fitted trial because eight iterations
removed the observed sprint strain failure; release performance acceptance
still requires live measurement. `peony-refitnative5/cost-summary.json` retains
all four capture hashes and the scope. Windows6 cooking is now running.

### Lower-fit replacement installed

`peony-windows6` cooks successfully in 80.0 seconds with inputs unchanged.
Cooked readback passes for both meshes and 12 shader materials, with two
negative controls and a maximum cloth-buffer residual of 0.0149885 cm.
`fitted-physics-check.json` confirms the fitted hip endpoints, bending 0.65,
eight iterations, two substeps and self-collision survive the Windows cook
on both normal and facial meshes.

`peony-refit2` assembles successfully with 642 uniquely owned assets and
passes combined imports, catalog behavior, unchanged six prior Eve variants,
CW/Genessa adapters and facial-curve preservation checks. It was installed
with Mortal Shell II closed. `deployment.json` records verified installed
hashes and rollback at `/run/media/eins0fx/theark/CSS/peony-before-refit2`.
The CSS core remains `6d751cbdab2cab4c`; no save or user setting was edited.
The user can now relaunch this candidate. Live lower-fit acceptance, body-slider
checks, gameplay and performance regression remain pending. Do not call the
broader three-outfit goal or Peony release complete from this installation.
