# CCS preset catalog

Fifteen presets for the shipped 107 player moves, 134 enemy animations and nine sidearm
animations. Files are in [`../presets/`](../presets/); research and candidate tables are in
[`work/presets/notes.md`](../work/presets/notes.md). Some native player entries have a blank catalog source; their table cells say Unspecified
in catalog, while the move ids and design reasons identify the intended weapon. JSON keeps
the canonical blank value. The base weapon is a design recommendation,
not a verified equipment constraint. Equip the suggested sidearm when a preset replaces R.

Length is the complete montage at play rate 1.0, including recovery, not a measured input
lock. Player lengths come from the existing [`montages.jsonl`](../work/movesets/montages.jsonl)
index; enemy lengths and counts come from [`enemy-catalog.json`](../data/enemy-catalog.json).
A hit-window count is the number of trace notifies, not guaranteed hits on one target;
Scythe and dual-weapon windows can coincide. Sidearm lengths and notify counts are **unknown**
in the supplied catalogs, so they are marked rather than invented.

These are animation designs, not damage guarantees. Player move payloads usually resolve
from the equipped weapon; Black Needle light holds are an explicit flat-25 exception.
Enemy moves are treated as **visual and timing verified, damage to be confirmed**, following
the brief, without claiming a new in-game check. The player catalog's `runtime_verified`
flags are false. No enemy stats, status passives, paired grabs or AI abilities are assumed
to transfer. The planned option to force the equipped weapon's payload is not enabled or
implemented by these presets; each theme is intended to work visually under either mode.

See [player movesets](combat-movesets.md), [enemy movesets](enemy-movesets.md),
[enemy damage](enemy-damage-pipeline.md) and [damage formula](combat-damage-pipeline.md#3-damage-formula).
Weapon matching follows documented motions and grips. Exact reach, collision, form changes,
queue/cancel behavior, sidearm fire notifies and the feel of longer clips need playtesting.
No game was launched and no game files were changed.

## Brigand_Rhythm

Alternating sword cuts with separate Brigand swipe and gap-swing endings.

File: [`Brigand_Rhythm.json`](../presets/Brigand_Rhythm.json). Base weapon: **HadernSword**. Suggested sidearm: **Crossbow**. 11 of 11 slots filled.

> Made for the Hadern sword and a Crossbow, this keeps the familiar sword chains and gives their endings a Brigand rhythm. The light finisher has two swipes; the heavy finisher steps into a longer two-hit swing.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_HadernsSword_A1_C` | HadernSword | Leftward sword opening establishes the alternating rhythm. | 3.200 | 1 |
| L2 | `GA_Player_HadernsSword_A2_C` | HadernSword | Rightward return follows the left opener. | 3.100 | 1 |
| L3 | `GA_Player_HadernsSword_A3_C` | HadernSword | Downward cut ends the ordinary light sequence. | 3.233 | 1 |
| LF | `enemy:A_BrigBase_Attacks_2hit_Swipes_Montage` | Brigands | Two enemy swipes are reserved for a committed ending. | 4.900 | 2 |
| LC | `GA_Player_Attack_HadernSword_A1_Hold_C` | HadernSword | Native sword hold keeps the charging grip coherent. | 3.820 | 2 |
| H1 | `GA_Player_HadernsSword_B1_C` | HadernSword | Native forward heavy begins the heavier sequence. | 2.833 | 1 |
| H2 | `GA_Player_HadernsSword_B2_C` | HadernSword | The second sword heavy retains the ordinary grip. | 2.967 | 1 |
| H3 | `GA_Player_HadernsSword_B3_C` | HadernSword | Downward heavy closes the familiar three-step chain. | 2.700 | 1 |
| HF | `enemy:A_BrigBase_Attack_Gap_Swings_2hit_Montage` | Brigands | The five-second stepping combo belongs in the finisher. | 5.167 | 2 |
| HC | `GA_Player_Attack_HadernsSword_B1_Hold_C` | HadernSword | Two-window sword hold supplies the charged ending. | 2.765 | 2 |
| R | `ranged:A_Shells_Crossbow_Shoot_Quick_Montage` | Crossbow | Crossbow stance complements the Brigand theme. | Unknown | Unknown (shot notify) |

Left native: none.

Commitment to test: HF 5.167 s (2 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

## Needle_Hunt

Compact spear openings with a single hunter thrust and a Spear Lady leap.

File: [`Needle_Hunt.json`](../presets/Needle_Hunt.json). Base weapon: **BlackNeedle**. Suggested sidearm: **ParasiteGun**. 11 of 11 slots filled.

> Made for Black Needle and a Parasite Gun, this starts with compact thrusts before opening into hunter-style spear work. The heavy chain has one enemy thrust, while the finishers add a double thrust and a Spear Lady leap.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_BlackNeedle_A1_C` | BlackNeedle | Native forward thrust starts without a running approach. | 2.312 | 1 |
| L2 | `GA_Player_BlackNeedle_A2_C` | BlackNeedle | The shortest native Needle clip keeps the second press compact. | 1.615 | 1 |
| L3 | `GA_Player_BlackNeedle_A3_C` | BlackNeedle | Downward Needle follow-through changes the light rhythm. | 2.212 | 1 |
| LF | `enemy:A_GrishaHunter_Spear_Attack_2HitThrusts_Montage` | GrishaHunter | Two thrust windows suit the long spear silhouette. | 3.200 | 2 |
| LC | `GA_Player_Attack_BlackNeedle_A2_Hold_C` | BlackNeedle | Native three-window light hold retains the spear grip. | 3.800 | 3 |
| H1 | `GA_Player_BlackNeedle_B3_C` | BlackNeedle | Start with the shorter downward heavy rather than a run-in. | 2.567 | 1 |
| H2 | `enemy:A_GrishaHunter_Spear_Attack_Thrust_01_Montage` | GrishaHunter | One 2.933-second enemy thrust supplies the hunter accent. | 2.933 | 1 |
| H3 | `GA_Player_BlackNeedle_B2_C` | BlackNeedle | Rightward native sweep recovers from the straight thrust. | 3.397 | 1 |
| HF | `enemy:A_CultistSpearLady_Attack_StabLeap_Montage` | CultistSpearLady | The two-window leap is isolated as a finisher. | 3.900 | 2 |
| HC | `GA_Player_Attack_BlackNeedle_B2_Hold_C` | BlackNeedle | Native heavy hold adds a slower two-window release. | 5.033 | 2 |
| R | `ranged:A_Shells_2H_Offhand_ParasiteGun_Shoot` | ParasiteGun | Parasite Gun provides the suggested hunting sidearm stance. | Unknown | Unknown (shot notify) |

Left native: none.

## Champion_Hammer

Hammer overheads and MS1 hammer-derived heavies, capped by arena combinations.

File: [`Champion_Hammer.json`](../presets/Champion_Hammer.json). Base weapon: **HeavyHammer**. Suggested sidearm: **Ballistazooka**. 11 of 11 slots filled.

> Made for Heavy Hammer and a Ballistazooka, this uses overheads and hammer-derived heavy swings to lead into Dungeon Champion endings. Both finishers are multi-window attacks, so they ask for a larger opening than the ordinary chain.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_HeavyHammer_A2_C` | HeavyHammer | The shorter native hammer overhead introduces the weight. | 3.637 | 1 |
| L2 | `GA_Player_Attack_BattleAxe_A2_C` | BattleAxe | An upward two-handed shaft motion contrasts with the overhead. | 2.833 | 1 |
| L3 | `GA_Player_Attack_AxeDagger_B3_C` | Unspecified in catalog | MS1 hammer-derived downward motion returns to a crushing cut. | 2.467 | 1 |
| LF | `enemy:A_DungeonChampion_Attack_Spins_Montage` | DungeonChampion | Three-window arena spin is a deliberate finisher commitment. | 4.367 | 3 |
| LC | `GA_Player_Attack_HeavyHammer_A2_Hold_C` | HeavyHammer | Native hammer charge preserves its overhead grip. | 6.633 | 1 |
| H1 | `GA_Player_Attack_AxeDagger_B1_C` | Unspecified in catalog | Upward heavy starts a rising crushing sequence. | 3.167 | 1 |
| H2 | `GA_Player_Attack_AxeDagger_B2_C` | Unspecified in catalog | The reused MS1 hammer swing follows across the body. | 3.333 | 1 |
| H3 | `GA_Player_Attack_HeavyHammer_A2_C` | HeavyHammer | Native overhead returns the sequence to its base weapon. | 3.637 | 1 |
| HF | `enemy:A_DungeonChampion_Attack_3Hit_Combo_Montage` | DungeonChampion | The 5.733-second three-hit combo stays out of chain slots. | 5.733 | 3 |
| HC | `GA_Player_Attack_HeavyHammer_B1_Hold_C` | HeavyHammer | Native two-window heavy charge anchors the slow branch. | 7.533 | 2 |
| R | `ranged:A_Shells_Locomotion_Ballistazooka_Shoot_Stand_Montage` | Ballistazooka | Standing heavy sidearm stance supports the arena theme. | Unknown | Unknown (shot notify) |

Left native: none.

Commitment to test: HF 5.733 s (3 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

## Cultist_Procession

Long-shaft player sweeps with a torch-style ending and a ritual spear spin.

File: [`Cultist_Procession.json`](../presets/Cultist_Procession.json). Base weapon: **Scythe**. Suggested sidearm: **CursedChild**. 11 of 11 slots filled.

> Made for Clockwork Scythe and Cursed Child, this mixes shaft sweeps with a Cultist torch ending and a Spear Lady spin. The two finishers provide the ritual flourishes, while the chains stay with player weapon animations.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_ClockworkScythe_A1_C` | Scythe | Native paired-window sweep establishes the scythe grip. | 4.400 | 2 |
| L2 | `GA_Player_Attack_ClockworkScythe_A2_C` | Scythe | The second sweep continues the long-shaft movement. | 3.800 | 2 |
| L3 | `GA_Player_Attack_ClockworkScythe_A3_C` | Scythe | The third native sweep completes the light phrase. | 3.930 | 2 |
| LF | `enemy:A_CultistBase_Attacks_Torch_Attack_new_06_Montage` | CultistBase | Torch-style motion supplies a close ritual accent, with two windows. | 4.233 | 2 |
| LC | `GA_Player_Attack_ClockworkScythe_A1_Hold_C` | Scythe | Native scythe hold maintains the shaft grip. | 4.800 | 4 |
| H1 | `GA_Player_BlackNeedle_B1_C` | BlackNeedle | Leftward polearm sweep begins the wider heavy phrase. | 3.793 | 1 |
| H2 | `GA_Player_BlackNeedle_B2_C` | BlackNeedle | Rightward polearm return contrasts with the first sweep. | 3.397 | 1 |
| H3 | `GA_Player_Attack_ClockworkScythe_B3_C` | Scythe | Native rightward heavy ends with the scythe silhouette. | 3.153 | 2 |
| HF | `enemy:A_CultistSpearLady_Attack_Spins_03_Montage` | CultistSpearLady | Four-window spear spin is confined to the heavy finisher. | 5.300 | 4 |
| HC | `GA_Player_Attack_ClockworkScythe_B3_Hold_C` | Scythe | Native four-window hold is the slower charged option. | 4.967 | 4 |
| R | `ranged:A_Shared_CursedChild_Shot_Montage` | CursedChild | Cursed Child stance supplies the suggested ritual sidearm. | Unknown | Unknown (shot notify) |

Left native: none.

Commitment to test: HF 5.300 s (4 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

The Cultist torch motion is reinterpreted on a long shaft. Hand placement and weapon contact remain a visual design risk to check.

## Bone_Morningstar

Alternating shaft strikes with Draugr overhead and three-hit mace endings.

File: [`Bone_Morningstar.json`](../presets/Bone_Morningstar.json). Base weapon: **BattleAxe**. Suggested sidearm: **Trebuchaxe**. 11 of 11 slots filled.

> Made for Battle Axe and Trebuchaxe, this alternates rising and crossing swings before a downward sword cut. Draugr endings give the light and heavy finishers a mace-like rhythm without filling the ordinary chains with enemy lunges.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_BattleAxe_A2_C` | BattleAxe | An upward axe cut gives this opener its own contour. | 2.833 | 1 |
| L2 | `GA_Player_Attack_BattleAxe_A3_C` | BattleAxe | Rightward axe cut follows the upward opening. | 3.367 | 1 |
| L3 | `GA_Player_HadernsSword_A3_C` | HadernSword | Downward two-handed sword motion completes the shaft sequence. | 3.233 | 1 |
| LF | `enemy:A_Draugr_Attack_Overhead_Montage` | Draugr | One-window mace overhead is the restrained enemy ending. | 4.133 | 1 |
| LC | `GA_Player_Attack_BattleAxe_A2_Hold_C` | BattleAxe | Native axe hold matches the rising light opener. | 6.100 | 2 |
| H1 | `GA_Player_Attack_BattleAxe_B2_C` | BattleAxe | Leftward heavy starts the wider branch. | 4.323 | 1 |
| H2 | `GA_Player_Attack_HeavyHammer_A2_C` | HeavyHammer | A hammer overhead adds the blunt-motion accent. | 3.637 | 1 |
| H3 | `GA_Player_Attack_BattleAxe_B3_C` | BattleAxe | Upward axe heavy contrasts with the preceding overhead. | 4.532 | 1 |
| HF | `enemy:A_Draugr_Attack_3hit_Montage` | Draugr | Three-window Draugr clip gives the heavy finisher a longer phrase. | 4.267 | 3 |
| HC | `GA_Player_Attack_BattleAxe_B1_Hold_C` | BattleAxe | Native axe heavy hold retains the expected two-handed grip. | 6.600 | 2 |
| R | `ranged:A_Shells_Locomotion_TrebuchAxe_Shoot_Big_Montage` | Trebuchaxe | The axe-themed sidearm stance supports the base weapon. | Unknown | Unknown (shot notify) |

Left native: none.

## Twin_Sisters

Dual-weapon light cuts with compact Twin Sister flurries at both endings.

File: [`Twin_Sisters.json`](../presets/Twin_Sisters.json). Base weapon: **AxeDagger**. Suggested sidearm: **Crossbow**. 11 of 11 slots filled.

> Made for Axe & Dagger and a Crossbow, this keeps the dual-weapon light chain and places Twin Sister flurries in the finishers. The light ending has two windows; the heavy ending has four, followed by native player holds for charging.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_AxeDagger_A1_C` | Unspecified in catalog | Native leftward dual-weapon opener fits the equipment. | 2.400 | 1 |
| L2 | `GA_Player_Attack_AxeDagger_A2_C` | Unspecified in catalog | Rightward return keeps both weapons in their native pattern. | 2.400 | 1 |
| L3 | `GA_Player_Attack_AxeDagger_A3_C` | Unspecified in catalog | Forward dual-weapon motion closes the ordinary light string. | 2.667 | 1 |
| LF | `enemy:A_TwinSisters_01_Duel_Combat__combo_daggers_2hits_Montage` | MS1 | A 2.7-second dagger pairing suits the small offhand blade. | 2.700 | 2 |
| LC | `GA_Player_Attack_AxeDagger_A2_Hold_C` | AxeDagger | Native three-window charge keeps the dual grip. | 2.817 | 3 |
| H1 | `GA_Player_Attack_AxeDagger_B1_C` | Unspecified in catalog | Native upward heavy begins the slower branch. | 3.167 | 1 |
| H2 | `GA_Player_Attack_AxeDagger_B2_C` | Unspecified in catalog | The shipped hammer-derived heavy retains the base moveset. | 3.333 | 1 |
| H3 | `GA_Player_Attack_AxeDagger_B3_C` | Unspecified in catalog | Native downward heavy closes the branch. | 2.467 | 1 |
| HF | `enemy:A_TwinSisters_Attack_Combo_02_Montage` | MS1 | Four-window Twin Sister combo is a separate ending. | 3.167 | 4 |
| HC | `GA_Player_Attack_AxeDagger_B2_Hold_C` | AxeDagger | Two-window native heavy hold keeps the charge coherent. | 3.333 | 2 |
| R | `ranged:A_Shells_Crossbow_Shoot_Quick_Montage` | Crossbow | Crossbow stance echoes the Twin Sister weapon pairing. | Unknown | Unknown (shot notify) |

Left native: none.

## Reaper_Nobility

Reordered scythe phrases with increasingly committed Aristocrat endings.

File: [`Reaper_Nobility.json`](../presets/Reaper_Nobility.json). Base weapon: **Scythe**. 10 of 11 slots filled.

> Made for Clockwork Scythe, this reorders scythe sweeps around an Aristocrat overhead ending. The heavy finisher is a deliberate 7.633-second four-window combo; reserve it for a large opening.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_ClockworkScythe_A3_C` | Scythe | Start with the third sweep for a different opening beat. | 3.930 | 2 |
| L2 | `GA_Player_BlackNeedle_B1_C` | BlackNeedle | Leftward long-shaft sweep broadens the middle beat. | 3.793 | 1 |
| L3 | `GA_Player_Attack_ClockworkScythe_A2_C` | Scythe | The second native sweep resolves the reordered phrase. | 3.800 | 2 |
| LF | `enemy:A_Aristocrat_Attack_PunchOverhead_Montage` | Aristocrat | Two-window overhead provides the shorter noble ending. | 4.033 | 2 |
| LC | `GA_Player_Attack_ClockworkScythe_A3_Hold_C` | Scythe | Native charged third sweep matches the reordered opening. | 5.633 | 4 |
| H1 | `GA_Player_Attack_ClockworkScythe_B3_C` | Scythe | The third heavy opens with the scythe rightward cut. | 3.153 | 2 |
| H2 | `GA_Player_Attack_ClockworkScythe_B1_C` | Scythe | First native heavy changes the direction downward. | 3.633 | 2 |
| H3 | `GA_Player_Attack_ClockworkScythe_B2_C` | Scythe | Second native heavy completes the deliberate reordered sequence. | 3.789 | 2 |
| HF | `enemy:A_Aristocrat_Attack_Combo_02_Montage` | Aristocrat | Long four-window scythe combo is used only as a finisher. | 7.633 | 4 |
| HC | `GA_Player_Attack_ClockworkScythe_B2_Hold_C` | Scythe | Native heavy hold provides a distinct charged closing motion. | 5.367 | 4 |
| R | Weapon's own | Equipped sidearm | Keep the equipped sidearm animation; the theme concerns the scythe. | Not selected | Not selected |

Left native: R: Keep the equipped sidearm animation; the theme concerns the scythe.

Commitment to test: HF 7.633 s (4 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

## Tarred_Executioner

A shorter greatsword opener with the player-adapted Tarred Corpse endings.

File: [`Tarred_Executioner.json`](../presets/Tarred_Executioner.json). Base weapon: **MartyrsBlade**. 10 of 11 slots filled.

> Made for the Great Martyr's Blade, this opens with its shorter third light cut and follows with deliberate sword work. Tarred Corpse player-adapted clips provide an overhead light finisher and a three-window heavy finisher.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_MartyrsBlade_A3_C` | MartyrsBlade | The shortest native greatsword light is used as the opener. | 1.867 | 1 |
| L2 | `GA_Player_HadernsSword_B1_C` | HadernSword | Forward sword heavy keeps the long blade in a two-handed motion. | 2.833 | 1 |
| L3 | `GA_Player_Attack_MartyrsBlade_A1_C` | MartyrsBlade | Broad native greatsword cut closes the light branch. | 2.500 | 1 |
| LF | `enemy:A_TarredCorpse_Attack_Overhead_Player_Montage` | TarredCorpse | The three-second player-adapted overhead avoids a long enemy combo. | 3.000 | 1 |
| LC | `GA_Player_Attack_MartyrsBlade_A2_Hold_C` | MartyrsBlade | Native charged greatsword motion keeps the base grip. | 3.933 | 1 |
| H1 | `GA_Player_Attack_MartyrsBlade_B2_C` | Unspecified in catalog | Second greatsword heavy becomes the opening heavy beat. | 3.433 | 1 |
| H2 | `GA_Player_Attack_MartyrsBlade_B1_C` | Unspecified in catalog | First native heavy keeps the broad blade movement. | 3.533 | 1 |
| H3 | `GA_Player_Attack_MartyrsBlade_B3_C` | Unspecified in catalog | Rightward third heavy changes direction at the end. | 3.633 | 1 |
| HF | `enemy:A_TarredCorpse_Attack_3hit_Player_Montage` | TarredCorpse | The 3.25-second player-adapted clip supplies three windows. | 3.250 | 3 |
| HC | `GA_Player_Attack_MartyrsBlade_B1_Hold_C` | MartyrsBlade | The resolved native heavy hold supplies two charged windows. | 5.033 | 2 |
| R | Weapon's own | Equipped sidearm | Preserve the sidearm animation to keep the preset focused on sword work. | Not selected | Not selected |

Left native: R: Preserve the sidearm animation to keep the preset focused on sword work.

## Marsh_Maul

Crossing hammer movements that end in single-window FrogMama slams.

File: [`Marsh_Maul.json`](../presets/Marsh_Maul.json). Base weapon: **HeavyHammer**. Suggested sidearm: **NailShotgun**. 11 of 11 slots filled.

> Made for Heavy Hammer and a Nail Shotgun, this mixes crossing shaft swings with familiar overheads. FrogMama supplies the slow forward slam and overhead finishers, each with one hit window rather than a rapid flurry.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_AxeDagger_B2_C` | Unspecified in catalog | The MS1 hammer-derived crossing swing changes the opening. | 3.333 | 1 |
| L2 | `GA_Player_Attack_HeavyHammer_A2_C` | HeavyHammer | Native overhead follows the crossing motion. | 3.637 | 1 |
| L3 | `GA_Player_Attack_BattleAxe_A3_C` | BattleAxe | Rightward shaft cut changes direction after the overhead. | 3.367 | 1 |
| LF | `enemy:A_FrogMama_Attack_Slam_Fwd_Montage` | FrogMama | A 5.6-second single-window slam is confined to the finisher. | 5.600 | 1 |
| LC | `GA_Player_Attack_HeavyHammer_A3_Hold_C` | HeavyHammer | Native third overhead hold keeps the crushing theme. | 6.600 | 1 |
| H1 | `GA_Player_Attack_HeavyHammer_A2_C` | HeavyHammer | Native overhead starts the heavy branch from a familiar pose. | 3.637 | 1 |
| H2 | `GA_Player_Attack_BattleAxe_B2_C` | BattleAxe | Leftward shaft swing broadens the heavy pattern. | 4.323 | 1 |
| H3 | `GA_Player_Attack_AxeDagger_B3_C` | Unspecified in catalog | Hammer-derived downward motion ends the sequence. | 2.467 | 1 |
| HF | `enemy:A_FrogMama_Attack_Overhead_Montage` | FrogMama | The 5.833-second overhead is a committed heavy ending. | 5.833 | 1 |
| HC | `GA_Player_Attack_HeavyHammer_B3_Hold_C` | HeavyHammer | Three-window native heavy hold supplies the multi-impact charge. | 7.033 | 3 |
| R | `ranged:A_Shells_NailShotgun_Shoot_Quick_Montage` | NailShotgun | Quick shotgun stance is the suggested close-range sidearm option. | Unknown | Unknown (shot notify) |

Left native: none.

Commitment to test: LF 5.600 s (1 windows), HF 5.833 s (1 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

## Quarry_Breaker

Reordered axe cuts and descending shaft swings, ending with Miner strikes.

File: [`Quarry_Breaker.json`](../presets/Quarry_Breaker.json). Base weapon: **BattleAxe**. 10 of 11 slots filled.

> Made for Battle Axe, this reorders its cuts around downward shaft swings. Miner finishers are deliberate commitments: the light ending is a 6.667-second single strike, and the heavy ending is a six-second two-window combo.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_BattleAxe_A3_C` | BattleAxe | Rightward axe cut makes a distinct opening. | 3.367 | 1 |
| L2 | `GA_Player_HadernsSword_A3_C` | HadernSword | Downward sword motion is a shaft-compatible design contrast. | 3.233 | 1 |
| L3 | `GA_Player_Attack_BattleAxe_A2_C` | BattleAxe | Upward native axe cut recovers from the downward middle. | 2.833 | 1 |
| LF | `enemy:A_Miner_Attacks_Attack_01_Montage` | Miner | Long single-window pick strike is isolated as an ending. | 6.667 | 1 |
| LC | `GA_Player_Attack_BattleAxe_A3_Hold_C` | BattleAxe | Native third light hold follows the opening axe choice. | 5.367 | 2 |
| H1 | `GA_Player_Attack_BattleAxe_B3_C` | BattleAxe | Upward native heavy starts the heavier phrase. | 4.532 | 1 |
| H2 | `GA_Player_Attack_AxeDagger_B3_C` | Unspecified in catalog | MS1 hammer-derived downstroke suits a mining-shaft accent. | 2.467 | 1 |
| H3 | `GA_Player_Attack_BattleAxe_B2_C` | BattleAxe | Leftward native heavy completes the crossing return. | 4.323 | 1 |
| HF | `enemy:A_Miner_Attacks_Combo_05_Montage` | Miner | Two-window six-second combo avoids the longer seven-hit Miner clip. | 6.000 | 2 |
| HC | `GA_Player_Attack_BattleAxe_B2_Hold_C` | BattleAxe | Native second heavy hold keeps the axe charging grip. | 6.700 | 2 |
| R | Weapon's own | Equipped sidearm | The mining theme does not require a different sidearm stance. | Not selected | Not selected |

Left native: R: The mining theme does not require a different sidearm stance.

Commitment to test: LF 6.667 s (1 windows), HF 6.000 s (2 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

## Sicario_Crosscut

Twin-katana cuts with compact dual-weapon and assassin endings.

File: [`Sicario_Crosscut.json`](../presets/Sicario_Crosscut.json). Base weapon: **Axatana**. Use twin-katana form. Suggested sidearm: **NailShotgun**. 11 of 11 slots filled.

> Made for Axatana in twin-katana form and a Nail Shotgun, this keeps alternating blade cuts and gives both finishers multi-window flurries. It uses Sicario and Twin Sister clips instead of the fifteen-second Sicario combo.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_Katanas_A1_C` | Axatana | Native leftward katana opening fits the requested form. | 2.427 | 1 |
| L2 | `GA_Player_Attack_Katanas_A2_C` | Axatana | Native rightward return keeps the alternating blades. | 2.779 | 1 |
| L3 | `GA_Player_Attack_Katanas_A3_C` | Axatana | Paired third-light windows retain both katana traces. | 2.433 | 2 |
| LF | `enemy:A_Sicario_Test_Attack_2Hit_Swing_Montage` | Sicario | The 4.167-second test swing has six cataloged trace windows. | 4.167 | 6 |
| LC | `GA_Player_Attack_Katanas_A1_Hold_C` | Axatana | Native four-window katana hold preserves the twin grip. | 2.933 | 4 |
| H1 | `GA_Player_Attack_AxeDagger_A3_C` | Unspecified in catalog | Forward dual-weapon motion gives the heavy opener a different beat. | 2.667 | 1 |
| H2 | `GA_Player_Attack_Katanas_A1_C` | Axatana | Return to the native leftward katana cut. | 2.427 | 1 |
| H3 | `GA_Player_Attack_Katanas_A2_C` | Axatana | Native rightward cut resolves the shorter blade phrase. | 2.779 | 1 |
| HF | `enemy:A_TwinSisters_Attack_Combo_03_Montage` | MS1 | Four-window Twin Sister phrase suits the twin-blade silhouette. | 3.567 | 4 |
| HC | `GA_Player_Attack_Katanas_A2_Hold_C` | Axatana | A second native katana hold avoids an axe-form charge. | 3.767 | 4 |
| R | `ranged:AM_Player_2H_Offhand_Shoot_R_NailShotgun` | NailShotgun | Regular shotgun stance differs from the quick maul-sidearm option. | Unknown | Unknown (shot notify) |

Left native: none.

The Sicario asset is labeled Test and has six trace windows despite its 2Hit name. Its final pose, dual-weapon traces and transitions still need inspection in play.

## Pilgrim_Staff

A reordered spear sequence with torch and spinning spear endings.

File: [`Pilgrim_Staff.json`](../presets/Pilgrim_Staff.json). Base weapon: **BlackNeedle**. 10 of 11 slots filled.

> Made for Black Needle, this treats the shaft as a pilgrim staff with reordered thrusts and wider heavy sweeps. A close Cultist torch motion and a Spear Lady spin supply the endings, with one hunter thrust in the heavy chain.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_BlackNeedle_A2_C` | BlackNeedle | The shorter second thrust becomes the opening. | 1.615 | 1 |
| L2 | `GA_Player_BlackNeedle_A3_C` | BlackNeedle | Downward native Needle motion changes the middle beat. | 2.212 | 1 |
| L3 | `GA_Player_BlackNeedle_A1_C` | BlackNeedle | Forward first thrust becomes the closing light beat. | 2.312 | 1 |
| LF | `enemy:A_CultistBase_Attacks_Torch_Attack_new_06_Montage` | CultistBase | Two-window close torch motion provides the pilgrim accent. | 4.233 | 2 |
| LC | `GA_Player_Attack_BlackNeedle_A1_Hold_C` | BlackNeedle | Native first light hold supplies three spear windows. | 4.500 | 3 |
| H1 | `GA_Player_BlackNeedle_B2_C` | BlackNeedle | Rightward native sweep opens the wide branch. | 3.397 | 1 |
| H2 | `GA_Player_BlackNeedle_B3_C` | BlackNeedle | Downward native heavy changes the direction. | 2.567 | 1 |
| H3 | `enemy:A_GrishaHunter_Spear_Attack_Thrust_01_Montage` | GrishaHunter | A single enemy thrust closes rather than opens this heavy chain. | 2.933 | 1 |
| HF | `enemy:A_CultistSpearLady_Attack_Spins_03_Montage` | CultistSpearLady | The longer four-window spear spin remains a finisher. | 5.300 | 4 |
| HC | `GA_Player_Attack_BlackNeedle_B1_Hold_C` | BlackNeedle | Native first heavy hold keeps the charging shaft grip. | 5.300 | 2 |
| R | Weapon's own | Equipped sidearm | Leave the chosen sidearm animation alone for the staff-focused theme. | Not selected | Not selected |

Left native: R: Leave the chosen sidearm animation alone for the staff-focused theme.

Commitment to test: HF 5.300 s (4 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

The Cultist torch motion is reinterpreted on a long shaft. Hand placement and weapon contact remain a visual design risk to check.

## Grave_Swordsman

Sword cuts with different opener directions and two restrained undead endings.

File: [`Grave_Swordsman.json`](../presets/Grave_Swordsman.json). Base weapon: **HadernSword**. Suggested sidearm: **SimpleLute**. 11 of 11 slots filled.

> Made for the Hadern sword and a Simple Lute, this opens with a rightward cut and reverses the heavy sequence. Draugr swipes and a Tarred Corpse overhead give the two finishers an undead rhythm without a paired grab.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_HadernsSword_A2_C` | HadernSword | Rightward sword cut reverses the usual opening direction. | 3.100 | 1 |
| L2 | `GA_Player_Attack_MartyrsBlade_A3_C` | MartyrsBlade | Short forward greatsword cut supplies a contrasting middle. | 1.867 | 1 |
| L3 | `GA_Player_HadernsSword_B3_C` | HadernSword | Downward native sword heavy resolves the light sequence. | 2.700 | 1 |
| LF | `enemy:A_Draugr_Attacks_2hit_Swipes_Montage` | Draugr | Two undead swipes create a separate longer ending. | 4.900 | 2 |
| LC | `GA_Player_Attack_HadernSword_A2_Hold_C` | HadernSword | Native second sword hold follows the reordered opener. | 3.940 | 2 |
| H1 | `GA_Player_HadernsSword_B2_C` | HadernSword | Start with the second native heavy for a changed contour. | 2.967 | 1 |
| H2 | `GA_Player_HadernsSword_B3_C` | HadernSword | Downward heavy follows the first forward beat. | 2.700 | 1 |
| H3 | `GA_Player_HadernsSword_B1_C` | HadernSword | First heavy becomes the final forward return. | 2.833 | 1 |
| HF | `enemy:A_TarredCorpse_Attack_OVerhead_Montage` | TarredCorpse | One-window enemy overhead supplies the heavier undead ending. | 4.133 | 1 |
| HC | `GA_Player_Attack_HadernsSword_B2_Hold_C` | HadernSword | Native second heavy hold supplies the charged sword branch. | 3.036 | 2 |
| R | `ranged:A_Shells_Locomotion_Lute_Chord_Montage` | SimpleLute | Lute chord is the suggested grave-procession sidearm stance. | Unknown | Unknown (shot notify) |

Left native: none.

## Axe_Conversion

Axatana axe-form heavies distributed across both chains, with axe-compatible endings.

File: [`Axe_Conversion.json`](../presets/Axe_Conversion.json). Base weapon: **Axatana**. Use axe form. Suggested sidearm: **MachineGun**. 11 of 11 slots filled.

> Made for Axatana in axe form and a Machine Gun, this distributes its axe heavies across both chains with Battle Axe cuts between them. A Brigand axe phrase ends the light chain, while a player axe finisher closes the heavy chain.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_BattleAxe_A2_C` | BattleAxe | Upward shaft cut starts without selecting a katana animation. | 2.833 | 1 |
| L2 | `GA_Player_Attack_Axatana_Axe_B1_C` | Axatana | First native Axatana axe heavy adds the axe-form movement. | 4.417 | 1 |
| L3 | `GA_Player_Attack_Axatana_Axe_B3_C` | Axatana | Downward native axe heavy ends the light sequence. | 4.833 | 1 |
| LF | `enemy:A_BrigElite_Attack_Swipe_BBB_Montage` | Brigands | Three-window Brigand axe clip supplies the enemy ending. | 5.400 | 3 |
| LC | `GA_Player_Attack_BattleAxe_A1_Hold_C` | BattleAxe | Player axe hold avoids unavailable axe-form light abilities. | 6.400 | 2 |
| H1 | `GA_Player_Attack_Axatana_Axe_B2_C` | Axatana | Second native axe heavy becomes the heavy opener. | 3.933 | 1 |
| H2 | `GA_Player_Attack_BattleAxe_A3_C` | BattleAxe | Rightward Battle Axe motion changes the middle direction. | 3.367 | 1 |
| H3 | `GA_Player_Attack_Axatana_Axe_B1_C` | Axatana | First native axe heavy returns at the end. | 4.417 | 1 |
| HF | `GA_Player_Attack_BattleAxe_B_Finisher_C` | BattleAxe | Resolved player axe finisher provides a shorter ending. | 3.800 | 1 |
| HC | `GA_Player_Attack_Axatana_Axe_B3_Hold_C` | Axatana | Native axe-form charge keeps the chosen form coherent. | 5.240 | 2 |
| R | `ranged:A_Shared_Actions_MachineGun_Recoils_04_Montage` | MachineGun | Machine Gun recoil is the suggested ranged contrast. | Unknown | Unknown (shot notify) |

Left native: none.

Commitment to test: LF 5.400 s (3 windows). These whole clips occur only in finisher slots; their practical lock time is not verified.

## Martyr_Vigil

A player-only greatsword study that separates measured cuts from long charged endings.

File: [`Martyr_Vigil.json`](../presets/Martyr_Vigil.json). Base weapon: **MartyrsBlade**. 10 of 11 slots filled.

> Made for the Great Martyr's Blade, this keeps a measured sword rhythm using player animations throughout. Reordered cuts lead to the resolved Martyr finishers, with two distinct sword holds for the charged slots.

| Slot | Move id | Source | Why it is there | Length s | Hit windows |
| --- | --- | --- | --- | --- | --- |
| L1 | `GA_Player_Attack_MartyrsBlade_A1_C` | MartyrsBlade | Native broad greatsword cut introduces the blade weight. | 2.500 | 1 |
| L2 | `GA_Player_Attack_MartyrsBlade_A3_C` | MartyrsBlade | Shorter third light gives the middle a compact response. | 1.867 | 1 |
| L3 | `GA_Player_HadernsSword_B2_C` | HadernSword | Forward two-handed sword heavy adds a deliberate closing beat. | 2.967 | 1 |
| LF | `GA_Player_Attack_MartyrsBlade_A_Finisher_C` | MartyrsBlade | Resolved native greatsword finisher keeps the sword silhouette. | 3.467 | 1 |
| LC | `GA_Player_Attack_MartyrsBlade_A1_Hold_C` | MartyrsBlade | Native first light hold provides a slower downward charge. | 4.533 | 1 |
| H1 | `GA_Player_Attack_MartyrsBlade_B3_C` | Unspecified in catalog | Rightward third heavy is moved to the opening. | 3.633 | 1 |
| H2 | `GA_Player_HadernsSword_B1_C` | HadernSword | Shorter forward sword heavy gives the middle a different pace. | 2.833 | 1 |
| H3 | `GA_Player_Attack_MartyrsBlade_B2_C` | Unspecified in catalog | Second native greatsword heavy closes the phrase. | 3.433 | 1 |
| HF | `GA_Player_Attack_MartyrsBlade_B_Finisher_C` | MartyrsBlade | Resolved native heavy finisher avoids enemy payload uncertainty here. | 4.933 | 1 |
| HC | `GA_Player_Attack_HadernsSword_B3_Hold_C` | HadernSword | Two-window sword hold avoids the unresolved Martyr heavy third hold. | 4.808 | 2 |
| R | Weapon's own | Equipped sidearm | Preserve the equipped sidearm; this is the player-only sword comparison. | Not selected | Not selected |

Left native: R: Preserve the equipped sidearm; this is the player-only sword comparison.

## Coverage and validation

| Preset | Base weapon | Player weapon sources | Enemy families | Filled | R / recommended sidearm |
| --- | --- | --- | --- | --- | --- |
| Brigand_Rhythm | HadernSword | HadernSword | Brigands | 11/11 | Crossbow |
| Needle_Hunt | BlackNeedle | BlackNeedle | CultistSpearLady, GrishaHunter | 11/11 | ParasiteGun |
| Champion_Hammer | HeavyHammer | BattleAxe, HeavyHammer | DungeonChampion | 11/11 | Ballistazooka |
| Cultist_Procession | Scythe | BlackNeedle, Scythe | CultistBase, CultistSpearLady | 11/11 | CursedChild |
| Bone_Morningstar | BattleAxe | BattleAxe, HadernSword, HeavyHammer | Draugr | 11/11 | Trebuchaxe |
| Twin_Sisters | AxeDagger | AxeDagger | MS1 | 11/11 | Crossbow |
| Reaper_Nobility | Scythe | BlackNeedle, Scythe | Aristocrat | 10/11 | Native |
| Tarred_Executioner | MartyrsBlade | HadernSword, MartyrsBlade | TarredCorpse | 10/11 | Native |
| Marsh_Maul | HeavyHammer | BattleAxe, HeavyHammer | FrogMama | 11/11 | NailShotgun |
| Quarry_Breaker | BattleAxe | BattleAxe, HadernSword | Miner | 10/11 | Native |
| Sicario_Crosscut | Axatana | Axatana | MS1, Sicario | 11/11 | NailShotgun |
| Pilgrim_Staff | BlackNeedle | BlackNeedle | CultistBase, CultistSpearLady, GrishaHunter | 10/11 | Native |
| Grave_Swordsman | HadernSword | HadernSword, MartyrsBlade | Draugr, TarredCorpse | 11/11 | SimpleLute |
| Axe_Conversion | Axatana | Axatana, BattleAxe | Brigands | 11/11 | MachineGun |
| Martyr_Vigil | MartyrsBlade | HadernSword, MartyrsBlade | None (player-only comparison) | 10/11 | Native |

| Collection requirement | Result |
| --- | --- |
| Exactly 15 preset files | 15 |
| At least 8 player weapon sources | 8: Axatana, AxeDagger, BattleAxe, BlackNeedle, HadernSword, HeavyHammer, MartyrsBlade, Scythe |
| At least 12 enemy families | 12: Aristocrat, Brigands, CultistBase, CultistSpearLady, Draugr, DungeonChampion, FrogMama, GrishaHunter, MS1, Miner, Sicario, TarredCorpse |
| R in at least 6 presets | 10 presets, 8 sidearm sources |
| Every preset fills at least 8 slots | Minimum 10/11 |
| At least 5 presets fill all 11 | 10 |
| No pair shares more than 3 identical slot assignments | 105 pairs checked; maximum 2 |
| Every selected melee/charge/finisher has hit windows | All positive; no loops, paired victims or zero-window spells |
| Chains avoid long enemy combos | Only two enemy chain placements, both 2.933 s standalone spear thrusts |
| Canonical tool check | `15 file(s), 0 problem(s)` |

Overlap means the same **slot and move id** in two presets. Sharing a move in different slots
is not an identical assignment; omitted native slots are not assignments. All 105 comparisons,
the exact shared slots and final file hashes are in [`validation.json`](../work/presets/validation.json).
The matrix below provides the complete counts; its diagonal is each preset's filled count.

| # / preset | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1. Brigand_Rhythm | 11 | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2. Needle_Hunt | 0 | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 3. Champion_Hammer | 0 | 0 | 11 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 4. Cultist_Procession | 0 | 0 | 0 | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| 5. Bone_Morningstar | 1 | 0 | 0 | 0 | 11 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |
| 6. Twin_Sisters | 1 | 0 | 2 | 0 | 0 | 11 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| 7. Reaper_Nobility | 0 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 8. Tarred_Executioner | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 9. Marsh_Maul | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 11 | 0 | 0 | 0 | 0 | 0 | 0 |
| 10. Quarry_Breaker | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 | 0 | 0 |
| 11. Sicario_Crosscut | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 11 | 0 | 0 | 0 | 0 |
| 12. Pilgrim_Staff | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 10 | 0 | 0 | 0 |
| 13. Grave_Swordsman | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 11 | 0 | 1 |
| 14. Axe_Conversion | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 11 | 0 |
| 15. Martyr_Vigil | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 10 |

Validation commands, from `CustomShellSystem/`:

```sh
python3 CCS/tools/preset_check.py fix CCS/presets/*.json
python3 CCS/tools/preset_check.py check CCS/presets/*.json
PYTHONDONTWRITEBYTECODE=1 python3 CCS/work/presets/report.py
```

The same tool commands were run from `CCS/` as `python3 tools/preset_check.py ... presets/*.json`.
Their output is retained in [`fix.log`](../work/presets/fix.log) and [`check.log`](../work/presets/check.log).
The collection assertions additionally verify names, descriptions under 400 characters, positive
melee windows, resolved ids, canonical fields, source coverage, filled counts and all pair overlaps.

Still unverified: enemy payload damage on the player, inherited hit shapes, practical cancel
and queue behavior, motion-warp targeting, sidearm projectile spawning and animation transitions.
Player targeting warps are retained throughout the ordinary chains; the one-enemy-move limit
counts deliberate enemy substitutions, not every short targeting notify. The longest enemy
finisher is Reaper_Nobility's 7.633 s combo. Treat that and the Miner/FrogMama endings as
committed designs to test, not measured claims about control or combat effectiveness.
