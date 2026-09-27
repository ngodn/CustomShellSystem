# Eve outfit fitting playbook

Updated September 27, 2026. Read this before starting the next outfit, then check the [current status](eve-outfit-goal.md) and [evidence index](eve-outfit-index.md). This records reusable lessons, not a claim that Holiday physics is solved.

## Start each outfit from evidence

1. Inventory every garment and accessory against the source: dress, sleeves, underwear, hair, hat, earrings and shoes as applicable. A partial diagnostic export is not the complete outfit.
2. Audit missing weights, influence counts, material sections, shape keys and garment ownership before fitting. Gemini's Holiday source contained unweighted parts; apparent static fit did not establish usable deformation.
3. Preserve the original body and the current shared skeleton. Record hashes before edits. The production baseline has 386 bones, 82 sockets and nine virtual bones. Older private 379-bone imports are diagnostic assets only.
4. Establish a skinning-only baseline across idle, walk, jog, sprint, turns and representative weapon poses. Include supported morph extremes and combinations. Diagnose clipping already present here before adding physics.
5. Change the garment locally where the baseline proves a garment error. Keep the body's proportions. Verify the newly exported mesh, weights, normals, UVs and morphs against the fitted Blender source.
6. Add physics to an already understood baseline. Validate particle motion, render attachment and body contact separately. Inspect front, side and rear views and late motion frames, not just a neutral pose or the first successful frame.
7. Measure cost with identical input poses and settings. Repeat the baseline when a small difference could be run-to-run variation. Editor timings identify expensive paths; they do not establish game FPS.
8. Assemble the whole outfit, integrate with the current shared rig and retained body/hair physics, then cook and test in the game. Only after fitting is sound, finish garment controls, Original plus five clothing palettes, persistence and release validation.

## Mistakes and the replacement rule

| What went wrong or misled us | What to do instead | Evidence |
| --- | --- | --- |
| Treating unusual cloth motion as a physics-only problem when the original garment skinning already deformed badly | Compare identical poses with physics disabled first. Preserve or repair the original animation-follow behavior before solver tuning. | Goal log: animation-follow baseline, `CR_HolidayFollow` |
| Expanding the hip to compensate for a swinging forearm intersecting almost-pinned cloth | Identify the actual contacting body region and cloth drivers. Separate garment fit, arm contact and attachment error. | `panel-repeat-trace.json`, `panel-support-trace.json` |
| Trusting simulation particles while native render attachments stretched fur into spikes | Check render reconstruction at rest and in motion, including normals, barycentric extrapolation, weight normalization and fully skinned vertices. | `db7be81`, `check_panel_mapping.py` |
| Copying weights from the rendered garment when updating a simulation proxy | Keep proxy provenance explicit. Preserve that proxy's own weights and correspondence when moving its vertices. | Rejected `panel-waist-proxy.json`; corrected `panel-fit-proxy.json` |
| Applying a local displacement along inconsistent normals | Check directions against the body surface and re-evaluate the affected poses before saving a new large Blender file. | F12 corrected 39 directions; 62 sampled pose/morph cases improved |
| Increasing triangle density because coarse attachment seemed suspicious | Test a geometry-preserving refinement as a controlled trial. Reject it if stretching, contact or cost worsens. More triangles alone are not a fix. | Rejected `CA_Hip`: edge ratio 7.919, worse clipping and cost |
| Selecting an attachment because numerical clipping counts decreased | Inspect the visible result. A larger exposed patch can be worse despite fewer intersecting samples. | Rejected single-support replay: `panel-localmap-*`, frame-68 views |
| Attributing a trajectory change to a smaller collider without measuring baseline variation | Repeat identical runs first. Check collider transforms separately from cloth trajectories. | Identical runs differed by 3.559 cm; subset change was 1.977 cm |
| Assuming a CCD toggle applies to every collider type | Read the installed engine's collision path. UE 5.6.1 skinned triangles use a separate constraint skipped by ordinary CCD. | `PBDSoftBodyCollisionConstraint.cpp`, `panel-contact-history.json` |
| Solving late-sprint tunneling by raising temporal substeps without a cost gate | Treat it as a diagnostic that identifies a temporal problem, then find an affordable solution. | 16 substeps reduced particle penetration but cost about 71.4 ms in the editor trial; rejected |
| Trusting capsule coverage from the rest pose | Measure animated and morphed body coverage, including secondary-bone deformation. Four extra spheres did not solve the observed gaps. | `capsule-motion-bands.json`, rejected `secondary-coverage.json` |
| Generating body collision from a combined body-and-outfit mesh | Extract and verify a separate body-only source first. Otherwise garment geometry can contaminate the collision volume. | `prepare_collision_body.py`, `cbody-source.json` |
| Treating a successful import, build or save as finished clothing | Reload saved assets, measure deformation and cost, visually inspect, then validate the cooked candidate in game. | Private collider generation is still unaccepted |
| Accepting a level-set collider because its resting surface fits | Compare direct lattice mapping with independently verified skinning in a difficult pose, separately from signed-distance lookup. Inspect lattice weights before another resolution trial. | `ls128-map68-audit.json`: 24.18 cm maximum skirt-region mapping error despite improved resting fit |
| Assuming automatically generated collision weights retain the body's local influences | Trace the actual lattice corners and compare their weights with the body. Test local surface transfer while holding geometry fixed before adding resolution. | `ls128-trace3-audit.json`: thigh samples gained unrelated finger/arm/spine weights; `ls-local-weights3.json` reduces three selected-point errors to 1.2–1.5 cm, still unaccepted |

Evidence filenames above are under `work/eve26/` unless a script, asset or commit is named. The evidence index and dated goal appendices carry the detailed results and limitations.

## Do not reopen a rejected trial on a hunch

Before trying a change, write its hypothesis, the one variable being changed, baseline artifact, expected measurable result and rejection condition. Check the evidence index for prior attempts. Revisit a rejected method only when there is new evidence that addresses its recorded failure, and state what changed.

After the trial, record the result as adopted, rejected or still unverified, with artifact paths and the next action. Keep failed results beside successful ones. A new script without a completed trial is not evidence that the method works.

## What Planet Diving can reuse

### September 27: a promising sample is not a complete result

Local collision-weight transfer reduced error at three selected thigh points, but the full coarse-grid trial still reached 25.62 cm default and 27.32 cm combined hip/waist correspondence error across 582 recorded cases. Half-sized cells improved the maximum in all 291 default poses and 284 of 291 morph poses, but still reached about 15.27 cm. Neither is an accepted collider. Reports: `ls-local16e/motion.json` and `ls-local32/motion.json`.

The reusable lesson is to run the complete recorded pose set before building on a selected-frame success. Separate weight transfer from grid spacing, preserve the same bounds, and record the worst vertex indices so the next investigation has a concrete location. Correspondence distance also includes tangential motion; it is not automatically penetration depth. Check the actual surface and native distance queries before making that claim.

Follow-up demonstrated that distinction: the 15.27 cm correspondence outlier has a 2.64 cm nearest-surface gap on the mapped triangulation. Its corners span both legs. Quarter-sized cells improve the visible hip shape, but retain inner-thigh and underarm folds and require 130,585 nodes. Do not turn finer spacing into an unlimited remedy: native containment and measured cost must decide whether it is useful. A mapped body triangulation is only an approximation of an implicit collider surface.

The native recipe then passed save/reload checks but failed actual cloth fitting. Inverse queries can find several overlapping deformed cells and choose an interior point from another body region. CCD rescued one frame, yet the later sprint frame still broke through. Reuse the recipe verifier and query tracing, not this collider as a production preset. Also separate fully skinned clothing from simulated clothing in clipping reports: all 334 fixed failures at the checked frame contacted the upper arm and were unchanged by collider choice.

Two numerical checks initially stopped these trials. Tiny negative nearest-triangle barycentric values were corrected only when clamping preserved the closest point within 0.0001 cm. Independent original-grid replay differed by up to 0.004063 cm overall and 0.000369 cm in the skirt region. The offline guards now allow 0.01 cm overall and 0.001 cm locally, while reporting actual errors. Those numerical agreement tolerances are not garment-fit acceptance thresholds. Keep failed-run diagnostics rather than silently loosening checks.

### Record after each useful experiment

- Problem and hypothesis, including what evidence justified another attempt.
- Source revision, exact command, changed variable and baseline.
- Result, measurements and representative visual evidence when applicable.
- Decision: adopted, rejected, or unverified, with the remaining limitation.
- Reusable script/check and the next action. Put outfit-specific settings separately from general rules.

Update this playbook for a changed workflow, the evidence index for artifact lookup, and the goal checkpoint for what to do next. Do not turn every tool call into a diary entry.

Reuse the source/weight audit, protected-asset checks, recorded pose inputs where compatible, morph comparisons, export verification, render-attachment checks, contact analysis and visual review sequence. Begin with the original Planet Diving source and identify its actual flexible parts. Do not copy Holiday's skirt weights, collision cutoffs, proxy density, stiffness or clearance offsets blindly.

The faster route is to avoid disproven experiments and detect source errors early. Holiday's final physics recipe is not established yet, so there is no proven universal preset to apply to every outfit.

## Storage and handoff

Keep originals, the accepted working source, useful rollback points, scripts, measurements and representative renders. Delete only confirmed superseded generated files after recording their identity and why they are safe to remove. Do not use broad cleanup commands in shared worktrees.

After a coherent checkpoint, update the current status and evidence index and commit only this effort's explicit paths. Keep concurrent CSS runtime/UI work separate. A next session should find the current reference, rejected alternatives and next test without reading the entire transcript.

## September 27: fix the animated target before restricting cloth to it

A garment-follow proxy plus a 3 cm movement limit and native backstop improves the skirt envelope, but the animation-only garment already intersects the body at the late sprint pose. Backstops follow animated cloth targets; they cannot establish correct fitting if those targets are wrong. Inspect skin-only fabric, simulated fabric and trim separately before another constraint experiment. Evidence: `panel-stop-clearance68.json` and inspected rear/side views.

Read actual saved movement maps for verification. A vertex below 0.1 cm MaxDistance is not necessarily fixed; only exactly zero counts as the kinematic reference in this check. Movement-map changes can also alter render mapping, so reload and verify attachments for each candidate. Keep geometry-only proxy updates separate from deliberate weight trials and hash the unchanged data.

When interrupted, poll the existing process and inspect completed artifacts before rerunning. This session recovered all 69 frames and the process exit status without generating another Unreal trial.

### Rest-space weight transfer can preserve the wrong driver

The lower-garment transfer trial changes only 2,815 garment weight rows and preserves geometry, body, morphs and skeleton data. Smooth blending reaches full nearest-face body weights below 100 cm and retains original weights above 112 cm. It excludes arm-dominated source faces. Across 62 sampled default/combined-morph locomotion cases, the worst signed vertex distance regresses from -4.31 to -5.68 cm; 36 cases have deeper minima and 23 have more failing vertices. Rear/side renders still show a folded hem. This candidate is rejected.

At the worst sample the nearest rest body face still produces about 96% pelvis weight, while the intersecting animated surface is thigh-driven. Nearest-rest-surface transfer therefore does not establish correct skirt following. Do not repeat it as a generic fix for this hem. Keep the no-arm contact isolation, but include arm contact again before final acceptance. Reports and renders: `work/eve26/garment-transfer/`; script: `tools/eve-fit/prepare_garment_transfer.py`.

Blender documents nearest-face interpolation as a transfer mapping, not a fitting guarantee: [Data Transfer Modifier](https://docs.blender.org/manual/en/latest/modeling/modifiers/modify/data_transfer.html).

### Compare actual solver settings, not asset names

Freshly authored candidates used one substep while the older reference used four. Always compare native per-frame iterations/substeps, particle counts and collision settings before interpreting timing. The four-substep Stop run still clips and costs 11.49 ms; the earlier cross-setting timings cannot isolate a weight/backstop benefit. Evidence: `panel-stop4-*`.

Backstop direction is relative to the animated cloth, not automatically to a moving body surface. Particle 329 is outside its reconstructed backstop but inside the thigh. More allowed movement or more substeps did not fix that relationship. Keep reconstructed math distinct from solver readback.

Inspect both simulation edges and attachment coefficients before calling a trim spike a mapping-only bug. An edge grew 0.245→2.095 cm, and an attached fur record extrapolates beyond its triangle. Clamping the coefficients would lose exact rest reconstruction. Fix the underlying surface/contact problem and then validate any attachment change in the same pose.

### Regional collision needs both coverage and clearance

Rigid regional point correspondence did not tell whether a union of volumes could fit the body. Testing the actual seven-hull union answered that question for this recipe: it leaves body points outside while also enclosing garment points that clear the body. Enlarging it is not a sufficient repair. Record both body-to-volume coverage and garment-to-volume interference, use the same pose/morph cases, and visually inspect overlaps. These are offline hulls, not native collision behavior.

For overlapping volumes, the minimum per-volume signed distance classifies membership, but its negative value is not the exact distance to the union boundary. Internal hull faces also appear in surface sampling. State those limitations rather than presenting all distances as physical penetration depth. Evidence and scripts: `region-hulls/`, `audit_region_hulls.py`, `render_region_hulls.py`.

Pinned UE5.6.1 `ChaosClothingSimulationCollider.cpp` has a convex collision path (around line 237) and separate skinned-level-set paths. The [Epic collider API reference](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosCloth/ChaosCloth/FClothingSimulationCollider?application_version=5.5) documents physics-asset extraction, but the installed engine source determines this project's implementation. Existence of a collision type does not prove its fit or performance.

### Keep provenance when cutting collision regions

Store original body-point coefficients through cuts, then use them to interpolate both skin weights and morph deltas. Check closure, connected components, volume and reconstruction error before importing anything. Cutting the pelvis slab also captured disconnected forearm remnants and internal geometry; the first export was therefore superseded. Removing those components is appropriate only in this derived collision copy, not in the production body.

Interpolating weights at a cut point is not identical to interpolating already-skinned points. Measure that difference over motion instead of assuming equality. The current regional candidate reaches 0.278 cm across sampled poses. Source provenance, bounds and component decisions are recorded in `skin-regions2/receipt.json`. The pinned [Blender 5.2 BMesh API](https://docs.blender.org/api/5.2/bmesh.ops.html) documents the cut, fill and normal operations used by `prepare_skin_regions.py`.

### Validate the sign before counting clipping

At folded or self-overlapping surfaces, nearest-triangle normal sign can disagree with containment even when two surfaces share the same triangle. In sprint12, one supposed extra contact had body normal distance +4.194 cm but body ray distance -4.194 cm, matching the region. Investigate sign reliability before modifying geometry to fix a count. Use multiple rays, record disagreements, and disclose open reference boundaries. The remaining three sprint60 candidates are near the body and overlap/cut regions; they still need native testing.

### Verify collider roots instead of inferring them from filenames

Pinned UE5.6.1 automatic generation computes a common ancestor of merged bones. Original pelvis influence can keep a thigh-shaped collider rooted at the pelvis. Separating geometry may reduce cross-leg lattice mixing, but that alone does not establish better rigid motion for continuous collision. Export and inspect root, used bones and relative bind transforms before comparing a native regional trial with the rejected whole-body lattice. Do not alter the accepted body weights to force a preferred root.

### Match native triangle validation before import

Closed topology does not rule out tiny sliver triangles. The right-thigh regional cut passed closure but failed CSSImportMesh's float cross-product squared threshold of 1e-12. Three source-copy vertices were welded within 0.001 cm, maximum displacement 0.00093443 cm, removing six collapsed faces while retaining a closed surface. This is less than 0.01 mm, and changes only the private collision copy. The corrected import exits successfully; `region-right-import.log` is the rejected run, `region-right-import2.log` the repaired run. Preserve both receipts and do not loosen native validation to hide the defect.

`export_skin_regions.py` now checks the area threshold and reports any optional weld, requiring every surface edge to have exactly two incident faces. Its `--weld` is capped at 0.001 cm. The retained point supplies its original weights and morph provenance. Larger topology or deformation repairs need their own checks; this small tolerance is not general outfit fitting permission.

## Exported winding and clearance proposals

The Planet interchange uses Unreal clockwise winding. Reflecting Y for Blender already converts the face order; reversing it again makes geometric normals point inward. Check against exported normals before displacement. The first rejected Planet candidate moved 16,812 vertices; corrected authored-normal proposals move 1,639. There are 190 local geometric/authored-normal disagreements among 61,814 body faces, so a global convention check does not make every nearest face reliable. Keep displacements bounded and visually inspect residuals. `planet-fit3` improves rear seat/calf breakthrough but leaves chest, upper-back and footwear issues. It is not a source or game repair yet.

## Match authored visibility before reshaping footwear

Prototype's apparent bare-foot problem repeats an earlier Black Pearl preview mistake. Hiding the entire covered-foot material removes too much calf skin. Evaluate the author's outfit-specific mask instead: original `SuitBMask` maps exactly to the current body, hiding 7,029 faces while preserving the open calf strip in inspected views. Keep those faces in reversible covered sections tied to the garment toggle; do not delete them or change body proportions. Extend original shader and color bindings to duplicated slots. Tail/ribbon sections must be independent from suit material sections before offering independent visibility.

## Verify garment morph deltas, not just key names

Prototype had all six keys but combining documented maximum values produced large sheets and spikes. Neutral fitting and weight normalization did not detect this. Check individual and combined morphs before physics. Rebuilding suit deltas from the unchanged body's nearest triangles removed the large distortion in the inspected combined-max views, and fresh source reload verifies the six arrays. This remains a candidate: check edge clearance, accessories, mixed values and motion rather than declaring all morphs solved from one combination.

An accessory bone's name also does not establish its bind location. Prototype's existing diagnostic tail chain root is 23.59 cm from the garment attachment and extends in a different direction. Gemini's earlier single-tail-bone assignment already caused fan deformation and was removed. Preserve that rejection and measure the chain before weighting it again.

## Unreal material array readback

An indexed Unreal struct can be returned as a copy. For material assignments, modify a local slot, write it back into the array, assign the array, then compare every material reference before saving and again in a fresh editor process. A successful save is not proof the references changed. Prototype caught this in `planet-assembly-check.log`; `apply_planet_materials.py` contains the corrected writeback.
# Check original material panels before calling exposed skin clipping

Skin Suit's original garment contains a separate `MI_EVE_Costume_Temp_Inner_Skin01` panel. Gemini removed those faces, but the original body mask still assumes they exist. Applying that mask to the stripped garment creates holes. Conversely, broad exposed rear-leg skin is intentional in the original design, as shown in `work/eve26/skin-author-panels/back.png`; it must not be inflated over as a clipping fix. Inspect the original garment with material panels distinguished before changing geometry or applying its mask. A footwear close-up cannot validate the full-body mask. Keep authored openings and repair only the demonstrated ankle discontinuity.


### Match geometry checks to importer precision

Skin lining4 passed a permissive area check but failed the native importer. Validate triangle cross products in float32 with the exact native squared threshold (currently `1e-12` in CSSImportMeshCommandlet). Keep seam vertices fixed during local repairs and recheck original geometry, weights and morph deltas. Do not loosen the importer to admit collapsed projected triangles. Lining6 repaired four vertices by less than 0.05 mm and passed the matching check; game motion remains separate acceptance.


### Diagnose a visible triangle patch before more global fitting

Skin Suit sleeve triangle63272 still showed skin after its centroid was moved outside the body. The reviewer's optional `--probe side 225 490` identified the exact body/garment faces and measured a0.515 mm ray overlap. Vertex and centroid checks alone missed the rest of the triangle surface. A quarter-step barycentric grid improved coverage, but independently requiring every corner to fix the entire surface sample overconstrained the patch. F14/F15 did not remove the visible spot and were not promoted.

For the13-vertex neighborhood, the coupled solver uses barycentric surface constraints across all selected vertices and retains each vertex's0.4 cm bound. Against rest and sprint8,24,32,48, each at default and combined-max shapes, F16 changed five vertices with no unresolved local constraints. The inspected max-sprint24 spot disappears; the same ray hits garment0.814 mm before skin. This is local sampled evidence, not whole-outfit collision acceptance. Preserve the original body and inspect neighboring poses before importing.


### Check footwear in a standing pose before diagnosing the neutral export

Vacation Bikini Extras Heels have identical geometry in the author source and Gemini export. Their saved author armature pose does not change it. The neutral export's long heel tips look less unusual during sprint, but a recorded standing pose confirms heel/forefoot contact mismatch. Compare both sole ends in a grounded pose; translating the entire character cannot fix their relative height. Keep separate conclusions for authored geometry, saved rig state, garment fit and actual game floor contact.


### Preserve shoe silhouette when solving contact

The Bikini support-compression trial closes an8 cm neutral support/sole gap but destroys the tall heel profile. It is rejected. Do not equate matching sole heights with a correct fit. Evaluate a heel-pose/footwear adaptation that preserves the design, with separate standing and motion checks.

### Initialize newly added garment morphs explicitly

The Bikini top/shorts omit some shared body morph keys. In these sources, newly added keys started at1; explicitly set new keys to0 before saving replacement deltas. Fresh-load both default values and numerical deltas. No existing mask issue was found in this case.

### Treat rigid shoe alignment as a diagnostic, not a fitted foot

Bikini heelpose1 rotates the original shoes about the foot bind pivots by approximately 34.68 degrees. This preserves the heel silhouette and aligns neutral sole minima, but recorded idle120 shows the unchanged feet outside the shoes. Do not promote this result or apply its measured height offset. A matching foot pose, ankle continuity and posed contact checks are required. Preserve original body proportions and keep the shoe-only trial separate from current garment sources.
