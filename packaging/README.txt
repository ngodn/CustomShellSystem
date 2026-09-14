MSII - CSS v@VERSION@
Custom Shell System by _eins0fx

Installation
1. Close Mortal Shell II.
2. Install the compatible UE4SS build listed below if it is not already present.
3. Extract the CustomShellSystem folder into:
   MortalShell2/Binaries/Win64/ue4ss/Mods/
4. Install CSS outfit packages separately under MortalShell2/Content/Paks/~mods/.
5. Launch the game, load a save, and press N to open CSS.

This archive contains the wardrobe runtime only. It does not include UE4SS,
outfit packages, personal settings, favorites, saved looks or game saves.

Requirements
Mortal Shell II, UE 5.6.1.
UE4SS experimental v3.0.1-1028-gd7e7826d, GameShippingWin64 ABI.
https://github.com/UE4SS-RE/RE-UE4SS/releases/download/experimental/zDEV-UE4SS_v3.0.1-1028-gd7e7826d.zip
Verified UE4SS.dll SHA-256:
4cdd44e79df2a01fb00cf885791f933c1dd3a83324767c7a084c4da5a82f33bc
Use the Microsoft Visual C++ x64 runtime required by the game/UE4SS.
CSS uses this exact native ABI. Other UE4SS builds have not been verified.

Controls
N: open/close. Esc: close. Mouse: select outfits, variants and colors.
Mouse wheel or scrollbar: scroll the appearance list. D-pad navigation keeps
the selected row visible.
Controller View/Back + Y: open/close. B: close. D-pad: browse/adjust.
A: wear. Y: favorite. LB/RB: sections. X: saved looks.
Right stick: orbit, vertical inverted. Left stick: zoom/frame.
LT/RT: lower/raise framing. Right-stick click: reset view.

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
Reset CSS preferences only with the game closed by moving state/ to a backup.
CSS does not modify the game's save files.

Known limitations
The wardrobe preview can look different from gameplay or inventory lighting.
A lighting experiment is not included in this release. Extended combat, travel
and outfit-specific physics still need broader testing. New outfit containers
require restarting the game; core DLL development supports live reload.
