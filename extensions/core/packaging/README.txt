MSII - CSSX v@VERSION@
Custom Shell System Extensions by _eins0fx

Standalone extension platform for Mortal Shell II. Needs the UE4SS build
listed in release.json (UE4SS 3.0.1 g97b7e501, the same build the CSS alpha
uses). It does not need CSS. It works alongside CSS v1.0.0-alpha.1.

Install (game closed):
  Extract this archive so that you get
  MortalShell2/Binaries/Win64/ue4ss/Mods/CSSX/
    enabled.txt, dlls/main.dll, core/cssx_core-@VERSION@.dll, core.json,
    README.txt, THIRD_PARTY_NOTICES.txt, release.json

Open the menu in the world: F6 on the keyboard, or press both thumbsticks on
a controller. Change the keys in Mods/CSSX/settings.json (Unreal key names).
Extensions install as folders under Mods/CSSX/extensions/. CSSX Cheat Menu is
a separate download.

If you used the old CSSX that lived inside CustomShellSystem: close the game
and run tools/cssx_migrate.py from the source tree, or move your
extensions/ folders and state/extensions/*.json by hand, then delete
CustomShellSystem/cssx.json. Until then CSSX shows a notice and loads no
extensions, so two hosts never own the same extension.

Update: close the game, replace dlls/ and core/, keep settings.json, state/
and extensions/. Remove: delete the Mods/CSSX folder. CSS is unaffected.

Files CSSX creates: settings.json, state/<id>.json (+.bak), logs/cssx.jsonl,
logs/<id>/current.jsonl (rotated at 1 MiB, five backups), output/<id>/,
runtime/*.json (live status), CSSX.log (loader).

Source and authoring guide:
https://github.com/ngodn/CustomShellSystem/tree/main/extensions/core
