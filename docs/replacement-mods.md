# Use Non-CSS Mod

The SHELL tab's Appearance group has a fifth row, **Use Non-CSS Mod** (1.0.0-beta.6).
It lists the replacement mods installed under `Content/Paks` that change a shell, enemy
or NPC, and wears that character's look on your current shell the way Use NPC / Enemy
does: the mesh swaps, the character keeps the materials the game gives it, and your
shell keeps its abilities and progress. Left / Right step through every look, the
details window groups them per mod, and Search mods opens the picker.

A replacement mod is an IoStore container (`.pak` + `.utoc` + `.ucas`) without CSS
metadata that ships game assets at their original paths. The engine loads the mod's copy
instead of the game's, so the character it covers already looks different wherever the
game draws it. The row does not add anything to that: it puts the mod's look one row
away, by the mod's own name, without the player having to know which character it
replaced. Wearing "ProximaFitShape" on Genessa wears Proxima's mesh, which that mod
replaced; wearing a Harbinger reskin wears the Harbinger form it covers.

## How CSS recognizes one

A container names its packages only by id, a CityHash64 of the lowercase UTF-16 package
path. CSS cannot read folder names out of it, so it ships a table instead:
`Mods/CustomShellSystem/catalog/replacement-targets.json` holds the ids of every game
package under the character folders CSS can wear from, one entry per folder. At each
scan, every pak the package scan ignored (no CSS metadata) has its `.utoc` header and
chunk-id table read, a few KB, never the compressed payload. Each export-bundle chunk
carries its package id in plain form; an id found in the table means the mod touches
that folder. Only patch containers are candidates: the engine mounts a pak named `*_P`
above every base container (`FPakPlatformFile::Mount` adds 100 per patch version to its
order), which is what lets a mod win over the game's copy. The game's own containers
carry no suffix and are base content by that same rule, and a mod without the suffix has
no promise of winning, so neither is listed. A pak without a `.utoc` carries no cooked
packages on this engine and is skipped, and a container whose table cannot be read is
recorded and skipped.

A folder is a character's own folder under `Characters/<Group>/` (Shells, Enemies,
NPCs, Bosses, and one level deeper inside the Brigands and MS1 Fallgrim families). It
covers the character's mesh, materials, textures, physics and animation assets alike,
so a texture-only reskin is listed with the same looks as a mesh replacer. Shared
folders such as `Shells/_Shared` hold no wearable mesh and are left out on purpose: a mod
that only touches them changes many characters at once and no single look would be
honest to list. Weapon, effect, level and other mods match no folder and never appear.

The looks a folder offers are the ones CSS already knows there: the Use NPC / Enemy
roster (`catalog/npc-appearances.css.json`) and the official shells, which CSS reads
from the game once the player exists. A mod over an official shell therefore appears a
moment after the shell list does. A mod over a character CSS has no look for (a
creature on its own rig, or an NPC not on the roster) is recorded in the diagnostics but
gets no row.

## What the look supports

- **CUSTOMIZE** offers Ground height only. Colors and parts are whatever the mod's
  author cooked; there is no CSS recipe to dye.
- **LOCOMOTION** offers Default and the CSS feminine idle and walk, as for any look.
  Custom animation packs come only from CSS packages.
- **MISC** works unchanged: it hides item actors on the pawn and never touches the body
  mesh.
- **Profiles** save the look like any other selection (`css.mod.<container>` plus the
  variant). If the mod is removed later, loading the profile shows the shell's default
  and says so in `CSS.log`.

Two notes appear in the details window when they apply: a mod over the shell you are
wearing is "already shown by the game" (there is nothing to wear), and two mods over one
character are named, since the engine keeps whichever mounted last.

## Conflict warning

The same scan names a container that overrides a shared asset CSS itself depends on:
the human skeleton (`Humans/_Shared`, the compatibility gate for every look), the
player's animation graphs and blend spaces (`Humans/Player`, which the walk override
hooks) and the shell base classes (`Core/Characters/Player/Common`). Such a mod can
make every CSS look fail to apply or animate wrongly. CSS never blocks it: it cannot
unmount a container and the files belong to the player. It says so instead, in
`CSS.log` ("CSS warning: ... overrides a shared asset CSS depends on"), in
`runtime/status.json` (`catalog.replacement_conflicts`) and at the top of the Use
Non-CSS Mod details window, naming the mod and the asset. A container that only
overrides shared assets is recorded there and gets no look to wear.

## Diagnostics

`runtime/status.json` under `catalog`:

- `replacements`: each recognized mod with its path, stem, folders, export-bundle count
  and how many were in a listed folder.
- `replacement_scan`: containers examined, base (non-patch) ones skipped, pak-only ones skipped.
- `replacement_errors`: containers whose table could not be read, with the reason.
- `replacement_conflicts`: containers that override a shared asset CSS depends on, with
  the asset names.
- `replacement_targets`: the table's folder and package counts, or `missing` /
  `invalid: ...` when the row cannot work.

`CSS.log` prints one line per recognized mod ("CSS replacement mod: ProximaFitShape_P
(2 of 2 packages in a character folder) -> /Game/Sparta/Characters/Shells/KnightLady/").

## The table

`tools/build_replacement_targets.py` writes the table from the game's file list
(`work/research/list/files.txt`, GameDump tool `list` mode), the NPC roster and the
official shells' default meshes. It carries its own CityHash64 (verified against a
reference implementation on 25,206 inputs) and records the game build it was made from.
Rebuild it after a game patch that adds or moves character assets, then run
`css_replacement_tests`. The runtime never hashes anything: it compares the ids it reads
from a container against the table.

`css_replacement_tests <table.json> <mod.utoc>...` prints the folders a real container
touches, which is the quickest way to check why a mod is or is not listed.

## Limits

- An enemy mod that redirects a blueprint to a new mesh instead of replacing the mesh
  package is listed (the blueprint sits in the character folder) but wears the game's
  mesh path, which that mod did not change. Official shells are read live from the
  game's shell classes, so the same trick on a shell wears the mod's mesh.
- Mods installed while the game runs need the `rescan` command or a restart, like any
  package. The engine mounts containers at start, so a mod added mid-session is not
  drawn even when CSS lists it.
