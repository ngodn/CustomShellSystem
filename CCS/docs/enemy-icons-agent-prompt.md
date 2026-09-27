# Task: produce enemy weapon icons for Custom Combat System (CCS)

You are producing artwork assets for an existing, working Unreal Engine 5.6.1 mod. You do not
touch the mod's C++ or the running game. Your deliverable is a folder of PNG icons, one per
enemy family, rendered from the game's own enemy weapon meshes, in the style of the game's
weapon icons. The mod already knows how to load them (see "How CCS consumes the icons").

Read this whole document before doing anything.

## Ground rules

- Everything you write goes under `/home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/enemy-icons/`
  (scripts, exports, intermediate renders, notes) and the final PNGs under
  `/home/eins0fx/development/mods/msII/CustomShellSystem/CCS/assets/enemy-icons/`. Never `/tmp`,
  never the scratchpad, never `~/Pictures`.
- Do not modify any file outside those two folders. In particular do not edit
  `CustomShellSystem/work/research/GameDump-tool/` (copy it into your work folder if you need
  to extend it), nothing under `CCS/src`, nothing in the game installation.
- The game may be running. Read-only access to its `Content/Paks` and `Binaries/Win64/ue4ss`
  folders is fine. Do not write into the game folder and do not launch or drive the game.
- Do not print large raw dumps into your transcript; write them to files and summarise.
- No em dashes in anything you write. Plain, direct prose.

## What the icons must look like

The game's own weapon icons are the reference. Facts read from the cooked asset
`T_UI_Icon_ClockworkScythe` (`/Game/Sparta/UI/Icons/Weapons/`):

| Property | Value |
| --- | --- |
| Size | 230 x 230 pixels |
| Pixel format | B8G8R8A8 with alpha (transparent background) |
| Texture group | UI, never streamed |
| Content | the weapon alone, diagonal from lower left to upper right, filling about 80 to 90 percent of the square, soft warm key light from upper left, faint cool rim, no ground shadow, no frame |

Export the eight existing weapon icons first and look at them; match their framing, angle,
lighting mood and edge softness. Export with the dump tool below using the regex
`UI/Icons/Weapons/T_UI_Icon_.*\.uasset$`; the JSON includes the mip data, or convert the
`.uasset` with any CUE4Parse texture export path (`ETexturePlatform` decode to PNG).

Deliverable size: render at 920 x 920 and downscale to 230 x 230 with a high quality filter,
transparent background, 8-bit RGBA PNG. Keep the 920 masters in your work folder.

## Which enemies need an icon

The list of enemy families is the `source_name` field of every move in
`/home/eins0fx/development/mods/msII/CustomShellSystem/CCS/data/enemy-catalog.json`
(28 families today, for example `Brigands`, `CultistSpearLady`, `Sicario`, `Aristocrat`,
`Draugr`, `Wraith`, `Miner`, `CannibalKnight`, `GrishaHunter`, `MS1`). Produce exactly one PNG per
family, named `<source_name>.png`, for example `assets/enemy-icons/CultistSpearLady.png`.

`MS1` groups legacy enemies from the first game (Twin Sisters, Heavy Cultist); pick the most
recognisable weapon among them. If a family has no separate weapon at all (bare-handed or
creature attacks), render a tight three quarter portrait of the enemy's head and shoulders
instead, same framing rules, and say so in your report.

## Where the meshes are

Cooked game content: `/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Content/Paks`.
A full file list already exists at
`/home/eins0fx/development/mods/msII/CustomShellSystem/work/research/list/files.txt` (112k lines);
search it instead of listing paks. Enemy content lives under
`MortalShell2/Content/Sparta/Characters/Enemies/<Family>/`.

Weapons are found in three ways, in this order:

1. A separate weapon mesh under the family, `Art/Mesh/SM_*` or `SK_*` whose name says what it is
   (`SM_CultBrigand_Mace`, `SK_CultistSpearLady_Spear` style names). Only a few families have
   these (Brigands, CultistSpearLady, Batman, Batushka, DungeonChampion, BladeSlave).
2. The family's blueprint (`BP_<Family>*.uasset` under the same folder or `Sparta/Core/AI/...`)
   references its weapon actor or mesh component. Export the blueprint with the dump tool and
   look for `SkeletalMesh`, `StaticMesh`, `WeaponMesh`, `Weapon` object references.
3. The weapon is part of the enemy's skeletal mesh. Then render the character mesh and crop to
   the weapon, or fall back to the portrait rule above.

Record for every family which route you used and the exact asset path, in
`work/enemy-icons/sources.json` (`{"<source_name>": {"asset": "...", "route": 1|2|3}}`).

## Tools already on this machine

- Offline asset export (CUE4Parse console tool, already built, run in place):
  ```
  DOTNET_ROOT=/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401 \
  /home/eins0fx/.local/share/mise/installs/dotnet/10.0.401/dotnet \
  /home/eins0fx/development/mods/msII/CustomShellSystem/work/research/GameDump-tool/bin/Release/net10.0/GameDump.dll \
  PAKDIR USMAP OUTDIR export '<regex on the file path>'
  ```
  `PAKDIR` is the Paks folder above, `USMAP` is
  `/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap`.
  It writes one JSON per matching asset (properties and references, no geometry). Mounting
  takes about a minute, so batch your regexes.
- Mesh and texture extraction: the tool has no geometry export. Copy its folder into
  `work/enemy-icons/GameDump-mesh/`, and add a mode that uses CUE4Parse's converters
  (`CUE4Parse_Conversion.Meshes.MeshExporter` with `EMeshFormat.Gltf2` or `ActorX` for meshes,
  `CUE4Parse_Conversion.Textures.TextureDecoder` for PNG) to write glTF/PSK plus textures per
  asset. Build with the same dotnet 10 SDK path (do not use `mise exec`, it resolves to the 9.0
  SDK). Source of the existing tool: `Program.cs` in that folder; the CUE4Parse package is
  already referenced in its csproj. If CUE4Parse_Conversion is not in the csproj, add the NuGet
  package with the same version as CUE4Parse.
- Blender: installed and reachable through the Blender MCP server (tools `get_scene_info`,
  `execute_blender_code`, `get_viewport_screenshot`) and as a command line (`blender -b -P
  script.py`). Use it headless for batch renders. Import glTF, set up a camera looking at the
  weapon along a diagonal, a key area light upper left, a dim fill and a faint cool rim, film
  transparent, Cycles or Eevee at 920 x 920, then downscale with Pillow or ImageMagick.
- Image tools: ImageMagick (`magick`), Pillow via `python3`, `ffmpeg`.

## Working method

1. Export the eight game weapon icons to PNG and put them in `work/enemy-icons/reference/`.
   Open two and write down the framing numbers you will reproduce (angle, fill ratio, light
   direction). Do not skip this; the icons must sit next to these in the same list.
2. Build `sources.json` for all 28 families before rendering anything.
3. Extend the dump tool copy to export the chosen meshes as glTF with textures into
   `work/enemy-icons/meshes/<source_name>/`.
4. One Blender script, parameterised by mesh path and output path, renders all families with
   identical camera, lights and material handling. Keep it in `work/enemy-icons/render.py`.
5. Contact sheet of all icons next to the eight game icons: `work/enemy-icons/contact-sheet.png`.
   Fix outliers (too dark, too small, wrong angle) before delivering.
6. Copy the 230 x 230 results to `CCS/assets/enemy-icons/<source_name>.png`.

## How CCS consumes the icons

Nothing to code on your side. The mod's stage tool copies `CCS/assets/enemy-icons/*.png` into
`Mods/CCS/assets/enemy-icons/`, and the menu imports a file named after the move's
`source_name` for every enemy row and tile (`file:assets/enemy-icons/<source_name>.png`),
rooted once like the banner. A missing file simply means no picture for that family, so partial
delivery is fine; deliver families as you finish them.

## Report

Write `work/enemy-icons/README.md` when done: what each family's icon was rendered from
(the `sources.json` content in a table), which families fell back to a portrait and why,
the exact commands you ran, and anything that looked off in the contact sheet that you left as
is. Keep it under one page.
