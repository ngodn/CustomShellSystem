### Added

- Open CSS directly from the game's Inventory menu, between Inventory and Tarstones.
- Browse Shell, Color and Templates sections with native fonts, button prompts, outfit thumbnails and scrolling lists.
- Save named appearance templates, load them, replace them or delete them.
- Use the [modding guide](https://github.com/ngodn/CustomShellSystem/blob/v0.2.0/docs/modding/README.md) and [project utility](https://github.com/ngodn/CustomShellSystem/blob/v0.2.0/tools/css_project.py) to prepare outfit packages, group mesh variants and configure colors. Source tools are separate from the runtime ZIP.

### Changed

- Use the game's Inventory character display, lighting and camera instead of the separate wardrobe preview.
- Rotate with right-stick left/right, zoom with right-stick up/down, and move framing with the left stick. Mouse users can right-drag to rotate, scroll to zoom and left-drag to frame the character.
- Reset the view with right-stick click, Home or the Reset view button.
- Match menu transitions and keep action prompts beside the list or panel they control.

### Removed

- Remove the standalone N wardrobe and its View/Back + Y shortcut. Open Inventory and select CSS instead.

### Fixed

- Replace the misleading "CSS is off" startup message with guidance for selecting an outfit or installing missing packages.
- Find outfit packages from the game's content directory, independently of a custom UE4SS Mods folder.
- Smooth character rotation across full turns and restore the native camera when leaving CSS.

### Upgrading

Close the game and extract `CustomShellSystem` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the existing runtime files. Keep `CustomShellSystem/state/` to retain your choices, favorites, colors and saved looks, now shown under Templates.

**Existing CSS.Package v1 outfit ZIPs remain compatible.** BeauteGenessa, BeauteKnightLady, HIT2, Seductress and the other converted CSS bundles do not need repacking. Keep them installed under `MortalShell2/Content/Paks/~mods/`. Update the CSS runtime to this ZIP.

Open **Inventory → CSS** after entering the game world. With default bindings, use **I**, then **Q/E** or click CSS. On controller, open Inventory normally and use **LB/RB** to select CSS. **LT/RT** or **Z/X** switches Shell, Color and Templates. The page shows the remaining controls.

Requires UE4SS `v3.0.1-1028-gd7e7826d` with the GameShippingWin64 ABI. Other UE4SS builds and storefront binaries have not been verified.

The runtime ZIP includes one loader DLL, one core DLL and the required logo. It includes no outfit assets, personal state, caches or development cores. Outfit-specific physics, clipping, longer combat/travel sessions and more display/controller combinations still need broader testing.

The converter handles compatible cooked Mortal Shell II outfits. Some source mods need material, dependency or color-mask adjustments and in-game checks. The guide includes current limits; it does not turn arbitrary meshes from another game into ready-to-use outfits.
