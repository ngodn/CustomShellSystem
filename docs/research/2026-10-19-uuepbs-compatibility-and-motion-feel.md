# UUEPBS compatibility and body motion feel

Trigger: Nexus comment by questman, 19 Oct 2026, on the CSS page: animation transitions much smoother; asks for UUEPBS compatibility (few adjustable body parts, especially the chest) and reports the new physics "feels stiff rather than smooth" even after tuning.

UUEPBS = Universal Unreal Engine Player Body Sliders (XTGMods, https://github.com/XTGMods/UUEPBS), cloned read-only at `CSS-eins0fx-collections/reference-tools/UUEPBS` (v2.6.1, commit 7c54929). Not installed in the game on 19 Oct.

## What UUEPBS does, as read from its source

- Pose edits: it finds `FinalizeBoneTransform` in the skeletal mesh component vtable (`source/UUEPBS/src/core/resolver.cpp`) and, after the engine has written the final component-space pose each frame, scales, rotates and moves the requested bones in place and rebuilds the dependent hierarchy (`core/sculpt.hpp`, header comment; `sculpt.cpp:391-409` composes the edit onto the animated local transform). Nothing is cumulative. So its edits sit ON TOP of whatever the CSS post-process AnimBP produced, springs and rigs included. Scale multiplies the animated bone; rotate and move compose with the animated local pose.
- Simplified Panel sliders bind bones by name through `mod/UUEPBS/Scripts/BoneDictionary.json` (31 groups, prefix/suffix stripping, `*`/`#` patterns). A slider appears only when the skeleton has a matching bone. Per-game overrides go in `Scripts/GameProfiles/<project>.json` as `BodyGroups` (real bone names) and `MorphGroups` (morph name patterns become extra sliders).
- Detailed Panel > Morphs lists every morph target on the tracked meshes, sliders 0..1 (typed -2..2). A moved morph replaces the game's value and is re-applied when the game writes it again.
- Game profile per project folder (`MortalShell2`), `Target` defaults to the controlled pawn; a pose-hook pin (`Slot`, `Buffers`, `ReadIndex`) can be shared per game. Meshes are rescanned on F7, level change, new pawn or destroyed mesh.

## Why CSS bodies show few chest parameters

1. Bone names. `SKEL_Base` chest helpers are `brust001` and `brust002` (German, leaf bones under `spine_04`, carrying 341 weighted body vertices each per `CSS-Mod-Authoring/docs/next-gen-body-dynamics.md`). The Breasts group patterns are `boob, breast, bust1, bust, pectoral, breastbase, chestbreast, *breast, breast*, boob*, bust*, *oppai*, *rufang*, *breast*`. `brust` matches none, so the Breasts slider never appears on a CSS body. `butt001/002` match `butt#`, `belly` matches `belly`, `thigh_l/r` match `thigh`, `spine_02` cleans to `spine02` (Rib cage), `spine_01` to Waist, `clavicle_l/r` to Shoulders. The chest is the one region that is silently missing.
2. Morph count. Every released eins0fx body carries exactly six morphs (`PBMBreastsSize, PBMGlutesSize, PBMHipSize, PBMWaistWidth, PBMThighsTone, FBMBodyTone`), because the body solver's measured model is built on those six (`next-gen-body-morph-layout.md`). UUEPBS therefore offers one chest morph. Stock shells carry none.

## Compatibility plan

A. Bone naming, two layers:
- Upstream: propose to XTGMods that the Breasts group also accepts `brust`, `brust#`, `*brust*` (German is used by several rigs). One-line dictionary change; file an issue or PR with the SKEL_Base bone list as evidence.
- Ours, immediately: ship and document a Mortal Shell II game-profile snippet players paste into `Scripts/GameProfiles/MortalShell2.json`:
  `"BodyGroups": {"Breasts": ["brust001", "brust002"]}` plus `MorphGroups` for our morph names. Put it in the CSS README/Nexus description and in each package README. A later SKEL_Base revision could add real `breast_l/r` root bones, but that is a Shared Assets release with re-skinning; not needed for sliders.

B. Morph set expansion (per package, Blender lane): add a chest shaping set and a few others so both UUEPBS's Morphs tab and CSS CUSTOMIZE have real controls. Candidate names follow the DAZ Genesis 8 vocabulary the six already use: `PBMBreastsCleavage, PBMBreastsDiameter, PBMBreastsPerkSide, PBMBreastsUpwardSlope, PBMBreastsNatural, PBMBreastsImplants, PBMBreastsShape01..04, PBMNipplesSize, PBMNipplesDepth, PBMAreolaeSize, PBMAreolaeDiameter, PBMChestWidth, PBMUnderbustDepth`, and for the rest `PBMGlutesWidth, PBMGlutesUpDown, PBMHipBoneSize, PBMThighsSize, PBMCalvesSize, PBMShouldersSize, PBMWaistDepth`. Source deltas: the G8F morph library if we have it, else authored in Blender from the Peony body (Eve), the Genessa W3 body and the Commander White body. Rules: ranges 0..1 (negative allowed where the DAZ morph is bipolar; UUEPBS types -2..2), neutral at 0, overlay or garment meshes carry the same names (surface-deform bake), `check_shapes` on export.
- Solver model: the body solver's contact envelopes are measured at the six-morph corners. Extra morphs that move the chest surface need either (a) inclusion in the measured corner set (`build_body_runtime_geometry.py` extended to more axes; cost grows as 2^n, so pick the three to four chest morphs that change support the most), or (b) a documented tolerance. Decide after measuring the surface displacement of each candidate.
- CSS controls: new `shape_*` rows (role figure) in each package's customize block; 128-control limit allows it.

C. Runtime interplay and documentation:
- Morph writes: CSS `WornItems::sync_morphs` (`native/src/engine.cpp:1847-1860`) writes `SetMorphTarget` on wear and customize changes; UUEPBS re-applies its value when it notices. Document: a morph moved in UUEPBS wins while its slider is set; reset it in UUEPBS to hand it back to CSS. No code change needed.
- Rescan: CSS swaps the skeletal mesh asset on the same component; UUEPBS must rescan (F7) after changing an outfit or variant to list the new mesh's morphs. Document. Optional later: a CSS-side `uuepbs rescan` console call after a wear, if a safe path exists through UE4SS.
- Scaled helpers vs contact envelopes: a breast scaled by UUEPBS can intersect the chest envelope CSS measured on the unscaled mesh. Expected and cosmetic; note it.

D. Live test (needs the user's go to install a mod into the game): install UUEPBS beside CSS, open F6 on Eve, Genessa, Commander White and tHorny Proxima, record which Simplified sliders appear per outfit, confirm springs keep moving under a scaled chest, confirm the hook pin for `MortalShell2`, and capture the profile snippet. Then write the CSS doc section "Using UUEPBS with CSS outfits".

## Why the new physics feels stiff

- The body rigs are `kind: rig, solver: angular_body` (control-convention.md "CSS ControlRig body inputs"): per-region frequency, damping ratio and motion amount on the post-process AnimBP, replacing the stock `SpringBone` translation springs of the 2.0.x era (memory: spring physics tuning, Better Jiggle values K120/D7.5 with a 1.5 cm travel clamp).
- Eve 1.5.0 changelog: "Reduced excessive Eve chest and glute motion with corrected motion limits." The record (`docs/development/three-outfit-rig-physics-status.md` around lines 798-840): Firm/Natural produced about 20 degrees at the chest and 18 at the glutes under Iris Out; restoring the earlier angle/travel limits brought that to about 4 and 8 degrees; seven angle/travel limits were restored in the shared Peony rig and accepted live. Those limits are rig defaults, not controls: the sliders change frequency, damping and amount inside the clamp, so no slider setting can recover the older freedom of movement. That matches the complaint exactly.
- Hard clamps also explain "stiff rather than smooth": a clamp stops the helper dead at the limit, which reads as a hitch, where the old spring overshot and settled.

## Motion-feel plan (CSS core + rig, Opus/Sol lane)

1. Measure, do not guess: record chest and glute helper angle traces over the same clips (walk, run, Iris Out) for (a) the installed 1.5.0 rig, (b) the pre-limit rig, (c) the 2.0.x SpringBone reference values, and compare peak angle, settle time and jerk. Existing tooling: the motion sample channel (`runtime/motion-sample.json`), the rig fixtures in `CSS-Mod-Authoring/docs/next-gen-body-timing.md`.
2. Expose the limit: add a fourth live channel `travel` (max angle, degrees) to `angular_body` controls, defaulting to the 1.5.0 values, with the physics templates setting it (Firm 4/8, Natural 6/10, Bouncy 10/14, Earthquake 20/18). The runtime already has the pattern from the SpringBone `max_displacement` channel. Validator, convention doc, UI (Bounce/Settle/Travel style) and host tests follow.
3. Soften the limit: replace the hard clamp with a soft saturation (spring-back over the last 25 percent of the travel) in the rig graph so reaching the limit never hitches. Verify with the timing fixtures at 15 to 240 fps.
4. Default feel: after 1 to 3, retune the four templates against the trace comparison, with the user's live verdict as the gate, and ship in CSS 1.0.2 plus a rig recook per outfit (the rig is in the outfit packages).

## To do per package (Astra lanes)

Eve (CSS_SeduXtress_eins0fx), Unholy Genessa, Commander White, tHorny Proxima:
- Inventory: current morphs, helper bones, which UUEPBS Simplified sliders bind (table).
- Author the expanded morph set in Blender on the package's body (and garments/overlays that must follow), bake, export, `check_shapes`, cook, verify.
- Add the matching `shape_*` controls and a MorphGroups snippet for the package README.
- After the CSS core travel channel lands: recook the rig with the new template values.

## Status 19 Oct 2026, evening

- Package lanes (Eve, Unholy Genessa, Commander White) each authored the 18 new morphs with followers, validated 24 shape controls, measured chest-support displacement and wrote the README snippet; exports and cooks wait for the core rig builder. Reports: `<package>/work/uuepbs/REPORT.md`.
- Chest-support displacement at 1.0 (Eve, max / weighted RMS, cm): Implants 3.65 / 2.51, Cleavage 2.42 / 1.57, Underbust 1.60 / 0.50, Diameter 1.15 / 0.77, Natural 1.11 / 0.52, ChestWidth 1.09 / 0.40, the rest under 0.85 / 0.50. Genessa and Commander White measure the same order.
- Decision (Fable): the measured solver corner set grows from six to eight axes, adding `PBMBreastsImplants` and `PBMBreastsCleavage` (256 corners instead of 64). The other new morphs stay outside the model; their support error is within the envelope inflation already measured (about 1 cm), documented as tolerance. `build_body_runtime_geometry.py` and the geometry packer take the axis list from the recipe instead of a fixed six; the solver reads the corner count from the JSON.
- Core: travel channel and presets coded (b2c89a5); rig builder soft zone, AnimBP arrays, fixtures and the UI slider in progress.

## Decisions 19 Oct 2026, night (Fable)

- Soft limit: pass two measured the 25 percent band as too narrow at 120 Hz (crossed in about two steps; speed-change ratio 0.45 to 1.32) while 0.5 measured 0.20 to 0.54. Default SoftZone becomes 0.5 with a cubic push curve, Earthquake 0.6; gates re-run in pass three. Firm and Natural already show zero hard contacts at every frame rate; SoftZone 0 stays a bit-identical rollback.
- Rig and AnimBP: every outfit must be recooked with the V11 rig (RegionTravel, Travel, SoftZone, ClampContacts) and the AnimBP travel inputs before the Travel slider does anything; until then the UI dims it with "This outfit's rig predates Travel".
- Order to the live check: core 1.0.2 candidate plus Eve recook first (Eve lane, step 6, after pass three), user installs and judges the feel against the four presets, then templates retune from the traces, then Genessa, Commander White and tHorny Proxima recook with the same builder.

## Status 19 Oct 2026, night: eight-axis body geometry

Built, not cooked. Stage: `work/body-geometry-axes8` (inputs, outputs, fixtures) and `work/body-travel-softlimit2-module` (editor check).

- Axes come from the recipe. A `shape` control with `"solver_axis": i` is axis i; 6 to 8 axes, numbered without gaps, one morph each, the same list in every variant (`tools/css_controls.py` validates it, `docs/control-convention.md` documents it). Eve's order: the six measured axes in their existing order (`PBMWaistWidth, PBMHipSize, PBMThighsTone, PBMGlutesSize, PBMBreastsSize, FBMBodyTone`), then `PBMBreastsImplants` (6) and `PBMBreastsCleavage` (7), so corners 0..63 are the old corners. The recipe used is a copy of Eve's `authoring/uuepbs/manifest.json` with those eight fields added (`eve-manifest-axes8.json`); the package manifest itself is unchanged and needs the same fields in the Eve lane.
- `tools/authoring/body-geometry/`: `measure_body_geometry_axes.py` (numpy port of the layout and ellipsoid measuring steps for n axes), `build_body_runtime_geometry.py` (the packer, copied from the SeduXtress library and generalised), `body_geometry_axes.py` (n-axis evaluators, recipe reader), `add_archive_morphs.py` (stage-only fixture meshes).
- Model format: schema 2, `{"schema":2,"morphs":[n names],"corners":2^n,"helpers":[...]}`. Schema 1 (the cooked six-axis model, no `corners`) still loads unchanged in the core and the AnimBP step.
- Eve on her current export (`work/grip-grounding-v1/heel-support-export-v2/candidate.mesh.json`, the accepted Black Pearl import) plus the authored deltas (`work/uuepbs/body-deltas.npz`, matched to the export within 7.7e-6 cm): 8 axes, 256 corners, **151416 bytes**, under the 262144 bound of the AnimBP step and the core. The 64 six-axis corners reproduce the v2 ellipsoids within 5.4e-6 cm (centers) and 8.4e-7 (scale); the polynomial matches direct moments of the morphed points within 2.7e-13 cm and 1.9e-12 cm^2 over 320 cases. The six old coefficient rows move by at most 0.0006 cm (offset deltas) and 0.022 cm^2 (linear inertia) on the chest against the V40-based model, because the support surface is the current import's. The new axes move the chest centers by 1.97 cm (Implants) and 1.28 cm (Cleavage) at 1.0.
- Six-axis regression: rebuilding from the existing measured files with a six-axis recipe reproduces the cooked model's helpers and all 128 parity fixtures exactly (schema 2 adds 13 bytes, 44585).
- Core: `BodyGeometryModel` takes 6 to 8 axes; `CSSBodyShape0..n-1` inputs, when present, must match the axis count. Parity passes on the cooked six-axis model with its 128 fixtures (max 2.5e-14 cm) and on the 8-axis model with 320 fixtures (max 3.2e-14 cm, moments exact). `css_body_geometry` is now a ctest suite with synthetic 6, 7 and 8 axis models.
- AnimBP step: `tools/authoring-patches/body-geometry-axes.patch` accepts schema 1 (six) and schema 2 (6..8, corners 2^n). In the patched editor the cooked schema 1 model, its schema 2 rebuild and the 8-axis model all author and keep their strings exactly; the 8-axis model is refused on V40 (no new morphs) and when it claims 255 corners. The 8-axis case runs on a stage-only fixture mesh (`SK_SeduXtress_GeometryAxes8Fixture`: the current export plus the two morphs, body only, garments do not follow), because no package export carries the new morphs yet.
- Next, Eve lane: add the `solver_axis` fields to the package recipe, export with the new morphs, rerun the two tools on that export, cook with both authoring patches, then the live geometry probe.

## Decisions 20 Oct 2026, early (Fable), after core pass three

- Soft limit accepted for the first live check as built: cubic push, SoftZone 0.5 (Earthquake 0.6), zero hard contacts on all 18 Firm/Natural/Bouncy smooth fixtures, SoftZone 0 bit-identical to V10. The synthetic gate 4 (speed change under 0.6 on impulsive bounce fixtures, 5 of 12) and one gate 6 case stay open; those rows reach the hard limit whatever the curve, so the next option if the user still feels a hitch is the output soft clip, not another curve.
- Core candidate: work/core-1.0.2-rc1/css_core-1.0.2-rc1.dll (sha256 565c0299...), not installed; install steps in docs/development/2026-10-19-body-feel-candidate.md. Alone it behaves like 1.0.1; the feel needs an outfit recooked with the V11 rig and the AnimBP travel and soft-zone inputs.
- Lane order (one editor at a time): Eve recook first (24 morphs, solver_axis on Implants and Cleavage, travel rows, V11 rig, patched AnimBP), then the user's live check with the core candidate, then Unholy Genessa and Commander White, then tHorny Proxima when its session is free.
