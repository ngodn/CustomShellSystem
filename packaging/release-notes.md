### Fixed

- Fix white, shredded-looking body trails during dodges with CSS outfits while preserving the normal translucent dodge trail.
- Keep standalone cloth-driver materials intact when preparing outfit materials.

### Upgrading

Close the game and extract `MSII-CSS-v0.3.1.zip` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the CSS runtime files. Keep `CustomShellSystem/state/` and your installed outfit packages.

**Existing CSS outfits and ports remain compatible. Their ZIPs do not need updating or repacking.**

**CSSX, CSSX UI Kit and CSSX Cheat Menu remain at 0.3.0.** Keep them installed; this patch does not change their extension interface or require an update.

The separate Seductress V1/V2 blood-coverage issue has been traced to the skin texture mask. It is not fixed in this release, and neither outfit package has been changed. Deferred attachment, cloth/foot alignment and HIT2 aiming issues also remain.

The dodge correction was tested in-game on Steam build 25265616 and confirmed by _eins0fx. Material checks covered one variant of each of the nine installed outfits. Older game-build adapters are retained; this does not establish a fresh test on older executables or every outfit variant.
