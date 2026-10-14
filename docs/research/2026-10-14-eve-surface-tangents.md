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

## Explicit tangent storage trial

The next private trial is under `eve-surface-20261014/tangent-bake`. The native
UE 5.6.1 commandlet matches source triangles to the already verified ordered
render data by material, source point IDs, winding, positions, normals and all
UV channels. It copies valid render tangents into the source mesh description,
repairs the single zero corner, and disables tangent recomputation on the
duplicate. Geometry, UVs, weights, morphs and accepted sources are not edited.
The cooked checks must allow only that build flag and the formerly zero basis
to change. This does not change the game's runtime Skin Cache settings.

Attempt 1 saved and freshly reloaded all four meshes with valid tangent lengths,
but is rejected. Its fallback used the mesh description's existing tangent and
sign. For Bikini, the fallback's dot product with the independently calculated
triangle UV tangent is only 0.37895. Skin Suit's direction agrees, but its sign
does not agree with the valid render triangle neighbors. Length checks alone
would have missed both problems. See `rejected-v1-gradient-comparison.json`,
the attempt 1 create/reload receipts, and the archived module/source under
`previous-builds/1`.

Attempt 2 derives the replacement from triangle positions and UV0 in double
precision, projects it onto the source normal, and requires consistent
handedness from the valid corners of the same render triangle. It rejects
collapsed UVs, an invalid direction or ambiguous neighbor signs. The remaining
valid tangent values are copied unchanged. The independent cooked comparator
requires exact preservation of every previously valid packed basis, the same
repaired corner locations, a direction matching the UV gradient within packed
precision, and matching neighbor signs. Comparing the unchanged broken mesh
against itself correctly fails (`reject-unchanged-zero.json`).

Attempt 2's four editor saves/reloads and Windows cook completed. The first
packaged Skin Suit comparison passes geometry, weights, morphs, UVs, colors,
normals and the strict tangent comparison. There are zero new tangent errors;
every previously valid packed basis remains byte-identical. The replacement
direction differs from the independent packed-input gradient by 0.002594.
However, the assembly check fails: rebuilding reset the normal mesh's imported
sphere radius from 105.62397 to 92.85891. The decoded Bikini normal similarly
changed from 110.09827 to 97.380066. Box centers/extents and both facial mesh
bounds remain unchanged. `bounds-failure/result.json` retains the failure;
`pack/meshes.json` records all four decoded assets. No acceptance is inferred
for unchecked meshes from the first passing tangent check.

The commandlet now restores the source's imported bounds after rebuilding.
Only the two regular meshes need another save/reload (attempt 3); facial
sources remain at attempt 2. The matching cook/readback commands use
`--bounds-repaired` and separate `cook3`/`pack3` output folders.

The corrected four-mesh cook, IoStore roundtrip and independent readback now
pass (`pack3/verified.json`). Each mesh has zero zero-length tangents, and every
previously valid packed tangent/handedness value remains identical. The repaired
direction errors are 0.002594 for Skin Suit and 0.005334 for Bikini against the
independent gradient calculated using packed normals. Both are below the 0.02
comparison bound. Geometry, weights, all UVs, colors, normals, material slots,
rig references, reference transforms, respective imported bounds and all morphs
are preserved. The sole intended non-morph property change is
`bRecomputeTangents: true -> false`, to retain the verified explicit tangents.

| Pair | Render vertices per mesh | Triangles per mesh | Regular/facial morphs |
| --- | ---: | ---: | ---: |
| Skin Suit | 122,596 | 153,507 | 22 / 137 |
| Bikini | 142,353 | 200,619 | 22 / 137 |

All four retain 284 known unit-X fallbacks on collapsed UV0 triangles, with
zero unexpected nonorthogonal vertices and unit lengths within the packed
precision bound. Those fallbacks remain a shading limitation. This is an
offline preservation result, not rendered acceptance or proof of the cause of
Vistani's report. Permanent paths, full Eve packaging, the three cloth-bearing
repairs and gameplay/performance/release checks remain. No installation or
launch request. The prewritten removal list is
[tangent bake cleanup](../development/cleanup-2026-10-14-eve-tangent-bake.md).

## Permanent Skin Suit and Bikini paths

The four repaired source meshes are now saved and cooked at their existing
release paths under `/Game/CSS/eins0fx/EveSB`:

- `Fit/SK_SFit16` and `Faces/SK_Face_Jali3V1_SFit16`.
- `Fit/SK_BFit1` and `Faces/SK_Face_Jali3V1_BFit1`.

The final-path project is `eve-surface-20261014/eve-tangent-final`.
Creation and a separate editor reload pass exact mesh-description/built-section
comparison against the verified private sources, with no added morphs excluded.
There are 28 direct dependencies across the four meshes; none is a diagnostic
package. The input assets use read-only absolute symlinks, with source hashes
checked before and after each operation.

Windows cooking, IoStore conversion/roundtrip and independent decoded comparison
pass (`pack/verified.json`). Every final mesh matches its private cooked source
for all recorded render fields, packed tangent basis, reference transforms,
morphs, normalized properties, material slots, imported bounds and empty Nanite
data. Counts remain 122,596 Skin Suit vertices or 142,353 Bikini vertices, with
22 regular and 137 facial morphs. No previously repaired value was regenerated
into a different result by the path change.

The reader mount deliberately excludes the original Eve containers. It contains
the candidate, base-game/shared dependencies and a fixture of exactly 27 direct
Eve material/rig packages. This prevents an old mesh provider from masking a
missing replacement. That fixture is for readback only, not a release package.

Recipes: `run_eve_surface_final.py`, `finalize_eve_surface_editor.py`,
`cook_eve_surface_final.py`, `check_eve_surface_final.py` and
`cleanup_eve_surface_final.py` under `tools/diagnostics`. The existing Black
Pearl recipes and their pinned evidence are unchanged.

No installation or live check. Full Eve container integration, the three
cloth-bearing outfit repairs and the goal's remaining runtime/performance/
release gates are still open. The 284 known collapsed-UV fallbacks per mesh
remain recorded; this does not close the separate sunlight-noise report.

After the [prelisted cleanup](../development/cleanup-2026-10-14-eve-final-paths.md),
2,131,005,002 duplicate/disposable logical bytes were removed. Final sources,
recipes, packages and readbacks remain. Immediate `df -h` readings changed from
root/theark 94G/7.9G to 94G/8.8G; logical bytes are not a physical-space measure.

## Planet Diving cloth transfer

Planet's reordered source preserves all original corners, weights and morphs.
Its first Windows cook/readback passes geometry and surface correspondence,
but leaves one zero tangent. The retained `planet-ordered/pack/tangent-fallbacks.json`
records that failure alongside 294 known collapsed-UV unit-X fallbacks and no
unexpected nonorthogonal vertices. Both regular and facial editor tangents now
pass the explicit bake and separate reload with zero zero-length/nonunit
vectors (`tangent-bake/planet-normal-1-verified.json` and
`planet-facial-1-verified.json`). Counts are 136,940 vertices, 175,142 triangles
and 22/137 morphs.

The first native cloth transfer failed its post-rebuild binding check and saved
no target asset. Its command, failure log, exit receipt and original host recipe
remain under `cloth-transfer`. UE 5.6.1's `BindToSkeletalMesh` itself triggers a
scoped post-edit rebuild. The failed helper wrote persistent section data only
after that call. The revised helper follows the engine's
`FSkeletalMeshEditor::ApplyClothing`: hold an outer post-edit scope around both
the bind and the section-data write, so the rebuild sees the persistent binding.
Local source: `SkeletalMeshEditor.cpp:958` and `SkeletalMesh.cpp:279` in the
pinned engine. Epic also describes the required persistent section fields in
[its cloth-binding support reply](https://forums.unrealengine.com/t/stitching-together-meshes-issues-with-cloth/2593003/3).

Both meshes now pass transfer and separate reload
(`cloth-transfer/planet-normal-2-verified.json` and
`planet-facial-verified.json`). They preserve the accepted
18-vertex/16-triangle simulation, all reflected cloth/config values except the
new asset GUID, transition arrays and tether data. A deliberate physical-vertex
mutation is rejected by the comparator. The reordered tail has 1,174 mapped
render vertices on material slot 26; its lower vertex count reflects the repaired
corner sharing, not removed tail triangles: all 1,648 tail render triangles remain.

The Windows paired cook, IoStore roundtrip and independent source checks now
pass. Every previously valid packed tangent/handedness value is unchanged from
the ordered standalone cook. Exactly one zero corner is repaired in each mesh,
with UV-gradient direction error 0.002736 and matching handedness. All 294
known collapsed-UV fallbacks remain; no unexpected exceptions or zero tangents
remain. Geometry, weights, all UVs/colors/imported normals and 22/137 morphs
pass source comparisons.

`cloth-transfer/planet-pack/bindings-verified.json` confirms decoded physical
cloth/config preservation except the new GUID, unchanged materials, bounds,
bind references and mesh properties except explicit tangent storage. Normal
and facial render and cloth-mapping digests match. This does not prove runtime
collision or row-for-row mapping equality with the differently indexed old
mesh. Native transfer checks also cover transition and tether arrays that the
cooked summary does not fully expose.

The first cooked binding comparator rejected the shifted file offset of an
empty Nanite bulk header. Its source and failure explanation remain in
`planet-pack/failed-comparator`. The current check reuses the established
`empty_nanite` comparison, which first requires zero elements, zero disk size
and no bulk flags. All other Nanite fields still match. No asset was changed
to resolve that comparator failure.

Private outputs remain `SK_PlanetNormalCloth2` and `SK_PlanetFaceCloth1` under
`/Game/CSSAuthoring/EveSurface20261014`. Permanent paths, full Eve integration,
Knitwear/Alice repairs and runtime/performance/release gates remain open.

The new tools operate on private copies and hash their inputs before and after
use. No game installation, settings change or launch request. The earlier
175 checkpoint pins still match. The cloth simulation was preserved, not retuned.

After the [prelisted cleanup](../development/cleanup-2026-10-14-eve-planet-cloth.md),
2,406,783,008 logical bytes of paired cook duplicates and disposable prefix
were removed. Both target sources and all shipping payload hashes remain intact.
Immediate `df -h` changed from root/theark 94G/6.3G to 94G/7.0G available.
The earlier standalone cleanup removed another 1,462,198,829 logical bytes.
Retained data includes sources, compiled module, useful DDC, package/readbacks
and failure evidence. No unknown or unrelated folders were touched.

## Permanent Planet Diving paths

Both Planet meshes now use their existing release paths:
`/Game/CSS/eins0fx/EveSB/Fit/SK_PFit14` and
`/Game/CSS/eins0fx/EveSB/Faces/SK_Face_Jali3V1_PFit14`.
Sources and receipts are under `eve-surface-20261014/planet-cloth-final`.

Creation and a separate fresh reload preserve the entire mesh-description
byte stream, all 27 built sections including their cloth mappings, bind poses,
persistent cloth data and all 22/137 morphs. There are 27 direct dependencies
across the pair, with none in a diagnostic namespace.

Windows cook and two-package IoStore roundtrip pass. Independent decoded
comparison (`pack/verified.json`) matches the verified private pair exactly
for render, tangent, bind, morph and cloth-mapping digests, normalized mesh and
cloth/config properties (including GUIDs), material slots, bounds and empty
Nanite metadata. The reader excludes the old Eve mesh containers and uses
only 24 direct Eve material/rig dependencies plus shared/base-game packages.
This fixture is not a playable release package.

After the [prelisted cleanup](../development/cleanup-2026-10-14-eve-planet-final.md),
2,034,305,402 logical bytes of duplicate cook inputs/output and the disposable
Wine prefix were removed. Retained final sources and shipping payload hashes
remain intact. Immediate `df -h` was root/theark 94G/6.2G before and 94G/6.9G
after cleanup. Sources, useful DDC, receipts and package/readback evidence remain.

No installation or live check. The 294 known collapsed-UV fallbacks per mesh
remain. Knitwear/Alice repairs, combined Eve packaging and the full goal's
runtime, performance and release requirements are still open.

## Knitwear cloth repair

Knitwear's reordered import and fresh reload preserve all 216,613 triangles,
379 bind bones and 22 body morphs. Its first Windows cook, IoStore roundtrip
and independent source comparisons pass for geometry, weights, morphs and
surface fields. The repaired corner sharing produces 150,466 render vertices.
The initial tangent report retains one zero-length tangent, 284 known collapsed-
UV fallbacks and no unexpected nonorthogonal vertices; it is not a complete
shading pass.

The existing material/rig bindings and 115 facial expressions have been applied
to separate regular/facial intermediates and verified after fresh reload.
Both tangent bakes now fix exactly one zero corner and reload with no zero or
nonunit tangents. The regular/facial cloth transfers pass creation and separate
reload, preserving the accepted 1,679 physical vertices, 2,999 simulation
triangles, reflected settings, transition arrays and tether data. The comparator
rejects a deliberately changed physical vertex.

The paired Windows cook, IoStore roundtrip and independent geometry, surface,
weights, morph and tangent comparisons pass. Every previously valid packed
basis stays unchanged; each repaired zero corner matches the UV-gradient
expectation within 0.005335, with matching handedness. Both meshes retain
284 known collapsed-UV fallbacks and zero unexpected nonorthogonal vertices.

`cloth-transfer/knit-pack/bindings-verified.json` confirms exact decoded cloth/
config preservation except the new GUID, and matching material slots, bounds,
bind references and mesh properties except explicit tangent storage. The
regular/facial render and cloth-mapping digests match. The assigned garment
retains 38,792 render triangles and now has 20,324 mapped render vertices.
Regular/facial morph counts remain 22/137. These checks do not prove runtime
collision or row-for-row mapping equality with the differently indexed old
mesh. Permanent paths, full Eve integration and gameplay remain unverified.

First-cook cleanup follows the
[prelisted removal groups](../development/cleanup-2026-10-14-eve-knit-cloth.md).
It removed 1,465,397,199 logical bytes of duplicates and disposable prefix,
retaining source assets, package payloads and the failed tangent classification.
Immediate `df -h` changed from root/theark 94G/6.0G to 94G/6.7G available.
No game files or saved settings changed.

The [paired cleanup](../development/cleanup-2026-10-14-eve-knit-pair.md) then
removed 2,520,604,139 duplicate/disposable logical bytes after verifying both
retained sources and every shipping payload. Immediate root/theark free space
changed from 94G/4.6G to 94G/5.3G. Receipts, sources, DDC and readbacks remain.

## Permanent Knitwear paths

Both repaired Knitwear meshes now use their existing release paths:
`/Game/CSS/eins0fx/EveSB/Fit/SK_KCloth4` and
`/Game/CSS/eins0fx/EveSB/Faces/SK_Face_Jali3V1_KCloth4`.
Source assets and receipts are under `eve-surface-20261014/knit-cloth-final`.

Creation and fresh reload preserve the entire mesh-description byte stream,
all 28 built sections including cloth mappings, bind poses, persistent cloth
data and all 22/137 morphs. Neither final mesh has diagnostic dependencies.
The pair has 28 direct dependencies; the readback fixture uses 25 Eve material/
rig packages plus shared/base-game inputs and excludes the old Eve mesh providers.

Windows cook, IoStore roundtrip and independent decoded comparison pass.
`pack/verified.json` records exact agreement with the verified private pair
for render, tangent, bind, morph and cloth-mapping digests, normalized mesh
and cloth/config properties including GUIDs, material slots, bounds and empty
Nanite metadata. Counts remain 150,466 vertices and 216,613 triangles per mesh.

The [prelisted final cleanup](../development/cleanup-2026-10-14-eve-knit-final.md)
removed 2,054,587,132 duplicate/disposable logical bytes after checking the
retained final sources and all shipping payloads. Immediate `df -h` changed
from root/theark 94G/4.7G to 94G/5.4G. Useful DDC, sources, package/readbacks
and receipts remain. This is an offline final-path result, not installation
or visual acceptance. Alice, combined Eve packaging and the full goal's live,
performance and stable-release gates remain. The 284 known collapsed-UV
fallbacks per mesh are still recorded as a shading limitation.

## Shared retained cook cache

The [cache cleanup list](../development/cleanup-2026-10-14-eve-cook-caches.md)
covered six completed caches, all exact file-by-file subsets of the retained
`knit-ordered/cook/out/ddc`. Each old directory was renamed aside, replaced by
an absolute symlink, and checked through its original path before removing
the duplicate. The canonical cache must remain while these links exist.
Knitwear's later paired cook successfully read its cache through the original
Skin Suit/Bikini path.

`cache-dedup-audit.json`, `cache-dedup-plan.json` and
`cache-dedup-completed.json` retain the selection, every file hash and results.
2,133,428,593 duplicate logical bytes were removed; immediate `df -h` remained
root/theark 94G/6.6G, so this is not a claim of equivalent physical space gained.
All 230 existing checkpoint pins still matched afterward. Black Pearl, Bikini
and the first Planet caches were not identical subsets and were left intact.
Everything outside the explicit list was unassessed and untouched.

## Cloth-transfer build cleanup

The [prelisted build cleanup](../development/cleanup-2026-10-14-cloth-transfer-build.md)
removed only `cloth-transfer/editor/Intermediate` after confirming the successful
build receipt, compiled module hashes and no active project compiler/editor
process. Its 335,578,238 logical bytes were regenerable object files and the
precompiled header. Source files, the compiled module, earlier build evidence
and all assets remain, with retained hashes checked after removal.

Receipts: `cloth-transfer/intermediate-cleanup-plan.json` and
`intermediate-cleanup-completed.json`. Immediate `df -h` read root/theark
94G/6.1G both before and after; a later reading was 94G/6.4G. The logical-byte
count is not a physical-space claim.

## Combined Eve packaging notes

The final six-pair integration must replace only the twelve repaired normal/
facial mesh packages in the current accepted Eve container. Preserve Peony,
rigs, animations, materials, textures, saved-look IDs and controls. Verify all
unchanged export/bulk payloads and the final twelve mesh readbacks, then refresh
Eve container fingerprints in the current shared facial-adapter metadata.
The existing `tools/eve-fit/assemble_body_limit_trial.py` demonstrates that
metadata update and its catalog/import checks, but its rig-only replacement set
and historical baseline are not the new assembly recipe.

`tools/eve-fit/release_peony.py` still requires the historical beta.12 tag and
shared version. Do not use it as the stable-release path. The active goal
requires CSS/shared 1.0.0 and Eve 1.5.0, alongside the other two outfit releases.
Use the current shared metadata that includes accepted CW walking support;
reusing the older Peony baseline would risk discarding later changes. Final
packaging remains pending Knitwear/Alice and full-goal release gates.

## Alice cloth repair

The accepted input is `work/eve26/alice-import4/alice.mesh.json`. Ordered-corner
preparation, import, fresh reload, Windows cook and independent IoStore readback
pass under `alice-ordered`. The candidate preserves source geometry, skinning,
UVs, colors, normals and all 22 morphs, with 161,517 render vertices, 220,611
triangles and 379 bones. Its first cooked readback has one zero-length tangent
and 284 known collapsed-UV unit-X fallbacks, with zero unexpected nonorthogonal
frames. The failed unit-length result remains in `pack/tangent-fallbacks.json`;
the standalone pass does not clear that quality gate.

The regular/facial binding intermediates preserve 379 bones, nine virtual bones
and 22/137 morphs. Their fresh reload passes the accepted material/rig bindings
and 115-expression preservation checks. Native tangent baking repairs exactly
one zero corner per mesh, leaving valid tangents intact; both versions reload
with zero zero-length/non-unit tangents and recomputation disabled. Receipts:
`tangent-bake/alice-{normal,facial}-1-verified.json`.

Native cloth transfer then passes creation and fresh reload for both versions.
The existing 624 simulation vertices, 1,224 simulation triangles, cloth settings,
transitions and tethers are preserved except for the new asset GUID. The new
mapping covers 937 render vertices. A deliberate physical-vertex mutation is
rejected by the comparator. Private assets are `SK_AliceNormalCloth1` and
`SK_AliceFaceCloth1` under `/Game/CSSAuthoring/EveSurface20261014/`; their receipts
are `cloth-transfer/alice-{normal,facial}-verified.json`.

### Split-drive Alice cook

Theark had 6.1G free at the start. New cook scratch uses
`build/cook-scratch/alice-ordered/out` on the root filesystem, reached through
`alice-ordered/cook/out`. The existing work relocation is unchanged.
`tools/diagnostics/cook_eve_alice_candidate.py` preserves the earlier pinned
recipes, checks 3 GiB free for work plus 8 GiB for scratch, verifies the engine
build and stages the same accepted module/config inputs. Python is 3.14.7;
`mise current` declares no Python override.

[Docker's bind-mount documentation](https://docs.docker.com/engine/storage/bind-mounts/)
specifies an existing host source and container destination. The new command
binds the resolved root scratch directory to `/out` explicitly. Live Docker
inspection showed that exact read/write mount during the cook. The later
attempt to save a second inspection found the container already removed after
successful completion; it was not restarted. `command.json`, `storage.json`,
logs and `exit.json` retain the cook evidence.

The initial storage guard stopped before staging because Btrfs reports distinct
device numbers for `/` and `/home` subvolumes. `findmnt` confirms both belong to
UUID `eb461adf-5d9b-4722-9b4a-11b5ade2d24f`. The guard now checks that filesystem
UUID and the ordinary, resolved scratch path. It does not lower the free-space
requirements or allocate on theark accidentally.

Following the [prewritten list](../development/cleanup-2026-10-14-eve-alice-cloth.md),
the finished cook's prefix, duplicate inputs and regenerable cooked tree were
removed after retained hashes passed. Removed logical file bytes total
1,469,207,752. Immediate `df -h` reads root/theark 93G/5.5G both before and after;
this is not a claim of equivalent physical space reclaimed. Sources, reordered
candidate, editor assets, DDC, package payloads and pass/fail readbacks remain.
Receipts are `alice-ordered/candidate-cleanup-{plan,completed}.json`.

The paired cook uses `tools/diagnostics/cook_eve_alice_cloth.py` with normal and
facial attempt 1. Both `cloth-transfer/alice-cook/out` and `project` link to
`build/cook-scratch/alice-cloth/{out,project}` on root. A live Docker inspection
records both exact read/write mounts in `alice-cook/mount-verified.json`.
The Windows cook exits zero with no changed protected inputs. Root/theark read
91G/5.2G afterward, before paired readback and cleanup. This proves the scoped
split-drive cook, not installed gameplay behavior.

The private pair's independent cooked readback now passes source geometry,
weights, UVs, colors, normals and 22/137 morph preservation. All valid packed
tangents and handedness are unchanged; the one repaired corner per mesh matches
the double-precision gradient within 0.005334023. Unit lengths pass. The 284
known collapsed-UV fallbacks remain, with zero unexpected nonorthogonal frames.

Decoded cloth/config fields match the installed pair except for the new GUID.
Mesh properties match except for disabling tangent recomputation. Materials,
bounds, bind transforms and empty Nanite data are preserved. Both meshes retain
all 1,224 cloth render triangles and have identical render/mapping digests.
Receipts: `alice-pack/readbacks-verified.json` and `bindings-verified.json`.
Permanent paths, combined packaging and live checks remain open.

After the [prelisted pair cleanup](../development/cleanup-2026-10-14-eve-alice-pair.md),
2,477,776,543 logical bytes of prefix/duplicate inputs/cooked outputs were
removed with retained hashes unchanged. Immediate root/theark free space changed
from 91G/4.9G to 92G/4.9G. The useful cache, editor assets, private packages and
readbacks remain; root scratch links still resolve. Receipts:
`cloth-transfer/alice-cleanup-{plan,completed}.json`.

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
