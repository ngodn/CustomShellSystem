# Custom Combat System

CCS is a standalone UE4SS C++23 mod for Mortal Shell II (UE 5.6.1). It lets you choose which
animation each attack plays, in thirteen slots: the light chain L1 L2 L3, the light finisher LF,
the charged light LC, the same five for heavy (H1 H2 H3 HF HC), and the sprint attacks S+L and
S+H. The sidearm's fire slot R exists in the code but is parked (`sidearm_slot_enabled`). The
candidates are every player weapon's own moves, the enemy attacks authored on the player's rig,
and any new montage the running game lists after a patch. Attack speed is scalable and the
result saves as shareable presets. Charged attacks need the Acolyte's or Unwieldy Stone, or the
Settings cheat that applies the game's own unlock effects. The page is the last tab of the game's Player Menu, built
from the game's own widgets the way the CSS and CSSX tabs are. There is no hotkey. CCS needs
neither CSS nor CSSX and never asks either to update.

How it works: every player attack starts its animation through one native function, the
montage task factory `AbilityTask_PlayMontageAndWaitWithNotifies:PlayMontageAndWaitWithNotifies`.
A loader-owned pre-hook on it reads the attacking ability, classifies its class name into a slot
once, and rewrites the montage and play rate in the parameter frame before the native runs.
Damage, hit windows and Resolve stay the weapon's own. Nothing runs per frame in combat; one
callback per swing, about ten microseconds (`docs/discovery-probe-03.md`).

## Layout

- `src/loader`: `dlls/main.dll`, permanent. Loads the core named in `core.json`, owns the
  native pre-hook service, and switches cores live when `core.json` changes (developer path).
- `src/core`: the core DLL. `combat.*` the engine, `menu.*` and `menu_page.*` the native page,
  `discovery.*` the Asset Registry scan, `rig.*` the skeleton rule, `engine.*` the reflection
  and UMG helpers, `core.*` the model and events. `*_probe.*` are the read-only diagnostics that
  verified the call site.
- `src/runtime`: portable code covered by host tests (settings, storage, catalogs, writer,
  controls, search, tab order).
- `data/catalog.json` (107 player moves), `data/enemy-catalog.json` (134 enemy attack montages
  on the human rig), `data/ranged-catalog.json` (9 sidearm fire montages): warm-up catalogs
  extracted from the cooked game. The running game's Asset Registry is the authority: entries
  it no longer lists are shown as "Not in this game version", montages it lists that no catalog
  knows appear as new, unverified candidates.
- `assets/`: banner and, once produced, `enemy-icons/<source_name>.png`
  (`docs/enemy-icons-agent-prompt.md` is the brief for making them).
- `packaging/`: the player README and third-party notices that go into the release ZIP.
- `docs/`: research and design records. `work/`: probes, traces and scripts (not committed).

## Slots and eligibility

Class-name classification of the attacking ability: `_A1.._A3` are L1..L3, `_B1.._B3` H1..H3,
`_A_Finisher`/`_A3_Finisher` LF, `_B_Finisher`/`_B3_Finisher` HF, `_A<n>_Hold` LC,
`_B<n>_Hold` HC, sidearm `GA_<X>Attack_Primary` and `GA_SidearmRanged*AttackBase` R.

Which candidates a slot offers: chain moves fit 1/2/3 of either chain, finishers only F,
holds only C, sidearm fire only R. Enemy melee attacks fit the chain and finisher slots; enemy
shots, crossbow and throw montages fit R. Only montages on a rig the player's body can play are
offered or loaded (`src/core/rig.hpp`): the game's `SKEL_Human_Skeleton`, any skeleton a CSS
package cooks under `/Game/CSS/`, or the exact skeleton of the body worn right now. The registry
scan reads each montage's `Skeleton` tag, so creature-rig montages never reach the list.

## Build, test, run

From the CustomShellSystem repository root:

```sh
python3 CCS/tools/ccs.py build                       # Windows loader + core (clang-cl, pinned UE4SS SDK)
cmake -S CCS -B CCS/build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build CCS/build/host && ctest --test-dir CCS/build/host
python3 CCS/tools/ccs.py stage --confirm-game-stopped    # install into Mods/CCS (game closed)
python3 CCS/tools/ccs.py swap-core                   # build and switch the running game to the new core
python3 CCS/tools/ccs.py release                     # dist/ccs-v<version>/MSII-CCS-v<version>.zip + .sha256
```

Installed layout: `Mods/CCS/{enabled.txt, dlls/main.dll, core/ccs_core-<version>[-<hash>].dll,
core.json, catalog.json, enemy-catalog.json, ranged-catalog.json, assets/, settings.json,
presets/*.json, logs/ccs.jsonl, runtime/status.json, CCS.log}`. `runtime/status.json` is
rewritten every five seconds with the combat, hook, menu and discovery counters. The release
ZIP extracts to `ue4ss/Mods/` and carries `CCS/release.json` with a checksum of every member.

## Player Menu

Tab order is Inventory, Tarstones, Map, then CSS and CSSX when installed, then CCS. The shipped
CSSX 1.2.0 refuses to attach when an unknown tab precedes it, so CCS waits up to three seconds
for the CSS and CSSX tabs and appends itself last. Customize: A/D or left/right picks a slot,
W/S or up/down moves through the candidates, Space or the gamepad's bottom button assigns, F or
the gamepad's left button clears the slot, typing filters the list, Escape clears the search and
then closes the menu. The page is rebuilt only when the model revision changes; the candidate
list is windowed to 80 pooled rows.
