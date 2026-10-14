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

Skin Suit and Bikini have since reached paired cooked preservation, but fail
the strict tangent-length check described below. Resolve or explicitly assess
that small residual defect before promoting either pair to final paths.

1. Assemble the verified permanent-path Black Pearl payloads into Eve's final
   container with the remaining repairs. Carry the correction through the other
   five original normal/facial assemblies, preserving materials, cloth,
   post-process rigs and customization.
2. Preserve the collapsed-UV limitation explicitly; do not claim every tangent
   frame is repaired or change accepted UVs merely to clear this diagnostic.
3. Validate shading, motion and actual performance before replacing accepted
   installed assets. Retain rollback and the accepted Peony/CW behavior.

## Skin Suit and Bikini paired cook

Both ordered candidates now pass fresh editor import/reload, Windows cook,
IoStore roundtrip and independent source geometry, skin, UV, color, imported
normal and morph comparisons. Their bound normal/facial pairs repeat these
checks and preserve the current installed material/rig references, non-morph
properties, respective bounds and raw bind pose. The normal and facial render
digests agree; all 22 original morphs survive and the facial mesh has 137.

| Outfit | Render vertices | Triangles | Material slots | Sections |
| --- | --- | --- | --- | --- |
| Skin Suit | 122,596 | 153,507 | 24 | 23 |
| Bikini | 142,353 | 200,619 | 32 | 32 |

Both pairs have zero unexpected nonorthogonal frames and 284 known collapsed-UV
unit-X fallbacks per mesh. However, each also has one zero-length tangent. The
overall `binding-pack/assembly-verified.json` deliberately records `passed:false`
and exits 1, while `bindings_and_base_render_preserved:true` records the checks
that did pass. The tangent gate has not been weakened.

The remaining zero is not the old installed zero at the same source corner:
the exact installed-corner comparison fails for both. Bikini's vertex is in
`heel-1`, at approximately (-23.8883, -2.98826, 0.990429) cm. Skin Suit's is in
`SkinToenailLining`, at (22.88835, -2.96447, 6.280544) cm. These are separate
foot-area defects, not the same vertex. Their trace reports remain failed.

`probe_eve_mikk_zero.py` links a small C17 probe against the unmodified MikkTSpace
archive bundled with the installed UE 5.6.1 editor. It reproduces zero tangents
from the exact source triangles without UE import or cooking. Skin Suit has one
matching triangle, Bikini two opposite windings. Both degenerate-processing
settings reproduce the zeros. Normalizing the supplied normals in float32 or
float64 also leaves them zero. These results isolate a tangent-generator edge
case for these triangles; they do not prove its full numerical cause, a fix,
visible severity or the cause of the sunlight noise report. The upstream
[MikkTSpace interface](https://github.com/mmikk/MikkTSpace/blob/master/mikktspace.h)
describes unit tangent output, but the local probes establish this exception.

Artifacts live under `skinsuit-ordered` and `bikini-ordered`. Key receipts are
`verified.json`, `pack/verified.json`, `bindings-{create,reload}.json`,
`binding-cook/exit.json`, `binding-pack/readbacks-verified.json`, the failed
`binding-pack/assembly-verified.json`, and `mikk-zero-probe/result.json`.
The diagnostic commands use the existing Python 3.14/UE 5.6.1 pipeline; the
standalone C probe was compiled as C17 with GCC 16.2.1.

The first Bikini binding invocation failed because the staging loop overwrote
the variant name with an asset filename. The corrected script uses a separate
variable. Both original plans and Bikini's failed command/log/exit remain under
`binding-plan-name-recovery`; the corrected pair passes separate create/reload
processes. Its wrongly named mesh has been inventoried and removed. Skin Suit's
plan was corrected before running its binding command. Earlier Black Pearl
recipes and their hashes were left intact.

Only the two new meshes enter each private test container. Existing dependencies
remain byte-identical, and editor staging uses absolute links instead of extra
copies. Nothing is installed. Permanent paths, full Eve assembly, the three
cloth-bearing outfits and existing live/performance/release gates remain.
Cleanup follows [the prewritten list](../development/cleanup-2026-10-14-eve-surface-pairs.md).

## Permanent Black Pearl paths

The final editor pair uses the existing paths:

- `/Game/CSS/eins0fx/EveSB/SK_BlackPearl2`
- `/Game/CSS/eins0fx/EveSB/Faces/SK_Face_Jali3V1_BlackPearl2`

Fresh-process preservation checks exclude zero added morphs, so all original
mesh-description and built-section data must match the verified private pair.
Both saved meshes pass. Their 33 direct editor dependencies are the existing
Skeleton, body physics, post-process graph and 30 materials; neither mesh
references a diagnostic package.

The first create command saved both meshes and passed the preservation checks,
then exited 255 because the registry returned `None` for a newly saved package.
The failed command/log/exit and initial plan remain. The correction explicitly
rescans the saved files before querying dependencies, as supported by the
[UE 5.6 registry API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetRegistry?application_version=5.6).
A recovery process checks the existing files without rebuilding, then a separate
reload process repeats the checks. Both exit zero with protected inputs unchanged.
This is not a successful first invocation or a retry after an observation timeout.

Windows cooking and the final two-package IoStore roundtrip pass. A reader-only
fixture supplies the 32 direct Eve material/rig dependencies from the installed
container, alongside the accepted shared container and base game. The original
Eve container is excluded from this readback mount, so the old Black Pearl
packages cannot mask the replacement. This fixture is not a playable release
package and will not be installed.

Independent readback matches the verified private pair exactly for its complete
decoded mesh digests, normalized properties, material-slot data and imported
bounds. The digest's `Normal` field is the CUE4Parse packed basis array, including
tangent and normal entries, not only a single surface normal. The comparison
therefore retains the tangent correction and known fallback limitation, as well
as topology, skin, colors, all UVs, morphs and bind data. Reader and mapping
identities still match the earlier verified readback.

Recipes and receipts are under `blackpearl-final`, including `recover.json`,
`reload.json`, `cook/exit.json`, `pack/verified.json` and
`pack/reader-identity.json`. Final cooked payloads remain under `pack/stage`.
No installed asset, catalog or user setting changed. Combining these payloads
with the complete Eve release container remains, along with the other five
older outfits and gameplay/performance checks.

For the three cloth-bearing outfits, the existing authoring code shows the
required persistence step: `BindToSkeletalMesh` updates render mapping, and
`UserSectionsData` must retain `CorrespondClothAssetIndex` and `ClothingData`
before rebuilding. The next repair must preserve the accepted simulation mesh,
weight maps and solver settings while rebinding the reordered render mesh.
Regenerating cloth with the original factory defaults is not equivalent.
Local references: UE 5.6.1 `ClothingAsset.cpp:139` and project
`CSSBindClothCommandlet.cpp:167`. This transfer is not implemented or verified yet.

The installed normal/facial cloth baseline is now hash-pinned in
`cloth-preservation-baseline.json`. Physical data and explicitly serialized
Chaos configuration properties match within each pair:

| Outfit | Simulation vertices / triangles | Render section / material |
| --- | --- | --- |
| Planet Diving, PFit14 | 18 / 16 | 24 / 26 |
| Knitwear, KCloth4 | 1,679 / 2,999 | 16 / 16 |
| Midsummer Alice, ACloth3 | 624 / 1,224 | 26 / 26 |

Each has one cloth asset at mesh LOD0. These are existing cooked section indices,
not safe assumptions for the rebuilt mesh. The baseline pins physical data,
LOD data, explicit solver properties, physics reference and section identity.
It does not establish editor-default equivalence or successful transfer.

UE 5.6.1 already provides `UClothingAssetFactory::CreateFromExistingCloth`
(`ClothingAssetFactory.cpp:193`): duplicate under the target mesh, assign a new
GUID, empty `LodMap`, refresh bone mapping and invalidate cached data. Clearing
the old LOD map matters because `BindToSkeletalMesh` rejects an already mapped
asset LOD. The native skeletal mesh editor then persists section user data after
binding (`SkeletalMeshEditor.cpp:981`). Prefer this existing copy path over
recreating simulated geometry. Verify simulation inputs and solver properties
after cache rebuilding, then save/reload and cook before treating it as preserved.
The [upstream API entry](https://dev.epicgames.com/documentation/unreal-engine/API/Editor/ClothingSystemEditor/UClothingAssetFactory/CreateFromExistingCloth)
documents the method; the implementation details above were checked against the
local 5.6.1 source, not the site's newer default version.

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

Final-path cleanup followed
[its prewritten list](../development/cleanup-2026-10-14-blackpearl-final.md).
The terminal cook's prefix, 45 verified duplicate cook inputs and regenerable
cooked tree were removed after all four mesh payloads matched retained staging.
Another 89 byte-identical editor copies became absolute links to retained sources.
Total removed duplicate/disposable logical file bytes: 2,611,650,239. Immediate
`df -h` changed from root 95G/theark 11G to root 95G/theark 13G. The created
mesh sources, receipts, readbacks and useful DDC remain.
