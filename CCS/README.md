# Custom Combat System

CCS is a standalone UE4SS C++23 mod for Mortal Shell II (UE 5.6.1). It lets you choose which
animation each attack of the light and heavy chains plays (ten slots: L1 L2 L3 LF LC and H1 H2
H3 HF HC), scale attack speed, and save the result as shareable presets. The page lives in the
game's Player Menu as the CCS tab, after CSS and before CSSX when those are installed, built
from the game's own widgets the way the CSS and CSSX tabs are. There is no hotkey.

How it works: every player attack starts its animation through one native function, the
montage task factory. A loader-owned pre-hook on it reads the attacking ability, classifies its
class name into a slot once, and rewrites the montage and play rate in the parameter frame
before the native runs. Damage, hit windows and Resolve stay the weapon's own. Nothing runs per
frame in combat; one callback per swing, about ten microseconds.

Layout:

- `src/loader`: `dlls/main.dll`, permanent. Loads the core named in `core.json`, owns the
  native pre-hook service, switches cores live when `core.json` changes (developer path).
- `src/core`: the core DLL. `combat.*` the engine, `menu.*` and `menu_page.*` the native page,
  `engine.*` the reflection and UMG helpers, `core.*` the model and events. `*_probe.*` are the
  read-only diagnostics that verified the call site (`docs/discovery-probe-03.md`).
- `src/runtime`: portable code covered by host tests (settings, storage, catalog, writer,
  controls, search, tab order).
- `data/catalog.json`: the move catalog extracted from the cooked game (107 player moves).
- `docs/`: research and design records. `work/`: probes, traces and scripts.

Build and test from the CustomShellSystem repository root:

```sh
python3 CCS/tools/ccs.py build                      # Windows loader + core (clang-cl, pinned UE4SS SDK)
cmake -S CCS -B CCS/build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build CCS/build/host && ctest --test-dir CCS/build/host
python3 CCS/tools/ccs.py stage --confirm-game-stopped   # install into Mods/CCS (game closed)
```

Installed layout: `Mods/CCS/{enabled.txt, dlls/main.dll, core/ccs_core-<version>-<hash>.dll,
core.json, catalog.json, settings.json, presets/*.json, logs/ccs.jsonl}`. During development a
new core is staged next to the old one; editing `core.json` while the game runs switches to it.
