MSII - CCS v@VERSION@
Custom Combat System by _eins0fx

Alpha release, made to gather feedback. Standalone mod for Mortal Shell II.
Pick which animation each attack of your weapon plays, in thirteen slots:
the light chain L1 L2 L3, the light finisher LF, the charged light LC, the
same five for heavy (H1 H2 H3 HF HC), and the sprint attacks S+L and S+H.
Candidates are every player weapon's moves, the enemy attacks authored on
the player's rig, and whatever new montages the running game lists after a
patch. Base damage and Resolve are always your weapon's. Save the result as
presets and share the JSON files.

Needs the UE4SS build listed in release.json (the same build CSS and CSSX
use). It does not need CSS or CSSX and works alongside both.

Install (game closed):
  Extract this archive so that you get
  MortalShell2/Binaries/Win64/ue4ss/Mods/CCS/
    enabled.txt, dlls/main.dll, core/ccs_core-@VERSION@.dll, core.json,
    catalog.json, enemy-catalog.json, ranged-catalog.json,
    running-catalog.json, assets/, README.txt, THIRD_PARTY_NOTICES.txt,
    release.json

CCS is the last tab of the game's Player Menu. Open the Player Menu in the
world and switch tabs to CCS. There is no hotkey. The footer always shows
the keys for the device you are using.
  Customize: left/right picks a slot, up/down moves through the candidates,
  Space or the gamepad's bottom button assigns, F or the gamepad's left
  button puts the weapon's own attack back. Tab or the right stick click
  opens the slot's own settings on the right: Speed, Feel (Game's or Move's
  own), Damage (Move's own or Weapon's own), Visual (My weapon or Move's
  weapon), Armor (Move's own, or Hyper armor over the whole move, a
  cheat) and Steer (Whole move: the stick turns you through the move and
  cancels the recovery after the last hit; or Move's own). The search key
  shown in the footer (keyboard only) puts the caret in the search field;
  typing filters the list.
  Presets: save, load and delete your own presets in Mods/CCS/presets/.
  Settings: the master switch, Charged attacks, menu scale, reset all slots.

Charged attacks (the LC and HC slots) exist in the game only with the
Acolyte's Stone (light) or the Unwieldy Stone (heavy) equipped. Without the
stone a long press does the normal attack, and the page says so. The
Settings row "Charged attacks: Always (cheat)" grants both unlocks without
the stones; the charge still costs Resolve. The sidearm's fire cannot be
customised in this release.

Update: close the game, replace dlls/, core/, core.json and the catalog
files, keep settings.json and presets/. Remove: delete the Mods/CCS folder.
CCS changes nothing in the game's own files.

Files CCS creates: settings.json, presets/*.json, logs/ccs.jsonl,
runtime/status.json (live status), CCS.log (loader).

Source:
https://github.com/ngodn/CustomShellSystem/tree/main/CCS
