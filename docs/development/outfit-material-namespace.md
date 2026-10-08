# Outfit material namespace migration

## Compiled animation blueprint preservation

`rewrite_material.rs` also supports explicit `animation_blueprints` requests.
This mode accepts one animation generated class, its default object, and
Function/ScriptStruct exports. It preserves name indices, object names and
every export payload byte. Unsupported classes, resource tables, soft-path
tables and inline renamed paths are rejected. Header serialization must roundtrip
unchanged before migration and return exactly to the original through the inverse
map. This is a bounded header rewrite, not a bytecode editor.

Eve's `ABP_Secondary` requires this route: editor recompilation changes graph
layout, generated property names and mutable data. `eve-secondary-namespace1`
matches all seven released compiled exports after path normalization and retains
identical bytecode. Its three Control Rig dependencies are supplied explicitly
for readback. `eve-rig-pack2` then verifies it in the complete outfit assembly.

`verify_outfit_rig_trial.py --preserved-rig` accepts a verified preservation
receipt only for selected animation blueprints, pins its evidence and payloads,
and still compares the final assembled packages against the released baseline.
The optional `--work-root` places bulky private trials on another disk; it does
not change installation paths. `header-writer-regression2` checks 68 instances,
three textures and rejection of a material in blueprint mode.

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

## Eve texture preservation

`eve-material-namespace1` preserves 68 cooked instances from the pinned Eve
container. Its remaining material inventory is one authored hair parent and
204 Texture2D packages. The new texture path avoids recooking those pixels.

`migrate_outfit_textures.py` uses the existing extraction and namespace map,
then independently reads the original and candidate IoStore containers with
`AssetReadback --texture-digests`. It compares all decoded texture settings and
hashes every mip, including streamed bulk payloads. Candidate imports resolve
with only the base game and texture candidate mounted.

The narrow Retoc header writer accepts the observed UE 5.6.1 Initial resource
table with ordinary inline (`0x48`) or separate bulk (`0x10501`) payloads. These
flags are defined in the pinned engine's `CoreUObject/Public/Serialization/BulkData.h`.
Retoc's legacy/Zen converters retain the offsets within the payload streams;
the inline reader consumes bytes at its saved stream position. Names may grow,
but FName indices and export/bulk bytes remain unchanged. Unchanged and inverse
header roundtrips must both match. Unknown resource versions, compressed or
optional/memory-mapped layouts, cooked indices and invalid extents are rejected.

`eve-texture-namespace2/verification.json` records **204 textures and 2,322 mip
payloads** with identical metadata and mip hashes after path normalization.
The first attempt rejected the resource tables without producing a candidate;
its layout inspection remains in `eve-texture-namespace1/resources.json`.
`eve-material-writer-regression1` also confirms the extended writer produces
the same 136 header/export files for all 68 material instances. A truncated
external mip payload is rejected before any output is written in
`eve-texture-negative-check1`.

`eve-hair-editor1` migrates the hair parent and its three texture dependencies,
then passes a fresh reload without legacy redirectors. All five staged source
inputs (including the native reference skeleton) remain unchanged.
`eve-hair-cook1` cooks that group with the original v1.4.0 hair renderer settings.

`verify_outfit_material_trial.py --textures` combines those cooked parent
materials with the preserved texture and instance payloads. A cook-emitted
texture dependency cannot overwrite its verified preserved provider.
`eve-material-pack1` passes all **273** normalized package comparisons,
including hair shader maps, all **405** texture export/bulk file comparisons
and **539** resolved imports. Candidate checks mount no original outfit pack.

This verifies cooked asset preservation, not rendered appearance or gameplay.
The full rig/mesh assembly remains. The completed cook's disposable Wine prefix
was removed (1,084,763,593 logical bytes, `eve-hair-cook1/prefix-cleanup.json`).
Byte-identical export/bulk readback copies were also removed after rechecking
their retained stage files; `cleanup-eve-material-readback1.json` records them.
Candidate containers, stage payloads, original inputs and reports remain.

After the complete Eve material assembly passed, its byte-identical texture
stage superseded `eve-texture-namespace2/stage`. That earlier texture-only stage
and container were removed, together with rejected Genessa cook1/pack1 payloads.
`cleanup-superseded-material-trials1.json` records each hash and the retained Eve
payload location, totaling 2,344,561,871 logical bytes. Original extracted data,
the full material assembly, texture mip comparisons and rejected shader
differences remain. Reproducing the earlier standalone texture container now
requires rerunning its recorded recipe.
