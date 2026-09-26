# Eve outfit evidence index

Updated 2026-09-27. Start here after a context reset; the goal log's current checkpoint supersedes historical approaches below.

Before another outfit, read the [fitting playbook](eve-fitting-playbook.md). It distills mistakes, replacement checks and what Planet Diving can reuse without copying Holiday-specific settings.

## Findings to preserve

- Latest Prototype interchange: `planet-export/`, regenerated from F7 with corrected normals/triangulation and preserved UV/color corners. Private Unreal import and fresh hierarchy audit pass (`planet-compat.json`); protected production hashes unchanged. No cloth assets, production binding or game acceptance yet.

- Latest Prototype source: `planet-suit-f7.blend`. Bounded pose-derived offsets change 532 garment vertices by <=0.400 cm; fresh reload error <0.00001828 cm, weights and relative morphs preserved. Frame24 combined-max knee breakthrough is removed in inspected views. `planet-fit7/receipt.json` contains pre-correction hits and skipped larger proposals, not a post-correction all-clear verdict.

- Latest Prototype garment source: `planet-suit-f6.blend`, with repaired six morphs and body-corresponding suit weights above 22 cm. Fresh-load weight error <2.99e-8; geometry/keys unchanged from F5. Sampled upstream sprint frame16 collar gap is closed. Combined-max frames8/24 show remaining small edge/knee breakthrough. No full-motion or game acceptance.

- Latest Prototype garment source is `planet-suit-f5.blend`, with six repaired morphs and a fresh-load receipt. Combined-max inherited morphs produced sheets/spikes; repaired front/back/side views remove that failure. Body/base preserved. The diagnostic tail root is 23.59 cm from the attachment; Gemini already rejected blindly weighting this chain. Read the latest checkpoint before tail authoring.

- Prototype authored `SuitBMask` resolves the footwear preview without blanking the open calf strip. Exact source mask in `planet-body-mask.json`; inspected views in `planet-fit4/source-mask/`. `planet-sections/` adds reversible covered-body slots and independent tail/ribbon sections; tail-hidden rear inspected. Body faces are preserved, not deleted. Production rig/material/color integration, tail physics, morph/motion and game checks remain open.

- Latest Prototype source is `planet-suit-f4c.blend`, fresh-load verified with one garment, eight shape keys and 0.00001532 cm maximum fitted-point error. It has placeholder materials with original slot names for reassembly. Production rig/materials must be reattached; no game acceptance. Earlier two dependency-heavy library copies were removed with a cleanup receipt. Next inspect source outfit masking because blanket covered-foot hiding exposes a calf opening.

- Prototype fit4 improves chest, seat and calf bind-pose coverage in inspected front/back renders. `planet-fit4/` contains the candidate, offsets and receipt; no source blend, import or release changed. Source modifier/driver evidence is `planet-source.json` and `planet-foot-source.json`. See the latest goal checkpoint before further edits.

- **Latest user instruction: skip Holiday. Active order is the existing Planet Diving model, Skin Suit, then others.** Reference checks identify this model as Prototype Planet Diving Suit, not 6th. Its lower-back strip needs a fixed upper attachment and dedicated motion below it; see [installed-game evidence](eve-planet-reference.md). All Holiday experiments below are parked.
- Final Holiday diagnostics reject automatic regional colliders: native frame68 approximate surface gaps 2.14 / 11.38 / 6.75 cm for pelvis/left/right. Independent pose checks pass. `region-native/` contains measurements and inspected views; `region-*-local16*` contains completed offline local-transfer comparisons, not adopted native assets.

- Native preparation now uses `export_skin_regions.py`, isolated SK_CPelv/SK_CThighL/SK_CThighR imports and the guarded `-Mesh` level-set probe. Generation metadata roots all three at pelvis, with 16/10/10 used bones. Smaller volumes are a deformation-partition experiment, not a proven root-motion repair. Fresh native distance and cloth checks remain required.
- Right-thigh import initially rejected microscopic cut slivers. `region-right-fix/receipt.json` records three welded vertices, maximum 0.00093443 cm displacement, six collapsed faces removed and closed edge incidence preserved. `region-right-import2.log` is the corrected successful import. Native area preflight is now in the exporter.

- Current: closed source-weighted pelvis/thigh regions (`skin-regions2/`) preserve source provenance and pass sampled geometry coverage across 62 cases. Three residual garment contact candidates lie within 0.048 cm of the reference body in sprint60. Native deformation, cloth behavior, morph support and cost remain untested. Read the latest goal checkpoint before native authoring.
- Concave/folded surface nearest-normal signs can falsely label embedded garment points as clear. Regional three-ray voting and reference-body ray crosschecks resolve most candidates, but the arm-excluded body is open and 364 queries have disagreeing votes. Do not call this watertight proof or use minimum overlapping-region negative distance as exact penetration depth.
- UE5.6.1 automatic skinned-level-set generation selects a common ancestor root from merged bones (`PhysicsAssetUtils.cpp:391`); a mesh called thigh does not guarantee a thigh root. Inspect actual native root/influences before claiming improved rigid collision motion.

- Current: `region-hulls/coverage.json` tests actual regional rigid hull union coverage over 62 sampled pose/morph cases. It rejects this seven-hull recipe: 3.42/4.47 cm worst undercoverage and avoidable garment overlap. Rear/side overlays inspected. Next investigate separately skinned regional volumes with inherited source geometry/weights, not rigid enlargement.

- Latest backstop audit: worst particle is outside its reconstructed backstop sphere but inside a crossing thigh. `panel-stop-direction.json` is offline reconstruction, not solver-buffer readback. More substeps still fail visually (`panel-stop4-*`). Current next step is regional collision surface coverage, not more scalar tuning.
- Correct timing comparison: CA_Fit used 4 substeps; Follow/Near/Stop/Room used 1. Stop normalized to 4 costs 11.49 ms in one editor run and still clips. Do not claim the earlier 17.39→2.7 ms change isolates a backstop/weight benefit.

- Current follow-up rejects nearest-body garment transfer and 6 cm backstop movement allowance. `garment-transfer/comparison.json` and `panel-room-*` document the regressions. Before another parameter trial, inspect animated backstop normal direction and failing render support attachments. The 3 cm CA_Stop comparison is still unaccepted.

- Latest: `panel-stop-*` completes the interrupted garment-follow/backstop trial. Radius 30, offset 0, movement limit 3 cm improve the visible envelope but leave hem clipping and side-trim distortion. Not accepted. Animation-only frame 68 already has -4.05 cm minimum signed fabric distance; investigate garment deformation before more physics tuning. Read the current goal checkpoint before older collider proposals.
- `rigid-regions.json` measures 582 pose/morph cases: simple dominant-bone following reaches 11.17/11.73 cm error. This is correspondence evidence, not a universal rejection of regional collision.

- Mapped-surface follow-up: `ls-local32-surface/surface.json` traces cross-leg corner weights at right-thigh index 8275. Its 15.27 cm correspondence error is a 2.64 cm nearest-surface gap. `ls-local64-surface/motion.json` completes 582 cases with 8.47 cm worst correspondence; frame-56 surface gap p95 is 0.390 cm, maximum 2.20 cm in the skirt ROI. Inspected subdivision-2 rear comparison and subdivision-4 rear/side mapped surfaces show residual folds. This is approximate mapped triangulation, not native SDF containment. Next native recipe import/query, then cost if fit supports it.

- Complete local-transfer comparisons supersede the selected-three-point result: `ls-local16e/motion.json` and `ls-local32/motion.json`, 582 cases each. Coarse-grid worst local errors are 25.62/27.32 cm; half-sized cells improve every default pose but still reach 15.27 cm. These are correspondence measurements, not signed-distance or cloth acceptance. Inspect sprint frame 56, body index 8275 next. Recipes and native replay reports live beside each report. The fitting playbook records the numerical preflight failures and replacement checks.

- Lattice failure traced to badly mixed influences at three worst thigh samples: `ls128-trace3-audit.json`, `ls-local-weights3.json`. Exact tetrahedron reconstruction passes. Effective left-thigh weight drops from about 72–74% on the body to 14% on the generated lattice, with unrelated arm/finger/spine weights. Local triangle-based weight transfer on the same corners reduces their 23.5–24.2 cm errors to 1.2–1.5 cm. Native corner replay is independently verified. Next test full-grid local transfer before claiming a fix; no new collider was authored by that counterfactual.

- Private body-only level-set generation: `cbody-source.json`, `cbody-inspect.json`, `cbody-levelset.json`, `cbody-readback.json`. The export preserves 36,787 body points, 61,814 faces, weights and 22 morphs; saved import retains all morph names. Generation and fresh-load metadata match for a 33-bone collider. Coverage, deformation, morph support and solver cost are still unverified. Never replace the shared skeleton with the private 379-bone import. See `tools/eve-fit/native/README.md` for replay commands.

- User's September 27 outfit order: Holiday Reveler, then Planet Diving 6th (planet suit), then the others. All outfit requirements remain active.
- Late-sprint native collider penetration responds strongly to 16 substeps, but that costs about 71 ms and still clips render surfaces. Ordinary UseCCD does not cover the separate skinned-triangle constraint path. Keep this as diagnosis, not a release setting.
- Current private reference: F12 / SK_Waist / CA_Fit, full body-joint6, contact 0.3 cm. Repaired render attachments pass rest reconstruction; hip/arm fitting and physics cost remain unresolved. No production deployment.
- Identical reported full-collider runs differ by up to 3.559 cm, yet frame-64 clipping remains nearly unchanged. Do not attribute a smaller candidate's 1.977 cm trajectory difference to its geometry alone or demand exact particle reproducibility as fitting acceptance.
- Skirt motion must retain the fitted garment's original leg-follow deformation. Same-pose upstream renders expose clipping from the old four-chain weights before physics. `CR_HolidayFollow` and its paired export preserve original influence values through eleven existing carrier bones; direct rig execution passes 291 recorded poses. This is the current base for adding dynamics, not finished cloth.
- Five palette entries do not prove five working garment palettes. All ten Gemini additions currently redirect garment palette colors to hair.
- Preserve the accepted original body proportions, V44 hand repair, camera translation repair and corrected S1 movement while extending outfits.
- The user requires five additional clothing palettes plus Original for every outfit, not five saved looks.
- Claude Code owns concurrent CSS UI/UX and performance changes. Isolate asset work and stage explicit paths only.
- Gemini's improved custom skeleton is the baseline. Its editor reload report records 386 bones, 82 sockets and nine virtual bones; verify the cooked copy separately and never silently revert to the older 379-bone skeleton.
- Latest user priority is outfit fitting, starting with Holiday Reveler. Palette work is parked. `Variants_Fixed` has fully unweighted sleeve/leg/underwear/Christmas heel objects and mostly unweighted dress vertices; do not export it as a repaired candidate without resolving this.

## Failed approaches and traps

- Seven regional rigid convex hulls (mass >=0.25, Z75..142) fail actual surface coverage and garment clearance. `region-hulls/*` gives measurements and images. Do not interpret successful generation or much smaller point error as accepted collision fitting; native assets were deliberately not created.

- CA_Room increases CA_Stop movement allowance 3→6 cm with unchanged source proxy and backstop. Frame-68 deepest mapped intersection worsens -4.61→-6.34 cm, recorded max edge ratio 10.49→16.24, and inspected side trim spikes worsen. Reject increased allowance alone; `panel-room-*`.

- Lower-garment nearest-body weight transfer is rejected: `garment-transfer/comparison.json`, 62 sampled cases, worst -5.68 cm versus -4.31 cm baseline. Rest-nearest weights can remain pelvis-driven while a moving thigh intersects the fabric. Geometry/body/morphs were preserved; rear/side renders inspected. Do not repeat this as an assumed hem fix.

- Native whole-body Local64 recipe, with and without CCD: rejected by actual CA_Fit cloth renders. CCD prevents frame-64 collapse, but frame-68 thigh breakthrough remains and dense failing samples rise to 25,479 versus the earlier repeat baseline 17,245. See `lrecipe-cloth*`, `lrecipe-ccd*`, `lrecipe-querytrace.json`. Accurate saved weights and improved direct lattice mapping do not prove correct inverse collision queries. Do not repeat resolution/CCD alone. Current-geometry intersection uses old/current rigid transforms; investigate regional motion representation next.

- Whole-body level-set 128/16 is rejected as an animated collider. Finer SDF resolution reduced resting skirt-region undercoverage, but frame 68 has 24.18 cm direct lattice mapping error and 30.69 cm maximum positive signed-distance sample. Independent body skinning passes within 0.000390 cm. Evidence: `ls-rest*`, `ls128-rest*`, `ls128-f68*`, `ls128-map68*`; rear and side heatmaps inspected as recorded in the goal log. Inspect lattice weight distribution next, not a blind resolution increase. No cloth/game trial used this collider.

- Single locally scored attachment is not adopted: lower clipping counts at frames 60/64/68, but a larger visible hip patch at frame 68. `panel-localmap-*`, `map-{base,local}-68-views` are hypothetical replay only, not native assets. Do not equate sample-count improvement with visual acceptance.
- CA_Hip centroid refinement is rejected: unchanged rest surface but worse mapped clipping, edge ratio 7.919 and roughly double editor solver cost. See `panel-hip-motion.json`, `panel-hip-clearance.json`, and inspected `panel-hip-views`; do not repeat density increases alone as an untested fix.
- Holiday AnimDynamics trials are rejected. Equal-axis inertia removes the numerical explosion, but moving simulated centers onto the fitted dress still produces 18.30 cm jog displacement and visible hip clipping. Do not repeat spring/inertia/center tuning. Next adapt the existing constrained ControlRig approach, with cloth-specific geometry and morph checks.
- Do not redirect unsupported garment colors to hair. It hides missing customization and produces the reported behavior.
- Do not assume generic object-name matching or first-material selection identifies hair versus accessories.
- Do not rerun Gemini fitting/packaging scripts blindly: some overwrite source blends or delete staging directories.
- Do not accept `saved=1` followed by process termination as proof of a complete successful cook.
- Do not hot reload a DLL for this audit; earlier animation work crashed that way.

## Evidence map

Paths in this table are relative to the workspace root unless linked.

| Artifact | Finding or purpose |
| --- | --- |
| `CustomShellSystem/tools/eve-fit/review_outfit_export.py`, `work/eve26/planet-first/` | Planet exported bind-pose review: visible chest/back/seat/calf breakthrough; all parts weighted; intentional side openings preserved |
| `CustomShellSystem/work/eve26/region-native/`, `region-*-trace.json`, `region-*-local16*/` | Parked Holiday native deformation failure and offline counterfactuals; do not continue after user's priority switch |
| `CustomShellSystem/tools/eve-fit/export_skin_regions.py`, `work/eve26/region-import/`, `region-right-fix/` | Private native mesh serialization and bounded cut-sliver repair; first right-thigh JSON rejected |
| `CustomShellSystem/work/eve26/region-*-create.json`, `tools/eve-fit/native/CSSEveLevelSet.inl` | Regional generation metadata and guarded mesh selection; all automatic roots remain pelvis |
| `CustomShellSystem/tools/eve-fit/prepare_skin_regions.py`, `work/eve26/skin-regions2/` | Closed regional surfaces with original body provenance, interpolated weights and recorded discarded components |
| `CustomShellSystem/work/eve26/skin-regions2-parity/`, `skin-regions2-trace12b/`, `skin-regions2-trace60/` | Sampled geometry coverage, corrected sign diagnostics and the final three near-body overlap candidates |
| `CustomShellSystem/work/eve26/skin-regions2-check/views48/`, `skin-regions-protected.json` | Inspected regional silhouette and unchanged protected asset hashes after interruption |
| `CustomShellSystem/tools/eve-fit/audit_region_hulls.py`, `render_region_hulls.py`, `work/eve26/region-hulls/` | Offline regional rigid hull union fails body coverage and overfills otherwise clear garment points across sampled poses |
| `CustomShellSystem/tools/eve-fit/audit_backstop_direction.py`, `work/eve26/panel-stop-direction.json` | Reconstructed sphere clearance and animated-normal comparison at clipped particles |
| `CustomShellSystem/work/eve26/panel-stop-support68.json`, `panel-stop-attachments68.json`, `panel-stop4-*` | Saved movement-map support audit, attachment amplification evidence, rejected normalized four-substep trial |
| `CustomShellSystem/tools/eve-fit/prepare_follow_proxy.py`, `work/eve26/panel-follow-proxy.json` | Explicit source-garment weight trial, 289 changed proxy rows, unchanged geometry/topology |
| `CustomShellSystem/work/eve26/panel-stop-comparison.json`, `panel-stop-clearance68.json`, `panel-stop-views68/`, `panel-stop-protected.json` | Completed backstop trial, inspected residual fit defects, original assets unchanged |
| `CustomShellSystem/tools/eve-fit/audit_rigid_regions.py`, `work/eve26/rigid-regions.json` | Full recorded pose/morph check of dominant-bone approximation |
| `CustomShellSystem/tools/eve-fit/audit_capsule_coverage.py`, `work/eve26/capsule-motion-bands.json` | Original primitive recipe undercovers even lower hip/thigh bands across seven poses and combined morph; arm-dominated points excluded |
| `CustomShellSystem/tools/eve-fit/fit_secondary_spheres.py`, `work/eve26/secondary-coverage.json` | Four rest-inscribed secondary-bone spheres barely improve coverage and slightly worsen one garment-overlap case; not imported or accepted |
| `CustomShellSystem/tools/eve-fit/trace_panel_contact.py`, `work/eve26/panel-contact-history.json` | Frame-end dynamic contact history and six-ray checks establish deep late-sprint penetration; no internal solver readback |
| `CustomShellSystem/work/eve26/panel-step16-contact.json`, `panel-step16-clearance.json`, `panel-step16-views/` | More temporal sampling reduces deep particle penetration but costs too much and leaves visible surface clipping; side/rear inspected |
| `CustomShellSystem/work/eve26/panel-support-trace.json` | 55 supports traced at frame 64; dynamic support vertices clear collider within 1 mm, one fixed support penetrates; radius checks do not justify global loosening |
| `CustomShellSystem/tools/eve-fit/reweight_panel_mapping.py`, `work/eve26/panel-localmap-preserved.json`, `map-local-68-clearance.json` | Isolated attachment counterfactual improves counts but fails visual comparison; no native implementation or installation |
| `CustomShellSystem/work/eve26/panel-repeat-trace.json`, `panel-hip-trace-summary.json` | All 19 distinct clipped dynamic lower vertices have positive skin-only clearance; preserve fitted shape and address cloth contact separately from 56 fixed upper arm-contact vertices |
| `CustomShellSystem/tools/eve-fit/refine_panel_hips.py`, `work/eve26/panel-hip-prepared.json`, `panel-hip-mapcheck.json` | Private centroid refinement preserves rest surface, original weights and anchors; fresh native mapping passes, motion acceptance required |
| `CustomShellSystem/tools/eve-fit/compare_panel_repeat.py`, `work/eve26/panel-repeat-comparison.json` | Strict reported-setting comparison and hashed 69-frame repeatability evidence; cause of variation unproven |
| `CustomShellSystem/work/eve26/panel-repeat-clearance.json`, `panel-repeat-views/` | Repeated full-collider frame 64 still clips, with nearly unchanged sample counts; side/rear renders inspected |
| [Current goal/status](eve-outfit-goal.md) | Scope, acceptance criteria, takeover findings and next steps |
| `CustomShellSystem/work/eve26/follow-component.json`, `follow-skinning-verified.json`, `follow-component-views` | Original vs copied secondary graph comparison over 291 actual compressed frames; exact non-skirt local poses, carrier error below 0.000576 cm, two inspected jog/morph side views |
| `CustomShellSystem/tools/eve-fit/native/holiday-follow.patch` | Exact private mesh/graph probe extension; source backups remain in work/eve26 |
| `CustomShellSystem/work/eve26/jog-fit-upstream`, `jog-bones-upstream`, `jog-driver-views` | Same-pose fitting vs old skirt weighting, then rejected average-driver approximation |
| `CustomShellSystem/tools/eve-fit/prepare_skirt_follow.py`, `verify_skirt_follow.py` | One-to-one carrier export and offline skinning preservation check with default/combined morph geometry |
| `CustomShellSystem/work/eve26/skirt-follow-evaluated.json`, `jog-follow-views` | Fresh saved-rig direct execution over 291 poses; separately inspected offline fitted-coverage render |
| `CustomShellSystem/work/eve26/motion-controls.json`, `motion-inertia.json`, `motion-cube.json` | Force controls isolate initial inertia failure and residual contact displacement; no accepted motion |
| `CustomShellSystem/work/eve26/skirt-rest-contacts.json`, `skirt-body-centers.json` | Initial center/body overlap measured; geometry-derived reference clearance does not ensure animated clearance |
| `CustomShellSystem/work/eve26/holiday2-motion-verdict.json`, `jog-centers-views`, `holiday2-protected.json` | Second candidate rejected in jog; inspected actual-pose render and protected asset hashes |
| `CustomShellSystem/tools/eve_source_audit.py` | Distinguishes palette count from declared garment binding coverage; not a visual acceptance test |
| `CustomShellSystem/tools/authoring-probes/eve_inventory.py` | Read-only Blender object, material, weight and skeleton inventory |
| `CustomShellSystem/work/eve26/installed-manifest.json` | Metadata extracted from the installed Eve pak |
| `CustomShellSystem/work/eve26/installed-audit-v2.json` | Installed manifest semantic palette audit |
| `CustomShellSystem/work/eve26/blend-inventory.json` | Gemini Variants_Fixed blend inventory, with source digest |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/generate_variants_manifest.py` | Removes garment controls and remaps suit palettes to hair |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/disk_verification_report.json` | Gemini's fresh editor reload evidence for 386 bones, 82 sockets and nine virtual bones |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/sockets_and_bones_payload.json` | Added bone transforms/modes and socket definitions; preserve when exporting |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/fit_holiday_reveler_mastery.py` | In-place fitting and weight transfer; preserve source before adapting |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/renders_xmas_fitted_check` | Offline front/rear fitting evidence, not game material verification |
| [Camera repair](animation-camera-repair.md) | Prior accepted riposte/trap framing correction |
| [Modding colors](../modding/colors.md) | Dye layer format and material binding contract |

References already reviewed: local cloth-physics-guide, MustardUI README and CommanderWhite Daz/MHX addon inventory. MustardUI is an authoring interface, not proof of Unreal cloth export support. Use matching UE 5.6.1 source for Chaos implementation details.

## Fitting reconstruction evidence

- `CustomShellSystem/work/eve26/fit-probe.json`: intact original/master weights compared with the damaged saved blend; different source/target arm rest geometry.
- `CustomShellSystem/tools/eve-fit/holiday_candidate.py`: F1 reconstructs dress, sleeves, leg pieces and underwear from intact master geometry onto the target body.
- `CustomShellSystem/work/eve26/holiday-f1.json`: all four parts weighted; body geometry, morphs and weights unchanged; Blender rig unchanged. Offline candidate only.
- `CustomShellSystem/tools/eve-fit/render_holiday.py`: neutral-material review; `--bind` matches export's saved-fit-key/no-modifier policy. Do not confuse authoring pose previews with exported bind geometry.
- `CustomShellSystem/work/eve26/f1-views`: initial authoring pose views expose remaining chest penetration and sleeve silhouette problems. F1 is not accepted for deployment.

- `CustomShellSystem/work/eve26/original-visible`: pristine source with wardrobe visibility enabled. Reviewed front shows correct chest coverage and loose sleeves with the original deformation stack. This is the intended shape reference for F2. The source logs eight dependency-cycle warnings; evaluate and verify explicitly before baking.
- Do not reuse `original-views` as garment evidence: source visibility drivers hid the outfit in that first capture.

- `CustomShellSystem/work/eve26/holiday-f2.json` and `f2-bind`: evaluated-source candidate, unchanged body/rig, full garment weights, visually improved chest/sleeves. Current best offline baseline; a small sleeve clip remains.
- `CustomShellSystem/work/eve26/holiday-f3.log`: bounded clearance rejected dress displacement before saving. Inspect failing regions, do not blindly raise the threshold.
- `CustomShellSystem/work/eve26/evaluation.json`: initial source viewport modifier probe; F2 subsequently uses render-time enablement, so this probe alone does not describe F2's modifier state.

- `CustomShellSystem/work/eve26/clearance-regions.json`: material/body-region breakdown of negative signed samples; distinguish fur and inner walls from fabric.
- F3b failed fresh Basis validation. Do not use it or the nonexistent F4 from that failed build. F3c also stopped before saving. Current clearance writer snapshots keys, writes Basis directly and preserves relative deltas.

- `CustomShellSystem/work/eve26/holiday-f3d.json`: corrected clearance save. Fresh reload in the successful F4 build passed exact Basis/mesh equality; supersedes failed F3b/F3c candidates.
- `CustomShellSystem/work/eve26/holiday-f4.json`, `f4-morphs`: six transferred controls, unchanged body, 18 generated views. Default back improves; maximum chest reveals a central upper-band split. Fix seam/displacement continuity next; no morph acceptance yet.

- `CustomShellSystem/tools/eve-fit/probe_morph_seams.py`, `work/eve26/seam-probe.json`: near-duplicate gaps and connected-edge stretch. F4's worst chest edge stretches 33.45 times its base length; no expanding gaps found within the narrow 0.1 mm proximity test.
- `CustomShellSystem/tools/eve-fit/holiday_morph_continuity.py`, `work/eve26/holiday-f5.json`, `f5-morphs`: chest displacement continuity correction, unchanged body/default fit. Reviewed front closes the center opening; a crease and tiny skin point remain. Current continuity baseline, not deployment-ready.
- `CustomShellSystem/work/eve26/holiday-f6.json`: dress chest-key-only clearance trial, maximum additional move 0.865 mm. Four sub-clearance samples reduced to zero, with default mesh and other keys preserved. Sparse nearest-surface samples do not establish absence of all intersections; fresh visual review required.
- `CustomShellSystem/work/eve26/f6-morphs`: fresh-load renders. Chest and combined front checks no longer show F5's tiny skin breakthrough; center opening remains closed. F6 supersedes F5 as the working fitting baseline, with center crease, sleeve/leg sample limitations and motion/physics validation still open.
- `CustomShellSystem/work/eve26/f6-clearance-regions.json`, `f6-regions`: seven-sample outer-fabric region report and garment-only diagnostic highlights. Worst faces cluster inside cuffs and garter lining.
- `CustomShellSystem/work/eve26/holiday-f7.json`: rejected further Arms/Legs clearance trial. Reaches displacement bounds without convergence. Keep F6; do not repeatedly raise clearance budgets.
- `CustomShellSystem/work/eve26/f6-rays.json`: six-direction crossings and first-hit normals support genuine containment at the sampled failing points. Inspect intersecting topology next; inner closing faces are a hypothesis only.
- `CustomShellSystem/tools/eve-fit/probe_internal_faces.py`, `work/eve26/f6-internal.json`: unrestricted vertex distances expose deep fan hubs missed by the 8 mm search. Garter hub 3321 lies about 71 mm inside the thigh and connects to 56 neighbors.
- `CustomShellSystem/tools/eve-fit/holiday_open_caps.py`, `work/eve26/holiday-f8.json`: removes internal fan faces while asserting unchanged body, vertex/key coordinates, weights and surviving UV/material assignments. F8 supersedes F6 as the topology baseline.
- `CustomShellSystem/work/eve26/f8-clearance.json`, `f8-internal.json`, `f8-morphs`: fresh reload checks. Front/back retain silhouette; outer-fabric warnings reduce to 46 sleeve/four leg samples. Deeper inner walls remain. No motion/cloth/game acceptance yet.
- `CustomShellSystem/work/eve26/holiday-f9.json`: bounded sleeve/leg clearance now converges with fan caps removed. Zero sampled outer-fabric failures; additional movement at most 8.43 mm. Body unchanged, inner surfaces and motion still need review.
- `CustomShellSystem/work/eve26/f9-morphs`: fresh default back/quarter and combined front/side visually reviewed. Current working candidate for pose/motion validation; no game installation.
- `CustomShellSystem/tools/eve-fit/replay_holiday.py`, `work/eve26/f9-idle`, `f9-walk`: compatible authoring-rig replay with bone reconstruction checks and visible body. F9 fails deformation review: sharp skirt hem steps and extreme connected-edge stretching. Current 386-bone cooked skeleton is untouched.
- `CustomShellSystem/tools/eve-fit/holiday_weight_continuity.py`: F10 trial smoothing bone-weight discontinuities across connected fabric and coincident seams. Geometry/morph preservation asserted; recorded pose comparison required.
- `CustomShellSystem/work/eve26/holiday-f10.json`, `f10-walk`: fresh four-frame comparison substantially reduces edge stretch and visible hem steps. Dress correction did not fully converge and the hem still follows legs too strongly. Current working weight candidate, with broader motion/cloth acceptance open.
- `CustomShellSystem/work/eve26/f10-jog0`, `f10-sprint0`: three-frame S1 replay checks with matching bind, reviewed selected front/back/side views. Skirt still follows thighs strongly; sprint dress-edge stretch reaches 4.06. Not cloth simulation evidence.
- `CustomShellSystem/tools/eve-fit/audit_ue_cloth.py`, `work/eve26/ue-cloth.json`: fresh saved-editor mesh audit finds no bound Chaos clothing assets on Black Pearl, Holiday or YoRHa, despite assigned body physics and secondary ABP. Does not establish installed cooked contents.


Holiday local hip correction: `work/eve26/hip-patch-surfaces.json`, `hip-patch-nearest.json`, `hip-offsets.json`, `hip-fit-morphs.json`, `hip-fit-views/`, `holiday-f11.json`, and fresh `holiday-hip-source.mesh.json`. Scripts: `tools/eve-fit/{probe_hip_patch,prepare_hip_clearance,apply_hip_clearance,verify_hip_export}.py`. Geometry/morph comparisons pass; influence comparison remains intentionally failing pending the source weight audit. This revision is not installed.

Weight cleanup follow-up: `work/eve26/source-weights.json` reproduces old cleanup differences despite identical saved garment weights. `source-weights-stable.json` passes for both blends using `tools/eve-fit/clean_weights.py`, tested by `check_source_weights.py`. Eve's export wrapper records the cleanup hash. Fresh full exports through that wrapper are the next validation step; old output is not silently accepted.


Fresh corrected exports now pass: `holiday-base-clean.mesh.json`, `holiday-hip-clean.mesh.json`, receipts and `hip-source-verified.json`. This supersedes the pending-export statements above. Combined weight trial: `holiday-hip-bones.mesh.json`, `hip-skirt-bones.json` (zero discarded influence mass), and reviewed `hip-bones-views/`. Nine local region/morph cases pass in `hip-clean-morphs.json`. Full assembly, actual physics and game validation remain open.


Combined motion trial (rejected): `motion-create.log`, `holiday-dynamics-saved.json`, `holiday-{walk,jog,sprint}-motion.json`, and `holiday-motion-verdict.json`. Graph preservation/import succeed; actual skirt motion diverges at frame one. Existing assets protected by `motion-protected.json`. See the latest goal checkpoint and older `next-gen-dynamics-investigation.md` before changing spring targets or inertia. No simulation render, cook or deployment acceptance.


Broader sprint contact and hem feasibility: `follow-clearance.json`, `follow-sprint48-rays.json`, `holiday-hem.mesh.json`, `holiday-hem2.mesh.json`, `skirt-{hem,hem2}.json`, `sprint48-{hem,hem2}-verified.json`, and `hem2-{contact,radial,independent}.json`. The independent trial reduces deep hem penetration in one static pose but is not a solver or accepted fitting. `hip-sprint48.json` attributes residual upper-hip contact to the swinging left forearm; `hip-sprint48-noarms.json` isolates torso/leg clearance. See the goal log before editing hip geometry. Render tooling now supports rear views and lower-dress framing. Body, source blends, shared skeleton and installed assets remain unchanged.


Persistent contact projection rejected: `tools/eve-fit/probe_hem_sequence.py`, `work/eve26/hemseq2.json`, `hemseq2-pose.json`, and inspected `hemseq24-views/default-rear.png`. Across 65 sprint frames it produces correction jumps up to 8.392 cm and edge ratios up to 13.323. Do not mistake successful contact in a single frame for a usable solver. First-run report serialization failure is fixed; fresh rerun succeeds with identical pose output.


Joint shape/contact carrier test: `hemshape.json`, `hemshape-pose.json`, and inspected `hemshape8-views/default-side.png`. Smoother than contact-only but still clips and stretches, not accepted. Refreshed `skirt-f11.json` and `skirt-f11.audit.json` preserve current F11 positions/weights and include source vertex IDs for a direct fabric-surface reference. Existing panel assets and original source blends remain unchanged.


Direct fabric reference: `probe_fabric_surface.py`, `fabric1.json` (initial velocity artifact), `fabric2.json` (settling fixed), `fabric3.json` (surface probes added), inspected `fabric1-views/default-side.png`. Vertex clearance hides triangle/edge penetration; frame 8 has 168 failing surface samples despite passing vertex clearance. Renderer `--surface-motion` enforces matching motion/frame/morph. Cloth remains unaccepted and trim attachments are pending.


Surface-contact and anchoring audit: `fabricface.json` (4x8), `fabricsmall.json` (16x2), `fabricwaist.json` (waist pins), inspected `fabricwaist8-views/default-side.png`, and `bodytrace.json`. Persistent hip-pinned edges explain some unsatisfiable contact; waist pins improve a selected frame but the sequence still has severe stretch/speed spikes. No trial accepted. Source vertex IDs and exact comparisons are in the goal log.


Arm-spike attribution: `fabrictrace.json`, `fabrictrace-drivers.json`, `fabrictorso.json`. Right-upper-arm contact against almost-pinned waist cloth causes the largest corrections. Diagnostic arm exclusion reduces motion spikes but whole-body clearance still fails. Attachment renders `fabrictorso-attached` and `fabrictorso-rotated` were inspected and rejected for folding/spikes; do not deploy renderer mappings as cloth attachments.


Arm/torso check and numerical controls: `audit_arm_clearance.py`, `arm-torso.json`, `fabricvel.json`, inspected `fabricvel4-views/default-side.png`, and `fabricmass.json`. Exact traced arm vertices are outside the torso subset; no body/pose correction is justified by that sample. Velocity correction and equal free-particle masses both fail motion/shape acceptance. Do not repeat these as untested fixes or deploy the Python reference.
