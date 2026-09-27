# Brief: author 15 CCS presets

You are producing fifteen preset files for the Custom Combat System (CCS), a Mortal Shell II
mod that lets the player choose which animation each attack plays. A preset fills eleven
attack slots with moves taken from the player's weapons, from enemies, and from sidearms. Your
job is research and design, not guessing: read the catalogs and the research documents, work
out which moves belong together, write the files, validate them with the tool, and report.

## Where you work

- Presets go in `CustomShellSystem/CCS/presets/<name>.json`, one file per preset.
- Notes, scratch tables and scripts go in `CustomShellSystem/CCS/work/presets/`.
- Your final report goes to `CustomShellSystem/CCS/docs/presets-catalog.md`.
- Do not write anywhere else. Never use `/tmp`. Never modify `src/`, `tools/`, `data/` or any
  other file in the repository, and never copy source files from other projects into your
  folders; cite paths instead.
- Do not run the game and do not touch the game folder.

## How CCS uses a preset

Slots: light chain `L1 L2 L3`, light finisher `LF`, light charge `LC`; heavy chain
`H1 H2 H3`, heavy finisher `HF`, heavy charge `HC`; sidearm fire `R`. A slot that a preset
leaves out keeps the weapon's own attack. When the slot is filled, CCS plays the chosen
animation instead of the weapon's own, at the moment the game would have played the original.

Eligibility, enforced at load and by the tool:

- Chain moves (a weapon's 1, 2 or 3 of either chain) fit `L1 L2 L3 H1 H2 H3`.
- Finisher animations fit only `LF` and `HF`.
- Charge (hold) animations fit only `LC` and `HC`.
- Sidearm fire animations fit only `R`.
- Enemy melee animations fit the chain and finisher slots, not the charge slots.
- Enemy shots, crossbow and throw animations fit only `R`.

What a swap does to damage (from the exports, `docs/combat-damage-pipeline.md` section 3):
the hit windows and their payload live inside the animation as hit-check notifies, so a
swapped swing hits with the replacement move's timing, multiplier, poise damage and reaction.
For player moves the base damage is still the equipped weapon's. For enemy moves the payload's
base resolves from character data, which is unverified for a player; treat enemy moves as
"visual and timing verified, damage to be confirmed". A settings switch to force the weapon's
own payload is planned; design as if both modes will exist.

Things that make a slot feel wrong:

- Animations with zero hit windows in a damage slot (the tool prints the count). Loops and
  run-up starts (`RunUp_Loop`, `RunUp_Start`, `Loop`) are not attacks on their own.
- Very long animations in chain positions. A chain hit is usually 0.8 to 1.6 s; an 8 s boss
  combo in `L1` locks the player for the whole clip. Long clips belong in finisher slots if
  anywhere.
- Motion-warped lunges (`motion warp yes`) in every slot; one per chain is plenty.
- Ignoring the weapon in hand. The trace uses the equipped weapon's collision, so a dagger
  performing a two-handed hammer overhead looks off; the reverse can be fine. Say in the
  description which weapon the preset expects (`base_weapon`).

## The move catalogs

`CCS/data/catalog.json` (107 player moves), `CCS/data/enemy-catalog.json` (134 enemy
animations on the player's rig), `CCS/data/ranged-catalog.json` (9 sidearm fire animations).
Every entry has an `id`; that id is what a preset references. Player ids look like
`GA_Player_Attack_HadernSword_A2_C`, enemy ids like `enemy:A_Aristocrat_Attack_Combo_01_Montage`,
sidearm ids like `ranged:AM_Player_2H_Offhand_Shoot_R_NailShotgun`.

Player sources (`source_name`): BattleAxe, BlackNeedle, HadernSword, HeavyHammer, Axatana,
Scythe, MartyrsBlade, AxeDagger, plus a few shared combos. Enemy sources: Brigands (27 moves),
MS1 (17, the Mortal Shell 1 enemies), CultistSpearLady, GrishaHunter, CultistBase, Draugr,
TarredCorpse, DungeonChampion, FrogMama, Miner, Sicario, BallistaHead, CannibalKnight,
CentipedeGhost, Aristocrat, Batushka, HutchbackCarrier, BallBoy, Vampire, Wraith and single
moves from BabyEnemy, Grisha, ShellSnatcher, Slug, Snowworm, SpiderBro, TarredVestige, TheCoffin.
Sidearms: Ballistazooka, Crossbow, CursedChild, MachineGun, NailShotgun, ParasiteGun,
SimpleLute, Trebuchaxe.

Useful fields: player moves carry `payload` (damage multiplier, poise, reaction), `hits`
(timing windows) and `slots` (the move's original position); enemy moves carry `length`,
`hit_windows`, `motion_warp` and `notify_classes`; every move carries its `montage` path.
Nine player abilities are listed under `unresolved_abilities` (AxeDagger and Katana double
attacks, the Scythe finishers, the Martyr's Blade heavy hold); they are not in the catalog and
cannot be used.

## The tool

From the repository root (`CustomShellSystem/`), Python 3:

```sh
python3 CCS/tools/preset_check.py list                      # every move, its slots and a one-line summary
python3 CCS/tools/preset_check.py list --slot LF             # only moves that fit a slot
python3 CCS/tools/preset_check.py list --source Brigands     # one source
python3 CCS/tools/preset_check.py list --grep overhead       # name search
python3 CCS/tools/preset_check.py check CCS/presets/*.json   # validate; exit code 1 on any problem
python3 CCS/tools/preset_check.py fix   CCS/presets/*.json   # fill type/source/ability/montage from the catalogs
```

`check` is the acceptance test. All fifteen files must pass with zero problems.

## Research to do before designing

Read, in this order, and keep notes in `work/presets/notes.md`:

1. `docs/combat-movesets.md`: what each weapon's chain, finisher and charge look like.
2. `docs/enemy-movesets.md` and `docs/enemy-damage-pipeline.md`: enemy attack families,
   their rhythm, reach and payloads.
3. `docs/combat-damage-pipeline.md` sections 3 and 3.2: per-attack multipliers and poise.
4. The three catalogs, through the tool. Build yourself a table of candidate moves per slot
   with length, hit windows and the weapon they were made for.

Then design. For each preset decide the theme first (for example "a Brigand's rhythm on the
Hadern sword", "hammer that finishes like the Dungeon Champion", "all charge attacks are
enemy lunges", "crossbow user who fights like the Cultist spear lady"), pick a base weapon, and
choose every slot to serve that theme. Mix sources on purpose: across the fifteen, use at least
eight different player weapons, at least twelve different enemy families, and the ranged slot
in at least six presets. Every preset must fill at least eight of the eleven slots, and at
least five presets must fill all eleven. No two presets may share more than three identical
slot assignments.

## File format

`preset_name` must equal the file name without `.json`, using only letters, digits, dash and
underscore, at most 64 characters. `description` is required and is shown in the game: two or
three sentences on the theme, the expected weapon, and what the player will notice. Keep it
under 400 characters. `author` is your agent name.

```json
{
  "schema_version": 1,
  "preset_name": "Brigand_Rhythm",
  "author": "preset-agent",
  "description": "Hadern sword openers that turn into Brigand swings, closing with the Martyr's Blade finisher. Fast, wide, low poise cost. Made for the Hadern sword.",
  "base_weapon": "HadernSword",
  "light_chain": {
    "L1": {"type": "player", "source": "HadernSword", "move_id": "GA_Player_Attack_HadernSword_A1_C",
           "ability": "GA_Player_Attack_HadernSword_A1_C",
           "montage": "/Game/Sparta/Characters/Shells/_Shared/Animation/Attacks/HadernSword/A_Shared_HadernSword_A_01_Montage.A_Shared_HadernSword_A_01_Montage"},
    "L2": {"type": "enemy", "source": "Brigands", "move_id": "enemy:A_BrigBase_Attack_Gap_Swings_2hit_Montage", "ability": "",
           "montage": "/Game/Sparta/Characters/Enemies/Brigands/BrigBase/Animation/Attacks/A_BrigBase_Attack_Gap_Swings_2hit_Montage.A_BrigBase_Attack_Gap_Swings_2hit_Montage"},
    "LF": {"type": "tarstone_finisher", "source": "MartyrsBlade", "move_id": "GA_Player_Attack_MartyrsBlade_B_Finisher_C",
           "ability": "GA_Player_Attack_MartyrsBlade_B_Finisher_C", "montage": "..."},
    "LC": {"type": "hold", "source": "AxeDagger", "move_id": "GA_Player_Attack_AxeDagger_A1_Hold_C",
           "ability": "GA_Player_Attack_AxeDagger_A1_Hold_C", "montage": "..."}
  },
  "heavy_chain": { "H1": {}, "H2": {}, "H3": {}, "HF": {}, "HC": {} },
  "ranged": {
    "R": {"type": "player", "source": "NailShotgun", "move_id": "ranged:AM_Player_2H_Offhand_Shoot_R_NailShotgun",
          "ability": "GA_NailShotgunAttack_Primary_C", "montage": "..."}
  }
}
```

Each entry may carry per-slot tuning, all optional: `"speed"` (play-rate multiplier, 0.5 to
2.0, default 1.0), `"feel"` (`"move"`, the default: the animation plays with its own hit windows and rules;
`"game"`: the slot's own attack with the animation fitted into it, so movement lock, combo timing,
sounds and damage stay the game's), `"hit_damage"` (only with `"feel": "move"`; (`"move"`: the animation's own hit payload, the default;
`"weapon"`: the payload of the attack this slot normally plays, copied onto the animation's hit
windows, which is the safe choice for enemy moves) and `"weapon"` (`"inventory"`, the default,
or `"move"`: the move's own weapon mesh shows in hand for the swing; available for every player
weapon and for the enemy families with a static weapon mesh). Use them deliberately: a long
enemy clip at 1.25x can become a good chain hit; a Brigand swing with `"hit_damage": "weapon"`
hits like your weapon's own attack. An entry with `"move_id": ""` keeps the weapon's own attack
and may still set speed.

The `type`, `source`, `ability` and `montage` fields are derived from the catalog; write the
`move_id` and let `fix` fill the rest, then run `check`. The example's `{}` placeholders and
`"..."` are not valid; real files contain complete entries or omit the slot.

## Deliverables

1. Fifteen files in `CCS/presets/`, all passing `check`.
2. `CCS/docs/presets-catalog.md`: one section per preset with the theme, the base weapon, a
   table of the eleven slots (slot, move id, source, why it is there, length and hit windows),
   and any slot deliberately left as the weapon's own. End with a coverage table: weapons
   used, enemy families used, presets using the ranged slot, and the pairwise overlap check.
3. `CCS/work/presets/notes.md`: the research notes and the candidate tables you built.

Report in the catalog document what you could not verify from the files alone (damage of enemy
moves on the player, how a long clip feels in play); do not present those as facts.
