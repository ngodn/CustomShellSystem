### CSS 1.0.0-beta.5-hotfix.1

### Fixed

- Use Original Shell lists every official shell, including shells the save has not unlocked. Discovery reads the settings object's `Shells` soft references and loads each definition itself; `GetShellItemDefinition` only returns definitions already in memory, so a missing one used to drop the whole list ("the shell definition is not a class"). A bad entry is now skipped and logged, and discovery retries twice if the game was not ready.
- Beacon respawns, severing into the Harbinger and recovering a shell reapply dyed outfits without re-reading their colour masks from disk (Skin Suit: about 230 ms on the game thread down to about 25 ms).
- Assets CSS keeps loaded now actually stay loaded. UE 5.6 only honours roots registered through the engine's `AddToRoot`; UE4SS's `SetRootSet` sets the flag alone, which the collector ignores. CSS now lists what it keeps in the game instance's `ReferencedObjects`. This covers the worn mesh and materials, accessory meshes, colour masks, menu thumbnails and custom locomotion.

### Changed

- The log line for each appearance change reports its time and where it went (load, mesh swap, items, customize steps).
