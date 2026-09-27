# Midsummer Alice fitting

## September 27, initial review and garment morph repair

Source: Gemini's `exports/SK_Eve_MidsummerAlice.mesh.json`, SHA-256 `a60594690d1559e9b84776f8f7c65ceb3dd31dbf6d018d22feb11da73656fb61`. The audit identifies the 22,300-point suit, separate 624-point ribbon, common heels and hair. Suit and ribbon currently share material slot 16; separate visibility will require separating that section. Do not assume the old audit's CSSAuthoring paths are suitable for new imports. Use `/Game/CSS/`.

`work/eve26/alice-base1` front/back review shows a usable base fit. `alice-max1`, with all six supported shape sliders at +1, shows large distortions at the collar, chest and side lacing. This is a garment-morph issue visible without animation.

`repair_planet_morphs.py --garment alice` transfers the unchanged body's six morphs through nearest body triangles to the suit only. It preserves the base geometry, weights, body morph deltas and accessories. Output: `work/eve26/alice-morph1/alice.mesh.json` and receipt. The original source is unchanged.

`alice-morph1-max` front/back renders show the broad collar and lacing distortions removed. `alice-morph1-sprint` uses the same maximum morphs and frame 24 upstream pose from `knit-cloth4/sprint.json`. Front/back renders show no obvious broad suit breakthrough in that pose. This is one pose, not all-animation acceptance. Heel pose is uncorrected in these diagnostics and must use the proven corrective graph before runtime review.

All four review processes and morph generation exited 0 in Blender 5.2.2. The generator checks unchanged non-morph fields and body deltas. This is an interchange candidate, not yet a saved Blender source revision or game asset.

Next: persist and verify the repaired source, inspect the opposite sprint phase and individual shape extremes, separate the ribbon's section, reuse the verified heel setup with explicit material indices, then assemble motion/customization and appropriate ribbon physics. No Alice package is installed yet. Keep the accepted Knitwear fit locked while its game review waits for the user's next restart.
