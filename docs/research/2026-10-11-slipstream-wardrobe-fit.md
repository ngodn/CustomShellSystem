# Slipstream garment ownership and first torso fit

Slipstream Normal now has explicit source-triangle ownership for six removable
garments. This is authoring work in the Eve mod, not a runtime installation.
The installed Peony trial remains unchanged and awaits the user's gameplay check.

## Source ownership

The recipe is `authoring/slipstream-wardrobe.json` in
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx`.
It uses the verified `slipstream-retail-02` geometry and the existing
`new-outfits-aligned3.blend` study. The Malfunction alternative and original
archives are unchanged. Source material slots are retained independently of
garment ownership.

| Removable group | Triangles | Associated details |
| --- | ---: | --- |
| Jacket | 7,746 | Sleeves, yoke, upper-arm straps, studs and buckles |
| Bodysuit | 9,597 | Side ties, chest strap, zipper hardware and inset panels |
| Skirt | 8,000 | Waist belt, hip straps, buckle and studs |
| Boots | 6,014 | Top straps, buckles, studs and shin insets |
| Collar | 230 | Neck band with front/back fasteners |
| Forearm device | 38 | Separate panel; shader/transparency still needs reconstruction |

All 31,625 clothing triangles and 116 disconnected components are assigned
exactly once. The original 51,369-triangle source body is reference-only.
It lacks lower legs under the boots; the production assembly must use the
complete accepted Eve body and explicitly verify barefoot restoration.

`partition_slipstream_wardrobe.py` adds a face attribute to the saved library.
It does not split/reorder vertices, change source materials or modify the
source Basis, named weights, UVs, raw normals/tangents, colors or shape keys.
This ownership map will drive later export sectioning and visibility controls.

Fresh Blender reload passes the existing source-library and alignment checks:
all 23 original parts and 249,020 source triangles remain intact, the accepted
body points/triangles are unchanged, and saved alignment keys evaluate correctly.
The independent wardrobe checker uses the decoded triangle-component membership,
not the writer's reported counts. Missing components, duplicate ownership and
unknown groups are rejected by negative controls.

Fourteen diagnostic renders cover all garments together and each isolated
group from front/back. The source-group views inspected during this turn show
the straps/fasteners remaining with their intended garments. These are neutral
source views with diagnostic colors, not final material or target-body acceptance.

## Torso fitting evidence

`fit_slipstream_torso.py` projects the primary jacket, bodysuit and skirt
surfaces against the unchanged target body. It transfers fabric displacement
to the corresponding trim instead of flattening each ornament onto the skin.
The result remains a separate shape key with the original source and alignment
available for comparison.

The first pass omitted source twist-bone ancestry when selecting sleeve query
regions. The second resolves those influences through the verified alignment
ancestor map. Jacket ray misses fall from 344 to 42, and front/back renders
show improved sleeve coverage. The skirt and bodysuit have 87 and 198 missed
queries respectively. A missed query is not itself a collision, but it cannot
be counted as clearance verification.

The second pass is **rejected for export**. Its fresh reload preserves source
data and the target body, but the distortion screen finds the following face
normals reversed relative to the aligned surface:

| Source surface | Reversed normals | Minimum area ratio |
| --- | ---: | ---: |
| Skirt | 55 | 0.01756 |
| Jacket/sleeves | 110 | 0.00092 |
| Bodysuit `o-c` | 263 | 0.00372 |
| Hardware `o-d` | 24 | 0.00244 |
| Insets `o-h` | 15 | 0.12282 |
| Shoulder/chest panels `o-j` | 33 | 0.28407 |

There are no numerically collapsed triangles at the checker's threshold.
Normal rotation alone does not prove an inverted garment, but the severe local
area compression and remaining visible chest/shoulder intersections make this
candidate unsuitable for export. Boots were intentionally not fitted by this
torso pass and still intersect the target legs/feet.

Do not repeat blind projection or solver-iteration sweeps. Next, inspect the
recorded source-triangle witnesses and make the deformation continuous across
folds and seams, with explicit trim correspondence. Preserve the accepted Eve
body and source design. Then complete footwear, target skinning, body morphs,
materials, five palettes, physics, facial/Astral integration, cook and gameplay.

## Reproduction and retained evidence

All following paths are relative to the Eve mod:

- `work/slipstream-wardrobe1.blend` and `.json`: ownership-only library.
- `work/slipstream-wardrobe-review1`: source-group renders and hashes.
- `work/slipstream-torsofit2.blend` and `.json`: rejected fit retained as a
  diagnostic witness, not a production candidate.
- `work/slipstream-torsofit-review2`: target-body renders.
- `work/slipstream-torsofit2-source-check.json`: decoded source preservation.
- `work/slipstream-torsofit2-alignment-check.json`: accepted body and evaluated keys.
- `work/slipstream-torsofit2-ownership-check.json`: independent component coverage,
  negative controls and exact distortion witness triangle IDs.
- `work/slipstream-ownership1/receipt.json`: frozen recipes, source hashes,
  validation identities and cleanup record.

Run the authoring scripts with Blender 5.2.2, `--background --factory-startup
--python-exit-code 1`. The partitioner takes `--blend`, `--recipe`, `--output`.
The fitter takes `--blend`, `--target`, `--output`; the target is the verified
CSS `work/eve26/skin-f16-import/skin.mesh.json`. The wardrobe checker takes
`--blend`, `--components work/new-outfits-components1/result.json`, `--output`.
Use new output paths. Commands and source inputs are recorded with each result.

The fitting query behavior was checked against Blender's
[BVHTree API](https://docs.blender.org/api/current/mathutils.bvhtree.html).
Surface queries and this offline fit do not establish runtime collision,
skeletal compatibility or game performance.
