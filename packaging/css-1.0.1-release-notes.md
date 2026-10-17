### Added

- Choose **Vista rest animation** under Locomotion > Rest. Use the Idle Animation
  library while sitting at a vista, with a separate selection for each outfit.
- Save the vista choice in profiles. Default restores the game's sitting pose.

### Changed

- Include shoulder-support assets in CSS Shared Assets. No separate hand-support
  download is needed.
- Keep shared animation and facial-playback compatibility for Commander White
  0.1.1 alongside the existing 0.1.0 package.

### Fixed

- Improve shoulder-carried weapon support on Eve Peony and Black Pearl,
  Commander White, and both Unholy Genessa skins, with a relaxed thumb/index hold.
- Preserve Commander White's body motion while shoulder support is active.

### Upgrading

Close the game and update both CSS and CSS Shared Assets to 1.0.1. Keep your
existing CSS `state` folder. Replace the `CSS_SharedAssets_P` folder under
`MortalShell2/Content/Paks/~mods/`. If you installed the private shoulder test,
remove `CSS_HandSupport_P` from that folder. Restart the game after updating.

Existing Eve, Unholy Genessa and Commander White packages remain compatible.
Vista animation changes require an outfit with a CSS idle layer. The native
sit/stand transitions remain in control. Poses use their authored position;
standing and dancing clips may not fit the stone as closely as seated poses.
