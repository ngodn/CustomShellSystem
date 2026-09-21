# Short material and texture paths

2026-09-21. This completes the cooked material/texture part of the path migration. Mesh, skeleton, physics, animation, rig and editor material bindings remain pending. Nothing was installed in the game.

## Result

The isolated candidate now has 30 cooked materials and 90 cooked textures under readable paths such as `/Game/CSS/SeduXtress/Face/MI_Genessa_body` and `/Game/CSS/SeduXtress/Shoes/T_ShellKeeper_Hair_01_BC`. The longest mapped package path is 58 characters. Existing object leaf names are preserved; the readable folder identifies the actual use, even where a leaf still names its original game template.

All 30 material headers pass a byte-identical unchanged read/write round trip using the pinned Retoc serializer. They also pass inverse relocation: restoring the old package names and header size reproduces every original header byte. Each material's `.uexp` payload is copied unchanged.

All 90 textures were duplicated in the editor with matching texture settings and saved to new paths. A separate editor process cooked them through the new namespace, dependency and Windows path guards. The cook exited 0 with exactly 90 packages. Every resulting `.uexp` and `.ubulk` matches the corresponding V44 texture bytes exactly; headers change with the package names. All original editor texture files retain their hashes.

The new material and texture containers pass Retoc verification. CUE4Parse independently decodes all 30 old and 30 new materials. Every decoded field matches after applying only the declared package-path substitutions. Original material file hashes remain unchanged.

## Why the first readback failed

Before the new textures were cooked and mounted, all 30 material comparisons failed: CUE4Parse returned null for the three unresolved relocated texture references. That is retained in `validation.json` and `assets.json`; it was not ignored or normalized away. After adding the actual new texture packages, a fresh decoder process resolves those references and `validation2.json` passes without exclusions.

An intentionally injected inline package string in a material payload is rejected before writing output. The relocation tool handles material header references only; it must not silently resize opaque export data or be used for arbitrary mesh, animation or texture payloads.

## Implementation and reproduction

`tools/asset-paths/rewrite_material.rs` uses the existing patched Retoc checkout at upstream commit `d7b635039c3db60942efabcd29d49679f42ab089`. It preserves name-table indexes, rebuilds variable-length strings and header tables, and accounts for export offsets relative to the separate `.uexp` stream. The writer expects relative export offsets, whereas deserialization returns offsets including the old header size. Subtract the old header size before calling the serializer.

The tool rejects unsupported classes, inline payload references, cell exports, bulk-resource tables, out-of-line soft paths, object-name changes, output collisions and failed round trips. It validates the whole batch before writing any material.

Build as the `rewrite_material` example in the pinned Retoc workspace, using Rust 2024 and the existing lockfile. The checked host compiler is Rust 1.95.0. Copy the tracked source to `build/retoc-css/retoc/examples/rewrite_material.rs`, then run:

```sh
CARGO_TARGET_DIR="$PWD/build/retoc-css-target" cargo build --offline --release --locked \
  --manifest-path build/retoc-css/Cargo.toml -p retoc --example rewrite_material
```

`prepare_materials.py` accepts an explicit material plan, a fresh short output directory under CSS work, and `--windows-root`. It validates the 30-slot order, original file hashes, unique destinations and path limits, then writes the map, texture map, protected hashes and request before invoking the material rewriter. Its second execution at `work/paths/mat2` reproduces all 30 outputs byte for byte.

`copy_textures.py` runs in UE 5.6.1 editor Python with `CSS_PATH_WORK` pointing to that prepared directory. It duplicates only the mapped textures, refuses existing target assets and checks source hashes. The cook uses the new mapped package list and a short output root.

`tools/AssetReadback` is a .NET 10 reader against the pinned CUE4Parse project. It emits one JSON document keyed by full package path, so identical material leaf names cannot overwrite one another. It does not export meshes, generate textures or modify packages. `validate_materials.py` compares its complete decoded material documents, allowing only the explicit relocation map.

## Evidence

- `work/paths/mat1/report.json`: 30 unchanged and inverse header round trips.
- `work/paths/mat1/map.json`: complete 120-package material/texture map.
- `work/paths/mat1/texture-copy.json`, `copy-exit.json`: saved editor duplicates and protected source hashes.
- `work/paths/mat1/cook-exit.json`, `cook-engine.log`: successful guarded 90-texture cook.
- `work/paths/mat1/texture-bytes.json`: all 90 export streams and all 90 bulk streams unchanged.
- `work/paths/mat1/pack.log`, `verify.log`, `texture-pack.log`, `texture-verify.log`: diagnostic container construction and verification.
- `work/paths/mat1/decode2-exit.json`, `assets2.json`, `validation2.json`: fresh independent material comparison with actual texture dependencies.
- `work/paths/mat1/negative/result.json`: unsupported inline payload reference rejected with no output.
- `work/paths/mat2/reproduced.json`: repeatable material output from the tracked preparation CLI.

## Next integration step

Create the isolated editor material bindings at these short paths and migrate the heel mesh's remaining rig/physics/animation dependency closure. Update catalog, recipes, cook lists and the exact audited native skeleton compatibility paths together. Keep original V44 assets and accepted gameplay behavior as the baseline. Run full cooked closure checks before deployment, then resume footwear visibility, motion and grounding checks. This milestone does not establish new in-game appearance or finish the overall Next-Gen release.
