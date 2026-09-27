MSII - CCS v@VERSION@
Custom Combat System by _eins0fx

Standalone mod for Mortal Shell II. Pick which animation each attack of your
weapon plays: the three light hits, the light finisher, the light charge, the
same five for heavy, and the sidearm's fire (slots L1 L2 L3 LF LC, H1 H2 H3
HF HC and R). Candidates are every player weapon's moves, the enemy attacks
authored on the player's rig, and whatever new montages the running game
lists after a patch. Damage, hit windows and Resolve stay the weapon's own.
Save the result as presets and share the JSON files.

Needs the UE4SS build listed in release.json (the same build CSS and CSSX
use). It does not need CSS or CSSX and works alongside both.

Install (game closed):
  Extract this archive so that you get
  MortalShell2/Binaries/Win64/ue4ss/Mods/CCS/
    enabled.txt, dlls/main.dll, core/ccs_core-@VERSION@.dll, core.json,
    catalog.json, enemy-catalog.json, ranged-catalog.json, assets/,
    README.txt, THIRD_PARTY_NOTICES.txt, release.json

CCS is the last tab of the game's Player Menu. Open the Player Menu in the
world and switch tabs to CCS. There is no hotkey.
  Customize: A/D or left/right picks a slot, W/S or up/down moves through
  the candidates, Space or the gamepad's bottom button assigns, F or the
  gamepad's left button clears the slot back to the weapon's own attack,
  type to search, Escape clears the search and then closes the menu.
  Preset: save, load and delete presets in Mods/CCS/presets/.
  Settings: enable or disable the swaps, attack speed, menu scale.

Update: close the game, replace dlls/, core/, core.json and the three
catalog files, keep settings.json and presets/. Remove: delete the Mods/CCS
folder.

Files CCS creates: settings.json, presets/*.json, logs/ccs.jsonl,
runtime/status.json (live status), CCS.log (loader).

Source:
https://github.com/ngodn/CustomShellSystem/tree/main/CCS
