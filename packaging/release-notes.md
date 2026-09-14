### Added

- Scroll through the full appearance list with the mouse wheel or scrollbar. Controller navigation keeps the selected row visible.
- Support different materials, color parts and palettes for each outfit variant in a CSS package.

### Fixed

- Restore the selected appearance after beacon travel and player transitions, while waiting for game-owned effects and menus to finish.
- Keep loaded outfit assets alive while loading their material overrides.
- Verify and regenerate packaged color resources used by every variant.

### Upgrading

Close Mortal Shell II and extract `CustomShellSystem` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the existing runtime files. Keep your `state/` folder to retain favorites, saved looks and colors.

The ZIP contains one loader DLL, one core DLL and the required wardrobe artwork. No personal settings, saved state, development cores or outfit packages are included. Existing CSS outfit packages remain supported; packages with variant-specific colors require this update.

Requires UE4SS `v3.0.1-1028-gd7e7826d` with the GameShippingWin64 ABI. Install outfit packages separately under `MortalShell2/Content/Paks/~mods/`. Press **N** or **View/Back + Y** to open CSS after loading a save.

The wardrobe preview can still differ from gameplay and inventory lighting. That issue remains open.
