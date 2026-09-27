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
