# CSS project recipe

`css-project.json` is an authoring file for `tools/css_project.py`. It is not the installed package manifest. Keep it with your source project. All file paths inside it resolve relative to the JSON file, independent of the terminal's current directory. Machine paths for the game, retoc and repak are passed to `build` explicitly.

## One source set

```json
{
  "format": "CSS.Project",
  "format_version": 1,
  "id": "yourname.myoutfit",
  "name": "My Outfit",
  "author": "YourName",
  "version": "1.0.0",
  "description": "A fitted outfit with optional cloak.",
  "thumbnail": "thumbnail.png",
  "thumbnail_source": "author-provided",
  "inputs": ["source"],
  "mesh": "/Game/YourAuthoringProject/Characters/MyOutfit/Meshes/SK_MyOutfit.SK_MyOutfit",
  "shells": ["CharacterId.Player.Shell.Genessa"],
  "colors": "colors.json"
}
```

The mesh and shell above are examples, not assets supplied by this repository. Replace them with inspected source paths. Remove `colors` until you have a recipe; it is never inferred automatically. For a simple replacement, `mesh` and `shells` can be omitted when the converter can infer them. Custom authoring paths normally require both.

| Field | Meaning |
| --- | --- |
| `format`, `format_version` | Exactly `CSS.Project`, `1` |
| `id` | Required stable lowercase ID, up to 96 characters, using letters, digits, `.`, `_`, `-`; no `..` |
| `name`, `author`, `version` | Required display name, credit and outfit version; independent of the CSS runtime version |
| `thumbnail` | Required author PNG path |
| `description`, `source_url`, `thumbnail_source` | Optional description, source attribution and artwork provenance |
| `name_format` | Optional filename template; default `CSS_${NAME}_${AUTHORorMODDER}_P` |
| `inputs` | Source directories or any members of cooked container sets; companions are discovered |
| `mesh` | One original mesh package or full `Package.Object` path |
| `variants` | Array of `id=/Game/Path/Mesh.Object` strings instead of `mesh`, for multiple meshes in the same source set |
| `shells` | Source character tags; they do not bypass the runtime skeleton check |
| `materials` | JSON mapping component slot numbers to included original material object paths |
| `colors` | Color recipe shared by this outfit's variants |
| `include` | Explicit `/Game/Package/Path` dependencies to copy from the base game and relocate |
| `import_repairs` | Advanced, source-hash-bound repair recipe; never use guessed replacements |
| `variant_sources` | Separate group recipe for alternate containers, described below |

Unknown project fields are refused to catch misspellings. Mesh export selection, material existence, unresolved imports and relocation are checked during conversion. A successful project `check` is not a cooked-asset check.

Keep outfit IDs, variant IDs and color-part IDs stable. Renaming display text is different from changing identity. Every update needs a fresh output directory; change `version` intentionally. The version does not get appended to the container name by default.

## Alternate source containers

Use this when two alternatives overwrite the same original asset path. Do not combine their files in one directory.

Replace `inputs`, `mesh`, `variants`, `materials`, `colors`, `include` and `import_repairs` in the main project with:

```json
"variant_sources": "variants.json"
```

Then create `variants.json`:

```json
[
  {
    "id": "regular",
    "name": "Regular",
    "inputs": ["source/regular"],
    "mesh": "/Game/YourAuthoringProject/Characters/MyOutfit/Meshes/SK_MyOutfit.SK_MyOutfit",
    "colors": "colors-regular.json"
  },
  {
    "id": "cloak",
    "name": "With cloak",
    "inputs": ["source/cloak"],
    "mesh": "/Game/YourAuthoringProject/Characters/MyOutfit/Meshes/SK_MyOutfit.SK_MyOutfit",
    "colors": "colors-cloak.json"
  }
]
```

Group file paths resolve relative to `variants.json`. Each group selects one mesh and can provide its own `materials`, `colors`, `include`, `import_repairs` and `shell` array. Otherwise it inherits the main project's `shells`. Group IDs are lowercase and accept letters, digits, `_` and `-`, up to 64 characters.

Each color recipe must use the public outfit ID, such as `yourname.myoutfit`, not the group ID. Give distinct dye filenames to different mask bytes. Identical filenames may be shared only when their content is identical. A variant recipe replaces the outfit recipe for that variant; it does not merge with it.

The converter isolates each source group before combining them into one wardrobe entry and one trio. It can share byte-identical textures, but does not merge different mesh or physics payloads. See [the porting findings](../porting-variants.md) for real examples and repair limitations.

For folders still receiving downloads, use [css_sources.py](../../tools/css_sources.py) and pass its snapshot with `build --source-snapshot snapshot.json`. This gate currently supports grouped sources under the documented `*/original` layout. Ordinary project builds assume the author has finished writing the source files.
