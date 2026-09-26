### CSS 1.0.0-beta.5

### Added

- The CSS page is built from the game's own menu widgets: rows, headers, sliders, prompts, dialogs and the details window. Every prompt clicks, sliders drag, the details window scrolls, search lists every result.
- Use NPC / Enemy: 69 enemies, people and Harbinger forms on the player's skeleton, from an editable `catalog/npc-appearances.css.json`.
- Ground height per outfit variant (Customize > Placement), overriding the package's `ground_offset_cm`.
- Physics presets as one framework module; packages may declare their own per control (`presets`).
- Misc rows for the worn shell's own items (Gragu's Revered Heart, Eredrim's Diapason, Genessa's Catalyst...), with an Always Shown mode.
- Rows say how a part differs from the author's default (Hidden, Shown, Edited, a preset name, When in use).
- Host ABI 2: state and runtime files are written on the loader's own thread.

### Changed

- Menu closed: CSS frame cost 0.68 to 0.93 ms mean down to 0.19 to 0.45 ms; p99 11.3 ms to under 1.2 ms. Menu navigation 3.1 ms to 0.40 ms mean.
- Custom locomotion assets load at wear time and stay referenced while the look is active.
- The seal and sidearm servo measures against the anchor bone's body, not the root body only.
- Keys match their prompts (Reset all asks first, F on a colour opens Exact colour, Space on Template restores the original).
- PlayerRecovery checks every five seconds while idle.

### Fixed

- A variant's heel offset came off before the next mesh swap and stacked 3 cm on every return (sunk feet, a limp).
- The page redraws correctly after the menu reopens.
- The Harbinger mirrors the last living shell from a fresh core.
- F on an outfit row opens Search catalog.

### Upgrading

Close the game. Extract the runtime's CustomShellSystem folder into
MortalShell2/Binaries/Win64/ue4ss/Mods/. Keep your existing state folder.
Restart the game once so the new loader (main.dll) is the one running.
