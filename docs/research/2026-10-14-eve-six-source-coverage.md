# Source correspondence for all six original Eve outfits

The current part checker passes all 46 selected parts across the six original
outfits: 803,379 source points and 1,160,081 triangles. This extends the
[Bikini/Knitwear/Alice work](2026-10-14-eve-editable-source-recovery.md), adds
Black Pearl and Planet Diving, and refreshes Skin Suit with the current checker.
No installed asset or user setting changed.

| Outfit | Parts | Source points | Triangles | Manifest |
| --- | ---: | ---: | ---: | --- |
| Black Pearl | 11 | 133,234 | 193,589 | [Black Pearl/Planet](../development/eve-source-remaining-20261014.json) |
| Planet Diving | 8 | 123,959 | 175,142 | [Black Pearl/Planet](../development/eve-source-remaining-20261014.json) |
| Skin Suit | 6 | 114,373 | 153,507 | [Skin Suit](../development/eve-skin-source-20261014.json) |
| Vacation Bikini | 7 | 137,402 | 200,619 | [Bikini/Knitwear/Alice](../development/eve-source-parts-20261014.json) |
| Casual Knitwear | 7 | 146,464 | 216,613 | [Bikini/Knitwear/Alice](../development/eve-source-parts-20261014.json) |
| Midsummer Alice | 7 | 147,947 | 220,611 | [Bikini/Knitwear/Alice](../development/eve-source-parts-20261014.json) |

Each manifest verifies complete part coverage and input hashes, then links the
same import hash to its existing cooked comparison. Current Eve containers and
shared binary containers match those readbacks. Source checks cover positions,
triangle order, named weights, six body morphs and declared hand correctives,
three UV channels, corner normals and color bytes. No saved body-control values
are reset to obtain a pass.

## Complete Black Pearl replay

The retained `heel-support-export-v2/command.json` identifies
`CSS_SeduXtress_HeelSupportsV45C.blend`, its explicit V44B2 bind-pose file and
the original exporter flags. Replaying that command into a private output
reproduces the accepted interchange byte-for-byte:
`f8b62d9e4c6d7f9e472225a1db78ad27ff65078c8529abf9944536b3245b27bf`.

All fields match, including 379 bones, 30 material slots, 22 morphs, geometry,
weights and corner data. The exporter checks the actual source armature's bone
names, parents and joint heads against its declared references, and enforces
the explicit bind-frame policy. All six imports have exactly the same bone
array as this replay. That is evidence for the shared import foundation, not
proof of every editor skeleton extension or runtime animation binding.

Commands, input hashes, exit status and comparisons are retained in
`work/three-outfit-rig-physics/eve-source-remaining-20261014/blackpearl-full-export`.
The duplicate 108,976,873-byte interchange was removed only after exact equality
was verified; the original, regenerated audit and recipe remain.

## Planet Diving tail recovery

Seven Planet Diving parts pass directly from the retained libraries. The
original tail matches geometry, weights, topology, UVs, normals and colors,
but lacks the accepted Body Tone, Glutes, Hips and Waist deltas. The largest
missing delta is 1.34536 cm for Glutes. The earlier
`tools/eve-fit/repair_planet_tail_root.py` recipe explains their origin: morph
propagation at the non-simulated attachment strap, leaving simulated vertices
unchanged. This is a saved-source gap, not a new installed fit failure.

Eve's `work/eve-source-chain-20261014/planet-tail.blend` now reconstructs the
accepted 840-point, 1,648-triangle tail with all four deltas. It uses the same
guarded float-normal recovery method as Skin Suit and Alice. Saved SHA-256:
`bbce05f3ef1ca1f05b2bfdb1e63678c27ac8fbc18be41a1315909642a7e42876`.
Fresh reload and the independent part comparison pass. The original library
and failing comparison remain. The first invocation omitted Blender's script
argument separator and exited before reconstruction; its log is retained,
followed by the successful `rebuild-tail2` command and exit record.

## Evidence and limits

The remaining-source work directory contains the 19-part initial plan/results,
the repaired tail check, six fresh Skin Suit results, full-export replay and
`six-import-bind-equality.json`. `six-outfit-source-coverage.json` pins the three
manifests and totals. The existing Python 3.14 runner uses Blender 5.2.2 LTS;
no new engine cook or game trial was needed.

Across the new Black Pearl/Planet checks, UVs and color bytes are exact. The
largest weight error is 1.190e-7, positional-morph difference 0.000098464 cm and
normal-vector difference 1.827e-7. Black Pearl's full replay is exact despite
the part checker's bounded floating-point comparisons. Skin Suit's six fresh
checks also pass, including the previously repaired lining.

This closes the listed editable-part correspondence fields for the six original
outfits. Complete per-outfit assembly/material recipes, cooked normals/tangents
and additional UV channels, garment simulation, posed fit, gameplay and measured
performance are separate. Peony, Genessa and CW retain their own evidence.
Do not describe this as full release acceptance.

Cleanup followed the [recorded removal list](../development/cleanup-2026-10-14-source-replay.md).
Free space was root 95G/theark 15G before work. Immediately before and after
duplicate removal it read 95G/14G; the settled reading is again 95G/15G.
Logical bytes removed are not a claim of equal physical space reclaimed.
