# Author head controls recovered, October 7

The user supplied the optional Jail3 head and Cinderella animation archives.
The head contains all nine previously missing controls used by DumbDumb,
Iris Out and Jang Won Young Spin. This resolves their missing facial source
shapes. It does not identify the seven unresolved animation bone indices or
complete runtime acceptance.

## Source evidence

The author describes the optional custom MMD shape-key head in the
[DumbDumb posts](https://www.nexusmods.com/stellarblade/mods/2545?tab=posts).
It is the optional `Jail3_Head` download on the
[Cinderella files page](https://www.nexusmods.com/stellarblade/mods/2534?tab=files).
The archive spelling is Jail3; the contained mesh is `CH_P_EVE_Face_Jali3`.

| Input | SHA-256 |
| --- | --- |
| Jail3_Head-2534-v1-0-0-0-1764506935.7z | `3599f92ce5581cdfa9bfa1ef14eea977b95c89affc0c5f461f3543c43ca6e28c` |
| D_Cinderella_Main-2534-v1-1-0-1-1772027045.7z | `a5df5243ce63a775d0312eb50c8d76121521af98721ed4a57257f287457a0d9a` |

`work/cine-animation-port/author-head-source-v1/inventory.json` records the
safe member lists, separate extraction and extracted hashes. Both archives
remain unchanged. Their containers were mounted separately with the retail
containers and global dependency, so identical ATOOL paths cannot overwrite
one another during decoding.

The head has 24,296 render vertices, 190 morphs and 159 mesh bones. Its package
contains a mesh and physics asset, not the missing authoring Skeleton. Of its
morph names, 169 also exist in the original Eve assembly. The recovered names
are `smile`, `blink`, `lid_mid_closed_L`, `lid_mid_closed_R`, `iii`, `woo`, `eee`,
`ooo` and `brow_down_miner`.

| Performance | Duration (seconds) | Facial curves | Keys | Missing names on new head |
| --- | ---: | ---: | ---: | ---: |
| DumbDumb | 65.7 | 12 | 2,616 | 0 |
| Iris Out | 16.666666 | 23 | 1,892 | 0 |
| Jang Won Young Spin | 3.6666667 | 11 | 426 | 0 |
| Cinderella | 139.66667 | 12 | 3,897 | 0 |

Cinderella is an additional supplied animation at `/Game/Mods/HD-ATOOL_P/anim/972`,
with 8,381 samples. Indices 3269-3272 still have unidentified bindings. It has
been inventoried and decoded, but is not a retargeted or packaged body clip.
Keep it in the unfinished count. The earlier library counts exclude it.

## Transfer candidate

`MeshExport` now has a separate `CSS_MORPH_AUDIT=1` mode. It preserves original
render indices and all position/tangent deltas, avoiding the conversion
library's position-only glTF morph lookup at coincident vertices. A repeated
head decode is identical across the manifest and three binary payloads:
190 morphs, 580,434 delta records. Invalid mode values, conflicting modes and
an existing output are rejected. The .NET 10 exporter builds without warnings.

`capture_eve_face_correspondence.py` reads the immutable Eve assembly with
embedded scripts disabled. Its output preserves the raw facial basis,
per-corner UVs, triangle materials and the accepted fitted vertex order.
A fresh run matches every array in the earlier diagnostic capture.

`transfer_author_head_shapes.py` measures alignment from the matching eyes,
then uses position and UV correspondence. Scale is 0.9796633602; it adds an
axis-aligned offset after swapping the source horizontal axes. It directly
matches 23,824 facial vertices within 0.002 cm. At 174 welded vertices with
differing source deltas, incident triangles select the source corners and
corner angles weight their contribution. Two teeth vertices project onto
the teeth surface at 0.0176551 and 0.0006631 cm. The 44 unmatched neck-seam
vertices have no original facial deformation and remain stationary.

The candidate in `facial-author-head-export-v1` retains all 105 existing facial
shapes, all 21 expressions, existing performance entries and every fitted body
point unchanged. It adds the nine recovered controls plus Cinderella's
`surprise`, making 115 facial shapes. No nonfacial vertex receives a new delta.
The original 22 body/hand morphs are separate and remain intact, for 137 total
morphs per derived Eve mesh.

Four geometry tests cover distinct coincident surfaces, equal and unequal
welded corner angles, and rejection of an isolated corner. The neutral fitted
comparison is exactly zero. Twenty neutral/individual-shape renders and
26 selected performance renders compare source and candidate side by side.
The inspected front views show matching eyelid and mouth shapes without gross
collapse. This is sparse offline review, not full-motion or live acceptance.

Existing same-named source morphs differ locally around some mouth and eye
seams. The candidate deliberately retains the original 105 definitions;
these are not claimed to be numerically identical to the custom head. Full
performance review remains necessary, especially tongue and eye movement.
The rejected linear-combination and correspondence diagnostics are retained
as evidence, not used as accepted aliases.

## Unreal checkpoint

All six derived meshes under `/Game/CSS/AnimLab/SK_Face_Jali3V1_*` pass import
and fresh-process verification. Geometry, materials, bone binding, skeleton,
physics/post-process bindings, cloth properties and original 22 morphs are
preserved. The new shapes use the existing 0.015 cm morph filter; it was not
lowered. Black Pearl's maximum retained-delta difference is 0.000095176 cm.

Shared-path staging is under `shared-faces-jali3-v1`, using unique
`/Game/CSS/Locomotion/Faces/SK_Face_Jali3V1_*` assets. Fresh staging checks,
Windows cooking, container verification and independent cooked readback pass
for all six meshes. All 3,331,514 render-to-source vertex mappings were checked,
including retained body/hand morphs, render geometry, cloth and bindings.

The verified face catalog contains six adapters, 115 facial morphs, the same
21 expressions and five performances: Come Closer, Gesture 200, DumbDumb,
Iris Out and Spin. All 6,196 prepared facial keys retain their original source
chain; Gesture 200 retains its previously verified Bezier conversion bound.
Cinderella is not in this catalog because its body port is still pending.

The candidate is not installed or merged into the combined shared pack.
Complete performance review, combined packaging and live acceptance remain.
Commander White performance mapping and Unholy Genessa's rigid-mask feasibility
remain separate work. The custom head does not establish mappings for them.

The transfer's fresh replay produces a byte-identical 115-shape payload.
Three real-input rejection checks cover an existing output, an unknown shape
and an incomplete head audit. Verified duplicate payloads were removed after
comparison, with canonical copies and hashes retained in
`author-head-source-v1/duplicate-cleanup.json`.

Reproduction uses Blender 5.2.2 and the existing .NET 10 exporter. First capture
with `--source`, `--head-audit` (the audit JSON) and a fresh `--output`. Transfer
with `--head-audit` (the audit directory), `--body-samples` (the captured NPZ),
`--baseline` (the unchanged 105-shape export), repeated `--shape` options and a
fresh `--output`. Both scripts require Blender's `--` argument separator.
Run `test_head_transfer_corners.py` in background Blender. The existing
`run_facial_library.py` imports/inspects the export with `--revision Jali3V1`.
