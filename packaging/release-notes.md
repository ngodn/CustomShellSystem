### Changed

- Search all of `MortalShell2/Content/Paks`, including `~mods` and other subfolders, for CSS outfit packages.
- Record each scanned pak and its result in `CSS.log` and `runtime/status.json`. The empty wardrobe now distinguishes missing CSS metadata from package or folder errors.

### Fixed

- Keep healthy outfits available when another package is damaged, incomplete, duplicated or cannot write its cached resources.
- Preserve non-ASCII installation paths when creating or recovering settings and loading menu artwork.
- Recognize `.PAK` as well as `.pak` filenames.

### Upgrading

Close the game and extract `CustomShellSystem` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the runtime files. Keep `CustomShellSystem/state/` to retain your choices, colors, favorites and templates.

Existing CSS outfit ZIPs remain compatible and do not need repacking. Keep each outfit's `.pak`, `.utoc` and `.ucas` together, with their original filenames. Open **Inventory → CSS** after entering the game world.

This patch addresses reproduced loading failures. If your outfit list is still empty, send the new `CSS.log` and `runtime/status.json` from `ue4ss/Mods/CustomShellSystem/`; they now include the searched path and individual package results.
