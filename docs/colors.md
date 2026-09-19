# Color customization

CSS keeps the author's Original appearance and adds whatever palettes the package
ships. Select an outfit, open COLOR, choose a palette, then adjust individual parts.

The tab is a palette band, then one section per group. Parts that belong to the dress
sit under OUTFIT and parts that belong to the body under BODY, each opening with a Tint
row that shifts everything beneath it at once. Metal, gems, skin and the intimate
pigments take the tint's brightness and saturation but keep their own hue, so dragging
the Outfit hue from crimson to teal recolors the fabric and leaves gold as gold.
Tinting needs a palette: Original applies no dye at all, so there is nothing to move,
and the Tint rows are hidden there.

Picking a part opens a strip of swatches: the author's own colour first, then that part
in each palette, then hues and shades of it. Left and Right walk the strip from the list
without opening anything. **Exact color** switches to Red, Green and Blue sliders for
when you want a specific value, and **Back to swatches** returns.

Choosing a palette takes over the parts that palette sets and the tint of their groups,
and leaves the rest alone, so a custom skin survives changing the dress. Original is the
exception: it means no dye at all, and clears everything. *Reset part* returns one part
to the palette; *Reset all colors* returns to the bare palette.

What a package should declare, and the shared vocabulary of roles that drives all of the
above, is in [control-convention.md](control-convention.md). Packages published before that
convention keep working: CSS infers each control's group and role from its id.

| Outfit | Independent controls |
| --- | --- |
| Seductress V2, dressed | Garment, ornaments, skin, mask, eyes, eye glow |
| Seductress V2, topless variants | the above plus nipples and areolae |
| Seductress V2, nude variants | the above plus labia and inner |
| Seductress V1 | Garment, ornaments, skin, mask, eyes, eye glow |
| BeauteGenessa, both variants | Clothing, metal, skin, face, eyes, eye glow strength |
| BeauteKnightLady | Armor, metal highlights, waist cloth |
| HIT2 DE Scyther | Clothing, metal, ribbons, gems, skin, face, hair |

RGB sliders use the familiar 0 to 100 display. Glow strength uses its actual scalar
value. D-pad up/down chooses a row, left/right changes its value, and A on a color
channel resets that part. Mouse dragging updates the selected part, its swatch and its
value without rebuilding the panel. Analog camera controls remain available. Presets do
not recolor skin, face, hair or eyes unless you explicitly change those controls.

Colors belong to CSS's own save. Each outfit remembers its colors across variant
changes, and saved looks include the palette, custom values and group tints. Writes are
coalesced during dragging and flushed when unloading the core. The game save, shell
abilities and unlocks are untouched.

## Authoring a package

Use `--colors /path/to/outfit.colors.json` with `tools/css_convert.py`. A recipe identifies the same stable outfit ID as the package, and puts its masks beside the JSON. To add colors to an existing verified CSS trio without rewriting its cooked assets:

```bash
python3 tools/css_color_package.py dist/CSS_Example_Author_P \
  --recipe authoring/example.colors.json \
  --package-version 1.1.0 \
  --output dist/colors
python3 tools/css_package.py verify dist/colors/CSS_Example_Author_P
```

The tool creates a package-named directory under the output root and refuses to overwrite it. Metadata, thumbnail, conversion audit and dye masks all live inside the `.pak`. The `.utoc` and `.ucas` remain byte-identical during this update. The package still consists of three matching `_P` files. Installation with `--replace` backs up the old trio and retires matching local development recipes and masks.

`--package-version` is author-controlled; omitting it preserves the existing version. Packages with variant-specific colors are refused by default. Rebuild those through their project to preserve separate recipes. Use `--replace-variant-colors` only when intentionally replacing them all with one shared recipe.

A minimal masked color recipe:

```json
{
  "id": "author.example",
  "colors": {
    "schema": 1,
    "controls": [
      {"id": "clothing", "name": "Clothing", "default": [1, 1, 1, 1]}
    ],
    "surfaces": [
      {
        "id": "body",
        "parameter": "BaseColorMap  non VT",
        "slots": [0],
        "resolution": 2048,
        "layers": {"clothing": "dye-clothing.png"}
      }
    ],
    "palettes": [
      {"id": "crimson", "name": "Crimson", "values": {"clothing": [0.56, 0.14, 0.24, 1]}}
    ]
  }
}
```

The example parameter name contains two spaces before `non VT`. Use the exact name and material slots verified for your asset. Masks must be square 8-bit RGBA or grayscale-alpha PNGs at 1024, 2048 or 4096 pixels. RGB holds grayscale surface detail, alpha selects the recolorable region. Overlapping regions blend in control-ID order, so author disjoint masks where parts must remain independent. Preserve eyelids, lips, tattoo details and opacity boundaries explicitly. The converter does not infer anatomy for arbitrary input mods.

A control can instead bind directly to actual shader parameters, or link several of them:

```json
{
  "id": "eyes",
  "name": "Eyes",
  "default": [0.8, 0.9, 1, 1],
  "bindings": [
    {"slot": 5, "parameter": "Color"},
    {"slot": 10, "parameter": "Color"}
  ]
}
```

Bindings default to global parameters. `association: "layer"` or `"blend"` requires a `layer` index from 0 to 63. Scalar controls use `type: "scalar"`, the first default component, and optional `min`, `max`, `step`. RGB values are sRGB, converted to linear for Unreal. The fourth component is author-controlled and cannot be changed by presets or the user. A declared parameter must actually affect the shipped shader. Successful parameter read-back alone does not prove a visible effect.

The three POC recipes are generated by [build_color_recipes.py](../tools/build_color_recipes.py) from locally extracted original textures. Their UV selections are specific to those assets. Generated textures remain outside git with the other derived game assets. Current POC dye surfaces use 2048 pixels; authors can choose 4096 for more base-color detail at higher memory cost. Original uses the unmodified original texture resolution.

## Runtime implementation and limits

CSS creates a material instance on the character, keeping the authored material as its parent. Direct eye/glow controls override individual parameters. For the three outfits' color maps, it composites the original texture and selected grayscale layers onto reusable GPU render targets. The original normal maps, roughness/metallic maps, shader, skeleton, cloth and animation remain in use. The preview receives the same material instances as the real character.

Compositing runs when a relevant value changes, not every frame. Unchanged surfaces are reused. Imported masks use weak object references and can be reimported after garbage collection. Initial texture import is more expensive than a subsequent slider update. One measured later update took under 1 ms of game-thread time; this is not a GPU or frame-rate benchmark. Long-session profiling remains necessary.

Canvas drawing alone left generated mip levels black in the live game. CSS now calls the exact game's `UTextureRenderTarget2D::UpdateResourceImmediate(false)` after drawing. It checks the reflected caller address, call instruction bytes and callee prologue before calling. Separate adapters support the previously verified UE5.6.1 CL93241 executable and Steam hotfix build 25265616. Each requires its own reflected wrapper address, call bytes and callee prologue to match. An unrecognized executable is refused instead of calling an unverified address. See [game-build compatibility](development/game-build-compatibility.md). Only engine work is queued on the render thread, with no callback into the reloadable DLL. Unreal's [render-target API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UCanvasRenderTarget2D) and [material-instance API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UMaterialInstanceDynamic) provide the underlying rendering and parameter interfaces.

Colors retain the existing shaders' lighting and effects, but this does not prove compatibility with every temporary gameplay material replacement or another mod that owns the same component. CSS refuses an already-dynamic material when initially creating a dye instance. Death, travel, damage effects and unrelated material mods still need targeted testing.

## What CSS adapts from CNS

CNS's [advanced configuration guide](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-json-advanced.md) documents scalar/vector controls, material associations, linked controls and texture choices. Its [changelog](https://github.com/Dekita/SB-CustomNanosuitSystem-Docs/blob/main/guides/cns-changelog.md) also records saved custom configurations and fixes around sliders and restored materials.

CSS now adapts per-part RGB controls, bounded scalar controls, linked parameters, palette buttons, custom saved looks and original restoration. Its package format adds explicit texture masks for shaders whose tint parameters are compiled out. Generic texture-choice menus, shape keys, material toggles and independent accessory slots remain separate future work. No CNS Lua or cooked UI content is shipped in CSS.


## Variant-specific recipes

A variant may declare its own `colors` object. It replaces the outfit-level recipe for that variant; otherwise the outfit recipe remains the fallback. The package resource table is the union of the outfit-level recipe and all variant recipes. Identical filenames must have identical bytes. Native discovery verifies and caches that union, including resources used only by another variant.

When selecting a different variant, CSS keeps compatible color choices and drops unavailable part IDs or palettes. Original restores that variant's authored materials. Switching variants also rebuilds its dynamic materials and dye targets, preventing a previous variant's atlas from being reused. These additions require the updated native core from the variant-port work; the published v0.1.1 core predates this support.

[Porting variant bundles](porting-variants.md) documents the four new ports and their configured controls. Atlas masks are specific to each source texture. Unconfigured custom shader materials remain unchanged.
