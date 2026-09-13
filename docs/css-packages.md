# CSS outfit packages

`tools/css_convert.py` accepts a directory, a `.pak`, a `.utoc`, a `.ucas`, or multiple members of the same trio. It discovers companions and preserves the inputs. Python 3.14.7 is the tested converter runtime.

The default filename is `CSS_${NAME}_${AUTHORorMODDER}_P`. An outfit contains exactly three files:

```text
CSS_HIT2_DE_Scyther_XTGMods_P/
  CSS_HIT2_DE_Scyther_XTGMods_P.pak
  CSS_HIT2_DE_Scyther_XTGMods_P.utoc
  CSS_HIT2_DE_Scyther_XTGMods_P.ucas
```

The metadata and thumbnail are inside the package. The native CSS runtime is a separate installation.

## Convert

```sh
python3 tools/css_convert.py '/path/to/original/mod' \
  --name 'My Appearance' --author 'Modder' \
  --thumbnail '/path/to/author-thumbnail.png' \
  --game '/path/to/MortalShell2' \
  --retoc '/path/to/retoc' --repak '/path/to/repak'
```

Supply an author-created square PNG, 128 to 1024 pixels per side, at most 4 MiB. 512 pixels is recommended. Keep the face readable at small sizes; CSS supplies the surrounding title, border and favorite indicator. It does not substitute a generated character seal for a package thumbnail.

`--name-format '${AUTHOR}_${NAME}_P'` changes filenames. Quote dollar tokens to prevent shell expansion. `_P` is appended if the template omits it, so it always appears before all three extensions. `$AUTHOR`, `$MODDER` and `$AUTHORorMODDER` are aliases. Keep the stable `--id` across updates so favorites and selections survive renaming. Use `--output DIRECTORY` for the output root and `--work FRESH_DIRECTORY` for audit logs. Existing output/work directories are refused. `--description`, `--source-url`, `--package-version` and `--thumbnail-source` record metadata.

Automatic selection requires exactly one character skeletal mesh. Choose one with `--mesh ORIGINAL_OBJECT_PATH`, or group variants:

```sh
python3 tools/css_convert.py '/path/to/BeauteGenessa_P.utoc' \
  --name BeauteGenessa --author dantemk2 --id beaute.genessa \
  --thumbnail packages/thumbnails/beaute-genessa.png \
  --thumbnail-source rendered-from-source-mesh \
  --variant regular=/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/SK_Sester_Genessa_V6 \
  --variant corrupted=/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/Sk_Sester_Genessa_V4_Corrupted \
  --shell CharacterId.Player.Shell.Genessa \
  --shell CharacterId.Player.Darkform.CorruptedGenessa
```

Shell tags are inferred from `Characters/Shells/NAME` paths. NPC-based appearances require `--shell`. The native runtime checks the actual skeleton before replacement. No weapon, seal or shell unlock is required for these cosmetic selections.

Some replacements rely on material overrides on the original character. HIT2's default mesh materials resolve to `WorldGridMaterial`. Its [material recipe](../packages/recipes/hit2-materials.json) maps slots 0, 1 and 2 to body, face and hair material instances with the mod's corresponding textures. Supply `--materials RECIPE.json` to apply such overrides. The JSON maps slot numbers to original material object paths included in the input. The recipe applies to all variants; different per-variant recipes are not yet supported. CSS restores the original material overrides when removing an appearance.

## Embedded format v1

The IoStore files hold cooked assets under `/Game/CSS/<package-id-hash>/`. The uncompressed standard V8B `.pak` uses mount point `../../../` and contains:

```text
MortalShell2/Content/CSS/Packages/<id>/manifest.json
MortalShell2/Content/CSS/Packages/<id>/thumbnail.png
MortalShell2/Content/CSS/Packages/<id>/conversion.json
```

The manifest declares `format: CSS.Package`, `format_version: 1`, `game: MortalShell2`, `engine: 5.6`, stable `id`, `name`, `author`, `version`, `source_url` and `thumbnail_source`. It includes thumbnail dimensions/SHA-256, container filenames/byte sizes/SHA-256, and a schema-1 catalog with one outfit. Each variant declares `id`, `name`, `mesh` and optional `materials`. The outfit contains its description, compatibility tags and `thumbnail: thumbnail.png`. The conversion report records input hashes, relocations, export identities and checks. Structural conversion records `runtime_tested: false`.

The optional [color recipe](colors.md) and its checksummed `dye-*.png` resources are also embedded in this directory.

CSS scans packages on catalog load/rescan. It validates bounded pak index and entry hashes, manifest identity, the image hash, companion sizes and the small `.utoc` hash. The thumbnail is extracted to a rebuildable cache at `Mods/CustomShellSystem/cache/packages/<id>/<manifest-hash>/thumbnail.png`. The package remains self-contained if that cache is deleted. Large `.ucas` hashing stays outside the game thread:

```sh
python3 tools/css_package.py verify dist/CSS_HIT2_DE_Scyther_XTGMods_P
```

## Install and migrate

With Mortal Shell II closed:

```sh
python3 tools/css_package.py install \
  dist/CSS_HIT2_DE_Scyther_XTGMods_P \
  dist/CSS_BeauteGenessa_dantemk2_P \
  dist/CSS_BeauteKnightLady_dantemk2_P \
  --migrate-beaute
```

`--migrate-beaute` requires both Beaute replacements. It backs up and retires the old merged `CSS_Beaute_P` containers and prototype catalog to prevent duplicate IDs. Omit this flag for ordinary package installation. Running-game installs are refused. Use `--replace` to back up and replace packages with matching stable IDs, including renamed packages. Native code updates continue to use `python3 tools/css.py reload`; mounting new containers requires a launch.

## Actual model portraits

The three POC portraits in `packages/thumbnails/` are rendered from the original meshes and textures. HIT2 is credited to [XTGMods](https://www.patreon.com/cw/XTGMods), whose page links [Nexus mod 217](https://www.nexusmods.com/mortalshell2/mods/217). BeauteGenessa uses [mod 161](https://www.nexusmods.com/mortalshell2/mods/161). The user confirmed dantemk2 also created BeauteKnightLady.

The optional offline pipeline uses [CUE4Parse](https://github.com/FabianFG/CUE4Parse) via `tools/MeshExport` (.NET 10) and `tools/render_portrait.py` with `bpy==4.5.13` on Python 3.11. It exports glTF and texture PNGs, then frames the actual head bone. Materials approximate Unreal's shaders; procedural smoke cards without baked masks are transparent in the portrait. Authors may provide an in-game portrait instead.

```sh
mise exec uv@0.12.5 -- uv venv --python 3.11 build/render-env
mise exec uv@0.12.5 -- uv pip install --python build/render-env/bin/python bpy==4.5.13
build/render-env/bin/python tools/render_portrait.py work/exported-model author-thumbnail.png
```

MeshExport arguments are `CONTAINERS MAPPINGS OBJECT_PATH OUTPUT [EXTRA_PACKAGE...]`. Optional environment variable `CSS_MATERIALS` points to the same material recipe JSON. The developer request `export_mappings` writes a `.usmap` through the pinned UE4SS mapping generator. The CUE4Parse checkout fixes `ExportSession.ResolveOutputPath` to replace slashes only on Windows. Microsoft.Bcl.Memory is pinned to patched version 9.0.14 through central transitive pinning.

## Scope and verification

This tool converts cooked UE5.6 appearance assets. Arbitrary gameplay mods, encrypted packs, another game's skeletons and conflicting alternate input packs are outside its scope. Short paths that cannot fit an isolated namespace, unsupported loose files, ambiguous body selection, overlapping IoStore assets and unresolved imports are refused.

Relocation preserves encoded byte lengths and original export names. External references keep their base-game identity. Verification covers inverse relocation, export identities, retoc integrity, every cooked export/bulk payload after repacking, and all embedded metadata/image bytes. Native integration tests exercised all three output packages, cache reuse/repair and corrupt pak rejection. All three final `_P` packages loaded in-game. Their portraits appeared, all four appearances passed animation/cloth checks, and the user confirmed HIT2 movement and attacks. Broader testing remains listed in STATUS.md.

Packaging tools: [retoc](https://github.com/trumank/retoc) and [repak](https://github.com/trumank/repak). Local defaults use the existing retoc build and `build/repak/release/repak` v0.2.3. Both paths are overridable.
