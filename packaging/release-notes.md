### Fixed

- Fix Tiel's dagger and Eredrim's diapason floating near the feet when a CSS outfit lacks their attachment bones. Tested with Long Hair Proxima.
- Keep corrected attachments working when drawing or stowing the item, switching shells and returning to Original appearance.

### Upgrading

Close the game and extract `MSII-CSS-v0.3.2.zip` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the CSS runtime files. Keep `CustomShellSystem/state/`, CSSX, extensions and your installed outfit packages.

**Only CSS needs updating. Existing CSS outfit and port ZIPs do not need repacking. CSSX, UI Kit and Cheat Menu remain at 0.3.0.**

The separate reports of a stuck material-effect warning after portals and crashes after shell switching remain under investigation. This update does not claim to fix them.
