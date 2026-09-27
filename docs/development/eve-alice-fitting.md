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

## Bow trial rejected after native motion and visual replay

Process 46075 exited 0. The 65-frame sprint preserves pinned points within 0.00003318 cm, but maximum displacement is 6.1166 cm and edge ratio reaches 39.57. This exceeds the intended movement bound. Native render replay at frame 34 (`alice-cloth1-view34/front.png`) confirms a visibly inflated, stretched bow. Reject this trial.

`alice-bow-isolation.patch` enables existing temporary collision-isolation switches for this private Alice candidate. Build and process 16097 exit 0. With only body collision disabled, displacement drops to 0.8873 cm, limit excess to 0.00003318 cm, and maximum edge ratio to 4.173. Thus the copied body collider drives the large outward displacement, but removing it is not a fit fix.

Coordinate verification and render replay processes exit 0. `alice-nobody-view34/front.png` shows most of the bow buried in the chest in the bent sprint pose. The inherited spine-only skinning is a candidate cause. Before more collision tuning, compare the no-cloth skinned bow at that same postprocess pose and transfer attachment weights from the fitted suit/body as appropriate. Keep the original geometric shape and author pin partition. Do not ship collision-disabled trial as accepted physics or repeat the collider-only parameter loop.


## Chest priority and comparison with accepted Knitwear

User accepts the neck straps for now. Stop neck/bow micro-fitting and fix the nipple pokethrough while retaining the contour under fabric. Do not flatten the body or hide exposed regions as a shortcut.

The body-donor ribbon trial `alice-boww2` restores the visible bow at sprint frame 34; the suit-donor trial did not. Neither trial changes geometry or morphs. Physics still requires a fresh bind with corrected weights before acceptance.

Compare the accepted Knitwear fabric near body point 6397: fabric point 49259 carries brust001 0.9411, spine03 0.0554 and clavicle_r 0.0036, matching the body. Alice point 52130 instead carries brust001 0.9962, spine03 0.0035 and clavicle_r 0.0003. Reuse body-corresponding weights rather than treating this only as a fabric offset problem.

`alice-chestw1` transfers body weights to the explicit 1,834-point chest selection, preserving all geometry, morphs and body weights. Its frame-34 render still shows both nipple spots. Weight matching alone is therefore insufficient. `alice-chest3` applies a bounded 0.4 cm clearance to the corrected weights; its inspected frame-34 render also retains both spots. Neither candidate is accepted or installed. The earlier uncorrected-weight chest1/chest2 candidates must not replace the corrected branch.

Next candidate `alice-chest4` allows up to 1 cm local clearance against bind and sprint frames 24/34/48, at default and maximum supported morphs. It remains an offline trial until rendered and checked. Only the selected garment points may move. Preserve the lace openings, silhouette, original body and accepted neck scope.


## Chest correction saved and visually checked

`alice-chest4` changes 708 garment points, maximum 0.993854 cm, with body, skeleton and morph deltas unchanged. Seventeen sampled constraints remain unresolved, so this is not a claim of zero intersections everywhere. Inspected front renders `alice-chest4-base`, `alice-chest4-pose34` and `alice-chest4-max34` show both nipple contours covered, including the previously failing bent pose and all six supported sliders at maximum. Neck strap clipping remains within the explicitly accepted scope.

Saved garment source `alice-cw4.blend` contains the corrected clearance and body-corresponding weights. Intermediate `alice-c4.blend` preserves relative shape deltas; evaluated geometry differs from the JSON by at most 0.0000170923 cm. Fresh reload of final weights passes with maximum error 2.97943e-8 and identical geometry/shape-key digest. Both save processes exit 0. Final candidate JSON SHA256: `f72ebd548446c68acf6bf49d7925a3dd59691b85551ed1376ec003a667f8f8e9`.

Next: assemble this corrected suit and body-weighted ribbon, reimport the private Alice candidate, then complete restrained clothing motion and customization. Do not package the earlier `SK_AFit1` or `SK_ACloth1` as if they contain this repair. No game package or release archive changed in this step.


## Corrected Unreal candidate in progress

`setup_bikini_import.py --kind alice --alice-c4` creates `alice-import4` and `/Game/CSS/EveTest/SK_AFit4`. The import interchange differs from chest4 only in destination package names. Import and shared-skeleton bind have completed exit 0. Process 51490 continues material preparation and fresh reference/section verification, using `CSS_FIT_KIND=alice CSS_ALICE_C4=1`. Resume this handle; do not rerun completed steps or assume preparation completed from import alone. Logs use `alice4-*.log`.

The corrected bow is separately saved as `alice-rw2.blend`. Fresh reload verifies all 1,973 weight rows with maximum error 2.97937e-8 and unchanged geometry/shape keys. Original authoring source remains untouched.

`prepare_alice_cloth.py --repaired` creates `alice-cloth2` for `SK_ACloth2` and `PA_ACloth2`. Its proxy differs from the rejected first trial only in skin weights; particle positions, topology, pin limits, normals and backstops remain identical. This isolates the influence of repaired attachment weights before changing collider parameters. Copy/bind/motion have not run yet. Native probe support for ACloth2 is saved in `native/alice-bow-repaired.patch` and applied to the authoring project source, but requires an editor build after current commandlets finish.


## Corrected import verified; second bow simulation started

Process 51490 completed all four steps with exit 0. `alice-verified4.json` confirms 27 materials, expected physics/postprocess references, the shared skeleton, independent suit/shoes/hair/ribbon visibility and unchanged protected hashes. This verifies saved asset references and section behavior, not live CSS persistence or game rendering.

The editor build for `alice-bow-repaired.patch` succeeded (80275, exit 0, `alice2-build.log`). Process 60426 now performs copy, private physics copy, bind, fresh inspection/map verification, native sprint motion and particle diagnostics in sequence. Logs are `alice2-{copy,pa,bind,inspect,sprint}.log`; outputs live in `alice-cloth2`. Resume this exact handle. Do not repeat steps based on shutdown delay. The verification scripts now support Alice trial 2 and select its corrected import and separate collision asset.

Once terminal, inspect `alice-cloth2/sprint-check.json` and replay the worst frame visually before accepting any cloth motion. The previous rejected trial remains available for comparison. No release or installed game package changes yet.


## Repaired bow still exceeds its movement bound

Process 60426 completed copy, bind, inspection, native sprint and particle checks. Saved maps match within 2.969e-8 cm and protected hashes pass. Native simulation keeps pinned points within 0.00003670 cm, but displacement reaches 4.7154 cm, limit excess 3.8131 cm and edge ratio 24.0147. Worst edge frame is 58. This is improved over trial 1 but remains rejected.

Visual replay attempt 43730 failed because the inspection command omitted `-Geometry`, so readback has no `render_geometry`. No image was produced and no visual acceptance is claimed. Blender exits 0 for Python exceptions unless `--python-exit-code 1` is supplied; use that switch for subsequent scripted pipelines. Obtain a new `-Inspect -Geometry` report, then replay frame 58. Keep the existing valid map inspection receipt.

Process 72493 is running the temporary `-NoBodyCollision` diagnostic on ACloth2 (`alice2-no-body.log`, output `alice-cloth2/no-body.json`). Resume the same handle before another Unreal commandlet. It does not save the disabled collision setting. Compare its motion and rendered contact with the full collision trial before changing private bow colliders. Do not ship a diagnostic simply because particle limits pass.


## Collision comparison rendered

72493 and geometry inspection 91722 completed exit 0. `geometry.json` includes the missing render mapping. Replay pipeline 55754 uses `--python-exit-code 1` and completed both full-collision and no-body views at frame 58. Both mappings reproduce rest geometry within 0.000016013 cm.

Inspected `alice2-full58/front.png`: copied body collision visibly inflates and stretches the loops and tails. `alice2-nobody58/front.png`: the bow stays close to its attachment, but loops still crumple. Removing body collision reduces maximum displacement to 1.00150 cm and limit excess to 0.00150 cm, with pinned error 0.00003670 cm; it is not accepted as a final physics setup.

The six connected components identify a simpler appropriate motion scope: 36 and 48 points are hanging ends (minimum Z 145.19 and 143.91 cm); 152 and 168 points are side loops (minimum Z 149.53 and 149.51 cm); 120 and 100 points form the already pinned knot. Next preserve loops and knot with skinning, and restrict secondary motion to the two hanging ends. This retains clothing motion where it is useful while avoiding simulated collapse of a tied bow. Keep this separate from the accepted neck-strap tolerance and preserve repaired chest coverage. Body collision/backstop contact still needs verification for the revised tails.


## Tails-only bow candidate

`prepare_alice_cloth.py --repaired --tails-only` creates `alice-cloth3`. The two hanging components retain 8/36 and 7/48 pins, with moving limits scaled to a maximum 0.5 cm. The knot and side loops are fully pinned. All geometry and repaired skin weights are unchanged.

The collision-copy recipe uses a private copy of accepted Knitwear `PA_KCloth4` rather than the broad gameplay `PA_Body`. Its 32-sphere recipe has a minimum rest gap of 0.77253 cm to the bow. This supports a motion trial only; posed contact remains unproven. Gameplay physics and the original Knitwear asset remain untouched.

Native ACloth3 probe support is recorded in `alice-bow-tails.patch`. Editor build 80095 succeeded, exit 0. Pipeline 71184 is running copy, collision copy, bind, inspection with `-Geometry`, map verification and sprint diagnostics, sequentially. Resume this handle; logs use `alice3-*.log`, outputs `alice-cloth3`. Blender diagnostics explicitly use `--python-exit-code 1`. Render the worst frame after completion and check that the loops retain their silhouette and the hanging ends stay outside the body before adopting it.


## Tails-only motion passes bounded sprint trial

Pipeline 71184 completed every step exit 0. Saved maps match within 1.45302e-8 cm; all protected hashes remain unchanged. Native 65-frame sprint with collision and backstop enabled has maximum displacement 0.4989999 cm and pinned/limit error 0.00003802 cm. This removes the previous multi-centimetre inflation. Maximum all-edge ratio is 6.0136 (includes pinned skin deformation); the coordinate verifier's active-edge ratio is 3.6644.

Visual replay 80066 completed exit 0, using fresh saved render mapping with rest reconstruction error 0.000009934 cm. Inspected `alice3-worst/front.png` at frame 52: hanging ends stay attached and outside the visible chest, and the previous inflated bow is gone. The right loop remains compressed by skinning in this bent pose, and the already accepted neck-strap intersections remain. Do not claim perfect bow shape or universal collision clearance. The chest fabric still covers both nipple contours in this view.

Use `SK_ACloth3` as the bounded-motion candidate for the next game review, preserving this limitation for user assessment rather than restarting neck/bow perfection work. Next complete Alice's modular controls/body motion/palettes and package a private trial, then record in-game movement before merging into the final Eve mod. No game package or release archive changed here. All motion/replay processes are terminal.
