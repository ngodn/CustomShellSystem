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
SHELL / CUSTOMIZE / LOCOMOTION / PROFILE: LT/RT or Z/X.
Browse: D-pad Up/Down or W/S. Choose: D-pad Left/Right or A/D.
A or Space confirms. X or F performs the displayed secondary action.
Select/View or C performs the displayed contextual action, including favorites.
Y or I toggles Lighting. View controls lock while you move the light.
B or Esc goes back/closes. Follow each page's displayed hints.
Right stick rotates/zooms. Left stick moves framing.
Mouse: right-drag rotates, wheel zooms, left-drag moves framing.
Right-stick click or Home resets the view/light.
Equipped outfits appear before favorites, followed by the remaining outfits.

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

Alpha status and known limitations
This is CSS 1.0.0-alpha.2, a technology preview, not the final 1.0.0 release.
Eve Black Pearl is a separate sample package. Its movement and idle options are opt-in.
Eve walk, jog and sprint include the corrected leg trajectories accepted in
live gameplay review on 2026-09-21.
Custom idle animation and automatic in-hand weapon hiding are supported natively
via CSS post-process animation layers.
Broad weapon, combat and travel testing remains in progress.
Beacon teleport playback is not included yet. Missing custom animation options
fall back to the game; the UI rows only display authored options. The modder
kit and authoring tutorial are still being prepared.

Use normal game restarts when updating DLLs or outfit containers. Animation
DLL hot reload previously crashed the game and is not part of this alpha workflow.
CSSX is not included. Keep it disabled for this preview while its reported
performance regression remains under investigation.

