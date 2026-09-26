# Use NPC / Enemy

The SHELL tab's Appearance group has a fourth row, **Use NPC / Enemy** (1.0.0-beta.5).
It wears one of the game's own enemies, people or Harbinger forms on your current
shell, the way Use Original Shell wears an official shell: the mesh swaps, the
character keeps the materials the game gives it, and your shell keeps its abilities
and progress. Left / Right steps through the whole roster, the details window lists
it in three groups, and Search characters opens the picker.

## Where the roster comes from

CSS ships it as a plain catalog document, `Mods/CustomShellSystem/catalog/npc-appearances.css.json`,
loaded by the same code that reads any catalog file in that folder. It holds three
outfits, `css.npc.enemies`, `css.npc.people` and `css.npc.harbinger`, one variant per
mesh. Any outfit whose id starts with `css.npc.` is treated as roster: it gets the
shared row instead of a line under Custom Shells, and it is left out of Browse shells.

Only meshes on the player's skeleton are listed. Mortal Shell II builds its playable
shells, most humanoid enemies, the villagers and the Harbinger forms on
`/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton`; those animate with the
player's own animation blueprint, which is what makes them wearable. Creature rigs
(spiders, snails, golems, bats, The Head, Ruk, the shopkeepers) are not on it and are
not listed. CSS does not retarget, and its skeleton check refuses any mesh whose
skeleton is not the player's, so an entry added by hand that points at a creature is
refused with a message rather than applied.

The roster was built from the game data on 2026-09-26 by
`tools/build_npc_roster.py`, which reads a skeleton dump of every character
mesh (`work/research/skel/skeletons.jsonl`, from the GameDump tool's `skel` mode) and
keeps the meshes on the human skeleton, minus dismemberment, LOD, test and clone
variants. Names come from the game's text where it has them (`ST_Core_NpcNames`,
`ST_Core_MiniBosses`, `ST_Core_DarkForms`); ordinary enemies have no in-game name, so
they carry the names the community uses.

## Editing it

The file is ordinary JSON. To add a mesh, append a variant with an `id`, a `name` and
the mesh's object path (`/Game/.../SK_Name.SK_Name`); to drop one, delete its entry.
Reload with the `rescan` command or restart the game. The runtime still checks the
skeleton on every wear, so a wrong path costs a message, not a crash.

## Performance

The row's list is long, so the page creates its widgets under a per-build budget
(`native_budget_per_build`, four game widgets a frame) and streams the rest in over
the following frames. Wearing a character loads its mesh the way any outfit loads,
synchronously on first use.
