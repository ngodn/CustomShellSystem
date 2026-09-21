### Added

- Per-outfit Walk, Jog and Sprint choices, saved independently in character profiles.
- Eve (Stellar Blade), Black Pearl technology preview with modular outfit parts, color palettes, body controls and secondary motion. Install its separate sample download.
- Official-shell appearance choices that retain the equipped shell's gameplay abilities.
- Preview lighting controls with Y on controller or I on keyboard.

### Changed

- Use shared scrollable selectors and contextual footer controls.
- Show equipped outfits first, then favorites. Select/View adds favorites; B remains Back.
- Label the original animation choice Default.

### Fixed

- Correct Eve walk, jog and sprint footing to remove the reported sideways instability.

- Restore Feminine (CSS) standing idle when the character stops moving.
- Place status messages below the contextual control hints.
- Preserve the accepted Eve hand rig, heel supports, color boundaries and ground offset.

### Upgrading

Close the game. Extract the runtime's CustomShellSystem folder into
MortalShell2/Binaries/Win64/ue4ss/Mods/. Install the sample separately under
MortalShell2/Content/Paks/~mods/. Keep your existing state folder. Use the pinned
UE4SS NO AOB build listed in README. Keep CSSX disabled for this alpha.

This is **v1.0.0-alpha.1**, not the final release. The corrected Eve movement
passed the user's gameplay review. Custom Eve idle, weapon hiding and beacon
teleport playback are unfinished; broader combat/transition testing and the modder kit
remain pending. These known issues are part of the technology preview.
