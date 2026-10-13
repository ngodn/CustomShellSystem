# Three-outfit rig and physics research index

Updated 2026-10-14. The goal is active. See [implementation status](../development/three-outfit-rig-physics-status.md) for the baseline and migration audit, and the [active goal](../development/three-outfit-rig-physics-goal.md) for the latest trials.

CW's [swept-contact follow-up](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#swept-contact-and-joint-feasibility-october-14)
finds a demonstrated entry-plane/length/cone conflict at the first failed step.
A joint static solve clears the tested centerlines but still jumps; direct
swept-path solves also fail. Actual posed-body witnesses separate real calf
contact from excess proxy volume. Inspect the implicated supports before another
solver candidate. All 142 protected assets remain unchanged, with no new rig,
cook or install. Full stable-release gates remain active.

CW's [internal contact trace](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#internal-contact-trace-and-rejected-corrections)
reproduces the existing poses at 60 Hz and locates the calf failure at the
120 Hz solver rate. Coupled projection helps one frozen pose but fails motion;
a rest-direction restart clears sampled centerlines but still jumps. Both
remain offline experiments. Typed traces preserve full numeric arrays and
142 pinned inputs. Next investigate swept contact and approach-side continuity.
No new rig, cooked package or installation; all release gates remain active.

CW's [distal-contact follow-up](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#blended-waist-and-distal-contact)
completes 1,452 Belt13 surface cases. Belt14's 60-degree front cone clears the
tested sprint calf intersection but leaves 14.5 mm jog overlap; waist contact
also remains incomplete. Motion/reset checks and fresh reload pass. A sampled
clearer jog direction exists inside the current cone, directing the next work
to contact selection and continuity. No belt candidate is installed or accepted.
The exact-body blended-waist projection helps neutral poses, while the coarse
envelopes and their affine refits fail. Those experiments are retained separately
from runtime implementation. Full stable-release gates remain active.

CW's [moving front attachment](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#moving-front-attachment)
is integrated in private Belt13. Both normal/facial walking/reset checks and six
paired gait checks pass. Four sprint views improve the exposed buckle, but
blended waist witnesses still overlap by up to 10.86 mm with enlarged body
controls. The initial root footprint omits those vertices. Next account for
their blended weights and recheck contact, shape and cost. No installation or
full-fit acceptance; all stable-release gates remain active.

CW's [gradual waist weights](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#gradual-waist-weights)
repair the sharp skin-weight boundary implicated in root-translation stretch.
Private normal/facial meshes pass exact correspondence, preservation and fresh
reload checks. The four-pose counterfactual improves neutral contact and waist
strain, but maximum-body Sprint 24 retains 7.92 mm front overlap. Eight neutral
views reviewed; no belt installation or full-fit acceptance. Next integrate
the moving root in the actual chain simulation and repeat the recorded tests.

CW's [segment-contact study](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/belt-and-skirt-motion.md#segment-contact-and-front-attachment)
identifies and improves an endpoint-only collision miss in the jog capture.
Belt12 passes six paired normal/facial gait constraint checks, but sprint's
front attachment and enlarged-body rear contact still fail. Four diagnostic
renders confirm the front attachment crossing. Moving that root also deforms
the waist band, so attachment repair remains necessary. No belt candidate is
installed. Earlier full-frame and body-corner screens remain scoped to their
recorded candidates, not automatic acceptance of Belt12.

**Current scope:** complete the coordinated stable release: CSS v1.0.0,
Eve v1.5.0, Unholy Genessa v0.3.0 and Commander White v0.1.0, with matching
shared assets. The user corrected the erroneous Peony-only beta.12 delivery.
Those archives are withdrawn packaging evidence; nothing was uploaded.
See [release gates](../development/three-outfit-release-gates.md). Preserve
accepted Peony and keep Slipstream deferred until the user requests it again.

The [older Eve motion-input check](2026-10-12-eve-existing-motion-regression.md)
passes all 36 normal/facial mesh and frame-rate cases using the accepted tighter
rig and each saved secondary graph. Broader visual/gameplay/cloth checks remain.

The [public upgrade follow-up](2026-10-12-peony-public-upgrade.md) checks the new
shared pack against the actual published Genessa and Commander White packages.
All four cooked-import and native-catalog cases pass. The older revisions do
not gain the newer trial facial/playback adapters. Their live behavior remains
outside this offline check.

Latest status takes precedence over the historical trial entries below: Peony
V2 fit, flowing trim, facial playback, current-form doubles and customization
restoration have user acceptance. The tighter body-motion package is installed
with verified hashes and rollback. The user accepts Peony's Firm/Natural motion
while walking and playing Iris Out; live rig readback confirms the new limits.
Other Eve variants and broader regression remain open. See the
[active goal](../development/three-outfit-rig-physics-goal.md) for exact scope.
The [sunlight noise investigation](2026-10-12-sunlight-noise.md) now includes the
user's reproduced symptom and an installed optional MISC lighting control:
Default (game) or Reduced noise. UI switching, persistence and
[cold-load recovery](2026-10-13-sunlight-load-recovery.md) pass their recorded
checks. Reduced noise was left enabled at the last live check. Cross-map,
continuous-motion and performance acceptance remain open; this is not a claim
that the reported game rendering defect is fully fixed.

## Historical research checkpoints

The following entries retain their findings and original next steps. Their
pending Peony states and Slipstream instructions are superseded by the current
scope and acceptance above; they do not reopen completed work.

Slipstream's [continuous-cloth constraint isolation](2026-10-12-slipstream-constraint-isolation.md)
compares two batches with reversed order and identical animated poses. Removing
backstops or self-collision passes this stretch screen in both; normal simulation
still fails and varies between runs. No collision removal is accepted for shipping,
and no new package is installed.

The [combined V2 package](2026-10-12-peony-v2-package.md) now passes native
catalog, dependency and payload readback checks. CSS's facial namespace check
supports outfit-local replacement meshes. The
[V2 astral successor](2026-10-12-peony-v2-astral.md), `peony-v2-package3`, is now
installed with matching core and rollback. User visual acceptance and runtime
regression remain pending.

**Current priority:** the user retired the Better Cleavage source and selected
the [Peony/Hydrangea V2 CNS and recolor sources](2026-10-11-peony-v2-source.md).
All supplied color schemes are required, with black-and-white floral Hydrangea
as the default. Installed Peony refit2 is rejected, including its upper garment
and lower fit. Older trial notes below do not establish acceptance. Rebuild from
V2 while preserving the accepted body and requested customization/removals.

The newest [V2 binding study](2026-10-12-peony-v2-binding.md) retains a measured
knuckle-patch correction and creates a private copy with the 379-bone body rig
and six body controls. Source-preservation, saved correspondence and independent
pose-skinning checks pass. Neutral, maximum-body, walking and sprint views reveal
remaining rear collar, footwear and detailed seam/morph work. Continue from
`binding2` for those repairs; it is not an installed or accepted release.

The lower-garment V2 baseline is `calf-surface2`, built on `thigh-surface3`
and `waist-layers3`.
The waist correction removes the visible waistband/corset overlap. The thigh
correction follows the leg more closely after switching to exterior fabric
donors where source skin is absent. The upper calf now fits while retaining
the original heel junction. Neutral views and saved-file preservation pass for
these limited changes. Full-mesh contact still fails outside those regions;
boot coverage, jacket/glove fit, morphs, material matching and motion remain
open. The V2 source record contains the limits. No V2 trial is installed.

Jacket studies 1 through 6 remain rejected. The latest cumulative authoring
candidate, `jacket-layered-choker1`, uses guarded fitting steps and separate body
and garment-layer constraints. Tested exterior jacket/body and complete
choker/body, choker/jacket and jacket/corset edge crossings are zero. Three-view
review confirms the earlier hem and collar cuts are gone; small corset defects
remain. The full jacket/body inner contact gate still fails. Preservation passes
across 98 mesh objects. This is not whole-outfit or runtime acceptance. The
[V2 source record](2026-10-11-peony-v2-source.md#jacket-and-choker-layer-correction)
records the retained sequence, excluded lining experiment, limits and external
archive cleanup. Morphs, weights, all source palettes and motion remain open.

The next [corset and neckline correction](2026-10-11-peony-v2-source.md#corset-contact-and-deeper-neckline)
advances the cumulative candidate to `neckline-contact3`. It resolves the
measured front corset intersections and adds the requested deeper curved
opening. The independently inspected 2,364-face original fabric island and all
three trim parts pass body-edge checks; jacket/corset layering still passes.
Three-view review and saved-file preservation pass for these neutral changes.
Internal contacts, gloves, coverage, morphs, binding, materials and moving poses
remain unfinished. Neither rejected normal-based neckline fit is carried into
the current candidate. No V2 trial is installed.

The [glove comparison](2026-10-11-peony-v2-source.md#earlier-glove-baseline-and-local-contact-comparison)
verifies source UV correspondence and finds that the earlier fitted gloves also
fail the same neutral body-contact check. A local correction reduces some V2
crossings, but a knuckle breakthrough remains visible. It is archived as an
unaccepted diagnostic. `neckline-contact3` remains the cumulative baseline;
body/source preservation passes, while glove fit and full binding remain open.

Slipstream's [wardrobe and fit study](2026-10-11-slipstream-wardrobe-fit.md)
maps six removable garment groups while preserving the source data. Its latest
continuous fit clears the sampled primary-body contact screen. Differential
checks distinguish a strongly rotated chest-panel normal from a sampled
inversion. The [boot follow-up](2026-10-11-slipstream-footwear-fit.md) restores
the source heel profile, clears neutral visible-body intersections and prepares
reversible foot coverage. CSS's required taller-footwear range is built and
tested but not installed. Target binding, local compression, moving fit and
runtime preparation remain. Peony's installed trial still needs gameplay review.

The [Peony combined trial](2026-10-11-peony-combined-trial.md) is installed with
verified hashes and external rollback copies. It preserves all six existing
Eve variants and current CW/Genessa adapters. Native catalog, payload and
dependency checks pass. Gameplay fit, materials, cloth and regression results
are pending; the known editor arm-contact defect is not accepted as fixed.

Peony's [Astral runtime integration](2026-10-11-peony-astral-runtime.md) builds
with four new material definitions and source-texture checks. All thirty
palette instances pass portable selection; cooked SM5/SM6 uniform checks and
missing-texture negative controls pass. Live copying, rendering, fit and
package assembly remain. No new trial is installed.

Peony's [Windows cook and readback](2026-10-11-peony-windows-cook.md) now pass
with offline cooker worker threading disabled. The cooked meshes preserve the
cloth offsets and normal/facial base data; all 12 material variants contain
Windows skin/cloth shaders. Final fit, rendering, runtime adapters and gameplay
remain open. No new trial is installed.

Peony's [Astral companions](2026-10-11-peony-astral-materials.md) pass fresh
reload after correcting a saved mask-sampler mismatch. Four parents and their
culling variants preserve customization controls and shared-only dependencies.
Windows compilation and cook/readback now pass as recorded above. Rendering
and runtime integration remain unverified.

Peony's [cloth GPU comparison](2026-10-11-peony-cloth-gpu.md) passes 22
normal/facial cases and two negative controls. Cloth corrections match exported
morphs, non-cloth positions remain unchanged, and source assets are preserved.
This does not accept moving contact, shading, performance or gameplay.

Peony's [cook preparation](2026-10-11-peony-cook-preparation.md) now has canonical
normal/facial mesh copies and a complete 109-package editor dependency closure.
Fresh-process checks preserve geometry, morphs, cloth and deformer buffers.
Cooking is covered by the later readback above. Fit, adapters and gameplay
remain open; nothing is installed.

Peony's [walking body-contact comparison](2026-10-10-peony-hip-accessory-fit.md#body-contact-and-secondary-motion-control)
finds arm/torso crossings near the hip accessories with secondary motion both
disabled and enabled. This narrows the offline fit diagnosis; it does not verify
the game's complete pose or accept the accessory fit. Sources and installed
packages remain unchanged.

Peony's [cloth body-shape binding](2026-10-10-peony-cloth-morph-binding.md)
passes authoring and fresh-process reload on both normal and facial meshes,
preserving the original cloth and morph data. The GPU check above passes; cook/gameplay checks remain;
the shared graph and installed outfits are unchanged.

Peony's [control and catalog checks](2026-10-10-peony-customization-bindings.md)
now cover the merged 40 controls, all five palettes, automatic boot height and
unchanged inherited body/hair data. Native catalog/visibility/profile checks
pass. Fit, cooking, facial adapters, Astral support and
gameplay remain pending; no new outfit is installed.

Latest Peony result: [upper-fit rejection and facial-cloth integration](2026-10-10-peony-upper-fit-and-facial-cloth.md).
Both closer-fitting panel trials are rejected. The retained combined cloth
candidate now includes all 115 facial expressions; fresh native readback
preserves its two cloth bindings and all inspected cloth/rig fields. Upper
contact, cooking and game acceptance remain open. No new trial is installed.

Preceding Peony result: the [surface-coverage follow-up](2026-10-10-peony-surface-coverage.md)
passes all six selected-panel gait strain screens at four iterations, with
sampled leg contact clear. Upper attachment overlap with the walking arm
remains. Its weights already match the source body's surface transfer, so a
repeat transfer is rejected as a repair. A combined saved mesh passes native
preservation readback and all six saved-binding gait strain/anchor checks. No new game trial is
installed.

Preceding Peony result: the [joint-coverage trial](2026-10-10-peony-joint-coverage.md)
passes five of six selected-panel gait screens at four iterations; the targeted
right-sprint eight-iteration control passes too. Sampled left sprint contact
is clear, but right contact and upper attachment fit remain unaccepted. The
largest upper witnesses are fully skinned and unchanged by cloth collision.
No combined candidate is saved or installed.

Preceding Peony result: the [saved panel candidate](2026-10-10-peony-panel-backstop.md)
passes six short gait strain screens and all independent anchor checks, with
body collision, self-collision and full cloth simulation retained. Loose-panel
backstops are removed after controlled contact tests. The split preserves the
already merged trim; the earlier claim that trim ownership was unimplemented
was incorrect. Its older material assignment is now corrected. Full outfit
fit, body shapes, Windows cooking and game acceptance remain. No new trial is
installed. The subsequent mapped-body check finds calf/hip penetration up to
3.838 cm, including the underlying simulation particles. Collision coverage
still needs repair; a strain pass is not fit acceptance.

Peony's corrected `peony-character2` Windows cook retains 153,828 vertices and
passes the full surface, binding, skin-weight and 137-morph checks. Source
normals match; 335 zero-UV-area tangent fallbacks remain an explicit shading
limitation. `peony-footwear1` adds an exact, reversible covered-foot section
recipe without changing geometry or deformation data. Automatic height,
garment physics, customization integration and game acceptance remain. See
[the current evidence and next step](../development/three-outfit-rig-physics-status.md#peony-corrected-cook-and-removable-footwear).

The newer [Peony cloth study](../development/three-outfit-rig-physics-status.md#peony-cloth-binding-and-motion-study)
includes the repaired lining weights and joins all panel trim to one cloth
section. Saved binding and independent anchors pass. Sprint stretch fails;
self-collision contributes to the worst spikes, and removing body collision
alone does not fix it. Do not repeat the rejected multi-section binding or
present an anchor-coordinate pass as motion acceptance. No new trial is installed.

The [target-weight and collision continuation](../development/three-outfit-rig-physics-status.md#peony-target-weights-and-collision-fit)
reduces sprint strain failures from 135 to 13 of 145 frames by changing free-panel
simulation targets to follow the pelvis. Walk passes the gross-strain screen;
jog and sprint remain unaccepted. Extra substeps cost more without fixing the
remaining pulls. Six sampled endpoints sit inside the posed body despite
contact with the borrowed spheres. A Peony-specific leg-capsule candidate is
authored and read back correctly, but its native gait comparison fails. Expanding
free-panel travel clears individual-sphere feasibility conflicts without fixing
the motion. The [native collision readback](../development/three-outfit-rig-physics-status.md#peony-native-collision-readback) now verifies actual collider placement against both gait captures. Wrong-bone/space attachment is ruled out for this trial; CCD, removing animation drive and initial settling all fail to repair the strain. Do not shrink the old
collider to conceal strain or treat passing anchors as contact acceptance.

The [simulation-density comparison](../development/three-outfit-rig-physics-status.md#peony-simulation-density-comparison)
reduces the private proxy from 1,060 to 774 particles while preserving anchors,
boundaries and retained deformation data. All three gait captures still fail,
and the mapped render gains a sharp projection. The lower editor cost does not
justify adopting that visible regression. Keep cloth7 rejected.

The [spring and collider rest-fit audit](../development/three-outfit-rig-physics-status.md#peony-spring-initialization-and-collider-rest-fit)
verifies all movable edges, the native pin threshold and correct spring rest
response. Raising actual iterations from 4 to 16 still fails sprint and costs
more. Full capsule queries identify 445 resting cloth particles outside the
body but inside the collision envelope. A body-surface-derived leg recipe
clears those rest overlaps. Cloth8 improves walking but still fails all three
gait strain screens, and mapped renders retain stretched/bunched panels. Keep
it unaccepted. The next comparison restores anatomical free-particle target
weights with this collider while preserving all native pins. Do not confuse
the spring audit or rest-fit pass with an outfit fix.

Latest evidence: all 63 native alternatives and 60 idle carriers are installed.
The user accepts Black Hole and Iris Out playback/interruption on the tested
original human appearance. The three existing hair rigs pass 27 editor
lifecycle cases; Genessa's unweighted hair solver was confirmed active in game.
Its graph-default optimization and matching shared catalog are now installed,
with live verification pending. Follow the
[secondary-motion record](2026-10-08-secondary-motion-ownership.md#october-9-hair-lifecycle-and-genessas-unused-solver)
for source, component, cook and catalog evidence. Earlier entries below record
their original checkpoints, not the current installed state.

The October 8 goal amendment sets `CSS_SharedAssets_P` as the owner of reusable skeleton, locomotion and Astral assets, keeps character-specific work in each outfit mod, and adds necessary locomotion optimization followed by the remaining default-human animation conversions. The goal's ownership and conversion sections supersede older pack-organization instructions and the previous four-animation native limit. Historical research notes remain evidence of their original checkpoints.

The follow-up amendment explicitly adds implementation of facial-expression support for both Unholy Genessa variants. Its requirements and acceptance checks live in the goal's facial-support section; earlier wording about preserving facial support does not establish that this work is already complete.

Commander White's short-strand repair now has a saved UE mesh/skeleton/rig
candidate, rather than analysis-only added joints. Both normal/facial mesh
data checks, fresh typed-default readback, post-process structural comparisons,
723 gait samples and 60 fixed gait contact witnesses pass. Render-weight
quantization differs slightly from the prototype and is documented. Six
original/candidate full-component pairs now pass, including resets, hair
toggles, animation reinitialization, teleport and selected morph clearing.
Windows cooking, six-package roundtrip and independent cooked mesh/rig checks
also pass. All 2,548 changed cooked hair weights match the saved editor mesh;
cloth data, materials and morph deltas retain their installed values. Cooked
dependency materials are excluded. No new outfit is installed; cooked VM,
normal/UV, Astral/CINE, gameplay and performance checks remain. See
[the component and cooked evidence](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/hair-head-clearance-study.md#component-lifecycle-and-windows-cook)
and CW source commits `9a92d08` and `0c99728`.

The eight-asset final-path candidate now passes fresh reload without redirectors
(`hair-strand-final3`). Its two normal/facial scene-playback graphs pass
structural comparison and 2,892 paired component samples. Separate leg-pose
checks demonstrate scene enable and release to the input gait across all twelve
captures. This is editor execution, not runtime CINE acceptance. The final-path
Windows cook, eight-package roundtrip and independent mesh/rig comparison pass.
The optional surface digest proves cooked positions, normals and UVs unchanged
independently of changed skin weights. CSS's exact CW skeleton compatibility extension passes
all 24 host suites and the Windows build. The full CW/shared-metadata trial and
matching core are now installed with saved state preserved. Combined checks
cover 864 packages, 426 native-library checks and 1,396 nine-variant checks;
all CW export/bulk payloads roundtrip against the baseline plus the patch.
Live hair/eye/interruption and performance acceptance remain pending.
See the updated [CW status](../development/three-outfit-rig-physics-status.md#current-commander-white-hair-prototype).

## Entry points

- [Goal and implementation status](../development/three-outfit-rig-physics-goal.md).
- [Video observations and audio review](2026-10-08-rig-video-review.md).
- [Actual outfit/source/game-dump inventory](2026-10-08-three-outfit-rig-baseline.md).
- [Current editable mesh structure and migration sources](2026-10-08-editable-mesh-audit.md).
- [Genessa facial candidate and rejected trials](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/facial-controls-notes.md).
- [UE 5.6 mechanisms and local 5.6.1 source evidence](2026-10-08-rig-physics-ue56.md).
- [Local extraction, hashes, transcripts and frames](../../work/rig-physics-review-20261008).

## Settled findings, do not repeat from scratch

1. V1 uses Blender Cloth on a proxy plus Surface Deform, not Soft Body or a runtime bone rig. Exact cloth settings were in an unavailable description.
2. V2 sculpts a foot without a rig. V3 demonstrates finger-joint placement. V4 is a footwear design example, not a full IK or physical-load tutorial.
3. Scope is six current Eve outfits, two Unholy Genessa variants and Commander White. The Eve directory's `SeduXtress` spelling does not authorize work on legacy `Seductress`.
4. The historical current mesh prefix has 386 bones, with four additional Genessa wing bones. These counts differ from both the full native Skeleton and older mesh references. Revalidate identities before editing.
5. Commander White's active shared facial replacement is newer than its outfit body. Preserve E9 and its optical repair.
6. Gameplay Physics Asset, cloth collisions, Control Rig contact and spring limits have separate consumers. Larger colliders already failed to materially improve one Commander White cloth trial.
7. UE 5.6 has a simulation-morph path, but legacy skeletal cloth does not automatically expose those morph arrays. Identify the actual asset path.
8. Historical audits are not fresh runtime acceptance. On October 8 the installed interruption-fix DLL hash was verified and the user confirmed attack, aim/fire, doubles and Genessa Form work with Iris Out.
9. Canonical asset organization is now `/Game/CSS/<author>/<project>/...`, using `eins0fx` for these projects and `eins0fx/Shared` for reusable content. The goal defines migration requirements; existing paths are historical inputs.
10. All 790 migration rows have editable candidates or historical alias recipes. Sixty-one animation paths are generated aliases, not missing original clips. Eve's two v1.4.0 hair instances must come from their receipt-pinned private project, not the older main-project copies.
11. Fresh editor-buffer checks of nine outfit meshes and seven facial replacements found normalized weights, valid section/influence/morph indices and finite poses/deltas. This does not establish deformation or collision quality. Mesh raw-bone counts and Skeleton helper counts remain distinct.
12. Genessa's mask mouth is sealed. Direct expression transfer stretches this surface and produces poor eyelids. The retained facial candidate splits the lip contact line, uses mask-specific lid controls and adds an oral surface. Its underlying head must receive synchronized facial deltas to avoid obstructing the opening or breaking through the cheeks. This is proven on source-based previews; W3 integration and game acceptance remain pending.
13. The subsequent W3 candidates pass independent cooked geometry/morph readback and full cloth-data comparison. Their private facial catalog has two revision-bound adapters, 21 presets and six existing facial performances. Packaged metadata preserves Eve, Commander White and all 63 body entries. This establishes offline integration, not live acceptance. The status document points to current reports; do not repeat the original sealed-mouth or missing-catalog diagnosis.
14. The main authoring project's native-human skeleton is a smaller placeholder. Use the receipt-pinned `native-binding-v6/game-reference` skeleton only inside isolated migration projects. The rejected cook reduced native mappings from 1,208 to 182 tracks; the corrected cook matches all 20 installed native sequences and four blend spaces. The twenty Eve reference-pose refreshes have full source-key motion evidence, including virtual tracks.
15. The five playback adapters contain character-specific secondary-motion nodes and belong to their respective outfit packages. `namespace-3` corrects that ownership without changing any of the 63 public animation IDs. The 124-package editor7/cook6 trial passes independent readback and 547 imported references with explicit baseline dependencies. It remains a partial migration, not game acceptance or final split packaging.
16. `genessa-rig-pack2` verifies 101 outfit packages plus the shared base skeleton against the original cooked fields. Both W3 meshes also match full decoded render/weight/cloth-mapping/morph buffers. Runtime recognizes the verified old/new wing skeleton aliases. This assembly still uses the original W3 meshes; final facial integration remains separate.
17. Eve has 68 cooked material instances, 204 Texture2D packages and one authored hair parent. `eve-texture-namespace2` preserves all 2,322 mip payloads and texture settings through independent container readback, using bounded resource-table header relocation rather than pixel recooking. `eve-material-writer-regression1` retains identical output for all 68 instances. See the [material migration notes](../development/outfit-material-namespace.md); the full Eve rig assembly remains pending.
18. The hair parent migrates and reloads with three texture dependencies in `eve-hair-editor1`, then cooks under the pinned v1.4.0 renderer settings. `eve-material-pack1` passes all 273 material/texture package comparisons, including shader maps, 405 byte-identical texture payload files and 539 imports without the original outfit mounted. Do not recook Eve's 204 textures as part of the remaining rig assembly.
19. Commander White's `commander-material-pack2` passes all 160 material/texture packages, 929 texture mips and 428 imports. Six MaterialX parents required handling numbered FName bases, not just raw full strings. The corrected writer preserves indices/numbers and rejects unmapped sibling changes. `commander-rig-pack3` passes all 172 normalized package comparisons and 598 imports. Preserve the original B2/B3 compiled graphs because the editable sources rename `CW_BodyGeometryJson`; their 13 original exports match in `commander-secondary-namespace1`. Both C4/E9 mesh buffers remain verified. These candidates are not installed.
20. `genessa-face-namespace-pack2` integrates both 137-morph facial meshes and four preserved oral packages into the migrated outfit, now 107 packages. All 108 comparisons including the shared skeleton and 326 imports pass. Full facial mesh/cloth/morph buffers match `face-cook3` after path normalization. Catalog revision bindings and live acceptance remain; do not repeat the completed facial dependency migration. The first assembly's oral mismatch came from different shader-readback modes, not changed assets. Matching full readback modes proves equivalence without filtering any material field.
21. Independent Eve/CW trials shared an internal IoStore ID because they used the same temporary filename. The final-container assemblies use distinct final names and preserve all 509/242 asset chunks, package-store dependencies and 309/172 independently decoded exports. Use `eve-final-container1` and `commander-final-container1` for new mounts and revision bindings; the superseded trial bulk files were pruned after validation. Their stages and small evidence remain.
22. `catalog-namespace2` preserves nine variants, 63 body-animation IDs and three facial rigs across four candidate catalogs. An independent combined IoStore audit resolves all 735 packages and 106 catalog references without legacy outfit/shared containers. The native audit passes 1,380 checks, including 99 saved choices, 540 CINE resolutions and nine facial bindings; all 23 host suites pass. Legacy adapter dependencies, coexistence/upgrade handling and live acceptance remain unproven. Do not confuse preserved legacy alias IDs with a complete old-outfit facial compatibility path.
23. `catalog-namespace3` adds an explicit shared-layout requirement without changing assets or saved IDs. `dependency-upgrade2` passes 28 native scenarios, including the actual old/migrated/partial sets, incompatible or duplicate providers, rejected catalogs and transitive dependencies. The unchanged legacy set retains seven facial bindings; the complete migrated set resolves nine. An older outfit with the new library does not acquire a facial adapter from another revision. Upgrade the matching outfits together with the shared packs for full coverage. `dependency-runtime2` passes Windows compilation, 23 host suites, 1,380 catalog checks and the existing package/discovery regression checks. Live upgrade and playback acceptance remain pending; nothing is installed.

24. The coordinated namespace/facial trial is installed, with all four candidate trios and the core independently hash-verified. `namespace-deployment1` records the exact core and transaction. All six old trios remain in `MortalShell2/Content/CSS-namespace-trial1/backup`, outside the mounted tree. Fourteen installer tests cover replacement, rollback and interruption. Fifty-nine protected files, including profiles and CINE, are unchanged. This supersedes earlier not-installed statements; game acceptance remains pending. `cleanup-coordinated-upgrade1.json` records 1,779,421,184 observed bytes recovered from verified duplicate trial data before installation.
25. The remaining 59 native body alternatives are authored, cooked and merged in `shared-native-library1`, giving one native alternative for every one of the 63 entries. All 149 prior shared packages remain byte-identical; 66 sequence/carrier packages are added and the duplicate codec is identical. Cinderella's strict nominal-time discrepancy comes from ACL float sampling; full matched-clock readback passes without editing its motion or codec. Do not repeat the rejected C12 compression trial or use nearest-key decoding as runtime evidence. The merged pack passes catalog checks alone and with all outfits, but is not installed. [Native conversion evidence](../development/shared-animation-native-human.md#complete-native-candidate-and-shared-merge) records exact results and remaining visual/gameplay checks.

## Tooling notes

The current Genessa rear-panel report is reproduced at Glute Size 1 and clears
at 0 according to the user's matched setting test. `genessa-panel-world1`
captures equal mesh/morph/visibility state but different cloth enablement in
preview/world. The outfit's [rear-panel study](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/rear-panel-morph-notes.md)
records fresh section readback and the incomplete physical-shape transfer.
Do not repeat U51 pinning or the October 1 unsafe post-creation array mutation.
The current linear fit still has a 0.7985 cm glute residual; no repair is installed.

The first private Deformer Graph correction is rejected by actual GPU output.
`genessa-deformer-render1` attempt 3 completes 24 Vulkan captures and eight
vertex readbacks, reproducing size-1 exposure on both variants without improving
it. In exact UE 5.6.1 source, cloth sections disable the morph vertex-buffer
input before Optimus reads it, so adding the missing offset in graph math alone
cannot work. The prior Windows cook succeeded but did not prove visible fit.
See the [current evidence and next prototype](../development/three-outfit-rig-physics-status.md).
Do not install `DG_ClothMorphStudy1` as a repair. The next prototype needs an
independent morph-basis input, followed by Windows cook and in-game validation.

The independent-basis follow-up now has a positive GPU result.
`genessa-deformer-render1/gpu-basis-comparison.json` shows the glute-only
`DG_ClothMorphStudy3` restores the skirt offset on both variants with zero
size-1 position change outside the skirt. It uses scalar vertex attributes
and the engine's per-section cloth/morph permutations; it does not split the
329 shared source vertices. Fresh reload checks preserve the existing rig,
all morphs, render data and cloth. The five-shape follow-up `DG_ClothMorphStudy4`
passes 22 paired GPU cases, including cloth-disabled comparisons, and the
single-threaded Windows cook `genessa-deformer-cook3`. Its 15 scalar attributes
and normal-reconstruction half-edge buffers survive cooking and IoStore
roundtrip. `genessa-deformer-pack2/mesh-verification.json` checks all 152 exports
per mesh against the original, allowing only those intentional additions and
the empty Nanite bulk offset. All 137 morphs and original cloth data match.
The threaded cook2 crashed building a vertex attribute buffer; a cook-time race
is suspected, not proven. Do not retry that path without new evidence.
The private assets are not installed. Animated/full-physics behavior, shading,
runtime integration and performance remain open.
Do not repeat the rejected direct-morph-input trial or claim the entire
rear-panel repair is complete from the reference-pose result.

The first gait comparison completes 64 images and 32 GPU readbacks across both
variants, four gaits, neutral/size-1 and stock/trial. Body poses match and
non-cloth positions remain unchanged; inspected size-1 captures improve panel
coverage. Separate neutral cloth runs differ substantially during sprint.
`motion1-validation.json` accepts sanity checks only. The next comparison must
hold one simulation state while changing the graph, rather than treating those
separate simulations as identical. This result does not invalidate the static
input/cook checks or establish a gameplay regression.

The held-state follow-up now resolves that discrepancy. `held-render-exit2.json`
and `held-motion2-validation.json` cover both variants and four gaits at neutral
and size 1. Cloth positions/normals/transforms hash identically for each pair.
All neutral GPU positions, normals and tangents are exactly equal; size-1
corrections stay within the source bound and leave non-cloth positions unchanged.
The earlier large neutral differences came from separate simulation states.
Proceed to default-deformer/runtime binding and actual material/contact/cost
checks. Do not repeat the separate-run comparison or mark live acceptance done.

The [hand target study](2026-10-08-hand-target-ownership.md) resolves pinned
Eve/Genessa source graphs' 115 transform writers to 19 left-hand finger bones.
Parent propagation stays within those bones on the cooked target hierarchies.
All 16 named corrective morphs exist on active normal/facial meshes. Six checker
tests pass. This is source-target and cooked-consumer evidence, not decoded
cooked VM literals or live grip/contact acceptance.

The first live namespace trial exposed a retained Eve playback override on
Genessa. `live-face-visibility1` pins the reflected player graph, Genessa's
authored graph and the preview error. Commit `96b56eb` detaches owned playback
overrides before mesh changes and keeps preview visibility independent of physics
sync. Its installed core is hash-verified; user acceptance remains pending. Do not
mistake the loaded Genessa facial mesh for proof that facial playback is active.

The [secondary-motion ownership inventory](2026-10-08-secondary-motion-ownership.md)
records all nine variants and 18 normal/facial meshes from the final cooked
containers. `secondary-bindings5` passes five adapter comparisons across ten
graphs/eight rigs, checking binding identities, types, defaults and transfer
flags and serialized pose-link writer order. Fourteen checker tests cover
wrong-input, duplicate-writer, disconnected-node and cycle cases. No named
body/hair writer overlap was found; hand literal targets and live scheduling
remain outside that proof. Keep collision consumers and simulation-vertex counts
distinct from contact quality and performance acceptance.

`eve-rig-editor2` passes migration and fresh reload of all 309 selected packages,
with old staged paths absent and all 316 source inputs unchanged. Its 36 rig
assets also pass the private Windows cook. `eve-rig-pack2` now passes all 309
decoded packages and imports; its 22 meshes match original cooked buffers.
The secondary graph uses preserved compiled data because recooking changed
its exports. Do not replace that preservation step with an unchecked recook.

Commander White's initial migration stopped before renaming because the full
render JSON exceeded UE's 2 GB FString writer limit. All 184 inputs stayed
unchanged. The optional `array-blake3-v1` audit now hashes complete arrays a row
at a time. Its full C4 audit and independent comparison against 75 detailed
arrays pass. See `commander-rig-editor1/array-digest-probe`; do not remove mesh
data or reduce audit coverage to address this serializer limit. The implementation
uses UE 5.6.1's local `Hash/Blake3.h` and `Serialization/JsonWriter.h`, with the
[archive interface documented by Epic](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FArchive).

The local faster-whisper transcriber initially hit a PyAV `metadata_errors` API mismatch. Passing normalized PCM samples from extracted WAV files avoided that decoder path. Transcription completed for all four clips; versions are in `audio-transcripts.json`. After review, the task-local model and virtual environment were removed (582,088,872 logical bytes). All four source hashes were rechecked unchanged. Source videos, extracted evidence and the script remain; validation is recorded in `work/rig-physics-review-20261008/review-validation.json`.

## Open questions for implementation

The [October 9 hair-surface study](2026-10-09-hair-surface-contact.md) distinguishes
signed-nearest false alarms from confirmed fringe/face intersections using
matching unsolved poses, winding, rays and diagnostic renders. Gait/head-turn
captures preserve source hashes and non-hair input. A Commander White-only
head-clearance trial improved two witnesses but introduced a short-strand
flyaway intersection. The subsequent [separate-joint prototype](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/hair-head-clearance-study.md#original-strand-binding-and-separate-joint-prototype)
uses the original short-strand pivots and weights. Its 1,446 head-turn/gait
samples preserve the other six chains exactly and clear that regression.
The subsequent normal/facial CW-only skeleton, regular/scene graphs and core
are installed in `hair-strand-assembly1`; the user reports the requested
Iris Out/hair/eyes/interruption check looks fine. Two left-bang head-turn
witnesses now have source-opacity comparisons: visible intersections decrease
from 67 to 14 and 75 to 3 sampled points, but are not eliminated. The study
records the exact limitations and remaining CINE/doubles/performance checks.
Do not repeat the raw nearest-normal screen
as proof of penetration or modify all colliders from its maxima.

- Which exact final source recipes reproduce all installed meshes and active facial adapters?
- Which visible defects remain in the accepted current build, and which layer actually causes each?
- Which region/solver owns each moving surface and receives resets in preview, gameplay and doubles?
- What changes improve visible fit without unacceptable frame-time cost or gameplay regression?

Answer these against matched evidence. Do not assume all outfits need new skeletons or every physics technique.


The [hand study](2026-10-08-hand-target-ownership.md#full-w3-component-replay)
now includes 42 full editor-component lifecycle samples on both Genessa bodies.
Hand curves reach render morph weights and clear on disable. One small synthetic
thumb crease overlap persists; it is not a demonstrated gameplay regression.
The user accepted cross-character Iris Out faces/eyes and attack/aim interruption
on the installed trial. Variant-specific, CINE and remaining gameplay checks stay
open in the implementation status.


`skin-influences1` completes actual weighted-surface coverage for all 18 meshes.
All nine normal/facial region pairs agree. Genessa's hair rig has no identified
skin/cloth weight consumer; her fitted trim does use body helpers. See the
[weighted-surface findings](2026-10-08-secondary-motion-ownership.md#actual-weighted-surfaces)
before changing those owners or collision settings.


Peony's [target-weight and contact comparison](../development/three-outfit-rig-physics-status.md#peony-target-weights-and-collision-isolation)
rejects both restored anatomical and thigh-blended free targets with the
corrected colliders. CCD, no-drive, no-backstop and no-self-collision do not
repair that combination. A matched cloth8 no-body-contact control reduces
sprint failures from 109 to 1 of 145 frames; removing backstop too clears the
strain screen. This is diagnostic evidence, not permission to remove contact.
The next trial keeps the fitted hip portion attached while preserving lower
panel simulation. All native anchors remain correctly reconstructed.

The larger fitted region is now tested and rejected too: cloth11 fails every
frame, including without body contact and with the secondary graph enabled.
Two fixed vertices reproduce a large butt/thigh skinned-pose discontinuity.
Selected posed-body parity queries originally suggested target penetration
and collider overhang. Their containment interpretation needs rechecking under
the corrected winding method below. Do not treat them as established cause.

The [posed-contact follow-up](2026-10-10-peony-posed-contact.md) confirms that
the garment already uses final-body surface transfer. A harmonic loose-region
weight trial and larger cloth travel both fail native simulation. The earlier
outside-axis classification is withdrawn: overlapping body regions defeat the
ray-parity test. Corrected winding checks reject zero original-axis samples in
436 poses. No moved/shrunken collider is accepted. A gradual reference-pose
start also fails, so initialization is not the whole defect. Native isolation
now localizes most stretching to the thigh colliders, especially the left thigh.
Calf contact has a smaller separate failure. The regional radius trial still
fails all three gaits and is not accepted. Continue from the remaining measured
contact/constraint conflicts; no game installation is requested yet.

The follow-up rejects backstop/drive removal, lower friction and the combined
harmonic-target/regional-collider trial. Corrected body witnesses show that the
reduced collider can leave contact points inside the body. The new analytic
reach screen finds no individual collider/movement-bound conflict in 145 sprint
frames; it does not prove all constraints jointly feasible. Earlier native
triangle-collider studies already exposed coverage, temporal and cost limits
on another dress. Inspect actual contact corrections before repeating collider
or solver-setting sweeps. No candidate is accepted or installed.

Native independent constraint replay now shows the original witness edge
separating around the same thigh collider. Edge/area passes shorten it but
return its points inside collision geometry. The subsequent cloth-only
one-sided thigh trial preserves the original collider dimensions, yet produces
large failures on other edges and is rejected. Continue from contact-side
divergence and the first affected panel regions, not another global outward
correction or weakened constraint setting.

The [hip-accessory fit follow-up](2026-10-10-peony-hip-accessory-fit.md)
distinguishes the user's circled material-29 accessories from the loose cloth
panels. Closer mounts and repaired surface weights remove severe root-pinned
stretching, but walking arm contact remains. Missing body-slider offsets now
have a smoothed private candidate that passes the eight sampled bind-shape
contact checks. Its native body/corrective morph readback, cloth preservation
and all 115 facial expressions pass. Rejected placement and
arm-pose probes are recorded so they are not repeated as shipping solutions.
