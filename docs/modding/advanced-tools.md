# Experimental Unreal authoring source

The reusable source from the Seductress authoring work is now in this repository:

| Source | Purpose and limit |
| --- | --- |
| [export_css_mesh.py](../../tools/authoring/export_css_mesh.py) | Export a fitted Blender mesh using supplied Unreal reference transforms; does not fit foreign geometry |
| [CSSAuthoring project](../../tools/authoring/CSSAuthoring/CSSAuthoring.uproject) | Offline UE 5.6.1 editor module; source/configuration only |
| [Mesh schema](../../tools/authoring/CSSAuthoring/SCHEMA.md) | Interchange format for the importer, separate from CSS.Package |
| [CSSImportMesh](../../tools/authoring/CSSAuthoring/Source/CSSAuthoring/CSSImportMeshCommandlet.cpp) | Imports explicit geometry/rig data, creates authoring stubs; materials and game references still need staging |
| [CSSCookAssets](../../tools/authoring/CSSAuthoring/Source/CSSAuthoring/CSSCookAssetsCommandlet.cpp) | Limited Windows mesh/texture cook, explicitly without shader compilation |
| [CSSBindCloth](../../tools/authoring/CSSAuthoring/Source/CSSAuthoring/CSSBindClothCommandlet.cpp) | Seductress-specific cloth/secondary setup; section indices and tuning are not general defaults |

The original collection copies are preserved so ongoing asset work is not broken. This repository contains the publication copies. No engine binaries, extracted skeletons, reference meshes, source mod assets, generated content or local editor credentials were copied. Those files are not supplied by the project.

The code compiled and ran in the original UE 5.6.1 installed-editor setup. The source in this repository was also rebuilt successfully as `CSSAuthoringEditor` on Linux, with all eight build actions passing. This is source tooling, not a prebuilt SDK. Use the same engine revision and build its `CSSAuthoringEditor` target before invoking the commandlets. The commands below require a fitted mesh, supplied reference skeleton JSON, working engine installation and appropriate material/dependency staging. They are templates, not a complete Seductress rebuild:

```sh
'/path/to/UE5.6.1/Engine/Build/BatchFiles/Linux/Build.sh' CSSAuthoringEditor Linux Development "$PWD/tools/authoring/CSSAuthoring/CSSAuthoring.uproject"

'/path/to/blender-python' tools/authoring/export_css_mesh.py '/path/to/fitted.blend' --refskel '/path/to/reference.refskel.json' --output "$PWD/work/authored.mesh.json" --mesh-package /Game/CSSAuthoring/YourOutfit/SK_YourOutfit --skeleton-package /Game/CSSAuthoring/YourOutfit/SKEL_AuthoringStub

'/path/to/UE5.6.1/Engine/Binaries/Linux/UnrealEditor-Cmd' "$PWD/tools/authoring/CSSAuthoring/CSSAuthoring.uproject" -run=CSSImportMesh -Input="$PWD/work/authored.mesh.json" -unattended -nosplash -NullRHI
```

The export was tested with `bpy` 4.5.13 in Python 3.11. Importing the mesh does not cook it, connect the game's actual Skeleton asset, assign final material instances or add arbitrary physics. Stub assets and placeholders must not ship. `CSSCookAssets` requires an explicit package list and unused output directory; `-NoShaderCompile` is mandatory. This is not a compiler for new Windows master-material shaders.

For animation adapters that retain an older released outfit's skeleton,
`CSSCookAssets` accepts `-ExternalRigAssets=/path/to/external-rig-assets.txt`.
Each non-comment line contains the source `.uasset` SHA-1 followed by its
exact `/Game/CSSAuthoring/...` package name. Only loaded `USkeleton` and
`USkeletalMesh` assets with matching file hashes are accepted. This preserves
the animation's retarget source mesh as well as its skeleton. Hashing uses a
64 KiB buffer rather than allocating another copy of a large mesh. Entries
must be unique and actually
referenced by the requested cook. The cooker checks their hashes again before
returning success and does not emit them or traverse their dependencies.
New output packages still require short `/Game/CSS/...` paths; this option
does not permit recooking legacy packages or bypass other dependency checks.

The manifest records a dependency boundary, not proof that the dependency is
installed. Independently inspect the outfit's released container and verify
the new animation against that cooked skeleton before packaging or installation.
Keep that outfit container unchanged and record its hash in the package receipt.

The project ignores `Content`, `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache` and `Build`. Keep engine installations under ignored `reference/` or outside the repository. Make source changes in the tracked commandlets, not in generated build output. Consult [asset authoring](asset-authoring.md) and perform the [release checks](release-checks.md) before turning any output into a distributable outfit.
