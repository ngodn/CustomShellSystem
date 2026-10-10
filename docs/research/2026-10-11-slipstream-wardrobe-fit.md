# Slipstream garment ownership and continuous torso fit

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

The continuous-field follow-up below supersedes this rejected projection.

## Continuous-field follow-up

`slipstream-torsofit10` retains the source surface detail and smooths its
displacement field instead of independently projecting neighboring vertices.
Area-weighted Gaussian samples prevent dense trim from dominating the fit.
Radial clearance constraints are solved in that field; triangle-edge and
center constraints address intersections that a vertex-only check missed.
The lower bodysuit uses nearest body normals, and the skirt is fitted outside
the completed bodysuit. Associated trim receives the same field. Tiny rigid
fasteners retain their shape through a common translation.

The solver projects onto the most violated clearance inequality and rejects
non-convergence. The earlier unnormalized feedback method failed and is not
retained as the implementation. Analytic controls verify constant translation,
sample-area splitting, finite distant queries, independently evaluated vertex
and triangle constraints, and rejection of contradictory constraints. The full
field differential agrees with finite differences within `2.081e-11`.

Freshly loaded candidate 10 has no detected inward samples in the primary
garment screen, compared with 104 jacket and 400 bodysuit samples in the
rejected direct fit. The test uses a 0.5 mm inward threshold:

| Primary garment | Samples | Inward samples | Other observations |
| --- | ---: | ---: | --- |
| Jacket | 15,277 | 0 | One sample has less than 1 mm outward clearance |
| Skirt | 10,108 | 0 | 660 samples lie beyond the 5 cm query radius |
| Bodysuit | 25,426 | 0 | All samples lie within the query radius |

This is a local nearest-surface screen at vertices, edge midpoints and centers.
It does not prove closed-volume separation, self-collision, trim clearance,
body-shape limits or moving fit. Boots remain visibly unfitted. Front, side
and back diagnostic views were inspected; they are not final shaders.

One chest-panel triangle, source ID `82950`, turns 90.522 degrees relative to
its aligned normal, with an area ratio of 0.92651. Comparing world normals
alone flags this as reversed. Seven field differential samples across that
triangle have positive determinants (0.07226 to 0.34692), and its final normal
agrees with cofactor-transported orientation by 0.99725. This supports rotation
around the body rather than a sampled orientation inversion. It is not a global
invertibility proof. Local compression remains significant elsewhere in the
bodysuit, whose minimum area ratio is 0.15626; inspect it in posed skinning and
final material views before export acceptance.

The wider 3 cm field in candidate 9 clears the same body-contact screen but
introduces three over-90-degree normal rotations on the bodysuit. It is not
promoted over the narrower 2 cm candidate. Candidate 10 repeats candidate 8's
geometry exactly across all 53 saved coordinate arrays, adding the differential
witness rather than changing the garment to silence a diagnostic.

Source preservation, evaluated shape keys and independent wardrobe ownership
pass on candidate 10: all 23 source parts, 249,020 source triangles and the
unchanged 36,787-point/61,814-triangle accepted body remain intact. All 31,625
clothing triangles still belong to exactly one of the six wardrobe groups.

Next: complete footwear with reversible barefoot coverage, target skinning,
body morphs, materials, five palettes, physics, facial/Astral integration,
Windows cook and gameplay. The fit is still an authoring candidate, not a
completed outfit or an installed package.

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
- `work/slipstream-torsofit10.blend` and `.json`: retained continuous fit with
  differential witnesses.
- `work/slipstream-torsofit10-{source,alignment,ownership,contact}-check.json`:
  independent reload checks and their actual scope.
- `work/slipstream-torsofit-review8`: inspected views of candidate 10's matching
  geometry; equality is recorded in `slipstream-fit8-fit10-repeat-check.json`.
- `work/slipstream-offset-field-check7.json`: analytic controls and finite
  difference verification tied to the fitter's source hash.
- `work/slipstream-continuous-fit1/receipt.json`: frozen implementation,
  evidence hashes and removal records for superseded candidate blends.

Run the authoring scripts with Blender 5.2.2, `--background --factory-startup
--python-exit-code 1`. The partitioner takes `--blend`, `--recipe`, `--output`.
The fitter takes `--blend`, `--target`, `--output`; the target is the verified
CSS `work/eve26/skin-f16-import/skin.mesh.json`. The wardrobe checker takes
`--blend`, `--components work/new-outfits-components1/result.json`, `--output`.
Use new output paths. Commands and source inputs are recorded with each result.
The retained continuous fit adds `--smooth-width-cm 2 --enforce-clearance
--layered-lower-fit --interior-clearance`. Use the absolute target path under
`CustomShellSystem/work/eve26/skin-f16-import`, not an Eve-relative `work/eve26`.

The fitting query behavior was checked against Blender's
[BVHTree API](https://docs.blender.org/api/current/mathutils.bvhtree.html).
Surface queries and this offline fit do not establish runtime collision,
skeletal compatibility or game performance.
