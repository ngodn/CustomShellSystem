# Mortal Shell II combat system: weapons, seals, sidearms, shells

Written 15 September 2026 for mod development. Two kinds of evidence are separated throughout:

- **Game data**: read directly from the installed build (`MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`, UE 5.6, Steam build with the 15 September hotfix). Item definitions, weapon actors, attribute tables, ability defaults, upgrade tables and the English string table were exported with CUE4Parse plus the UE4SS usmap. Class names in this document (`ID_*`, `WP_*`, `GA_*`, `GE_*`, `DT_*`) are real asset names you can open the same way. Numbers from game data are the cooked defaults; live values can be modified by level, Tarstones, shell skills and patches applied through effects.
- **Web**: official Steam patch posts, the Cold Symmetry AMA of 14 August 2026, Fextralife, Game8, GameSpot, GameRant and other guides. Web claims are marked where they are single-source or in conflict. Two companion files hold the raw research with per-claim URLs: `work/research/research-seals-mechanics.md` and `work/research/research-weapons-shells.md`.

Where game data and the web disagree, game data wins and the disagreement is noted.

## 1. The loop in one page

- There is no stamina. Attacks, dodges and sprint cost nothing. The constraint is animation commitment: you cannot cancel a swing (official store copy and reviews).
- **Resolve** is the single resource. The Harbinger attribute set starts at `MaxResolve = 100`, `Resolve = 100`, `BaseResolveGain = 0.1` (`DT_PlayerAttributes`). Resolve is gained by landing melee hits (tutorial text: "Firing a sidearm costs Resolve, which regenerates when landing melee hits") and from resolve effigies in the world. It is spent on sidearm shots, weapon abilities, shell abilities, Infusion Tarstones and Ability Tarstones. No passive regeneration (web, consistent).
- **Break** (yellow bar under enemy health) is filled by Perfect Guard, Perfect Harden, Parry, Slayer Punch, sidearms under the Slayer Seal, Trauma status, Eredrim abilities and some Tarstones. A broken enemy shows a red dot; Light Attack on it performs a **Riposte** (`GA_Parry_Riposte`: 100% of the instigating weapon's damage, invulnerable through the animation since the Week 1 update, damage scales with enemy health since Week 1).
- **Stagger** (poise) is a separate stat: `Poise = 5` on the player; heavy hits, Shattering Stones and shell abilities add Stagger Damage.
- **Guard meter** exists only for the Untarnished Seal. Holding guard drains it and slows movement; empty meter stuns you; it recharges when released (tutorial strings `BlockLargeTut_2..4`).
- **Shells** are bodies with their own health (`MaxShellHealth`), ability, passive and 12-node skill tree. Losing all shell health severs you into the Harbinger (base `MaxHealth = 50`); melee damage refills the Shell Revive meter shown on the Seal.
- Loadout is one primary weapon, one sidearm, one seal, one shell, up to 10 Tarstones (4 shell, 2 Combat, 2 Infusion, 2 Ability) and PP items. Weapons and sidearms are not tied to shells (dev AMA).

## 2. Seals

Seals are `ID_Seal_*` items under `Sparta/Items/Seals`. Each spawns its own actor (`WP_TarnishedSeal`, `WP_StoneSeal`, `WP_InfiniteSeal`, `WP_SlayerSeal`, all `BaseDamage = 100`) and grants `Weapon.Seal.<Name>` tags. One is equipped at a time via `BP_Behavior_EquipSeal`. Seals are not upgradeable. There are exactly four.

| Seal | Ability | Tag | Obtained (web) | Data |
| --- | --- | --- | --- | --- |
| Untarnished Seal | Guard / Perfect Guard | `Weapon.Seal.Default` | Prologue with Harros | `GA_ActiveBlock`: `RequiredActivationTime 0.3`, `PerfectBlockInvulnerbilityDuration 0.36`, perfect-block cooldown effect `0.6 s`, knockback normal 800 / perfect 300 |
| Vatra's Seal | Harden / Perfect Harden | `Weapon.Seal.Stone` | Tar Golem in Disciple's Grotto | `GA_Harden_Original`: `StoneFormCooldown 5.0`, `StoneFormDurationAfterHit 1.0`, harden hit `PoiseDamage 50`, `PerfectStoneFormDuration 0.25`, perfect poise 100, `PerfectStoneFormBreakDamage 25`, `PerfectStoneFormCooldown 0.19` |
| Infinite Seal | Parry | `Weapon.Seal.Infinite` | Genessa's training at Marrow Keep (3 guards, 1 riposte) | `GA_Parry_Handler`, `GA_Parry_Action_Single`; `GE_ParrySafety` grants `GameplayEffect.Immunity.Damage`; `GE_Unlock_Parry_Medium` and `GE_Unlock_Parry_Long` widen the window (granted by Eredrim's and Tiel's Seal Affinity skills) |
| Slayer Seal | Gloomslayer + Slayer Punch | `Weapon.Seal.Slayer` | Throne outside Marrow Keep | `GA_SlayerSeal_Charge`: `CooldownDuration 15`; `GA_SlayerSeal_Attack` payload 100 damage, 100 poise, 100 break, warp to target up to 1500 units; disables achievements on the save (in-game warning strings) |

In-game descriptions (`ST_Core_Seals`):

- Guard: "Block incoming attacks with your Weapon. Time it right to Perfect Guard and deal Break Damage."
- Harden: "Harden your body to withstand the next attack damage. Time it right to Perfect Harden and deal Break Damage." Tutorial: attacks and movement pause while hardened; when harden is broken by damage the current attack resumes.
- Parry: "With proper timing Parry incoming attacks, Breaking enemies more efficiently but at a greater risk." Press only, no block fallback. Web guides put a parry at roughly three Perfect Guards of Break; the 5 September update widened the window for consecutive parries.
- Gloomslayer: "Sidearms deal Break Damage. Some Health and Resolve are restored when Riposting an enemy in a glorious manner." Slayer Punch: "A charged attack that allows you to dash a great distance to punch the target and deal massive Break Damage automatically Riposting them if they break."

Notes for mod work:

- The seal decides what the defensive input (`ActionSealAbility`, "Seal Ability") does. `GA_Action_Parry_Press/Release`, `GA_Action_ActiveBlock_Press/Hold/Release` and `GA_Harden_Original` are the entry abilities; `GE_Block_SealAbility` and `GE_TimedBlockSealAbility` suppress them.
- Harden number caveat: web guides quote "about 3 seconds" of stone form, which appears nowhere in the ability defaults; `StoneFormDurationAfterHit` is 1.0 s and the cooldown is 5.0 s. Treat the 3 s figure as unverified.
- Weapon equip is blocked while `State.Special.Harden.Active`, `State.ActiveBlock`, `State.Aiming`, `State.Attack.Melee`, `State.Ability.Buff.Weapon` or `State.Ability.Buff.Sidearm` is present (`UsagePreventionTags` on every weapon item).
- MS1 "empowered riposte" healing does not exist; only the Slayer Seal restores on riposte.

## 3. Primary weapons

### 3.1 Roster

Item definitions live in `Sparta/Items/Weapons/ID_*`. Each grants `GE_GameState_PrimaryWeapon_<Name>` (tags `Game.State.PrimaryWeapon.<Name>` and `Weapon.<Name>`) and spawns `Sparta/Core/Weapons/Player/<Name>/WP_<Name>`. Base damage below is the actor default per hit before the per-attack multipliers in the attack montages, before Tarforge level and before Harbinger level.

| Weapon (display name) | Tag | BaseDamage | Damage tags | Named ability (UI) | Obtained (web) |
| --- | --- | --- | --- | --- | --- |
| The Iconoclast | `Weapon.HadernsSword` | 35 | Blade, Metal | Plummet Strike | Prologue, after UnderMether |
| Axe & Dagger | `Weapon.DaggerAxe` | axe 30, dagger 20 | axe Blade, dagger Piercing | Deadly Flurry | Shrine of Trials dungeon (Chapel Key) |
| Veteran's Battle Axe | `Weapon.BattleAxe` | 38 | Blade, Metal | none in data ("Ability Name" placeholder) | King's Crypt, Blackridge Cliffs |
| Great Martyr's Blade | `Weapon.MartyrsBlade` | 80 | Blade, Metal | Spiral Surge | Martyr's Prison; Merrick 500 coins |
| Obsidian Hammer | `Weapon.HeavyHammer` | 109 | Blunt, Rock | Heavy Stomps | Obsidianite Mines; Merrick 1300 coins |
| Axatana | `Weapon.Axatana` (+ `Axatana_Axe`, `Axatana_Katanas`, `Axatana_Single`) | axe 35, each katana 25 | Blade, Metal | Morph Attack | Forgotten Tower, Vestige of Infinity |
| Black Needle | `Weapon.BlackNeedle` | 35 | Blade, Metal | Needle Storm | Twin Sesters boss, Sester's Bastion |
| Clockwork Scythe | `Weapon.ClockworkScythe` | 42 | Blade, Metal | Clockwork Chainsaw | Sariel boss, Chamber of Becoming |

Also present in data but not offered as pickups: Hallowed Sword (`ID_HallowedSword`, BaseDamage 25, own upgrade pool with a passive Burn stone), Fists (`ID_Fists`, used by the Harbinger and Smert's Fight Stance), and Memory variants (`*_Memory`) used in shell memory sequences. The `ASG_Weapon_Player_Primary` enum has 15 entries, which covers these internal variants.

Descriptions (`ST_Core_Weapons`) give flavour only. Only two weapons carry a gameplay blurb: Axe & Dagger "Fast and versatile combination of weapons with short range", The Iconoclast "A balanced two-handed sword with long reach". The rest read "Missing Weapon Effect".

### 3.2 Moveset notes (web, KosGames and GameRant, single-source for per-attack detail)

- Iconoclast: light one-two, heavy thrust into overhead, running light jump attack, running heavy launcher. Balanced.
- Axe & Dagger: fast multi-hit lights that stack status quickly, leap running heavy. Running attack animation improved 5 September.
- Veteran's Battle Axe: poke into combo, heavy overhead-spin-golf swing; GameRant advises light-heavy-light.
- Great Martyr's Blade: slow, long reach, running heavy spin into thrust is its best move; +20% damage in Balance Patch 1 (the devs said it did not land and buffed it again in Week 1).
- Obsidian Hammer: slowest, highest single hit, full balance pass in Week 1.
- Axatana: lights in twin-blade form, heavies morph to axe; running light is a multi-hit whirlwind. Week 1 gave the morph ability i-frames and Fragile stacks.
- Black Needle: fastest pokes, longest reach; heavies are slow.
- Clockwork Scythe: wide three-sweep light combo, best AoE; running attacks lock you out of parry (GameRant).

### 3.3 Attack inputs and abilities

Actions (`ST_Core_Actions`): Light Attack, Heavy Attack, Weapon Ability, Weapon Ability A / B, Infusion (Tarstone) hold, Seal Ability, Shell Ability, Shell Secondary Ability, Aim hold, Fire (Sidearm), Sidearm Ability, Dodge, Sprint, Use PP Item, Heal.

- The intrinsic weapon ability (Plummet Strike, Deadly Flurry, Spiral Surge, Heavy Stomps, Morph Attack, Needle Storm, Clockwork Chainsaw) is triggered with the Weapon Ability input and costs Resolve. In the data the same abilities are also exposed as weapon-exclusive Ability Tarstones (Shrike Stone, Magdalena's Memento, Captive's Scabstone, Colossus Stone, Hexapod Core, Conqueror's Reward, Scholar's Wormstone), and `GA_Axatana_Handler` handles the morph. The Fextralife claim that an ability costs "two bars" is pre-release wording; ability Tarstone costs are 50 to 100 Resolve per use (section 6).
- Weapon buffs (`GA_WeaponBuff_*` per weapon, `GA_WeaponBuffGranter`) are the Infusion Tarstone activations: hold the Infusion input to spend Resolve and coat the weapon with an element for N strikes.
- Charged attacks exist only through Acolyte's Stone (charged light, 40 Resolve) and Unwieldy Stone (charged heavy, 35 Resolve).
- Plunge attack from height with Light Attack, no fall damage (tutorial string).

### 3.4 Upgrades (Tarforge)

`DT_WeaponProgression` maps each weapon tag to a `WeaponProgression_*` curve asset (Black Needle shares the Martyr's Blade curve). `DT_ForgeModeRequirements` sets the material per level: levels 1 to 5 Ventrium, 6 to 10 Laterite, 11 to 15 Dorsalite, 16 to 20 Thoracium, 21 to 25 Ossinite ("Thoracium Prime" internally). Key items unlock forge modes: Muradean Actuator (weapons), Obsidian Lathe (sidearms), Foundry Stone (smelting), Etching Needles (Tarstones), Endless Core (removes the level cap).

Melee curves (all eight identical except where noted):

| Stat | Curve |
| --- | --- |
| Damage | +5% per level, 1.05 at +1 to 1.80 at +16, constant after (curve has no keys past 16) |
| Guard meter | 1.0 until +16, then 1.25 (Great Martyr's Blade 1.30) |
| Poise, elemental efficiency, crit chance, crit damage, resolve gain | curves exist to 2.0 at +20 but are flagged off |

So the effective melee cap is +16 (180% damage), matching Game8. The Endless Core allows forging beyond it but the damage curve does not grow further. Web guides claiming +20 or +25 are describing material tiers, not damage growth.

Harbinger levels (Gloom, `DT_PlayerExperience`) raise Health, Shell Health, Base Damage, Critical Chance and Shell Points; weapons have no stat scaling of their own.

## 4. Sidearms

Item definitions in `Sparta/Items/Sidearms/ID_Sidearm_*`, actors in `Sparta/Core/Weapons/Player/<Name>/WP_*` (parent `WP_SidearmBase`, interface `BPI_SidearmBase`). Each actor carries a `BPC_Ammunition` component that reads `SpartaHealthSet.Resolve` as its energy pool. Firing is `GA_Sidearm_Primary_Press` while `State.Aiming` (hold Aim), the secondary input runs the equipped Ability Tarstone, and the Infusion input while aiming applies the sidearm infusion. `GE_Sidearm_Active` grants `Character.State.Sidearm`.

| Sidearm (display) | Tag | Resolve per shot | Ammo / reload | BaseDamage | Projectile / notes |
| --- | --- | --- | --- | --- | --- |
| Naylshotte | `Weapon.NailShotgun` | 28 | 4, no timed reload | 7 per pellet | `BP_Projectile_NailShotgun_Primary`, `Weapon.Edge.Piercing.Shotgun`; starter sidearm |
| Forgotten Crossbow | `Weapon.Crossbow` | 17 | none | 45 | single bolt, Piercing |
| Ballistazooka | `Weapon.Ballistazooka` | 40 | 4, reload 10 s | 125 | highest single hit, MS1 returnee |
| Salvaged Trebuchaxe | `Weapon.Trebuchaxe` | 30 | 3, reload 7 s | 65 | charged throw (`Trebuchaxe_Overcharge` upgrade lengthens charge) |
| Triarch Repeater | `Weapon.MachineGun` | 1 | 3 bursts, reload 7 s | 3 (+1 poise) | full-auto, `CameraShake_Sidearm_MachineGun` |
| Caged Hystrix | `Weapon.ParasiteGun` | 2.5 | 30, reload 10 s | 8 | charge build-up (`ChargeAddAmount 0.08`, fade 0.02), Flesh material |
| Cursed Child | `Weapon.CursedChild` | 20 | none | 15 | summon-type projectile; Summoning Stone brings back the last slain foe |
| Troubadour's Lute | `Weapon.Lute.Simple` | 20 | none | 20 | inflicts Confusion; +100% damage in Balance Patch 1; playable instrument with tracks |

Minimum energy to fire equals one shot's cost (`MinRequiredShots` default 1). Balance Patch 1 removed the extra stored-Resolve minimum the Repeater and Hystrix had at launch. A sidearm with 0 Resolve shows "ready to fire" but `HasEnoughEnergyPrimary()` is false, which is the failure recorded in `sidearm-investigation.md`.

Sidearm Tarforge curves (`WeaponProgression_<Sidearm>`):

| Sidearm | Damage | Resolve cost multiplier | Other |
| --- | --- | --- | --- |
| Naylshotte | 1.05 at +1, cubic to 1.30 at +20 | 0.70 at +16 | |
| Crossbow | linear 1.0 to 1.80 at +20 | 0.70 at +16 | |
| Ballistazooka, Cursed Child, Trebuchaxe, Lute | stepped, 1.30 at +9, 1.60 at +12, 2.0 at +20 | 0.70 at +16 (Lute: linear to 0.30 at +20) | |
| Triarch Repeater | 1.0 until +9, 1.4 from +10, 1.7 at +20 | 0.85 at +16 | crit damage curve on, 2.0 at +20 |
| Caged Hystrix | 1.0 until +9, 1.125 from +10, 1.25 at +20 | 0.90 at +20 | crit damage curve on |

Per-sidearm upgrade pools (`DT_UpgradePool_*`) also contain non-Tarstone tuning items: Naylshotte Bravado (damage 1.25/1.35/1.40, range -20/-30/-50%), Gunner (damage 0.75/0.70/0.65, range +0/+20/+30%), Shortening / Widening (spread angle -1..-3 / +1..+3); Caged Hystrix Contagion (poison or curse, extra damage 10/25/50/100 to all afflicted targets) and Pulse Driver (fire rate 110/125/150%); Trebuchaxe Overcharge (charge 2.5/3/4).

Slayer Seal makes every sidearm deal Break Damage; `GA_AI_Sidearm_Vulnerability` and the tutorial "Exploiting Vulnerability" text confirm some enemies are weaker to ranged hits. `ID_Support_OnSidearmKill_GainResolve` (Devout Stone, sidearm variant) refunds 7/14/28 Resolve on a sidearm kill.

## 5. Shells

Shell items are `Sparta/Items/Shells/ID_Shell_*` with fragment `ItemFragment_SetShellClass` pointing at `BP_Shell_<Name>`; each grants a dash ability, ability set and `GE_GameState_Shell_<Name>`. Attribute defaults come from `DT_Attributes_<Name>` (values below are `BaseValue`; ability numbers are the shell ability set's own fields).

| Shell | Internal | Shell health | Ability (cost, cooldown) | Passive | Notes from data |
| --- | --- | --- | --- | --- | --- |
| Harros the Vassal | Harros | 150 | Stone Stun (70, 8 s, dmg 80, duration 3, up to 3 targets) | none (Harden comes from Vatra's Seal) | prologue only |
| Genessa the Wayward | Genessa | 70 (MaxHealth 80) | Faithful Doubles (25, 6 s, 1 to 5 doubles) | Duality: becomes Stray Genessa instead of severing | Stray form: `DT_Attributes_CorruptedGenessa`, MaxHealth 50, Stray Doubles cost 40, duration 20, cooldown 10 |
| Eredrim the Venerable | Eredrim | 110 | Shoulder Bash (70, dmg 50, poise 80, break 50); Ethereal Diapason (60, dmg 20, poise 100, break 15, radius 500) | Executioner: executes below a threshold (0.01 base), Slaughterer stacks on riposte | Seal Affinity skill widens parry window |
| Gragu the Insatiable | Gragu | 110 | Staggering Blow (50, 15 s, dmg 80 to 150 by charge, charge 1.4 s) | Revered Heart heal (meter 99, factor 20, custom cost 33) | |
| Proxima the Broodseeker | KnightLady | 100 | Biosampler (50, 7 s, dmg 60, Lightning) | Grafted Armor: 15% chance to mitigate, 50% reduction, no stagger | |
| Lazlo the Justiciar | Necrophage | 120 | Retribution shockwave (25, 15 s, dmg 20 to 50, poise 50 to 150, radius 500, overheat cost 34) | Fortified Plate 10% damage reduction (data `DamageReduction 0.1`; beta text said 20%) | |
| Sariel the Endless | Thorn (ThornBoi) | 95 | Exodus of Thorns (15, 3 s, thorn dmg 2, poise 15) | Purge: 50% of incoming damage becomes Pain when no Pain | |
| Smert the Apostate | Smert | 70 | Miracle (100 Resolve, 20 s, duration 7, halves health, Chaos dmg 3 per stack) | Deadly Revelation: Faith at 10% health, x2 resolve gain | Last Vow skill: permanent Fight Stance, no melee weapon |
| Tiel the Acolyte | Tiel | 85 | Lingering Shadow / Shadow Strike (40, 8 s, dmg 70, 2 stacks) | Shadow Dash: dash just before a hit for Shadow and stagger damage | Seal Affinity skill: failed guard grants Shadow, perfect defence deals extra Break |
| Solomon the Scholar | Solomon | 100 | copy of Proxima's table | | `ID_Shell_Solomon` exists with "Missing Shell Tagline"; not obtainable, treat as cut content |

Elemental resistances (Frost / Shock / Curse / Bleed) are also per shell in the same tables, e.g. Sariel 9/9/9/8, Eredrim 7/7/6/10, Genessa 4/6/9/5.

Progression (`ST_Core_Progression`, web): Gloom levels the Harbinger and gives Shell Points; Shell Points buy skills from the shell's 12-node tree and can be refunded freely; Shell Points Capacity per shell caps active skills (Game8: 40 to 44); Bond Tier I to IV is bought with Glimpses at the Shellkeeper (27 total per one guide) and gates skill tiers; Mether's Severance resets a shell. Skill names per shell are in `ST_Skills_<Shell>` (for example Eredrim: Alacrity, Massacre, Apathy, Anguish, Tank, Consecration, Radiance, Carnage, Resonance, Oppression, Persistence, Seal Affinity).

Shells do not change weapon speed, weight or moveset. What they change: shell health, resistances, Resolve economy through skills (Faith, Devout-type bonuses, cost reductions), and which defensive timing pays off (Seal Affinity on Eredrim and Tiel, Shadow Dash on Tiel, Grafted Armor on Proxima).

## 6. Tarstones

Tarstones are `ID_*` items under `Sparta/Core/Tarstones/{Melee,Sidearm,Support}` and `Sparta/Core/Player/Upgrades/{Melee,Sidearm}/Generic`. Categories in the UI: Support (all characters), Combat (weapon or sidearm passives), Infusion (elemental coat, costs Resolve), Ability (special attack, costs Resolve), Fragile (durability, added in Week 1). Slots: 4 shell, 2 Combat, 2 Infusion, 2 Ability. They level 1 to 3 (some Generic stones have a fourth value) with XP from kills, tempered at the Tarforge with coins and Tarcores.

Compatibility is a tag list on `ItemFragment_Tarstone.CompatibilityTags`. `DT_UpgradePool_<Weapon>` is the pool each weapon draws from when the game hands out stones. Values are per level L1/L2/L3.

### 6.1 Melee Infusion stones (Resolve cost, strikes coated L1/L2, permanent at L3)

| Stone | Element | Cost | Compatible weapons |
| --- | --- | --- | --- |
| Arbiter's Prize | Bloodcurse | 30 | Axe & Dagger, Iconoclast, Clockwork Scythe |
| Serpent Stone | Poison | 15 | Axe & Dagger, Black Needle |
| Inflamed Clawstone | Burn | 30 | Battle Axe |
| Curseblood Stone | Curse | 30 | Iconoclast, Battle Axe |
| Warden's Stone | Frost | 30 | Great Martyr's Blade |
| Voltaic Crown | Lightning | 30 | Clockwork Scythe, Great Martyr's Blade, Axatana |
| Nightgrasp Stone | Phantom | 30 | Black Needle |
| Torpor Stone | Stasis | 25 | Obsidian Hammer, Axatana |
| Wretchcaller's Stone | Trauma | 30 | Obsidian Hammer |

All last 4 strikes at L1, 8 at L2, permanent at L3. Only one elemental infusion can be slotted per weapon (`UpgradeRequirements: No Elemental`).

### 6.2 Melee Ability stones

| Stone | Ability | Cost L1/L2/L3 | Compatible | Key values |
| --- | --- | --- | --- | --- |
| Shrike Stone | Plummet Strike | 70/50/50 | Iconoclast, Axatana | x2 damage, 70 stagger, 150 shockwave splash, 25 Break at L3 |
| Magdalena's Memento | Deadly Flurry | 100 | Axe & Dagger | 2 Weak stacks per hit, 15% crit from L2, +50/100% crit damage |
| Captive's Scabstone | Spiral Surge | 100/100/75 | Great Martyr's Blade | x3 to x3.5 damage, 100/250 stagger |
| Conqueror's Reward | Storm Strike (Needle Storm) | 100/100/75 | Black Needle | x2 damage, +4 strikes from L2 |
| Colossus Stone | Heavy Stomps | 80/75/60 | Obsidian Hammer | shockwave 30/45/80 splash, radius 350 to 450, 100 stagger |
| Lost Clotstone | Weapon Throw | 60/60/40 | Battle Axe, Obsidian Hammer | x1/x3/x3 damage, 200 splash, radius 300 |
| Infused Stone | Homing Throw | 70/70/50 | Black Needle | 3 Perforation per hit, 3/4/6 targets, x1.5 at L3 |
| Hexapod Core | Katana barrage | 60/60/80 | Axatana | 3 Warp per strike, 30 stagger, Fragile from L2, second barrage at L3 |
| Scholar's Wormstone | Clockwork Chainsaw | drain 1 per tick | Clockwork Scythe | 15/25/35% base damage per spin, 5 stagger |
| Clockwork Shardstone | Clockwork Grinder | drain 2 per tick | Clockwork Scythe | 12/15/20% per spin, 5/15/15 stagger |
| Acolyte's Stone | Charged Light Attack | 40/40/30 | all | 15% crit, x1/x1.5/x1.5 crit damage |
| Unwieldy Stone | Charged Heavy Attack | 35 | all | 25 damage reduction during charge, heals 10/20/40 on crit |

### 6.3 Melee Combat stones

| Stone | Effect | L1/L2/L3 | Compatible |
| --- | --- | --- | --- |
| Grudge Stone | guaranteed crit every N attacks | 7/6/5 | all |
| Duality Stone | light combo strikes twice at 80% damage, 25% resolve gain | 1st / 1st+2nd / all attacks | Axe & Dagger, Axatana |
| Unyielding Stone | stagger immunity on light combo | 1st / 1st+2nd / all | Axe & Dagger, Black Needle, Iconoclast |
| Parasitic Stone | Leech stacks per kill | 2/4/6 | all |
| Thief's Stone | Warp stacks per hit | 3/4/5 | all |
| Stillblade's Stone | light combo finisher with Break | 10/15/25 | six two-handers (Martyr's Blade, Black Needle, Scythe, Hammer, Iconoclast, Battle Axe) |
| Zealot's Stone | light finisher resolve gain | x1.4/1.7/2.5 | same six |
| Clerik's Stone | heavy finisher crit chance | 30/35/40% | same six |
| Tyrant's Stone | heavy finisher Weak stacks | 4/5/6 | same six |
| Wounding Stone | Perforation per strike | 2/4/7 | all |
| Berserker's / Headsman's / Auspicious / Shattering / Devout Stone | low-health damage / crit damage / crit chance / stagger / resolve gain | 1.3-1.65x at 35-50% HP / +50/80/100% / 10/15/18% / 1.01-1.03x / 1.5/1.8/2.0x | all (Support slot) |

### 6.4 Sidearm stones

Infusions (cost 20, shots coated L1/L2, permanent L3): Viletongue Hedron (Bloodcurse: Naylshotte, Cursed Child, 4/8 shots), Emberseed Stone (Burn: Crossbow, Naylshotte, Ballistazooka, 4/8), Accursed Stone (Curse: Hystrix, Cursed Child, 15/30), Frostshard Stone (Frost: Repeater, 40/80), Voltaic Amber (Lightning: Trebuchaxe, Repeater, 4/8), Monarch's Vestige (Phantom: Trebuchaxe, 4/8), Hag Stone (Poison: Crossbow, Hystrix, 15/30), Weeping Stone (Trauma: Ballistazooka, 4/8). Sidearm infusions on elements the sidearm cannot natively carry apply only on critical hits (`TarstoneElementalDeficiencySidearm_*` strings).

Abilities (secondary input):

| Stone | Ability | Cost | Compatible | Key values |
| --- | --- | --- | --- | --- |
| Confessor's Keepsake | Deadshot charged shot | 60 | Crossbow, Naylshotte, Cursed Child, Ballistazooka | x2/x3/x3.5 damage |
| Fusillade Stone | flurry of shots | 80 | Naylshotte, Crossbow, Trebuchaxe, Ballistazooka | 2/3/3 extra shots, 20/35% crit, +100% crit damage L3 |
| Barrage Stone | cluster bomb | 40 | Naylshotte, Crossbow | 4/8/8 splash, 2/2/3 detonations |
| Solnir Shard | slingshot arc round | 25 | Crossbow, Naylshotte, Hystrix | 70/120/120 splash, radius 250 to 400 |
| Fulminant Stone | bombardment | 60 | Ballistazooka | 3/5/5 explosions of 40, 80 stagger, 20 Break at L3 |
| Infested Stone | sticky bomb | 60 | Ballistazooka | 100 damage, taunts at L2, scatters 3 more at L3 |
| Strange Remnant | threadweaver chain | 15 | Hystrix | chains 3/4/4 targets, 20/20/40 damage |
| Tarred Fragment | tilt shot spinners | 50 | Trebuchaxe | 6/8/10 s, radius x1.5 at L3 (redesigned Week 1) |
| Summoning Stone | summon last slain foe | 70 | Cursed Child | 2/5/5 attacks, 2 foes at L3 |
| Hand of Rock | homing riff swarm | 80 | Lute | 10 damage each, 5 Break from L2, fire rate +30% L3 |

Combat passives: Ironpiercer's Stone (pierce 3/4/5: Crossbow, Hystrix, Trebuchaxe, Naylshotte, Ballistazooka), Myriad Stone (3/5/7 projectiles in an arc at 60/40/30%: Crossbow, Hystrix), Splitting Stone (2/3/4 projectiles at 55/40/35%: Crossbow, Hystrix), Blackblood Stone (mid-air explosion 2/3/4 splash: Naylshotte), Unstable Stone (explosion after 15/15/10 hits: Hystrix, Crossbow, Repeater), Corroded Stone (8/12/15% DoT every 7 s for 30 s: Naylshotte, Crossbow, Trebuchaxe, Ballistazooka), Squall Stone (spin 3/4/5 s at 9-11%: Trebuchaxe), Pulse Stone (pull radius 350/500/650: Crossbow, Trebuchaxe, Ballistazooka), Shuddering Stone (knockback radius 350/500/650: Crossbow, Trebuchaxe, Ballistazooka, Lute), Volatile Fragment (corpse explosion 15/25/35: all but Repeater and Cursed Child), Charged Stone (charge speed +20/35/50%: Trebuchaxe, Cursed Child, Crossbow, Ballistazooka, Naylshotte), Rupturing Stone (crit hits deal 10/15/20 Break: all), Enfeebling Stone (5/6/7 Fragile per hit: all), Big Pockets (ammo +3/5/6: Crossbow, Naylshotte, Hystrix, Cursed Child), Rapid Refill, Inscribed Munitions, Marksman's / Deadeye / Siegebreaker's / Spite Stone (sidearm crit chance 10/12/15%, crit damage +20/30/40%, stagger 1.5/2/2.5x, low-health damage).

### 6.5 Support stones

Bulwark Stone (10/15/25% damage reduction, all characters), Bulwark on-kill variant (25/50/70% for 2/3/4 s), Retribution Stone (riposte damage +5/8/15%), Shattering on-kill (next melee 1.2/1.3/1.5x stagger), Devout sidearm-kill (7/14/28 Resolve), Gloombound Stone and Justiciar's Stone (30% more Gloom or Coin, Fragile, break at 3000 Gloom or 1500 Coin), Egon's Stone (dungeon respawn), Glimpse Stone (3 Glimpses on break).

## 7. Status effects (`ST_StatusEffects`)

Bloodcurse (massive damage at stack threshold), Burn (accumulated DoT), Chaos (random condition on expiry), Confusion (attacks allies, else Break), Cosmic Disease (DoT and reduced damage), Curse (nullifies and reflects the next melee strike), Execution (melee deals Break), Faith (extra Resolve from melee), Fragile (bonus weapon damage taken), Frost (freeze at enough stacks), Havoc (ranged deals Break), Infect (spreads Poison to attackers), Leech (max health per stack, lost on death), Lightning (DoT, shockwave on repeated application), Pain (grey health cured by melee), Perforation (bonus sidearm damage taken), Phantom (delayed splash and stagger), Poison (long DoT), Shadow (undetectable), Slaughterer (raises execution threshold), Stasis (slows), Trauma (Break over time), Vomit (stuns a poisoned enemy), Warp (attack speed per stack), Weak (less damage dealt).

Player base durations (`DT_PlayerAttributes`): Poison 2 s at 1/s, Burn 2 s, Bleed 5 s, Frost 2 s (freeze 5 s), Lightning 2 s, Fragile 3 s, Perforation 3 s, Phantom 3 s (5 health, 2 poise), Trauma 1 s at 2. `BreakResistance 75`.

## 8. Enemies, difficulty and patches that matter for builds

- Every enemy type has red-circle attacks that cannot be blocked, parried or (per a wiki comment, unverified) hardened; dodge them. Grabs and shield bashes beat parry. Multi-hit strings punish one-instance Harden and no-fallback Parry.
- Enemies have per-type break resistance (`DT_Attributes_*` under `Sparta/Characters`), scaled by Adaptive Difficulty (5 September, optional, off by default). Night Mode (Gloombound Flame to Thestus) raises health and damage. Slayer Seal is the easy mode.
- Patch summary: Balance Patch 1 (20 August) guard while walking, Martyr's Blade +20%, Lute +100%, Hystrix and Repeater minimum-Resolve removed, several Tarstones buffed. Week 1 (29 August) riposte scaling and full-animation i-frames, Obsidian Hammer and heavy weapon pass, Axatana morph i-frames and Fragile, Tarred Fragment redesign, Fragile Tarstones, Mether's Severance. 5 September: Infinite Seal consecutive parry window, Untarnished timing fix, Sariel buffs, Genessa infinite-Resolve fix, Adaptive Difficulty. 15 September hotfix: Colossus Stone cancel, stone fixes.

## 9. Reference for mod code

- Item definition pattern: `ID_<Item>` is a `SpartaItemDefinition` subclass with fragments `ItemFragment_Display`, `ItemFragment_Spawnable` (`ActorToSpawn`), `ItemFragment_AbilitySets` (granted abilities and effects), `IF_ItemTag` (`ItemTag`), `IF_UsableInventoryItem` (`OnUseBehavior` = `BP_Behavior_EquipWeapon/Sidearm/Seal/Shell`, `UsagePreventionTags`), `IF_WeaponAnimationData` (locomotion layer, e.g. `ABPL_Locomotion_H1` for the Iconoclast, `ABPL_Locomotion_ADS_Nailgun` for the Naylshotte), `IF_AddAbilityIndicator` (HUD cost attribute), and for Tarstones `ItemFragment_Tarstone`, `IF_UpgradeEffects`, `SpartaItemFragment_StatLevels`, `IF_Tags`.
- Attribute sets: `SpartaHealthSet` (Health, MaxHealth, MaxShellHealth, Resolve, MaxResolve, Poise, BreakResistance, status durations), `PlayerAttributeSet` (`PrimaryMeleeAbilityCost`), `SidearmAttributeSet` (`PrimaryEnergyCost`, `SecondaryEnergyCost`, ammo, reload), `ShellAbilityAttributeSet` (`PrimaryEnergyCost`, `SecondaryEnergyCost`, Cooldown, Duration, Damage). Default tables: `DT_PlayerAttributes`, `DT_DefaultSidearmAttributes`, `DT_Attributes_<Shell>`.
- Resolve effects: `GE_GainResolve`, `GE_ConsumeResolve`, `GE_FillResolve`, `GE_CanGainResolve`, `GE_ResolveBoost_Melee`, `GE_Melee_Generic_ResolveGain`, calculation `CCC_ResolveGain`.
- Tarforge effects: `GE_Permanent_WeaponDmg`, `GE_Permanent_WeaponPoiseDmg`, `GE_Permanent_WeaponGuardEnergy`, `GE_Permanent_SidearmDmg`, `GE_Permanent_SidearmEnergyCost`, with calculators `CCC_Weapon_Damage`, `CCC_Weapon_GuardEnergy`, `CCC_Weapon_PoiseDamage`, `CCC_Sidearm_Damage`, `CCC_Sidearm_EnergyCost` (all read the equipped `SpartaItemDefinition`).
- Weapon actor interface `BPI_WeaponBase`: `WeaponBuffStart/Complete/Cancel/End`, `WeaponChargeStart/Complete/Failed`, `SidearmChargeStart/Complete/Failed`, `SidearmBuffBurst/End`, `Add/RemoveWeaponEffectTag`, socket names for in-hand and stowed. Sidearm interface `BPI_SidearmBase`: projectile class and spawn transform, muzzle socket, aim offset, blend space, locomotion layers, camera state, optional movement speed.
- Enums (`ASG_*`): 15 primary weapon entries, 10 sidearm entries, 15 player shell types, 93 weapon types, 4 item types. Enumerator display names are stripped in the header dump; read the cooked `ASG_*.uasset` for names.
- Global events: `GlobalEvent_SealChanged`, `GlobalEvent_SealUnlocked`, `GlobalEvent_SlayerSealEquipped`, `GlobalEvent_EquipmentLevelChange`.
- Data tables worth exporting for tuning work: `DT_WeaponProgression` and `WeaponProgression_*`, `DT_ForgeModeRequirements`, `DT_UpgradePool_*`, `DT_Tarstones_Melee/Sidearm/Support`, `DT_Upgrades_All`, `DT_ShopItems`, `DT_PlayerExperience`, `DT_UpgradeTooltips`, `DT_Artifacts_New`.

## 10. Open questions

1. Per-attack damage multipliers and hit counts per weapon montage were not extracted; the actor `BaseDamage` is the per-hit base only.
2. Harden stone-form duration (web "3 s") is not in the ability defaults; measure in game if it matters.
3. Whether the intrinsic weapon ability button uses the same cost values as the weapon-exclusive Ability Tarstone is not confirmed; the data exposes the abilities through the stones.
4. Veteran's Battle Axe has no named ability in data or web.
5. Sidearm swapping outside beacons is claimed beacon-only by one guide; the inventory equip behaviour only rejects re-equipping the already selected sidearm.
6. Shell Points Capacity (40 to 44) and the 27 Glimpse bond cost are single-source web numbers.
