# CSSX 1.3.1

### Added

- Extensions can pass objects to game functions that expect interfaces and
  write large struct properties without reading them back.
- Extensions can draw guides and prompts while the game's HUD is hidden.
- Extensions can compare average frame time, slow frames and hitches over
  a chosen measurement window.

### Changed

- Enter exact numbers in native text fields. Press Enter or click Set value;
  Escape cancels. Controller arrows remain available.

### Fixed

- Pressing both thumbsticks during play no longer opens the menu and leaves
  controls stuck. Use the Player Menu's CSSX tab on controller; F6 still works.

### Upgrading

Close the game and extract the update into `ue4ss/Mods/`. Keep your
`CSSX/settings.json`, `CSSX/state/` and `CSSX/extensions/` folders.
