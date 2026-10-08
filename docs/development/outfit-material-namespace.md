# Outfit material namespace migration

This is part of the [three-outfit goal](three-outfit-rig-physics-goal.md).
The author namespace must preserve the released material behavior, including
native game shader parents and every shader variant the outfit uses.

## Genessa source distinction

The editable Genessa skin and eye instances are binding proxies. The mod's
`tools/stage_skin_variants.py` and `tools/bind_native_head.py` explicitly replace
them with native cooked instances before packaging. Recooking those proxies
would lose the native skin or eye implementation.

`tools/asset-paths/migrate_outfit_instances.py` extracts the hash-pinned outfit
and identifies instances by export class, including `M_Genessa_Eyes_Default`,
whose prefix alone would be misleading. The existing Retoc material-header
writer changes package names and references while preserving the export bytes.
It requires unchanged and inverse header roundtrips and rejects inline package
paths or unsupported layouts. This is not a general binary string replacement.

The Genessa batch has 33 instances: seventeen use the native Uber character
parent, two use native eye parents, and fourteen use authored garment parents
or instances. The remaining material dependencies are seven authored parent
materials and 48 textures. Their editable migration uses the existing property
and texture-source snapshots, followed by reload without the old redirectors.

## Cook settings are part of the source recipe

The first Genessa cook omitted its original RendererSettings. The independent
comparison passed 81 of 88 packages, but all seven parent materials lacked
the unlimited-influence skin and cloth vertex factories present in the release.
No non-shader material fields differed. All 96 texture export/bulk files matched
their released counterparts exactly. This candidate was rejected, not installed.

The original outfit's `tools/stage_shader_project.py` and
`work/ue/Config/DefaultEngine.ini` specify:

```ini
[/Script/Engine.RendererSettings]
r.GPUSkin.UnlimitedBoneInfluences=True
r.GPUSkin.UnlimitedBoneInfluencesThreshold=8
r.GPUSkin.Support16BitBoneIndex=True
r.SkinCache.CompileShaders=True
```

The local UE 5.6.1 `GPUSkinVertexFactory.cpp`,
`TGPUSkinVertexFactory::ShouldCompilePermutation`, explicitly gates the
unlimited-influence variant on that project setting. Epic's
[skeletal mesh rendering documentation](https://dev.epicgames.com/documentation/unreal-engine/skeletal-mesh-rendering-paths-in-unreal-engine?application_version=5.6)
explains the corresponding rendering paths. The pinned local source and released
shader readback establish the requirements for this specific cook.

`tools/shared-assets/cook-namespace-trial.py --renderer-config` copies the named
source's RendererSettings into the isolated cook project and records its hash.
This option leaves the existing Astral recipe unchanged when omitted. The
optional `--ddc-seed` copies a prior cache; it does not share writable cache files.

## Verification

`tools/asset-paths/verify_outfit_material_trial.py` combines the preserved
instances with the newly cooked parents and textures. It refuses overlapping
providers, verifies the requested package inventory, roundtrips IoStore payloads,
and independently decodes all packages including shader maps. Every decoded
field must match after the explicit namespace map is reversed. Texture export
and bulk payloads must also match the originals byte-for-byte.

The import audit mounts only the base game and candidate. An old outfit pack
cannot conceal missing imports. This material subset is not a complete outfit
and must not be installed on its own.

Evidence is under `work/three-outfit-rig-physics/`:

- `genessa-material-namespace2`: 33 preserved instances and original readback.
- `genessa-material-editor1`: 55 editor migrations and successful fresh reload.
- `genessa-material-cook1` / `genessa-material-pack1`: rejected shader variants,
  complete field differences and passing texture payload comparisons.
- `genessa-material-cook2` / `genessa-material-pack2`: corrected renderer-settings
  cook passes all 88 normalized decoded comparisons, including shader maps,
  all 96 texture payload comparisons and the independent import audit. The
  temporary subset is not installed; rig and mesh migration remain.

The earlier `genessa-material-namespace1` attempt finished header relocation but
its wrapper used an invalid log filename. Its duplicate payloads were removed
only after matching the successful second attempt; its reports and cleanup
receipt remain.
