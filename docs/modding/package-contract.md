# CSS.Package v1 contract

This is the installed outfit format used by the current tools and runtime. `CSS.Project` is a separate author recipe. The CSS runtime DLL version, outfit version, project recipe version and package format version are independent.

## Required output

One stable outfit ID owns one self-contained trio and one wardrobe entry. A release ZIP contains a folder named after the trio's stem:

```text
CSS_My_Outfit_YourName_P/
  CSS_My_Outfit_YourName_P.pak
  CSS_My_Outfit_YourName_P.utoc
  CSS_My_Outfit_YourName_P.ucas
```

The stem is portable ASCII and ends in `_P`. The three names must match the manifest's companion names. Do not distribute multiple alternatives under the same stable outfit ID; put those alternatives in its variants array. Different outfits need different IDs.

IoStore assets use isolated `/Game/CSS/<id-derived-hash>/...` paths. The converter rewrites references with matching encoded lengths and verifies the result. It preserves external game dependencies. Isolation lets several CSS outfits coexist without all replacing one original mesh path.

The uncompressed V8B pak is mounted at `../../../` and contains:

```text
MortalShell2/Content/CSS/Packages/<outfit-id>/
  manifest.json
  thumbnail.png
  conversion.json
  dye-*.png         (only when referenced by a color recipe)
```

Generate these through the tools. Editing the final manifest or renaming containers after verification invalidates the release checks.

## Manifest fields

| Field | Contract |
| --- | --- |
| `format`, `format_version` | `CSS.Package`, `1` |
| `game`, `engine` | `MortalShell2`, `5.6`; this is not proof of compatibility with every 5.6 game build |
| `id`, `name`, `author`, `version` | Stable identity, display name, credit and outfit version |
| `source_url`, `thumbnail_source` | Source/artwork provenance |
| `thumbnail` | `thumbnail.png`, dimensions and SHA-256 |
| `containers` | `.utoc` and `.ucas` filenames, byte counts and SHA-256 |
| `catalog` | Schema 1, exactly one outfit with matching identity and thumbnail |
| `resources` | Exact union of referenced dye images, each with dimensions, bytes and SHA-256 |

The catalog outfit declares `shells`, `compatibility`, `variants`, optional shared `colors` and display metadata. The converter emits `compatibility: "same_skeleton"`; the runtime also verifies Skeleton object identity before applying a mesh. A source shell tag is neither an unlock nor a gameplay-shell-switch instruction.

Each variant declares a stable `id`, `name`, relocated mesh object path, optional material-slot overrides and optional colors. At least one variant is required. A variant color recipe replaces the shared one. Resource filenames reused by multiple variants must refer to identical bytes.

## Validation and generated data

The converter records original/tool hashes, relocated paths, export identities and round-trip checks in `conversion.json`. `runtime_tested: false` means exactly that. In-game testing is recorded separately; an offline build does not mark itself gameplay-tested.

The offline verifier hashes the full `.ucas`. Runtime discovery checks the smaller metadata, thumbnail, resources and `.utoc`, and checks the bulk companion's name and size. It deliberately avoids hashing large bulk files on the game thread.

CSS extracts thumbnails and dye resources into a rebuildable package cache. User selections, favorite IDs and templates belong to CSS's own state. Cache files, loose developer catalogs, personal state and DLLs never belong in an outfit trio or outfit ZIP.

A new asset container requires restarting the game. Native core live reload does not hot-replace a mounted pak/IoStore set. See [verification and install details](../css-packages.md).

## Discovery failures

CSS 0.2.1 searches `Content/Paks` recursively, including `~mods`. Keep the recommended one-folder layout above. Invalid packages are rejected individually, with filenames and reasons in `CSS.log` and `runtime/status.json`; a bad neighbor no longer aborts the entire catalog. See [discovery and diagnostics](../package-discovery.md) for validation behavior and regression commands.
