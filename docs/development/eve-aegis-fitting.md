# War Aegis fitting baseline

Source: Gemini `exports/SK_Eve_WarAegis.mesh.json`, SHA256 `9c58a1b5dec47adc14ed1fa8a1a8e4af00753a5ba25febc2c5504399d84b6a67`. Original untouched.

While Alice's bow pipeline runs, baseline renders `work/eve26/aegis-base1` and `aegis-max1` completed exit 0 and front views were inspected. Default has broad chest, shoulder and arm intersections. All six supported sliders at 1 cause severe inflated and folded garment deformation. Do not accept this export or hide the body to cover the problem. Verify intended suit coverage against the original authoring reference before changing any apparent openings.

The suit is one source part, 38,242 points and 76,044 triangles, material slots 16/17/18. Body is 36,787 points; hair occupies slots 19/20/21. No separate shoe part is present in this export. Source garment has no baked fit keys. Check the original source fit settings and geometry first, then reuse validated body-corresponding morph and weight transfer where appropriate. No fitting edits or import have been performed yet. Alice remains the active outfit.


## Original author source inspected

`inspect_knit_motion_source.py --outfit aegis --output work/eve26/aegis-author1.json` completed exit 0, confirms the original blend hash is unchanged, and records shape values, modifiers and rig dependencies. Original suit has the same 38,242 points and active `Fix=1` and `VagShape=1`; the export audit lists no baked garment fit keys. This discrepancy needs an evaluated-coordinate comparison before attributing clipping to omitted keys. It is not yet a proven cause.

A separate original `Eve War Aegis` mesh has 44 vertices, a Cloth modifier with pin group `Pin` and quality 5, followed by the Eve armature. Preserve and inspect this authored motion source instead of inventing a solver from the export alone. No War Aegis source asset was modified.


## Export compared with saved Gemini geometry

Read-only comparison 29948 completed exit 0, `aegis-source-compare.json`. All 38,242 evaluated Gemini garment points match the export exactly. Therefore the default clipping shown in the baseline is already present in this saved garment path, not introduced by JSON export. Both blend hashes remained unchanged.

The original author's isolated object comparison has median difference 0 cm but maximum 33.7899 cm against the export. Locate affected regions and compare evaluated source/body correspondence before adopting any original coordinates. This script isolates library objects and clears driver animation for export-style evaluation; its active-key dictionaries are empty, unlike the full-scene inspection. It does not prove that the original full-scene corrective keys are absent or ineffective. Preserve that distinction and do not globally push the garment outward based on this result alone.


## Original surface-transfer candidate

`fit_aegis_source.py` reuses the Holiday body-surface correspondence method. Full-scene source garment/body saved fit positions are mapped to the unchanged exported CSS body, preserving local surface offsets. Garment weights and six slider deltas follow the same body triangles. Body points, skeleton, accessories, UVs and export faces are preserved; source blend hash remains unchanged.

Initial run stopped on missing helper import, corrected by adding the script directory. Next run stopped because fitted quad diagonals differ. The revised guard proves each exported triangle stays within its original source polygon, then retains exported topology. Final process 46858 exits 0 and produces `aegis-fit1`. Original-to-body surface distance reaches 8.44349 cm, including loose geometry, so do not blanket-collapse the entire garment onto skin.

Base and all-six-sliders-at-one renders (60936,39583) complete exit 0. Inspected front views: maximum-slider inflation is removed, but broad chest/arm clipping remains in base and morphed views. Do not accept or package this fit. Next correct garment surface clearance locally while preserving loose parts and intentional openings. Normals must be refreshed from the eventual saved garment before production. No Unreal or game assets changed.


## Base clearance and chest correction

`fit_aegis_clearance.py` produces fit2 from fit1 by lifting intersecting/near-surface garment points along saved body normals to 0.2 cm clearance, limited to points within 3 cm of the body. It changes 9,412 garment points, maximum offset 2.08453 cm, with one deeper point left unresolved. Body, rig, accessories and morph deltas are unchanged. Default and all-six-maximum front views show broad chest, shoulder and arm breakthroughs removed, with small nipple spots remaining.

The Alice clearance solver now supports the `aegis` stem. Fit3 selects 3,325 chest points, checks bind plus sprint frames 24/34/48 at default and maximum sliders, changes 1,288 points and leaves 95 conflicting constraints unresolved. This is not zero-intersection proof. Aegis and the recorded Alice motion have exactly identical body points and reference bones, verified before pose reuse.

Base and frame-34 front/back renders completed exit 0. Inspected views show nipple contours covered, broad torso/sleeve clipping removed, and rear hanging strips preserved. Some glove/finger skin still protrudes during sprint, requiring follow-up. The hanging rear strips currently use skin weights only and need their authored 44-point cloth source adapted. Fit3 remains an offline candidate, not installed. Save the corrected garment source and refresh normals before production; confirm supported slider extremes and glove correction before import.


## Glove correctives and cloth source recovered

`repair_aegis_hand_shapes.py` confirms all 16 existing body left-hand corrective targets had zero garment deltas. `aegis-hand1` adds body-corresponding glove deltas (70 to 298 points per target), preserving base geometry, body deltas, skeleton and all six customization shapes. Process 42892 exits 0. This is necessary support, not proof that the observed clipping is solved.

`review_outfit_export.py --frame-bone hand_l` adds reusable bone/descendant close framing. Frame-34 diagnostic with explicitly applied pJCMIndex1Dwn_90_L=1 completes exit 0 and shows remaining glove/finger breakthrough. It is an isolated corrective test, not recorded game curve activation. Next check local hand weight correspondence after clearance and actual glove coverage; do not alter the body or shared hand rig.

Original author inspection 57393 exits 0, `aegis-author2.json`, with source hash unchanged. The `Eve War Aegis` proxy has 44 points, 20 polygons and eight Pin weights, plus V0 through V10 and paired .001 groups. Points, topology and all weights are retained for adapting rear-strip physics. Inspect correspondence to rendered strips before binding.
