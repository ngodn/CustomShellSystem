# Making outfits for CSS

CSS is a wardrobe for Mortal Shell II. Build a working cosmetic skeletal-mesh outfit, then package it for CSS. The author controls the model, materials, supported variants and editable regions. CSS handles selection, material overrides, colors, favorites, saved templates and the character-menu integration.

This guide documents CSS 0.2.0. `CSS.Package` v1 remains the installed outfit format, so existing packages do not need repacking. Project and authoring tools are included in the GitHub source, separately from the player runtime ZIP. This is a working authoring contract, not a promise that the framework will never change.

## Choose your starting point

| You have | Start here |
| --- | --- |
| A working Mortal Shell II appearance `.pak`, or `.pak/.utoc/.ucas` set | [Package it](#build-an-outfit-package) |
| Several alternate versions of one replacement mod | [Group mesh variants](project-format.md#alternate-source-containers) |
| A Blender model, FBX, or an editable Unreal project | [Create and cook compatible assets](asset-authoring.md) |
| An outfit from another game | Port and fit it first using the [asset authoring checklist](asset-authoring.md); its original container is not CSS input |
| An existing CSS outfit that needs a release ZIP | [Verify and ZIP](#zip-an-existing-css-outfit) |

You do not need to write Lua or C++ to make a normal CSS outfit. You also do not need to change the player's gameplay shell or unlock all shells, weapons or seals. Extra gameplay code, custom component spawning and new physics plugins are outside the package contract.

## Four different kinds of data

| Data | Example | Who supplies it |
| --- | --- | --- |
| Outfit | One wardrobe entry with author and thumbnail | Modder |
| Mesh variant | Regular/corrupted, cape/no cape, heels/no heels | Modder, as cooked meshes and optional material overrides |
| Color palette and controls | Crimson preset, separate cloth/skin/eyes sliders | Modder, as effective shader bindings or texture masks |
| Saved template | A player's chosen outfit, variant and custom colors | Player, stored in CSS's own state |

An outfit can have one variant and no colors. Original appearance remains available. To advertise editable colors, configure and test those controls explicitly. CSS does not guess anatomy or invent missing source parts.

## Build an outfit package

Use Python 3.14 (tested with 3.14.7), [retoc](https://github.com/trumank/retoc), [repak](https://github.com/trumank/repak), and a local Mortal Shell II installation. See [tool setup](tool-setup.md) for the tested revisions and the retoc patch. The packaging scripts use Python's standard library. Blender, Pillow, .NET and the CSS native SDK are not required just to package an already-working replacement.

Commands run from the CSS repository root. Quoted paths below are placeholders for your files. These examples use a POSIX shell; use `python` instead of `python3` where appropriate. The tested conversion host is Linux; see the Windows limitations in tool setup.

1. Create a project:

   ```sh
   python3 tools/css_project.py init work/my-outfit --id yourname.myoutfit --name 'My Outfit' --author 'YourName'
   ```

2. Put the finished source containers inside `work/my-outfit/source/`. Add your portrait as `work/my-outfit/thumbnail.png`. Use a square PNG, 128 to 1024 pixels, at most 4 MiB; 512 × 512 is recommended. Keep alternate containers that replace identical asset paths in separate source groups.

3. Edit `css-project.json`. Keep `id` stable across releases. Add an explicit `mesh` and `shells` when the source cannot be inferred, and `colors` when you supply a color recipe. [Project fields and examples](project-format.md).

4. Check the recipe, then build:

   ```sh
   python3 tools/css_project.py check work/my-outfit/css-project.json
   python3 tools/css_project.py build work/my-outfit/css-project.json --game '/path/to/MortalShell2' --retoc '/path/to/retoc' --repak '/path/to/repak'
   ```

`check` checks project inputs, companion discovery and image/color declarations. It does not inspect skin weights or prove that a material works. `build` runs the existing converter, its cooked-payload checks, full package verification, and ZIP read-back. Neither command installs the package or edits the game.

The result is:

```text
work/my-outfit/dist/
  CSS_My_Outfit_YourName_P/
    CSS_My_Outfit_YourName_P.pak
    CSS_My_Outfit_YourName_P.utoc
    CSS_My_Outfit_YourName_P.ucas
  CSS_My_Outfit_YourName_P.zip
```

The ZIP contains that one folder and its three files. Users extract the folder into `MortalShell2/Content/Paks/~mods/`, with the game closed, then launch with the CSS runtime installed separately. Do not distribute the source project, your state, cache, core DLLs or base game containers in an outfit ZIP.

The default stem is `CSS_${NAME}_${AUTHORorMODDER}_P`. Name and author are made filename-safe. A custom `name_format` is supported; `_P` always precedes every container extension. Do not rename the trio afterward because the embedded manifest records its filenames. Updates need a fresh output directory, for example `--output work/my-outfit/dist-v1.1.0`; existing outputs are never overwritten.

## Zip an existing CSS outfit

```sh
python3 tools/css_package.py verify '/path/to/CSS_My_Outfit_YourName_P' --repak '/path/to/repak'
python3 tools/css_package.py zip '/path/to/CSS_My_Outfit_YourName_P' --repak '/path/to/repak' --output 'dist/releases/CSS_My_Outfit_YourName_P.zip'
```

The ZIP command takes a verified three-file package, makes a checked snapshot, reads the completed archive back and verifies its bytes. It refuses existing ZIPs and extra package-directory files. It does not make an ordinary replacement into a CSS package.

## Before publishing

Follow the [release checks](release-checks.md) for every mesh variant and editable part. A successful build means the containers and metadata passed structural checks, not that the outfit has passed gameplay tests. Record the CSS version and game build tested on the mod page. Keep original-creator and adaptation credits accurate.

## Reference

- [Asset authoring and Windows cooking](asset-authoring.md)
- [Experimental Unreal authoring tools](advanced-tools.md)
- [Project recipe and mesh variants](project-format.md)
- [Editable colors and palettes](colors.md)
- [Installed package contract](package-contract.md)
- [Troubleshooting and release checks](release-checks.md)
- [Primary-source research](../modding-research.md)
- [Existing converter CLI and technical format notes](../css-packages.md)
