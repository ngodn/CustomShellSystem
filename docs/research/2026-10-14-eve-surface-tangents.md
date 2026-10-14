# Older Eve surface data and tangent repair

The six original Eve meshes preserve all three UV channels, vertex colors and
imported normals within the check's packing bound. Their cooked tangent bases
have widespread alignment errors. A private Black Pearl import using the
triangle-corner ordering correction already used for Peony reloads, cooks and
passes independent packaged geometry and surface comparisons. Its remaining
tangent exceptions are confined to the known collapsed-UV fallback. Nothing
was installed. The user requests continued offline work for now.

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

## Private Windows cook and readback

`cook_eve_corner_trial.py` uses the validated CW Windows UE 5.6.1 cook recipe,
staging only the private mesh and skeleton. It exits zero with protected inputs
unchanged. `check_eve_corner_cook.py` builds a private IoStore container, verifies
it, extracts both packages, and confirms exact export/bulk payload roundtrips.
Independent skin/surface and morph readbacks both succeed.

Comparisons against the original accepted Black Pearl import pass for position,
triangle multiplicity/winding, all three UV channels, RGBA, imported normals,
bind transforms, named skin weights and all 22 positional morphs. The existing
packing bounds are unchanged. Maximum weight error remains 0.019562097;
maximum stored morph component error is 0.002006650 cm and omitted source
component magnitude is at most 0.014992058 cm. Render vertices remain 150,791.

The new classifier reuses the established Peony criterion: signed-byte unit
lengths within `sqrt(3)/127 + 0.00002`, absolute normal/tangent dot above `4/127`
flagged, and a fallback classified only when the tangent is exactly unit X and
every incident UV0 triangle has exactly zero area. These counts use that dot
bound, not the looser cosine threshold in the earlier table.

| Readback | Flagged frames | Collapsed-UV unit-X fallback | Unexpected exceptions |
| --- | ---: | ---: | ---: |
| Installed Black Pearl | 540,389 | 314 | 540,075 |
| Private ordered Black Pearl | 488 | 488 | 0 |
| Installed Peony V2 control | 430 | 430 | 0 |

All three satisfy the unit-length bound. The private candidate's maximum unit
length error is 0.006581356. Fallback frames remain unresolved shading, and
neither this classification nor the lower vertex count proves rendered quality,
normal-map handedness or performance. The classifier correctly distinguishes the
old widespread defect from the repaired candidate and accepted Peony control.

This private cook has placeholder materials and no full outfit cloth/post-process
or facial assembly. It is not suitable for installation as the finished outfit.
All cook/package commands, inputs, exit receipts and readbacks are retained in
`blackpearl-ordered/{cook,pack}` under the work directory below.

## Bound Black Pearl normal and facial pair

The next private candidate binds the verified geometry to Black Pearl's existing
shared Skeleton, `PA_Body`, `ABP_Secondary` and all 30 material slots. The earlier
Peony dependency workflow supplies 44 pinned editor dependencies. Their object
paths match the current installed readback. These dependencies are editor-only
inputs; no regenerated material, rig, texture or Skeleton enters the package.
Black Pearl has no cloth assets, unlike Planet Diving, Knitwear and Alice.

The accepted expression source adds 115 shapes to the rebuilt mesh's 22 existing
morphs. Creation and fresh-process reload pass the existing facial verifier,
including exact non-expression mesh-description and built-section preservation:
96,587,582 description bytes, 28 sections and zero cloth assets. The shared
Skeleton's bones, nine virtual bones and sockets remain unchanged. Both meshes
retain their original 379 raw mesh bones.

Windows cooking exits zero. The private IoStore contains only
`SK_BlackPearlBound` and `SK_Face_BlackPearlOrdered`; its `.ucas` is 28,213,218
bytes. Exact export/bulk roundtrip and independent geometry, all-UV/color/normal,
skin and positional-morph comparisons pass for both meshes against the accepted
import plus the accepted expression input. Each has 150,791 render vertices and
193,589 triangles. The normal mesh has 22 morphs and the facial mesh 137. Existing
weight/morph cooking bounds remain unchanged.

The paired cooked render digests are identical, and all original morph digests
are preserved by the facial addition. Compared with each corresponding installed
mesh, all decoded non-morph properties, material-slot structures, imported bounds
and bind-reference digests match. The normal and facial meshes already have
different bounding-sphere radii in the installed package (105.43666 and
93.069046 cm); the rebuilt pair preserves those respective values. Empty Nanite
resource fields match after excluding only the offset of their verified
zero-byte bulk header, which moves with the serialized file size.

Both meshes retain exactly 488 collapsed-UV unit-X tangent fallbacks and zero
unexpected exceptions under the established check. This is not proof of rendered
shading, normal-map handedness, runtime motion or performance. The repaired pair
still uses private paths and is not installed. Permanent-path integration and
the other five older Eve pairs remain.

Recipes are `stage_blackpearl_surface_bindings.py`,
`bind_blackpearl_surface_editor.py`, `run_blackpearl_surface_bindings.py`,
`cook_blackpearl_surface_bindings.py`, `check_blackpearl_surface_bindings.py`
and `verify_blackpearl_surface_assembly.py` under `tools/diagnostics`. Retained
receipts include `bindings-{create,reload}.json`, `binding-cook/exit.json` and
`binding-pack/{readbacks-verified,assembly-verified}.json`.

## Next required work

1. Integrate the verified Black Pearl pair at permanent paths, and carry the
   demonstrated correction through the other five original normal/facial
   assemblies, preserving materials, cloth, post-process rigs and customization.
2. Preserve the collapsed-UV limitation explicitly; do not claim every tangent
   frame is repaired or change accepted UVs merely to clear this diagnostic.
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

The Windows cook and package check also ran successfully through the relocated
work path, `/run/media/eins0fx/theark/CSS/CustomShellSystem-work`. After the cook,
readbacks and container exit, the
[prelisted disposable Wine prefix](../development/cleanup-2026-10-14-eve-corner-cook.md)
was removed (1,084,763,594 logical file bytes). Both immediate `df -h` readings
show root 95G and theark 13G available; rounded filesystem figures do not
establish equivalent reclaimed physical space. Recipes, useful DDC, cooked
outputs, original assets and readback evidence remain.

After the bound-pair checks, the
[binding cleanup list](../development/cleanup-2026-10-14-blackpearl-bindings.md)
removed the finished Wine prefix, 46 verified duplicate cook inputs and the
regenerable dependency cook. The four shipping mesh payloads remain byte-identical
in `binding-pack/stage`. Removed logical file bytes total 2,040,966,091.
Immediate `df -h` readings changed from root 95G/theark 12G to root 95G/theark
13G. Sources, private editor pair/dependencies, DDC, recipes, logs, packages and
readbacks remain. Cleanup receipt: `binding-cleanup-completed.json`.
