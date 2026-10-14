# Older Eve surface data and tangent repair

The six original Eve meshes preserve all three UV channels, vertex colors and
imported normals within the check's packing bound. Their cooked tangent bases
have widespread alignment errors. A private Black Pearl import using the
triangle-corner ordering correction already used for Peony reloads successfully
with substantially fewer render vertices. Nothing was installed.

The previous goal turn made progress by committing the completed editable-part
source correspondence. This continuation checks the remaining surface fields
and identifies the next repair, rather than treating the source checks as a
complete shading pass.

## Installed readback

The existing MeshExport binary read each normal mesh through the retained
inventory mount. Eve's three installed container hashes and the two shared
binary hashes match the previous import-to-cooked audit before and after the
readbacks. The game remains closed.

`check_import_cooked_surface.py` matches float32 position, material-slot index,
all UV coordinates and RGBA bytes per corner, then checks triangle multiplicity
and winding. Each cooked normal must match a source normal at that corner key
within 0.012 per component. The maximum observed error is 0.011747480. This is
a bounded correspondence check, not bit-identical normals or an exact tangent
comparison. Seven diagnostic tests pass, including deliberate changes to each
UV channel, color, normal, winding, corner reordering and nonfinite input.

The normal bound includes UE's signed-byte packing and the reader's legacy
offset-byte conversion. Local UE 5.6.1 `RenderCore/Public/PackedNormal.h` packs
at 127 steps; the pinned CUE4Parse `FPackedNormal.cs` exposes XOR-biased bytes
using 127.5. The separate tangent diagnostic measures both interpretations.
The large alignment errors persist in both, so that small decoder bias does
not explain them.

| Mesh | Render vertices | Tangent/normal absolute cosine above 0.1, signed-byte interpretation |
| --- | ---: | ---: |
| Black Pearl | 558,243 | 502,349 |
| Planet Diving | 510,980 | 459,520 |
| Skin Suit | 444,427 | 399,231 |
| Bikini | 569,795 | 511,169 |
| Knitwear | 616,658 | 552,484 |
| Midsummer Alice | 631,411 | 566,405 |
| Current Peony V2, comparison control | 151,979 | 388 |

Peony is not declared flawless by this control. Its remaining anomalies require
their own interpretation, including the previously recorded collapsed-UV
fallbacks. Five older meshes also contain one near-zero tangent each.

## Supported cause and private correction

The earlier [CW tangent investigation](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P/docs/mesh-tangent-build.md)
and [Peony character work](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/authoring/new-outfits-fitting.md#ordered-character-cook-and-footwear-coverage)
already establish the relevant import failure. UE 5.6.1's
`MeshUtilities.cpp:MikkSetTSpaceBasic_Skeletal` writes at `FaceIdx * 3 + VertIdx`.
The old importer can feed wedge indices in a different order. Peony's existing
preparation recipe expands and orders the wedge, normal and color records by
triangle corner, preserving each corner's point and attributes.

The new `prepare_eve_corner_order.py` applies that same operation to the retained
Black Pearl import. It reloads the written interchange and checks every original
corner plus all untouched fields. Geometry, weights, bind bones, material slots
and morphs stay unchanged; only diagnostic package names and corner indexing
change. It does not change the accepted Blender library or installed asset.

`run_eve_corner_trial.py` copies only Peony's pinned project descriptor and
2 MB editor module into a private project. Import and fresh-process inspection
both exit zero with protected inputs unchanged. The reloaded result retains
193,589 triangles, 379 bones, 30 material slots and 22 morphs. It has **150,791
render vertices**, compared with 558,243 in the installed mesh. That count is
not an FPS measurement or proof that the candidate's full tangent basis is fixed.

The initial staging invocation stopped before launching the editor because its
hash helper received a string instead of a Path. The corrected runner verified
the three partially staged files against the pinned module and resumed before
the first import. No editor process was restarted after an observation timeout.

## Next required work

1. Read the rebuilt render basis independently and cook/read back the private
   candidate. Verify its complete geometry, surface, skin and morph preservation.
2. Carry the demonstrated correction through the six original normal/facial
   assemblies, preserving materials, cloth, post-process rigs and customization.
3. Validate shading, motion and actual performance before replacing accepted
   installed assets. Retain rollback and the accepted Peony/CW behavior.

This is an asset import issue distinct from proving the cause of Vistani's
moving sunlight noise. It does not justify enabling a global graphics workaround
by default. The existing MISC workaround and its pending checks remain separate.
Epic documents tangent recomputation and packed basis precision as separate mesh
build options, and runtime Skin Cache tangent recomputation as another path.
[Build options](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSkeletalMeshBuildSettings?application_version=5.6),
[rendering paths](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-rendering-paths-in-unreal-engine?application_version=5.6).

## Receipts and disk use

The [checkpoint manifest](../development/eve-surface-20261014.json) pins six
surface reports, the tangent control, private import/reload, source preservation,
tests and cleanup. Commands, logs and readbacks are under
`work/three-outfit-rig-physics/eve-surface-20261014`.

Following the [prewritten cleanup list](../development/cleanup-2026-10-14-eve-surface.md),
18 duplicate readback files were replaced with links to byte-identical retained
files, removing 267,408,662 duplicate logical bytes. Unique surface streams,
candidate sources/assets and evidence remain. Free space started at root
95G/theark 15G and reads root 95G/theark 14G after the new evidence and candidate;
the immediate readings around duplicate cleanup were also 95G/14G.
