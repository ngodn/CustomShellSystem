MSII - CSS v@VERSION@
Custom Shell System by _eins0fx

Installation
1. Close Mortal Shell II.
2. Install the compatible UE4SS build listed below if it is not already present.
3. Extract the CustomShellSystem folder into:
   MortalShell2/Binaries/Win64/ue4ss/Mods/
4. Install CSS outfit packages separately under MortalShell2/Content/Paks/~mods/.
   For customized Genessa doubles, also install the separate CSS Astral Shared
   Assets download for this CSS version into that ~mods folder.
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
When naming a new profile, Enter saves it while the name field is focused.
Select/View or C performs the displayed contextual action, including favorites.
Y or I toggles Lighting. View controls lock while you move the light.
B or Esc goes back/closes. Follow each page's displayed hints.
Right stick rotates/zooms. Left stick moves framing.
Mouse: right-drag rotates, wheel zooms, left-drag moves framing.
Right-stick click or Home resets the view/light.
Equipped outfits appear before favorites, followed by the remaining outfits.

Appearance changes retain your current gameplay shell and abilities.
No weapon, seal or shell unlocks are required for supported cosmetic outfits.

Genessa doubles
When your gameplay shell is Genessa, MISC offers independent Faithful Doubles
and Stray Doubles choices. Use CSS copies supported current customization with
the ghost effect; Default keeps the original double. Profiles save both choices.
Genessa Form switches once after closing the menu, then the game controls later
death/revival/travel transitions. It is not a permanent form lock.
NPC/enemy compatibility is partial and may have visual or animation issues.
Unsupported appearances or missing shared assets retain the original double.
Keep the older CSS_SharedAssets_P Eve package if installed. The new package is
CSS_AstralSharedAssets_P and does not replace it. Existing outfit ZIPs stay valid.

Settings and updates
CSS creates state/state.json on first initialization with clean defaults.
It creates runtime/ acknowledgements and caches package artwork/color resources
as needed. catalog/npc-appearances.css.json is the Use NPC / Enemy roster; it is
ordinary JSON you may edit. catalog/replacement-targets.json is the folder table
Use Non-CSS Mod reads installed replacement mods against; keep it as shipped.
The rest of catalog/ is optional.
CSS starts with no selected outfit. Select one in the wardrobe to enable it.
A previous valid state is kept as state/state.json.bak after settings change.

For updates, close the game and extract over the existing CustomShellSystem
folder. Keep your state/ folder. The ZIP supplies no state files to overwrite it.
Only one core DLL is supplied; old cores from development are not included.
Existing CSS.Package v1 outfit ZIPs and saved choices remain compatible.
The standalone N menu is replaced by Inventory > CSS.
Reset CSS preferences only with the game closed by moving state/ to a backup.
The CSS wardrobe does not modify the game's save files. Extensions may do so.

Beta status
This is a prerelease of CSS 1.0.0. Outfit packages are separate downloads.
Animation, fabric and other customization options depend on the installed outfit.
Missing custom animation options fall back to the game.

Use normal game restarts when updating DLLs or outfit containers.
CSSX is optional and distributed separately. CSS works without it.
