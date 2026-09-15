MSII - CSSX v@VERSION@
Custom Shell System Extensions by _eins0fx

Requires CSS @VERSION@ and the UE4SS build supported by CSS.
Close Mortal Shell II. Extract this archive inside:
MortalShell2/Binaries/Win64/ue4ss/Mods/CustomShellSystem/

The cssx.json file sits beside core.json. The CSSX DLL goes in cores/.
Launch the game, enter the world, then open Inventory > CSSX.
Install extension folders separately under CustomShellSystem/extensions/.
CSSX UI Kit and CSSX Cheat Menu are separate downloads.
Ordinary UE4SS scripts and DLLs need an adapter for the CSSX API.

Updates: close the game and replace the supplied files. Keep state/.
This package contains no game saves, extension settings or development cores.
CSS remains usable without CSSX. To remove CSSX, close the game, remove
cssx.json and its selected cores/cssx_core-@VERSION@.dll, then restart.

Managed files created as needed inside CustomShellSystem:
state/extensions/<id>.json             Extension settings and .bak recovery
logs/cssx.jsonl                        Framework log
logs/extensions/<id>/current.jsonl     Extension log
output/extensions/<id>/               Exported text/data
Logs rotate at 1 MiB, keeping five backups. No player needs to create these.

C++ and Lua authoring guide:
https://github.com/ngodn/CustomShellSystem/tree/main/docs/extensions
