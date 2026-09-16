# CSS authored mesh interchange, schema 1

This is an offline authoring format for the UE 5.6.1 `CSSImportMesh` commandlet.
It is not the distributed CSS package format. The commandlet has compiled and
imported Seductress into the UE 5.6.1 installed editor, with saved-asset reload
verified. Authored Windows meshes and textures have also passed cooked readback;
game rendering and animation remain separate validation steps. Keep source files.

The importer creates a mesh and an authoring skeleton using caller-supplied
reference transforms. It does not change a mesh's rest proportions to match a
shared game skeleton. Material slots initially use placeholders. The skeleton
stub and placeholder materials must not enter a release package.

JSON fields:

| Field | Content |
| --- | --- |
| `schema` | Integer `1` |
| `mesh_package` | Unused package path below `/Game/CSSAuthoring/` |
| `skeleton_package` | Distinct unused package path below `/Game/` |
| `bones` | Array of `{name, parent, translation, rotation, scale}` |
| `materials` | Array of unique material slot names |
| `points` | Array of `[x,y,z]` in Unreal centimeters |
| `uv_channels` | Integer `1` (default) through `4` |
| `wedges` | Per-corner `[point_index,u0,v0]` extended with another `[u,v]` pair for each additional channel |
| `faces` | `[wedge0,wedge1,wedge2,material_index]`, clockwise front faces for Unreal |
| `influences` | `[point_index,bone_index,weight]`, at most eight per point |
| `normals` | Optional unit `[x,y,z]` per wedge in Unreal coordinates |
| `colors` | Optional `[r,g,b,a]` per wedge, integer bytes 0..255 |
| `morph_targets` | Optional array of `{name, deltas}`, at most 64 |

Bone translations use Unreal centimeters. Rotations are local Unreal
quaternions `[x,y,z,w]`; scales are local positive `[x,y,z]`. Bones occur in
parent-before-child order, with a single root whose parent is `-1`. Use the
mesh's exact reference skeleton, including extra deform bones. Vertex bone
indices refer to this array, regardless of Blender's bone traversal order.
All point weights must sum to one. Duplicate point/bone entries are rejected.

For the existing Genessa Blender scene, geometry converts as
`UE = (Blender.x*100, -Blender.y*100, Blender.z*100)`. Keep the triangle order
for that reflection: it already converts Blender's counter-clockwise front
faces to Unreal's clockwise convention. If the complete object-to-Unreal
transform has a positive determinant instead, reverse the triangle order.
Convert UVs using `v_UE = 1-v_Blender`. Normals use the
inverse-transpose transform and normalization. Do not apply this conversion a
second time to data already in Unreal coordinates. Imported FBX or GLB bone
axes must not replace the caller's original Unreal reference transforms.

The earlier schema text incorrectly said to reverse for the usual reflection.
That produced culled front faces and inverted two-sided shading. The correction
was verified by changing only triangle order in a controlled Unreal render;
the original audit remains private development evidence. Repeat the render and
cooked readback checks for a new mesh rather than relying on that result.

The importer accepts up to four UV channels (UE 5.6.1 `MAX_TEXCOORDS`), preserves supplied normals, rebuilds tangents with MikkTSpace,
and defaults missing colors to white. Missing normals are recomputed. LODs, cloth,
collision/physics assets, and material graphs are separate authoring steps. This mesh
schema does not imply that those features have been imported.

`morph_targets` carries Blender shape keys. Each entry is `{"name": "Hips", "deltas":
[[point_index, dx, dy, dz], ...]}`, where the deltas are in Unreal centimeters relative to
that point's position in `points`, and only the points that actually moved appear. Names
take letters, digits and underscores, at most 64 characters, and must be unique: the
importer refuses anything the engine would have had to rename, because the runtime
addresses a shape by that exact name. A delta beyond 100 cm is refused as a scene-scale
mistake. The importer checks every declared morph survived the build and fails if the
engine dropped one, so a silent drop cannot ship a mesh whose shape sliders do nothing.

Read a saved mesh back with the `CSSInspectMesh` commandlet, which reports bones, material
slots, LOD counts and every morph target with the number of vertices it moves, its largest
delta and a sample:

```sh
UnrealEditor-Cmd CSSAuthoring.uproject -run=CSSInspectMesh \
  -Mesh=/Game/CSSAuthoring/Example/SK_Example -Output=/absolute/path/readback.json \
  -unattended -nosplash -NullRHI
```

Invocation after building the editor module (see the
[advanced tools guide](../../../docs/modding/advanced-tools.md)):

```sh
UnrealEditor-Cmd CSSAuthoring.uproject -run=CSSImportMesh \
  -Input=/absolute/path/mesh.json -unattended -nosplash -NullRHI
```

Before publishing: load the saved asset back, compare reference transforms and
material sections, pose it with game animations, perform a Windows cook, and
inspect the cooked asset with the same extractor used for the original mesh.
