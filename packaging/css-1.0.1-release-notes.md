### Fixed

- Improve shoulder-carried weapon support on Eve Peony and Black Pearl,
  Commander White, and both Unholy Genessa skins, with a relaxed thumb/index hold.
- Preserve Commander White's body motion while shoulder support is active.

### Changed

- Include shoulder-support assets in CSS Shared Assets. No separate hand-support
  download is needed.

### Upgrading

Close the game and update both CSS and CSS Shared Assets to 1.0.1. Keep your
existing CSS `state` folder. Replace the `CSS_SharedAssets_P` folder under
`MortalShell2/Content/Paks/~mods/`. If you installed the private shoulder test,
remove `CSS_HandSupport_P` from that folder. Restart the game after updating.

Existing Eve, Unholy Genessa and Commander White packages remain compatible.
