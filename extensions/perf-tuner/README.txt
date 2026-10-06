CSSX Perf Tuner
===============

Tests performance tweaks for your machine and keeps measured gains.
Tune preserves your resolution, graphics quality, upscaler, frame cap
and v-sync. It never edits Engine.ini or GameUserSettings.ini.

Requires CSSX 1.3.1 or newer. Put the eins0fx.perf-tuner folder in:
Sparta/MortalShell2/Binaries/Win64/ue4ss/Mods/CSSX/extensions/

Using it
--------
Open Player Menu > CSSX > Perf Tuner.

- Tune: checks the scene, tests supported tweaks, then compares the
  enabled set together. Takes several minutes. Close the menu and stay
  in one area for a useful comparison.
- Enabled: use the selected tweaks. Off restores their original values.
- Stop: cancels the current test or scan and turns the tuner off.
- Tests: compare one tweak or the enabled set manually. The separate
  Resolution test asks before temporarily changing render resolution.
  It restores the current value afterward. Tune never runs it.
- Scene: scan actors, lights and effects. Offscreen animation is an
  experimental manual option; it can affect gameplay and shadows.
- Your Engine.ini / Settings: read-only checks. Full reports are in
  Mods/CSSX/output/eins0fx.perf-tuner/.

A gain needs two consistent off/on comparisons. No measured gain means
no measured gain, not a promise of extra FPS elsewhere. Default memory
and garbage-collection tweaks normally stay on; short tests cannot reliably judge
their effect on streaming and long-term hitches.
Tune rejects gains that worsen slow frames. If the combined set measures
worse, it turns the tuner off and restores the original settings.

Windows, Proton and Steam Deck have platform checks. Current game
measurements cover an RTX 3060 under Proton only.

tweaks.json is editable with the game closed. Unsupported tweaks are
disabled. Picture-quality changes are rejected by the catalog.

Texture-pool console priority remains until restart, so the game's
texture-quality menu cannot change that pool again during this session.
