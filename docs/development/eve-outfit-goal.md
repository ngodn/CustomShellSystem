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
