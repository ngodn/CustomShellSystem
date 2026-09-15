### Added

- Optional CSSX tab beside CSS, with a 3 by 3 extension library and shared menus for C++ and Lua extensions.
- Separate CSSX UI Kit technology preview and native CSSX Cheat Menu downloads.
- CSSX SDK guide, Lua and C++ starter examples, packaging utility, managed settings and rotating extension logs.

### Changed

- Reuse valid color materials when portals, launch points and gameplay shell changes refresh an appearance.
- Keep existing CSS outfit ZIPs and saved wardrobe choices compatible. Outfits do not need repacking.

### Fixed

- Recover CSS appearance after the game replaces a selected mesh or empties its material overrides.
- Remove owned color overrides left in extra material slots after changing gameplay shells.
- Recover the observed completed-intro restriction that left an equipped weapon attacking while stowed, without clearing unrelated story locks.
- Apply and restore Cheat Menu movement speeds without rewriting unrelated character data.

### Upgrading

Close the game. Extract CSS into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing its runtime files. Keep `CustomShellSystem/state/`.

CSSX is optional. Extract its ZIP **inside** `CustomShellSystem/`. Extract each extension folder into `CustomShellSystem/extensions/`. Install both CSS and CSSX before using an extension. All four downloads use version 0.3.0.

Before using CSSX Cheat Menu, disable or remove the original `MortalShell2Mod`, then restart the game. The port credits DevToolsMaster and retains the supplied MIT license. Cheats start off; apply settings explicitly. Progression grants can change the game save.

Open Inventory and select CSS or CSSX. Outfit-specific cloth, missing attachment bones, dodge shadows and HIT2 aiming reports remain under investigation or deferred. Older game-build adapters are retained, but this release's fresh live checks used Steam build 25265616. See the repository verification notes for remaining gameplay cases.
