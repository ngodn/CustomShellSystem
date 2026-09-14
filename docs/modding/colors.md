# Editable parts and color palettes

CSS exposes only parts the author configures. Each mesh variant always has its Original materials. Palettes are named color values, not additional mesh variants. Player templates save choices from both.

## Prefer real material parameters for new assets

Give a part a stable ID, a readable name, its default RGBA value and one or more material bindings. The slot is the component's material index. The parameter must exist and affect that material's cooked shader. Linking several bindings lets one control change matching regions across slots.

This complete example uses **illustrative parameter names** that you must author or replace. It does not claim that stock Genessa materials have `ClothTint` or `EyeIntensity`:

```json
{
  "id": "yourname.myoutfit",
  "colors": {
    "schema": 1,
    "controls": [
      {
        "id": "cloth", "name": "Cloth", "default": [1, 1, 1, 1],
        "bindings": [{"slot": 0, "parameter": "ClothTint"}]
      },
      {
        "id": "skin", "name": "Skin", "default": [1, 1, 1, 1],
        "bindings": [{"slot": 1, "parameter": "SkinTint"}]
      },
      {
        "id": "eye-strength", "name": "Eye glow", "type": "scalar",
        "default": [1, 0, 0, 1], "min": 0, "max": 8, "step": 0.1,
        "bindings": [{"slot": 2, "parameter": "EyeIntensity"}]
      }
    ],
    "palettes": [
      {"id": "crimson", "name": "Crimson", "values": {"cloth": [0.56, 0.14, 0.24, 1]}},
      {"id": "midnight", "name": "Midnight", "values": {"cloth": [0.10, 0.12, 0.18, 1]}}
    ]
  }
}
```

Save it as `colors.json` and set the project's `colors` field. The recipe ID must match the outfit ID. The runtime provides Original automatically; `original` is reserved and must not be declared as a palette. These palettes leave skin and glow out of their values. Whole-look presets may change them deliberately if the author declares them.

RGB inputs use sRGB values, converted to linear for Unreal. Scalar controls use the first component. The fourth component is protected from user/palette edits. Bindings default to global parameters; `association: "layer"` or `"blend"` requires a `layer` index. Static switches, texture-choice menus, morph sliders and arbitrary material toggles are not part of this color schema. [Unreal material-instance behavior](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine?application_version=5.6).

## Existing atlases without useful tint parameters

Use a dye surface when the original shader exposes a usable texture parameter but has no effective per-part tint. Keep its normal, roughness, metallic and other shader inputs intact. Author one RGBA layer per editable region:

- RGB stores grayscale surface detail to tint. It is not just a plain black/white selection mask.
- Alpha selects the affected UV region, including soft boundaries.
- Use disjoint regions when parts must remain independent. Overlaps composite in control-ID order.
- Preserve face details, opacity cutouts and unmapped regions explicitly.

A surface connects controls to a texture parameter and material slots:

```json
{
  "id": "body-color",
  "parameter": "YourBaseColorTextureParameter",
  "slots": [0],
  "resolution": 2048,
  "layers": {"cloth": "dye-cloth.png"}
}
```

Put this object in `colors.surfaces`; declare `cloth` in `controls` without a direct binding. Place `dye-cloth.png` beside the recipe. Files must use `dye-*.png` names, square 8-bit RGBA or grayscale-alpha PNGs, at 1024, 2048 or 4096 pixels. Each resource is limited to 32 MiB, and the combined resource budget is 256 MiB. Pick the lowest resolution that preserves the required details.

The packager embeds and hashes the recipe's referenced resources. It does not create UV masks for arbitrary mods. The source-specific generators in `tools/build_color_recipes.py` and `tools/build_port_colors.py` are examples, not universal anatomy detectors.

## Different variants

Use variant-specific recipes for different atlases or slot layouts. The [group recipe](project-format.md#alternate-source-containers) carries each variant's `colors` path. Keep a part ID consistent only when it means the same thing across variants. A variant recipe replaces the shared outfit recipe.

Original must restore the variant's authored materials. Test switching variant after editing colors, then reset it, leave the menu and return. Check shader effects visually under gameplay and character-menu lighting. A parameter read-back or successful PNG validation does not prove the result looks right.

For implementation details and existing examples, see [color customization](../colors.md). Mask compositing currently includes a game-build-specific mipmap adapter; supporting an arbitrary new game executable still needs runtime verification.
