MSII - CSS v@VERSION@
Custom Shell System by _eins0fx

Installation
1. Close Mortal Shell II.
2. Install the compatible UE4SS build listed below if it is not already present.
3. Extract the CustomShellSystem folder into:
   MortalShell2/Binaries/Win64/ue4ss/Mods/
4. Install CSS outfit packages separately under MortalShell2/Content/Paks/~mods/.
5. Launch the game, enter the game world, open Inventory, and select CSS.

This archive contains the wardrobe runtime only. It does not include UE4SS,
outfit packages, personal settings, favorites, saved looks or game saves.

Requirements
Mortal Shell II, UE 5.6.1.
UE4SS for MS2 NO AOB build v3.0.1-1111-g97b7e501, GameShippingWin64.
https://www.nexusmods.com/mortalshell2/mods/45?tab=files
Required UE4SS.dll SHA-256:
fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a
Use the Microsoft Visual C++ x64 runtime required by the game/UE4SS.
CSS links this runtime with retained d7e7826d headers. See the source SDK notes
for the exact build inputs and current live-verification status.

Controls (default game bindings)
Inventory / CSS / Tarstones / Map: LB/RB or Q/E.
Shell / Color / Templates: LT/RT or Z/X. Section labels are clickable.
Browse the left list: D-pad Up/Down or W/S. Mouse wheel scrolls the list.
Variant or selected color channel on the right: D-pad Left/Right or A/D.
Wear / reset selected part / load template: A or Space. Buttons are clickable.
Favorite / reset all colors / delete template: Y or C, shown by each action.
Next RGB channel / replace template: X or F, shown by each action.
Right stick: left/right rotates; up/down zooms in/out.
Left stick: moves character framing horizontally and vertically.
Mouse over the character: right-drag rotates, wheel zooms, left-drag moves framing.
Reset view: right-stick click, Home, or Reset view. Close: B or Esc.
CSS follows the game's mapped menu keys. The old N menu is removed.

Appearance changes retain your current gameplay shell and abilities.
No weapon, seal or shell unlocks are required for supported cosmetic outfits.

Settings and updates
CSS creates state/state.json on first initialization with clean defaults.
It creates runtime/ acknowledgements and caches package artwork/color resources
as needed. catalog/ is optional and is not required for packaged outfits.
CSS starts with no selected outfit. Select one in the wardrobe to enable it.
A previous valid state is kept as state/state.json.bak after settings change.

For updates, close the game and extract over the existing CustomShellSystem
folder. Keep your state/ folder. The ZIP supplies no state files to overwrite it.
Only one core DLL is supplied; old cores from development are not included.
Existing CSS.Package v1 outfit ZIPs and saved choices remain compatible.
The standalone N menu is replaced by Inventory > CSS.
Reset CSS preferences only with the game closed by moving state/ to a backup.
The CSS wardrobe does not modify the game's save files. Extensions may do so.

Known limitations
CSS uses the game's native Inventory preview and lighting. Extended combat,
travel and outfit-specific physics still need broader testing. New outfit containers
require restarting the game; core DLL development supports live reload.

Optional CSSX extensions
Install CSSX separately by extracting its files inside CustomShellSystem/.
Install extension folders inside CustomShellSystem/extensions/.
Open Inventory > CSSX. CSS works without CSSX or any extensions installed.
CSSX Cheat Menu requires removing/disabling the old MortalShell2Mod and restarting.
Progression grants in Cheat Menu can change the game save; wardrobe state is separate.
