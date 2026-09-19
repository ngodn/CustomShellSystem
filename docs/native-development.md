# Native runtime development

The installed mod lives in:

```text
MortalShell2/Binaries/Win64/ue4ss/Mods/CustomShellSystem/
  state/state.json       CSS settings, selections, favorites and saved looks
  state/state.json.bak   Previous valid state
  catalog/              Optional developer catalogs (normal outfits embed theirs)
  cache/packages/       Rebuildable package artwork and color-mask cache
  assets/               Original CSS interface artwork
  cores/                Versioned native core DLLs
  core.json             Requested core version
  runtime/              Live acknowledgements and diagnostics
```

State writes use temporary files, flushes, replacement and backup recovery. Settings include `invert_orbit_x`, `invert_orbit_y` and `auto_apply`. Edit settings with the game closed, or use the native development tab while running. Core reload reads saved settings again.

From this project directory:

```bash
python3 tools/css.py build
python3 tools/css.py reload
python3 tools/css.py open
python3 tools/css.py inspect
python3 tools/css.py close
```

`reload` builds and stages only the reloadable core, refreshes PNGs and catalogs, and waits for the exact version acknowledgement from the running game. It closes an active wardrobe cleanly. Loader ABI or mounted IoStore asset changes still require a restart; native core development does not.

This build targets UE5.6 and the installed UE4SS `97b7e501` GameShippingWin64 runtime, with retained `d7e7826d` headers and a separately generated runtime import library. It uses clang-cl, the xwin Windows SDK, C++23 and the dynamic release CRT. The Python tools target Python 3.14. See [SDK notes](ue4ss-sdk.md) before changing toolchain or UE4SS versions, and [repository conventions](repository.md) for tracked files, dependency patches and local checks.
