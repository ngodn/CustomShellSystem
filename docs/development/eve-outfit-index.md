# Eve outfit evidence index

Updated 2026-09-26. Goal resumed with the new outfit objective. Start here after a context reset.

## Findings to preserve

- Five palette entries do not prove five working garment palettes. All ten Gemini additions currently redirect garment palette colors to hair.
- Preserve the accepted original body proportions, V44 hand repair, camera translation repair and corrected S1 movement while extending outfits.
- The user requires five additional clothing palettes plus Original for every outfit, not five saved looks.
- Claude Code owns concurrent CSS UI/UX and performance changes. Isolate asset work and stage explicit paths only.
- Gemini's improved custom skeleton is the baseline. Its editor reload report records 386 bones, 82 sockets and nine virtual bones; verify the cooked copy separately and never silently revert to the older 379-bone skeleton.
- Latest user priority is outfit fitting, starting with Holiday Reveler. Palette work is parked. `Variants_Fixed` has fully unweighted sleeve/leg/underwear/Christmas heel objects and mostly unweighted dress vertices; do not export it as a repaired candidate without resolving this.

## Failed approaches and traps

- Do not redirect unsupported garment colors to hair. It hides missing customization and produces the reported behavior.
- Do not assume generic object-name matching or first-material selection identifies hair versus accessories.
- Do not rerun Gemini fitting/packaging scripts blindly: some overwrite source blends or delete staging directories.
- Do not accept `saved=1` followed by process termination as proof of a complete successful cook.
- Do not hot reload a DLL for this audit; earlier animation work crashed that way.

## Evidence map

Paths in this table are relative to the workspace root unless linked.

| Artifact | Finding or purpose |
| --- | --- |
| [Current goal/status](eve-outfit-goal.md) | Scope, acceptance criteria, takeover findings and next steps |
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
