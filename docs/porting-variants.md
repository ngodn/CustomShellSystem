# Porting alternate outfit containers

This workflow converts cooked Mortal Shell II appearance mods into one CSS entry with selectable mesh variants. It uses Python 3.14, the existing .NET 10 MeshExport helper and the C++23 native runtime. It does not convert another game's skeleton or add a physics engine.

## Local ports, 2026-09-14

Artifacts are under `../CSS-eins0fx-collections/ports/` relative to this repository.

| Folder | Source author | Variants | Editable parts |
| --- | --- | --- | --- |
| CSS_BHProxima_Erase | Erase | Original, Bulge, Cape, Cape Bulge, Crown Cape, Crown Cape Bulge | Armor, clothing, metal, cape |
| CSS_CurvyAndCutePROXIMA_CalCalMon | CalCalMon | Heels or no heels, each with original and two lingerie alternatives | Armor, clothing, metal, skin |
| CSS_LongHairProxima_Ducttus | Ducttus | Long Hair | Armor, clothing, metal, skin, hair |
| CSS_MileenaOverTiel_GetJinxyed | GetJinxyed | Mileena MK9 | Clothing, metal, skin |

Each includes Original, Crimson, Midnight, Ash and Verdigris palettes. Palettes preserve skin/hair choices. Original restores the source materials. Portraits are head renders of the actual source models, not generated likenesses. Each `<folder>/<folder>_P/` contains exactly one matching `.pak/.utoc/.ucas` trio; `<folder>/<folder>_P.zip` contains that folder for installation into `MortalShell2/Content/Paks/~mods/`.

Sources: [BH Proxima](https://www.nexusmods.com/mortalshell2/mods/201), [Curvy and Cute](https://www.nexusmods.com/mortalshell2/mods/241), [Long Hair Proxima](https://www.nexusmods.com/mortalshell2/mods/223), [Mileena](https://www.nexusmods.com/mortalshell2/mods/179). Mileena attribution follows the supplied source folder; its fetched page did not expose useful metadata. These are local conversions, not permission to redistribute someone else's assets. Curvy and Cute's source page requires permission for modification/redistribution and prohibits other-site uploads. No third-party port was uploaded by this workflow.

## Wait for downloads to finish

```sh
python3 tools/css_sources.py ../CSS-eins0fx-collections/ports \
  --output ../CSS-eins0fx-collections/ports/source-snapshot.json
```

Only `*/original` is watched. Every file and directory must be at least 60 seconds old by both modification and change time, and unchanged across two observations. Partial downloads, symlinks and incomplete IoStore companions fail the gate. Preserve the source archives. Extract each alternate archive separately into `extracted/01`, `extracted/02`, and so on. Do not merge conflicting original paths.

Pass the snapshot to grouped conversion. It checks that every group belongs to a ready source and checks the original entries again before publication. A new file or replaced archive invalidates the conversion. Rescan for new arrivals after finishing a batch.

## Describe source groups

Each mod's `authoring/variants.json` contains a list such as:

```json
[
  {
    "id": "original",
    "name": "Original",
    "inputs": ["../extracted/01"],
    "materials": "01-materials.json",
    "colors": "colors-original/colors.json",
    "include": ["/Game/Path/To/BaseMaterialInstance"]
  },
  {
    "id": "cape",
    "name": "Cape",
    "inputs": ["../extracted/02"],
    "materials": "02-materials.json",
    "colors": "colors-cape/colors.json"
  }
]
```

Optional fields include `mesh`, `shell` and `import_repairs`. Paths resolve relative to the recipe. Material slot numbers refer to the live character component's overrides, which can differ from the source mesh's defaults. Color recipes refer to reviewed UV regions in the source atlases. `tools/build_port_colors.py` contains the source-specific recipes for this batch; it is not an automatic skin detector.

```sh
python3 tools/css_convert.py \
  --variant-sources MOD/authoring/variants.json \
  --source-snapshot ../CSS-eins0fx-collections/ports/source-snapshot.json \
  --name 'Outfit Name' --author Author --id author.outfit \
  --thumbnail MOD/authoring/thumbnail.png \
  --thumbnail-source rendered-from-source-mesh \
  --source-url https://www.nexusmods.com/mortalshell2/mods/ID \
  --retoc build/retoc-css-target/release/retoc \
  --work MOD/work/new-conversion --output MOD/work/verified
```

Use a fresh work directory. The converter refuses to overwrite published packages. Default naming is `CSS_${NAME}_${AUTHORorMODDER}_P`; `--name-format` changes it while preserving the `_P` suffix. The empty user-created `_P` folder is the final destination, not an input source.

## Import and relocation findings

- Curvy and Long Hair use author-specific project mounts. Accept the project's `Content` root, while preserving package identity as `/Game/`. During inspection, select the actual source virtual path; `/Game/` can resolve the base game's texture instead.
- Unreal [FName comparison is case-insensitive](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/UObject/FName?application_version=5.5). retoc's case-sensitive imported-package assertion lost valid `Knightlady`/`KnightLady` dependencies. `patches/retoc-css-import-case.patch` fixes it and keeps extraction warnings visible when output is redirected. Build the patched retoc separately; the system tool is unchanged.
- Numbered package names such as `_1001` can be encoded as a base FName plus a numeric suffix. Relocation preserves the suffix and rewrites the shared base. Inverse verification restores the exact matched bytes, including original case.
- Curvy has two genuinely absent imports for a hair-slot material. Reviewed repairs use its existing hand-spear material, matching the component slot arrangement. Slots 6 and 7 use the source `strap` and `curtain` custom materials. Their cooked source exposes no editable parameters and no accompanying custom shader library. Those two materials are retained unchanged and require live visual validation.
- Mileena has 21 tiny dummy cubes inside the abdomen, plus the visible body at slot 21. Its 21 absent dummy materials are explicitly redirected to its included body material. They are not 21 missing visible body sections. Colors target the visible body slot only.
- `import_repairs` is bound to the exact source header SHA-256 and explicit import indices, with a reason. A changed source fails instead of applying a guessed repair. Opaque mesh, physics and animation payloads remain intact.
- retoc can emit unreferenced `/Engine/UnknownPackage` package-only holes after collapsing duplicate imports. Those null holes are accepted. Unknown exports and unknown packages used as import outers are rejected.

## Sharing textures without merging outfits

Only single-export root Texture2D packages with identical hashes for every original source file are candidates. The converter aliases equal-length relocated package references to one canonical texture, checks the inverse edits byte for byte, and removes only generated duplicate texture payloads. Different meshes, atlas bytes, physics and animation assets remain distinct.

The verified BH container decreased from 180.87 to 74.19 MiB. Curvy decreased from 1,282.99 to 386.90 MiB. These are container file sizes, not measured frame-rate improvements. Masks also share files only when their checksums match.

## Validation and runtime requirement

Every child and combined bundle passes retoc verification, export identity checks, opaque payload hashes after extraction, manifest/resource checksums and exact metadata unpacking. CUE4Parse resolves all fourteen packed variants' human skeleton, physics asset, postprocess animation and explicit material overrides. Native package tests cover fresh caches, unchanged cache reuse, damaged thumbnail/mask repair and corrupt pak rejection. Python tests cover source stability, numbered/case relocation, grouped recipes and texture sharing.

Per-variant color recipes require the updated native core from this work. Published CSS v0.1.1 predates the resource-union support. The local core was built in `work/ports-native` from c290af5 plus these color changes, excluding the unrelated wardrobe lighting experiment. The installed core pointer and full hash are recorded in the local runtime and installation backups. The existing loader has the same ABI; its only source differences are version/author labels. Existing CSS state was preserved.

Static validation and the automated in-game pass are complete. All fourteen variants passed four palettes, custom RGB changes, exact Original material restoration, preview animation/cloth, unchanged gameplay shell/animation and close cleanup. Evidence is in `work/ports-live.json`. The user also reported that the new outfits, including the highlighted lingerie and hair checks, seem correct after being asked to inspect movement and attacks. Long combat sessions and other gameplay shells still need broader testing. The retained source physics assets and postprocess blueprints do not prove universal physics compatibility. The previously reported wardrobe lighting difference remains a separate open issue.


## Larger wardrobe lists

The old five-item pages are replaced by one clipped ScrollBox. The panel/footer stay within the viewport, the scrollbar is visible for longer collections, and all filtered outfits participate in D-pad navigation. Selecting a row uses Unreal's [ScrollWidgetIntoView](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Components/UScrollBox/ScrollWidgetIntoView?application_version=5.5). Mouse wheel scrolling is native Slate input. Variant, favorite and color refreshes retain the scroll offset; changing category resets the list. Right-click drag remains reserved for camera orbit.

The user confirmed mouse/scrollbar access to the later outfits and D-pad navigation beyond HIT2. Runtime diagnostics report nine rows (Original plus eight outfits) and a nonzero scroll range at the tested resolution. The first live build caught a case-sensitive reflected `content` parameter lookup; it closed cleanly, and the corrected build passed.

During the first full appearance pass, Long Hair's initial request was cancelled by the stale-asset guard; retrying succeeded. Blocking imports may collect assets loaded earlier in the same operation. The runtime now temporarily roots the target mesh and override materials until the component owns their references, restoring only roots it added. Existing stale-player/skeleton checks remain in place. This is scoped loading protection, not a permanent asset cache.

Final tested core: `css_core-ddeb827a207f25a4-1789356524511079680.dll`. The complete second pass finished without a cancelled load. Each package's embedded conversion report still describes its original offline validation; the separate live report records the later game checks.

The user subsequently confirmed that all four new mods work correctly.
