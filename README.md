# Custom Shell System (CSS) for Mortal Shell II

A native C++ wardrobe mod by **_eins0fx**. Change your shell's appearance without changing its abilities, with outfit variants, per-part colors, favorites, saved looks and an animated preview.

I built CSS around a single wardrobe for compatible outfit packages, inspired by [Custom Nanosuit System](https://www.nexusmods.com/stellarblade/mods/1496?tab=description). Mouse, keyboard and controller controls are supported.

**[Download CSS 0.1.2](https://github.com/ngodn/CustomShellSystem/releases/tag/v0.1.2)** · [Release notes](packaging/release-notes.md) · [Converter](tools/css_convert.py)

## Install

1. Close Mortal Shell II and install [UE4SS for MS2](https://www.nexusmods.com/mortalshell2/mods/45?tab=files). CSS targets `v3.0.1-1028-gd7e7826d`, included in the verified main-file bundle version 1.1.
2. Extract `CustomShellSystem` from the runtime ZIP into `MortalShell2/Binaries/Win64/ue4ss/Mods/`.
3. Install CSS outfit packages separately into `MortalShell2/Content/Paks/~mods/`, following each package's instructions.
4. Launch the game, load a save and press **N** or **View/Back + Y**. Select an outfit to wear it.

The runtime ZIP includes one loader DLL, one core DLL and the required interface artwork. It does not include UE4SS, outfit packages or personal state. Keep `CustomShellSystem/state/` when updating to retain your choices. Restart the game after adding or replacing outfit containers.

## Features

- Scroll through installed outfits and their mesh variants.
- Use preset palettes or adjust author-configured parts with RGB sliders.
- Keep favorites and three saved looks.
- Orbit, zoom and frame an animated preview while gameplay is paused.
- Restore your original appearance or reset colors.

CSS stores its settings separately from the game save. It does not unlock equipment or change your gameplay shell. Compatible cosmetic outfits do not require unlocking all shells, weapons or seals.

## See it in action

[![Seductress with a polearm in the game inventory preview](docs/media/v0.1.1/seductress-inventory-polearm.png)](https://raw.githubusercontent.com/ngodn/CustomShellSystem/main/docs/media/v0.1.1/css-wardrobe-demo.mp4)

**[Watch the 63-second wardrobe demo (MP4)](https://raw.githubusercontent.com/ngodn/CustomShellSystem/main/docs/media/v0.1.1/css-wardrobe-demo.mp4)**.
Click the image or link to open the recording. The Seductress outfit is shown in the game inventory above; outfit packages are separate
downloads.

![CSS preset palettes and per-part color sliders with Seductress](docs/media/v0.1.1/colors-crimson.png)

Preset palettes and custom colors on the Seductress outfit.
Editable parts depend on the installed package. Wardrobe lighting can differ
from gameplay; this remains a known issue in 0.1.2.

<details>
<summary>More screenshots</summary>

![Seductress in gameplay with an axe resting on her shoulder](docs/media/v0.1.1/seductress-gameplay-front.png)

Front view of Seductress during gameplay.

![Seductress in the inventory with Axatana selected](docs/media/v0.1.1/seductress-inventory-axatana.png)

The same outfit in the inventory with Axatana selected.

![Close side view of Seductress in gameplay](docs/media/v0.1.1/seductress-gameplay-side.png)

A closer look at the Seductress outfit in the game world.

![BeauteGenessa in the CSS wardrobe](docs/media/v0.1.1/wardrobe-genessa.png)

The appearance browser with BeauteGenessa by dantemk2.

![Seductress selected in the CSS appearance browser](docs/media/v0.1.1/wardrobe-seductress.png)

Switch appearances while keeping your gameplay shell.

![Seductress with the Verdigris palette controls](docs/media/v0.1.1/colors-verdigris.png)

Adjust a selected part or reset its colors.

![Seductress in gameplay with the wardrobe closed](docs/media/v0.1.1/seductress-gameplay.png)

The Seductress outfit in the game world.

</details>

See [media credits and Nexus BBCode](docs/media/README.md) for the original
mod links and reusable image/video links.

## Controls

| Action | Controller | Keyboard / mouse |
| --- | --- | --- |
| Open / close CSS | View / Back + Y | N |
| Close | B | Esc or Close |
| Browse appearances | D-pad up / down, list follows selection | Mouse wheel, scrollbar or click a row |
| Change variant | D-pad left / right | Variant arrows |
| Wear selected appearance | A | Click the appearance |
| Favorite | Y | Star |
| Change category | LB / RB | All / Fav / Looks / Color rail |
| Save or replace a look | X in Looks | Save / Replace |
| Open saved looks | X elsewhere | Looks rail |
| Adjust colors | D-pad rows and left / right | RGB sliders and palette buttons |
| Reset selected color part | A on a color channel | Reset part |
| Orbit character | Right stick | Right mouse drag |
| Zoom / horizontal framing | Left stick vertical / horizontal | W / S to zoom |
| Raise / lower framing | RT / LT | E / Q |
| Reset view | Right-stick click | Reset view |

Right-stick vertical movement is inverted by default; horizontal movement is normal. Wardrobe changes keep the existing camera. They recenter a moved view, but leave already-default framing untouched.

Gameplay pauses while CSS is open. The preview stays animated, and closing the wardrobe returns control to the game.

## For modders

The C++ source and Python converter are available in this repository. CSS is still under development. I'll publish a full modding guide once the framework and package format are stable; the working technical notes are available now.

The converter handles cooked Mortal Shell II appearance mods. It accepts a directory or `.pak` / `.utoc` / `.ucas` inputs and creates a package with author metadata and a thumbnail. It does not fit another game's meshes to the Mortal Shell II skeleton.

Use Python 3.14, retoc, repak and your local Mortal Shell II files. From the repository directory:

```sh
python3 tools/css_convert.py '/path/to/original-mod' \
  --name 'My Outfit' --author 'YourName' --id yourname.myoutfit \
  --thumbnail '/path/to/thumbnail.png' \
  --game '/path/to/MortalShell2' \
  --retoc '/path/to/retoc' --repak '/path/to/repak' \
  --output './dist'
```

Provide a square PNG, preferably 512 x 512. The default output is `CSS_My_Outfit_YourName_P/` containing one matching `.pak/.utoc/.ucas` trio. Keep the package ID stable across updates. Check [tool setup and dependency versions](docs/repository.md) before building the tools.

Some source mods need material-slot mappings, missing-dependency repairs, skeleton checks or hand-authored color masks. Use `--variant-sources` for separate alternate packs and `--colors` for an outfit color recipe. Variant groups can each supply their own materials and colors. A successful conversion still needs in-game testing.

- [Package format and converter usage](docs/css-packages.md)
- [Combining variants and handling source-specific adjustments](docs/porting-variants.md)
- [Color recipes and masks](docs/colors.md)
- [Native build requirements](docs/ue4ss-sdk.md)
- [Build, install and live reload](docs/native-development.md)

The runtime is C++23, built with clang-cl and the pinned UE4SS SDK. The core supports live reload during development; installing a new loader or outfit containers requires a restart. [Runtime release packaging](docs/releases.md) documents clean builds and fresh-state checks.

## Compatibility and known issues

Regular mesh replacement mods do not automatically become CSS wardrobe entries. Base-game mesh or material replacers can affect CSS outfits that reference those assets. Editable parts depend on the outfit author.

Wardrobe lighting can look different from gameplay and inventory. This remains open in 0.1.2. Cloth, secondary animation and clipping also depend on the outfit; long combat sessions, other shells and performance still need broader testing.

If you report a problem, include your CSS version, outfit, active gameplay shell and reproduction steps. A screenshot or short recording helps. [Implementation and test status](docs/STATUS.md) tracks the current checks and limits.

## Credits

- **_eins0fx**: CSS development, interface and packaging.
- [Custom Nanosuit System by DekitaRPG](https://www.nexusmods.com/stellarblade/mods/1496?tab=description): feature and interaction inspiration.
- UE4SS contributors and Framecore: UE4SS and its Mortal Shell II distribution.
- **dantemk2**: the BeauteGenessa and BeauteKnightLady outfit examples.
- **XTGMods**: HIT2 DE Scyther, shown in the demo.
- **Skirbie, dantemk2, Larian Studios and Volno's Lazy Tailor library**: source credits for my separate Seductress adaptation.

CSS does not bundle CNS scripts, blueprints or artwork. Outfit assets are separate from the runtime ZIP. See [media credits](docs/media/README.md) and [Nexus descriptions](docs/nexus/README.md).
