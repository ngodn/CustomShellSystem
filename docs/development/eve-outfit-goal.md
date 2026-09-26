# Eve outfit goal and current status

Updated 2026-09-26, takeover session 1. Read [the evidence index](eve-outfit-index.md) before resuming.

## Active objective

Deliver `CSS_EveStellarBlade_eins0fx_P` with all available Eve outfit variants, properly fitted to the original body proportions, complete applicable clothing customization, and verified clothing and secondary-motion physics. Every outfit needs **Original plus at least five clothing color palettes**. Hair-only recoloring does not satisfy this requirement. The user clarified this on September 26 and reports the latest Gemini outfit work is incomplete.

The user supplied this updated objective and resumed the goal through the conversation goal control. The September 26 fitting milestone is active; do not revert to the older quota-pause instructions.

**Latest priority correction: fitting first.** The user explicitly asked to focus on fitting every outfit to the same standard as Black Pearl. Palette implementation is parked until the fitting foundation is sound. Start with Holiday Reveler, then apply the proven process to the other outfits. Keep all customization requirements queued.

Claude Code owns CSS runtime UI/UX and performance work. This effort owns Eve assets, authoring scripts, verification and documentation. Do not stage, revert or replace the other agent's native changes. Current beta source, local release artifacts and installed binaries must be checked separately; an old installed `release.json` is not reliable version evidence.

Use Gemini's improved custom skeleton as the current baseline, as the user explicitly reminded us during takeover. Gemini's `disk_verification_report.json` records **386 bones, 82 sockets and nine virtual bones** after a fresh editor reload. Its seven additions are `Eredrim_Diapazon`, `Eredrim_Shoulder_l`, `tiel_dagger`, `unrealHelmet1_M`, `unrealBarrel1_R`, `weapon_l` and `weapon_r`. Preserve socket names, parents and transforms as well as the prior camera/hand fixes. This is existing editor evidence, not a new verification of the installed cooked skeleton. Do not rebuild from the older 379-bone baseline or overwrite current shared assets with an old release.

## Acceptance for every outfit

- Preserve the original body proportions and accepted hand, camera, movement and contact repairs.
- Original restores authored textures. Five or more additional palettes visibly recolor the garment, with useful coordinated colors and distinct results.
- Expose applicable garment colors and independent clothing toggles. Shared material sections must not accidentally hide another garment or part of the body.
- Preserve separate skin/anatomical controls and their masks. Clothing palettes must not recolor those regions or silently redirect to hair.
- Clothing fits at neutral settings and across supported body morphs. Check shoulders, underarms, chest, waist, hips, knees and footwear during motion.
- Cloth, hair and body physics behave under idle, locomotion, turns, attacks and transitions. Check collision, excessive stretching, clipping and resets; a successful commandlet log alone does not establish this.
- Profile save/load, switching outfits and Original reset preserve the correct settings independently.
- Review actual game screenshots and short movement recordings. Offline renders support diagnosis but do not replace game verification.
- Package under `/Game/CSS/`, use short readable paths, retain source provenance, and commit by coherent change. Do not publish until the candidate has passed these checks.

## Current evidence

| Capability | Finding | Evidence |
| --- | --- | --- |
| Outfit catalog | Eleven variants exist in generated and installed manifests | `tools/eve_source_audit.py`; local `work/eve26/installed-manifest.json` |
| Black Pearl palette wiring | Six palettes target clothing controls; visual quality remains to be rechecked on current build | Installed manifest |
| Ten added outfits | Each has five palette entries, but all target only hair and hair accessories | Installed manifest; generator diagnosis below |
| Clothing toggles | Some controls share material sections; intent and independent visibility need mesh review | Audit JSON, export material lists |
| Eve animations | Installed definitions include I2 idle and S1 walk/jog/sprint | Manifest definitions, not new playback verification |
| Holiday fitting | Existing Blender renders reviewed; fitting and render/export agreement still need work | Gemini fitted-check renders and scripts |
| Physics | New authoring/runtime work exists; per-outfit cooked configuration and gameplay behavior not yet accepted in this takeover | CSSBindClothV2/V3 sources and Gemini bind scripts |

Known outfits: Black Pearl, Vacation Bikini, Casual Knitwear, Holiday Reveler, Midsummer Alice, Ocean Maid, Planet Diving 6th, Royal Guard, Skin Suit, War Aegis and YoRHa No.2 TypeB. Inventory additional source outfits before declaring this list exhaustive.

## Confirmed palette defect

`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/generate_variants_manifest.py`:

1. `build_variant_controls` removes clothing color controls for added outfits.
2. `make_surfaces` constructs only body and hair dye surfaces.
3. `generate_manifest` copies `suit_color` to `hair_color`, then drops values whose controls were removed.

The installed manifest confirms that result for all ten added outfits. This explains why valid-looking palette names and a count of five did not provide outfit customization. Removing the fallback alone is insufficient: supply reviewed garment material bindings and suitable dye layers, then retain the garment palette values.

Additional risks to inspect: Holiday top/bottom/accessory toggles share sections; the generic hair assignment uses object-name matching and arbitrary first-slot splitting; material staging forces every base-color alpha to opaque. These are source findings, not proof of every visual defect reported by the user. Do not copy Black Pearl's UV masks to unrelated outfit atlases.

The added meshes also have a different material order from Black Pearl. For example, Holiday export slot 2 is `Eve Legs`, while the generator applies the Black Pearl chest/body masks there; slot 5 is `Eve Fingernails`, while its dye surface uses the groin masks. Resolve surfaces by material identity and verify cooked bindings before deployment. The existing `inspect_materials_out.txt` captures an earlier unbound Ocean Maid state, so do not mistake that old WorldGridMaterial listing for the currently installed material state.

## Next implementation steps

1. Repair Holiday Reveler's fitting foundation in an isolated candidate, preserving Gemini originals. The `Variants_Fixed` blend inventory shows all 7,600 sleeve vertices, 3,347 leg-piece vertices, 3,401 underwear vertices and 7,044 Christmas heel vertices without any group assignments. Of 16,496 dress vertices, 14,188 are unweighted. These counts describe that saved blend, not necessarily the earlier cooked game mesh. Establish which source/export is currently installed before replacement.
2. Reuse Black Pearl's body-preserving method: garment-only fitting, stable triangulation, surface-aware weight transfer, supported morph clearance and motion checks. Inspect evaluated shape keys and bind transforms before projection. Validate preserved body geometry and the current 386-bone/socket skeleton; do not globally shrink the body or use indiscriminate nearest-vertex transfer.
3. Compare front, back and side renders, then motion/contact behavior. Keep dress shape, trim and intended loose regions intact. Extend the verified fitting process to remaining outfits, including cloth collision and physics.
4. After fitting, repair name-based material mappings, independent toggles and outfit-specific dye layers; deliver five clothing palettes plus Original and per-outfit save/load checks. Keep accepted Black Pearl behavior as a regression baseline.

Local diagnostic outputs are under `CustomShellSystem/work/eve26/`; source assets remain in the authoring collection. No new asset candidate was installed during the takeover audit. The previous quota-era queue entries below September 26 are history, not the current build state.

## Holiday F1 investigation

The original `eve_beta10.blend` and the authoring master both retain full Holiday dress/sleeve skin weights. The current `Variants_Fixed` has lost them. Gemini's fitting script clears all groups and applies Data Transfer without generating destination layers. Blender documents that the modifier does not create those layers: https://docs.blender.org/manual/en/4.3/modeling/modifiers/modify/data_transfer.html. The saved dress also has a 4.98 mm maximum discrepancy between mesh coordinates and its shape-key Basis, while the source has none.

The original sleeves have the same coordinates as the broken candidate, although the target body's arm pose differs from the original body. Weight repair alone therefore cannot establish a correct fit. F1 reconstructs four garment objects from the intact master using matching body topology and surface correspondence, preserving loose cloth offsets. It directly creates normalized CSS skin weights and keeps shape-key coordinates consistent. Body geometry, morphs and weights are hashed before/after; the Blender rig is checked unchanged. This is a separate offline candidate, not an installed repair.

Probe: `tools/eve-fit/probe_sources.py`, results: `work/eve26/fit-probe.json`. Candidate builder: `tools/eve-fit/holiday_candidate.py`; neutral-material review: `tools/eve-fit/render_holiday.py`. Pending acceptance includes visual silhouette/clearance, morph endpoints, deformation, cloth setup and installed cooked-skeleton preservation.

F1 outcome: all four rebuilt pieces have complete weights and unchanged body/rig checks pass. Front and side review, including the exporter-equivalent no-modifier view, still show chest penetration and distorted sleeves. **Rejected for deployment.** Surface correspondence alone is insufficient. Next inspect the pristine source's evaluated armature/surface-deform/corrective stack and reproduce its intended garment shape before adapting it to the CSS body; do not repeat F1 with arbitrary clearance increases.

Pristine source visual check: `work/eve26/original-visible/front.png` shows a covered chest, correctly shaped collar and coherent loose sleeves when the original deformation stack is retained. The initial `original-views` capture was invalid because wardrobe visibility drivers hid the outfit. The review tool now overrides visibility drivers and links requested pieces into a visible collection without saving the source. Source evaluation logs warn about eight dependency cycles, so explicitly validate the evaluated mesh and modifier dependencies before baking. Next candidate should start from that evaluated garment shape, not raw mesh vertices. Preserve the CSS body rather than importing any outfit-driven source body deformation.

## F2 evaluated-source fitting

`holiday-f2.blend` uses the pristine original, with render-time modifier enablement mirrored into a fixed viewport evaluation, source body/garment topology checked unchanged, and the target CSS body's saved shape-key mix. All four rebuilt parts have complete normalized weights. Body digest remains `3b0dc6afa992ee9921da702d135341856c5695ef51805412a7a99ffd3234d1ec`; the Blender rig is unchanged. The shared Unreal skeleton was not written.

Reviewed `work/eve26/f2-bind/{front,side,back}.png`: the large chest breakthrough from F1 is gone, the collar retains its intended shape, and sleeves are substantially more coherent. A small sleeve skin breakthrough remains visible from the back. This is a better static fitting baseline, not a finished outfit or game-verified asset. Source particle fur is deliberately omitted from these neutral-material checks.

The bounded F3 clearance attempt (`tools/eve-fit/holiday_clearance.py`) stopped before saving: dress iteration zero found 5,303 penetrating triangle/edge/vertex samples within an 8 mm nearest-surface radius; the proposed move exceeded its 8 mm displacement budget. No `holiday-f3.blend` was produced. These are repeated sample counts, not unique vertices, and signed nearest-surface tests can misclassify concave regions. Inspect affected material regions/locations before increasing clearance or changing the silhouette. Do not call F2 intersection-free based on the renders.

Next: locate the clearance violations, correct the sleeve breakthrough and any genuine dress penetration, then transfer/verify supported CSS morphs against the preserved body and test deformation. F2 currently retains transformed source shape deltas with baked default values; those deltas are not accepted as CSS morph conformance. No game installation or release replacement yet.

## Outer-surface clearance and morph preparation

Material-region probe `work/eve26/clearance-regions.json` separates fur/inner-wall warnings from clothing fabric. The `--outer` clearance mode excludes fur roots and faces opposing the nearest body normal, uses 2 mm steps and a 12 mm movement bound. Dress sampled violations fall from 603 to zero in three iterations; underwear from 1,103 to zero in two. Sleeves and leg pieces reach the movement bound with unresolved samples, so this is not whole-outfit acceptance.

**Discard F3b as a fitting baseline.** A fresh morph-build precondition detected mismatch between saved mesh coordinates and shape-key Basis. The clearance writer read key coordinates after modifying the mesh, allowing an offset to be applied twice. F3c snapshots keys first but its exact Basis comparison failed; the next writer constructs Basis directly from fitted coordinates and preserves other key deltas relative to it, with an input alignment check. No game deployment occurred. F2 remains the last visually reviewed baseline until a corrected save passes fresh reload.

`tools/eve-fit/holiday_morphs.py` transfers the six manifest-supported controls (range 0..1) from the preserved CSS body to garment surfaces. Its first run stopped before saving, correctly catching the F3b mismatch. Endpoint and combined-maximum rendering is prepared in `render_holiday.py --morph-review`; it is not yet acceptance evidence. Use Blender `--python-exit-code 1` for subsequent CLI runs, since otherwise script exceptions can return process exit zero.

### Corrected save and F4 morph review

F3d saved after explicitly constructing Basis from fitted coordinates and preserving other keys relative to the captured Basis. The subsequent fresh-load morph build passed exact mesh/Basis checks on all four pieces and produced `work/eve26/holiday-f4.blend`; the body hash remains unchanged. All six body controls were transferred at their manifest-supported 0..1 range.

`work/eve26/f4-morphs` contains 18 renders: four default views, front/side for each individual maximum and front/side for the combined maximum. Reviewed default back, chest maximum front and combined front/side. The visible rear sleeve skin patch from F2 is gone and garters look cleaner at default. **Chest maximum exposes a center split/pinch in the upper band**, plus a tiny front fabric breakthrough. Morph transfer is not visually accepted. Inspect duplicate/seam vertex correspondence and displacement continuity before applying further clearance; do not change the body or hide it to cover the defect. Other individual views were generated but not yet all visually reviewed.

Current best offline candidate is F4 for continued work. Its underlying F3d bounded sample results: Dress passes (max move 5.32 mm), underwear passes (2.08 mm), Arms retains 186 sub-clearance samples at the bound (10 mm accumulated move), Legs retains seven (12 mm). These sample limitations remain despite the improved rear render. Motion, cloth simulation, cooked shared-skeleton preservation and game acceptance remain open. No candidate has been installed.

### F5 morph continuity correction

The seam probe found no expanding gaps among dress vertex pairs within 0.1 mm at rest. It did find connected edges stretching sharply: one edge grows from 0.878 mm to 29.36 mm under `PBMBreastsSize`. This supports a discontinuous transferred morph, not changed garment topology. The proximity check does not rule out wider disconnected seams.

`holiday_morph_continuity.py` limits differences between neighboring garment morph offsets while preserving topology, the default fit and all body data. F5 converges after 289 iterations; maximum connected-edge stretch falls from 33.45 to 2.00. This is a geometric bound, not an aesthetic acceptance threshold.

Reviewed F5 chest maximum front and combined front/side against F4: the central opening closes, but a center crease and a tiny skin-colored point remain. Other individual morph front/side views reviewed so far retain the overall garment silhouette. Default back and quarter views retain loose sleeves and skirt shape. F5 is the current continuity baseline, still offline and not accepted for deployment. Sleeve/leg clearance limitations from F3d remain.

The clearance tool now supports a single morph endpoint and garment part. That mode writes only the selected garment key and asserts that the base, other keys and body remain unchanged. F6 is a chest-only dress clearance trial from F5; its result must be freshly loaded and visually checked before acceptance.

F6 fresh reload and rendering completed successfully. Reviewed chest-maximum front and combined-maximum front: the tiny skin-colored point visible in F5 is gone, and the central opening remains closed. Maximum additional garment displacement is 0.865 mm. The central crease remains visible, so do not describe the maximum-size surface as perfectly smooth. F6 is the current offline fitting baseline. Next inspect the remaining sleeve/leg clearance regions and garment deformation through actual poses, then clothing physics. No game installation or release replacement has occurred.

### Cuff and garter diagnostics

F6's seven-sample outer-fabric region probe reports 187 sleeve and 11 leg sub-clearance samples at the saved default (sample counts can differ slightly from in-memory fitting iterations). Highlighted front/back garment-only renders in `f6-regions` show the worst reported faces mainly at cuff openings and inside the garter. The diagnostic deliberately hides the body to expose internal faces; this is not a fitting workaround or an exported change.

F7 applies another bounded pass to Arms and Legs only. **Rejected as a working baseline:** sleeve failures fluctuate around the original count and again reach the displacement bound; legs also reach the bound with unresolved samples. Do not continue increasing this correction budget. Keep F6 while checking surface containment and folded geometry. `probe_clearance.py --rays` records six-direction ray crossings and first-hit normals for the worst samples to distinguish actual body containment from nearest-surface ambiguity. Motion and cloth validation remain pending.

F6 ray results (`f6-rays.json`) support genuine internal intersections: the 32 sampled entries across four material/body-region groups have odd crossing counts and exiting first-hit normals in all six directions. Some entries repeat triangle vertices, so this is not 32 unique locations. Do not dismiss these as signed-distance false positives. Next inspect the intersecting faces' topology, including possible internal closing faces or long triangles spanning cuff/garter openings, before deciding between local refitting and topology repair. That topology explanation is still a hypothesis, not a finding.

The unrestricted nearest-body vertex probe (`f6-internal.json`) identifies deep triangle-fan hubs: sleeve vertex 1 has 48 neighbors and is about 24 mm inside the body; garter vertex 3321 has 56 neighbors and is about 71 mm inside the thigh, with edges up to 90 mm. All faces incident to these hubs are triangles. The earlier 8 mm sampling radius cannot see the deep hubs and only encounters their triangles near the skin. This explains why that metric could not establish whole-garment clearance. There are also low-degree internal vertices, so removing fan caps alone may not resolve every intersection.

`holiday_open_caps.py` prepares a separate F8 candidate by removing faces incident to internal triangle-only fan hubs (at least 20 neighbors, more than 8 mm inside, longest edge over 25 mm). It preserves vertices, all shape-key coordinates, weights, and surviving material/UV assignments with exact assertions. The body is hashed unchanged. This is a targeted topology trial, not an accepted export. Blender documents the face-only operation in its [BMesh API](https://docs.blender.org/api/5.1/bmesh.ops.html#bmesh.ops.delete); preservation is checked against the actual installed Blender, not assumed from the API.

F8 completed and passed those assertions: 13 sleeve fan hubs account for 449 removed faces; five garter hubs account for 198. Unused hub vertices remain intentionally to preserve all vertex indices and shape-key correspondence. Fresh reload region checks reduce outer-fabric sub-clearance samples from 187 to 46 for sleeves and 11 to four for legs. Reviewed fresh front/back renders retain the visible garment silhouette. The unrestricted vertex probe still finds 21 sleeve and 95 leg vertices deeper than 8 mm, excluding unused hubs; these remaining inner-wall regions are not cleared by the cap removal. F8 is the current topology baseline, pending remaining fitting/morph/motion checks. F9 tries the bounded outer-surface correction again on this changed topology, not a larger budget on F7.

F9 converges after cap removal: Arms reaches zero sampled outer-fabric failures after seven correction steps, Legs after six. Maximum additional moves are 8.43 mm and 8.41 mm respectively, below the existing 12 mm bound. Body hash remains unchanged. This proves that the capped topology obstructed the prior bounded pass; it does not prove every inner surface is clear, nor that motion and physics are finished. Fresh saved-file renders are the next visual gate.

Fresh F9 renders completed. Reviewed default back/quarter and combined-maximum front/side: loose sleeve and skirt silhouettes remain coherent, garters remain fitted, and the earlier chest breakthrough does not return in these views. F9 is now the working candidate for pose/motion validation. Remaining inner-wall intersections require inspection in deformation; passing the restricted outer-fabric sample test is not whole-mesh clearance. Existing `tools/authoring-probes/animations/render_eve.py` provides the verified UE-pose replay math, but is hardcoded to old Black Pearl assets/379 bones and removes covered body sections. Adapt a separate Holiday review tool with explicit rig/bind compatibility checks and visible body geometry; do not overwrite the current 386-bone game skeleton or treat the old replay as current game acceptance.

### Recorded pose replay

`tools/eve-fit/replay_holiday.py` checks the candidate's authoring bind against `work/anim10/batch/target-bind.json`, then applies recorded component transforms with the existing verified UE-to-Blender matrix conversion. It renders the full body and Holiday pieces without saving the asset. The authoring rig has 379 bones; the shared current Unreal skeleton remains untouched at 386. This is an offline garment deformation test, not validation of the current cooked skeleton or cloth simulation.

`work/eve26/f9-idle` replays frames 0, 120 and 240 of the recorded I2 idle (`work/anim16/center-component.json`). Maximum reconstructed bone-position error is 0.000286 cm, with zero reported angular error; source blend hash is unchanged. Reviewed frame 0 front, frame 120 back and frame 240 side. The skirt hem develops sharp steps under the pose despite the clean static fit. Do not accept F9 motion. Measure garment edge deformation and skin-weight transitions, then correct the garment weights without changing the body or animation. The renderer now reports the eight most stretched edges per garment/frame to guide that investigation.

`f9-walk` replays frames 0/10/20/30 from the accepted S1 walk recording (`work/anim14/walk22-component.json`). Reviewed frame 10 front, 20 side and 30 back: pronounced stepped skirt hem and sharp fabric folds persist. At frame 20, connected dress edge 2390/2393 grows from 8.89 mm to 140.57 mm; underwear edge 1399/2361 grows from 2.68 mm to 48.31 mm. Sleeve edges also stretch about six times, while garter edges remain close to their original lengths. These results direct the next repair toward skin-weight continuity on Dress, Arms and Panties.

`holiday_weight_continuity.py` prepares F10: bound changes in the bone-weight vector between connected fabric vertices, adding links for coincident seams within 0.1 mm. Only edges belonging to surviving faces are used, excluding the orphan radial edges left by cap removal. Preserve mesh and morph coordinates and body data exactly. The 15-per-metre gradient bound is a trial parameter, not a release standard. F10 requires comparison against the same recorded poses before acceptance.

F10 saved with body/geometry/morph preservation assertions passed. Dress reaches the 500-iteration limit with 1,404 edges still above the trial tolerance (maximum gradient 17.16 versus target 15); do not claim full numerical convergence. Fresh `f10-walk` replays the same four S1 frames and passes bone reconstruction checks. Across those frames, maximum edge stretch falls from 17.47 to 2.45 for Dress, 6.25 to 1.50 for Arms, and 18.06 to 1.72 for Panties. Garter results are unchanged. Reviewed matching frame 10 front, 20 side and 30 back: abrupt hem steps are reduced and fabric transitions smoother, but the skirt still follows leg motion too strongly. F10 is the current weight candidate, not finished clothing motion. Next refine skirt deformation/cloth anchoring, replay idle/jog/sprint and combat poses, and inspect remaining inner-wall contacts. No asset has been installed.
