# Midsummer Alice fitting

## September 27, initial review and garment morph repair

Source: Gemini's `exports/SK_Eve_MidsummerAlice.mesh.json`, SHA-256 `a60594690d1559e9b84776f8f7c65ceb3dd31dbf6d018d22feb11da73656fb61`. The audit identifies the 22,300-point suit, separate 624-point ribbon, common heels and hair. Suit and ribbon currently share material slot 16; separate visibility will require separating that section. Do not assume the old audit's CSSAuthoring paths are suitable for new imports. Use `/Game/CSS/`.

`work/eve26/alice-base1` front/back review shows a usable base fit. `alice-max1`, with all six supported shape sliders at +1, shows large distortions at the collar, chest and side lacing. This is a garment-morph issue visible without animation.

`repair_planet_morphs.py --garment alice` transfers the unchanged body's six morphs through nearest body triangles to the suit only. It preserves the base geometry, weights, body morph deltas and accessories. Output: `work/eve26/alice-morph1/alice.mesh.json` and receipt. The original source is unchanged.

`alice-morph1-max` front/back renders show the broad collar and lacing distortions removed. `alice-morph1-sprint` uses the same maximum morphs and frame 24 upstream pose from `knit-cloth4/sprint.json`. Front/back renders show no obvious broad suit breakthrough in that pose. This is one pose, not all-animation acceptance. Heel pose is uncorrected in these diagnostics and must use the proven corrective graph before runtime review.

All four review processes and morph generation exited 0 in Blender 5.2.2. The generator checks unchanged non-morph fields and body deltas. This is an interchange candidate, not yet a saved Blender source revision or game asset.

Next: persist and verify the repaired source, inspect the opposite sprint phase and individual shape extremes, separate the ribbon's section, reuse the verified heel setup with explicit material indices, then assemble motion/customization and appropriate ribbon physics. No Alice package is installed yet. Keep the accepted Knitwear fit locked while its game review waits for the user's next restart.

## Saved source and separate ribbon

`alice-base.blend` extracts the original garment with zero offsets. `alice-m1.blend` saves the six repaired morphs. Fresh-load verification reports exactly unchanged base geometry and maximum morph error 0.000006303 cm, preserving other fit keys and default values. Receipts: `alice-base.json`, `alice-m1.json`; both Blender processes exit 0.

The maximum-shape sprint frame 48 front/back views in `alice-morph1-sprint48` show no obvious broad suit breakthrough. Footwear is still the uncorrected diagnostic pose. Frame 24 and 48 together are useful fitting checks, not a substitute for game motion or individual slider extremes.

`prepare_alice_sections.py` produces `alice-sections1/alice.mesh.json`: all 1,224 ribbon faces move from suit slot 16 to independent slot 26 (`AliceRibbon`). Positions, triangle vertices, weights and morphs remain unchanged. Its material alias must resolve to the original suit material during import; independent ribbon visibility is not yet tested live. New import paths use `/Game/CSS/EveTest/`.

Next outstanding work: individual slider extremes, heel correction, material/rig assembly, ribbon motion and game review. Do not redo the saved-source or section-split work.

## Heel reuse prepared

Alice's 31,668 shoe points and complete bone array exactly equal the accepted Knitwear W2 interchange. Its original shoe weights differ. `prepare_alice_heels.py` transfers only the verified shoe weights with local vertex remapping, preserving body weights, geometry and all morphs. The resulting 41,222 shoe influence rows are in `alice-heels1`, with source/reference hashes recorded.

`alice-heels1-sprint` quarter view was inspected at upstream sprint frame 24 using the measured foot correction from `bikini-heelpose1/receipt.json`. Shoes follow the feet in this diagnostic; this does not validate the runtime graph or floor contact. Both outfits use shoe material slots 17–22, so `ABP_KnitFeet1` is a candidate for reuse after binding checks.

`setup_bikini_import.py --kind alice` prepares `alice-import1/alice.mesh.json` at `/Game/CSS/EveTest/SK_AFit1` and snapshots protected production asset hashes. No Unreal import has run yet. The separately saved `alice-m1.blend` contains the suit morph repair, while shoe weight reuse is currently in the assembly interchange only. Preserve that distinction when preparing the final authoring kit.

## Unreal import and reference checks

The import and skeleton-only binding now completed with exit 0 (`alice-import.log`, `alice-bind.log`). `prepare_bikini_fit.py` supports Alice and assigns the original 26 materials plus a duplicate suit material for ribbon slot 26. It preserves `PA_Body`, assigns `ABP_KnitFeet1`, checks the mesh bind pose, and verifies the shared skeleton still has 386 bones, 82 sockets and nine virtual bones. Preparation exits 0; `alice-prepared1.json` records the saved references.

Fresh-load `verify_bikini_fit.py` produced `alice-verified1.json`: 27 materials and references match, protected assets remain unchanged, and transient hide/restore checks isolate suit, shoes, hair and ribbon sections correctly. Its commandlet shutdown was still pending when this note was written; resume process handle 52754 before calling its outer run successful. These editor checks do not establish CSS profile persistence or game rendering.

The separate ribbon is a small chest bow, 624 source points, entirely weighted to `spine_04`, spanning approximately 9.7 by 6.0 by 7.7 cm. Plan restrained motion with pinned attachment, not a large skirt solver. No cloth has been attached to Alice yet. Individual slider extremes, ribbon physics and runtime motion remain open.

## Original bow rig recovered

The fresh-reference commandlet completed exit 0 (process 52754). `inspect_knit_motion_source.py --outfit alice` now inspects Alice in the original `eve_beta10.blend` without saving it. `alice-author-motion2.json` retains world points and named weights for the ribbon. All 624 points match the exported ribbon in order after the documented axis conversion, maximum error 0.000007633 cm. The source hash is unchanged.

Unlike Gemini's rigid spine export, the author's ribbon has its own armature, neck/root groups and multiple left/right ribbon chains. No dForce maps are present. Root weights range from 0.0353 to 0.5529, so they must not be mistaken for a ready-made zero/one cloth pin mask. Inspect attachment geometry before deciding the pin transition; preserve the recovered point correspondence rather than guessing a spatial nearest-neighbor mapping.

An extra all-minus-one render (`alice-min1`) shows undesirable chest folding, but the current manifest explicitly supports shape values 0 through 1. This negative extrapolation is outside that range and is not a release blocker. Do not spend time correcting unsupported negative morph values. Individual positive slider checks remain useful.

## Bow physics trial prepared

Close-up `alice-bow1/front.png` confirms a chest bow with two loops and hanging ends. The close view also exposes small skin spots through the chest fabric; review those separately from the intentional lace openings before final fitting acceptance.

`prepare_alice_cloth.py` uses the exact 624-point correspondence. Each point's trial displacement limit is moving-chain weight divided by moving-chain plus root weight, capped naturally at 1 cm. Neck weights are excluded from this ratio. This is an authored bounded trial, not a reconstruction of the author's solver. Six disconnected pieces have 8/36, 7/48, 120/120, 100/100, 30/152 and 24/168 pinned points. Thus every piece retains an attachment and two pieces are fully fixed.

`alice-cloth1` contains the full-resolution bow proxy, 6-iteration settings, 5 cm nonlegacy backstop radii, and separate mesh/collider copy recipes. The separate copies preserve the gameplay mesh's original `PA_Body` reference while supplying a private identical collision asset. No shared rig changes are needed.

Pipeline process 76756 is running copy, collision copy, then cloth bind in sequence. Resume that exact handle; do not relaunch on an observation timeout. Logs: `alice-cloth-{copy,pa,bind}.log`. Binding and native motion are not yet verified. Next inspect the saved cloth, run a bounded native motion test, visually replay the bow and check attachment drift before adopting it.

## Saved bow binding verified

Process 76756 completed all three operations with exit 0. Fresh inspection `alice-cloth1/readback.json` and `verified.json` confirm 624 particles, six iterations, one ribbon section with 3,664 render vertices, original gameplay `PA_Body`, shared skeleton and `ABP_KnitFeet1`. Saved Max Distance differs from authored values by at most 2.969e-8 cm; both backstop maps match exactly. Protected production hashes pass.

`alice-bow-motion.patch` adds only the private Alice candidate and its 624-particle count to the native motion probe. Build succeeded. The inspection pipeline (5466) stopped at its duplicate verification call because the receipt had already been produced successfully while editor shutdown was pending. This was an output-exists guard, not failed asset verification; no inspection retry is needed.

The corrected motion-only pipeline is process 46075, using the actual `-Motion` commandlet switch, then the Blender particle checker. Resume it before launching anything else. Its output is `alice-cloth1/sprint.json` and `sprint-check.json`; log `alice-cloth-sprint.log`. Native motion, rendered bow contact and game acceptance remain pending.
