# Mortal Shell II: Enemy Damage, Attributes & Scaling Pipeline

Written 27 September 2026 from the installed build (`MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`, UE 5.6.1). Companion to [`combat-damage-pipeline.md`](./combat-damage-pipeline.md) and [`combat-pipeline-resolutions.md`](./combat-pipeline-resolutions.md). Working data, CSV exports, and scripts reside in [`CustomShellSystem/CCS/work/damage-pipeline/`](file:///home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/damage-pipeline/).

---

## 1. Enemy Damage Architecture: From Spawner to Player Impact

```
[Enemy Character Data: `CD_<Enemy>`]
       │
       ├─ Attribute Initialization (`USpartaCharacterData.AttributeData[]`)
       │    ├─ `SpartaHealthSet` (HP, MaxPoise, BreakResistance, StoneStunTime, Resistances)
       │    ├─ `SpartaCombatSet` (BaseKnockbackMultiplier/Strength, Mitigation)
       │    └─ `EnemyAttributeSet` (TarstoneEXP)
       ├─ BaseDamage Initialization (`USpartaCharacterData.BaseDamage`)
       └─ Passive Elemental Infusion (`USpartaCharacterData.UpgradeStatValues[]`)
       │
       ▼
[Enemy Attack Execution: `GA_<Enemy>_Attack_*`]
       │
       ├─ Instantiates `BP_SpartaHitPayload`
       │    ├─ HealthDamage: Type `InstigatingCharacterData` (inherits `CD_*.BaseDamage`)
       │    ├─ Multiplier: 0.4x to 1.7x (default 1.0x)
       │    ├─ DamageEffect: `GE_MeleeDamageBase_C`, `GE_RangedDamage_Base_C`, or `GE_AOE_Damage_Base_C`
       │    ├─ PoiseDamage: 20 to 120
       │    ├─ ReactionTag: `Event.Reaction.Hit.Medium`, `Heavy`, or `Flyback`
       │    └─ AdditionalEffects:
       │         • `GE_BlockPoiseRegen_C` (5s)
       │         • `GE_UnparryableHit_C` (strips player Parry / Harden / HyperArmor)
       │         • `GE_OnHitDamageImmunity_C` (0.15s multi-hit grace)
       │         • `GE_DamageIgnoreImmunity_C` (pierces player invulnerability)
       │
       ▼
[Damage Scaling & Aggregator]
       │
       ├─ Multiplicative Biome Difficulty (`GE_Regional_DamageMultiplier`)
       ├─ New Game Plus Scaling (`GE_NGP_DamageMultiplier` x `CCC_NewGamePlusBase_EnemyDamage`)
       └─ Adaptive Difficulty Scaling (`USpartaAssistSubsystem.FSpartaBossAssistScales.DamageScale`)
       │
       ▼
[Target Application: Player Takes Hit]
       │
       ├─ If Player Guards (`GA_ActiveBlock`):
       │    • Health damage = 0 (immune)
       │    • Break Damage taken = 100% of calculated raw damage
       ├─ If Player Hardens (`GA_Harden_Original`):
       │    • DamageReduction = 1.0 (100% immune)
       │    • Attacker takes 50 Poise + Stone Stun (3.0s to 8.0s)
       ├─ If Player Dodges (`GA_RollBase`):
       │    • Invulnerability window (0.6s) negates hit
       └─ If Unblocked / Open Hit:
            • `USpartaDamageExecution` calculates final damage
            • Deducted from `ShellHealth` (in Shell) or `Health` (Dark Form)
            • Stagger evaluated via `HRAS_Player` (Poise 5 broken by 20+ poise damage)
```

---

## 2. Enemy Roster & Tiered Attribute Breakdown

Across all 193 character data files (`CD_*`) and 158 attribute tables (`DT_Attributes_*`), enemies fall strictly into 5 mechanical tiers:

### Tier Summary Matrix:

| Tier | Health Pool | Base Poise | Break Resist | Stone Stun | Knockback Mult | Tarstone EXP | Typical Archetypes |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **1: Fodder** | 10 – 150 | 5 | 25 | 7.0 – 8.0s | 1.0 | 5 – 25 | Brigand Light, Cultist Base, Batman, Babka, Baby Snail |
| **2: Regulars** | 160 – 450 | 15 – 125 | 25 – 75 | 5.0 – 7.0s | 0.8 – 1.0 | 25 – 75 | Brigand Heavy, Cultist Spear Lady, Draugr, Tarred Corpse, Blade Slave |
| **3: Elites** | 550 – 950 | 130 – 200 | 75 – 125 | 5.0s | 0.6 – 0.8 | 80 – 150 | Aristocrat, Tanky Brigand, Bone Snail, Tarred Vestige |
| **4: Mini-Bosses** | 1,000 – 3,200 | 150 – 300 | 100 – 200 | 3.5 – 5.0s | 0.2 – 0.5 | 200 – 500 | Grisha, Sicario Miniboss, Heavy Cultist Miniboss, Batushka Miniboss |
| **5: Story Bosses** | 3,850 – 35,000 | 300 – 1,200 | 220 – 250 | None / 3.0s | 0.0 – 0.2 | 800+ | Final Boss, Monolith, Hexapod, Moth Knight, Dwarf Boss, Tar Golem |

---

### Full Boss & Mini-Boss Attribute Reference Table:

Extracted from cooked tables (`DT_Attributes_*`) and character data (`CD_*`):

| Boss / Mini-Boss | MaxHealth | Base Poise | Break Resist | Base Dmg | Stone Stun | Knockback | Key Traits & Passives |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **Final Boss** | **35,000** | **500** | **250** | **104** | None | **0.1** | Highest HP in game; Fire Sword ground hits, grab attack, spit fire |
| **Monolith** | **18,000** | **500** | **220** | **80** | None | **0.1** | Massive colossus; high area denial, heavy overhead slams |
| **Offspring** | **8,500** | **350** | **220** | **79** | None | **0.5** | Multi-phase horror; fast lunges, jumping sweeps |
| **Hexapod** | **8,000** | **1,200** | **140** | **81** | None | 0.0 | Highest poise in game (1200); burrow/dive attacks, acid spit, grab |
| **Moth Knight** | **7,200** | **300** | **220** | **60** | None | **0.0** | Completely immune to knockback push; teleport aerial plunge, quick riposte |
| **Parasite Golem** | **7,000** | **400** | **220** | **96** | None | **0.2** | Heavy club swings, parasite split phase |
| **Tar Golem (Headless)** | **7,000** | **400** | **220** | **91** | **3.0s** | **0.0** | Bleed 3 + Burn 3 on hit; plow charge, zero knockback |
| **Dwarf Boss** | **6,900** | **400** | **220** | **70** | None | **0.2** | 3-hit combo, spinning slams, weapon buff, head explosion |
| **Swordman** | **6,800** | **400** | **220** | **70** | None | **0.4** | Agile dual-blade duelist; quick parry-riposte windows |
| **Brigand Elite Winter** | **4,200** | **190** | **100** | 50 | **5.0s** | 0.4 | Frost 1 on hit; ice burst swings |
| **Multi-Headed Harpy** | **4,200** | **240** | **125** | **60** | **5.0s** | 0.5 | Bleed 5 on hit; aerial swoop strikes |
| **Lady of the Woods** | **3,850** | **500** | **220** | 45 | None | **0.5** | High poise, quick riposte, projectile evasion |
| **Grisha (Caged)** | **3,200** | **300** | **150** | **45** | None | 0.2 | Frost 5 on hit; claw flurry, roar shockwave, bite grab |
| **Sicario Miniboss** | **3,200** | **180** | **150** | **65** | **4.0s** | 0.3 | Bleed 4 on hit; counter-parry stance, rapid gap closer |
| **Grisha (Alien)** | **3,000** | **250** | **150** | **50** | None | 0.2 | Stasis 5 on hit; slow field aura, overhead crush |
| **Grisha (Pale)** | **2,800** | **300** | **150** | **45** | None | 0.2 | Bleed 5 on hit; aggressive bite grabs |
| **Heavy Cultist Miniboss** | **2,740** | **200** | **200** | **45** | **5.0s** | 0.4 | Stasis 6 on hit; colossal mace sweeps, shield bash |
| **BallistaHead MB** | **2,700** | **150** | **250** | **40** | **5.0s** | 0.3 | Lightning 3; long-range artillery volleys |
| **StoneCrab Miniboss** | **2,600** | **150** | **100** | 40 | **5.0s** | 0.3 | Bleed 3 on hit; impenetrable shell defense |
| **Wraith Miniboss** | **2,630** | **90** | **200** | 40 | **5.0s** | 0.6 | Teleport dashes, phantom blade strikes |
| **Tar Golem** | **2,100** | **125** | **175** | 45 | **3.0s** | **0.0** | Burn 2 on hit; MinHealth 1 (unkillable until phased) |
| **Grisha (Standard)** | **2,000** | **270** | **150** | **35** | None | 0.2 | Classic MS1 brute; unblockable grab, ground slam |
| **Hutchback Miniboss** | **2,000** | **100** | **75** | **42** | **5.0s** | 0.5 | Cleaver rushes, meat flail |
| **Dungeon Champion** | **1,900** | **250** | **150** | **50** | **5.0s** | 0.3 | Arena gladiator; shield charge, heavy overhead |
| **Brigand Elite Fire** | **1,750** | **250** | **75** | **65** | **5.0s** | 0.4 | Burn 1 on hit; fire infused halberd swipes |
| **Cannibal Knight MB** | **1,750** | **200** | **150** | **40** | **5.0s** | 0.4 | Relentless dual axes; frenzy strings |
| **Tarred Stoner** | **1,650** | **300** | **100** | **40** | **5.0s** | 0.2 | Burn 5 on hit; boulder throw, rock ground pound |
| **Miner Miniboss** | **1,600** | **200** | **125** | **35** | **5.0s** | 0.4 | Heavy pickaxe crush, mining shockwave |
| **Cultist Spear Lady MB** | **1,500** | **90** | **200** | **40** | **5.0s** | 0.4 | 5-hit spear dance, lunging grab, projectile evade |
| **Lord Vellen MB** | **1,500** | **160** | **200** | 40 | **5.0s** | 0.4 | Corrupted knight moveset; dark wave slashes |
| **Aristocrat Miniboss** | **1,400** | **220** | **100** | **55** | **5.0s** | 0.7 | Weak 5 on hit; rapier thrusts, evasive backstep |
| **Vampire Miniboss** | **1,400** | **150** | **150** | **35** | **5.0s** | 0.5 | Ceiling dive, vampire bite grab, blood leech |
| **Twin Sister Spear/Dagger**| **1,200** | **200** | **150** | **18** | **5.0s** | 0.5 | Phantom 2 on hit; synchronized paired attacks |
| **Batushka Miniboss** | **1,100** | **80** | **200** | **50** | **5.0s** | 0.6 | Bear brute; ferocious double paw sweeps |
| **Centipede Ghost MB** | **1,100** | **90** | **100** | **40** | **5.0s** | 0.5 | Ghostly coils, stasis burrow |
| **Tarred Vestige MB** | **1,000** | **200** | **150** | **33** | **3.5s** | 0.3 | **Burn 15** on hit! Immense flame burst |

---

## 3. Passive Elemental Infusion on Hits

Unlike player weapons which require Tarstones or infusion items, elite enemies and bosses carry baked-in elemental passives via [`USpartaCharacterData.UpgradeStatValues`](file:///home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/damage-pipeline/character_data.txt). When an attack lands, [`GA_AI_Melee_InflictStatusEffect`](./combat-damage-pipeline.md#L333) reads these tags and immediately stacks the condition onto the player:

| Upgrade Tag | Stack Value | Enemy / Boss | Effect on Player |
| :--- | :---: | :--- | :--- |
| `UpgradeStat.Melee.Passive.Burn` | **15.0** | `CD_TarredVestige_MiniBoss` | Instant massive burn ignition (1 dmg/s per stack = 15 dmg/s!) |
| `UpgradeStat.Melee.Passive.Stasis` | **6.0** | `CD_HeavyCultist_Miniboss` | Heavy slow; reduces player movement and dodge distance |
| `UpgradeStat.Melee.Passive.Stasis` | **5.0** | `CD_Grisha_Alien_MiniBoss` | Severe slow aura |
| `UpgradeStat.Melee.Passive.Bleed` | **5.0** | `CD_Grisha_Pale_Miniboss`, `CD_MultiHeadedHarpy` | Rapid bleed buildup toward the player's 10 BleedResistance threshold |
| `UpgradeStat.Melee.Passive.Frost` | **5.0** | `CD_Grisha_Caged_Miniboss` | Approaches 5-stack Freeze threshold (causing 5s frozen lockdown) |
| `UpgradeStat.Melee.Passive.Weak` | **5.0** | `CD_Aristocrat_Miniboss` | Reduces player outgoing damage by ~23% (0.95^5) |
| `UpgradeStat.Melee.Passive.Bleed` + `Burn` | **3.0 + 3.0** | `CD_TarGolemHeadless` | Dual condition trigger: active burn ticking while bleed meter fills |
| `UpgradeStat.Melee.Passive.Lightning` | **3.0** | `CD_BallistaHead_Lightning` | 3 shock stacks toward the 7-stack discharge limit |
| `UpgradeStat.Melee.Passive.Poison` | **2.0** | `CD_ParasiteKnight_Green` | Applies vomit stun and health leech |
| `UpgradeStat.Melee.Passive.Phantom` | **2.0** | `CD_TwinSister_SpearDagger_Miniboss` | Delayed explosive damage splash |

---

## 4. Special Enemy Attack Mechanics

### 4.1 Unparryable Attacks (Red Skull Indicator)
- Identified by payload class [`BP_SpartaUnparryableHitPayload`](file:///home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/damage-pipeline/enemy_attack_payloads.csv) or additional effect `GE_UnparryableHit_C`.
- **Pre-Damage Stripping**: `GE_UnparryableHit` forcefully removes:
  - `GE_Parry` (cancels player Infinite Seal window)
  - `GE_Harden_Active` (breaks player stone form)
  - `GE_ParrySafety` (removes post-parry invulnerability)
  - `GE_HyperArmor` (strips player poise resistance)
- Forces `ReactionTag = Event.Reaction.Hit.Heavy`. The player is guaranteed to stagger or fly back unless dodged with i-frames (`GA_RollBase`).
- Examples: `GA_FinalBoss_Attack_Defense_End`, `GA_FinalBoss_Attack_SpitFire`, `GA_Hexapod_Attacks_Attack_07_AoE_Shooting`, `GA_Hexapod_Attacks_Lurk`.

### 4.2 Grab Attacks (`GA_Attack_Base_Melee_GrabAttack`)
- Base class: [`GA_Attack_Base_Melee_GrabAttack`](file:///home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/damage-pipeline/ga_summary.txt#L17161).
- Triggers `GE_UnavoidableAttack` (`State.UnavoidableAttack`).
- Overrides motion warping to lock onto `CharacterId.Player.Shell.*` and `CharacterId.Player.Darkform.*`.
- Blends out the current player animation into a synchronized victim pin (`Event.Reaction.Sync.Grab.*`).
- Examples:
  - `GA_Vampire_Attack_Bite`: Pin down and bite neck, draining shell health.
  - `GA_Vampire_SkySpawn`: Ceiling drop grab from off-screen.
  - `GA_Grisha_Grab_Attack`: Picks up player, bites head off, tosses body (Heavy knockdown).
  - `GA_FinalBoss_Attacks_Grab`: One-handed choke grab followed by fire sword impalement.
  - `GA_Hexapod_Grab_Attack`: Pinned under front claws while mandibles chew.

---

## 5. Enemy Poise, Break Meter & Stun Windows

### 5.1 Poise (Stagger Resistance)
- Every hit from the player applies `GE_PoiseDamage` (20 base; up to 130 on hammer heavies).
- Enemies possess `MaxPoise` (5 on fodder, 30–125 on regulars, 200–300 on mini-bosses, 400–1200 on bosses).
- **Stagger Threshold**: In [`HRAS_*`](./combat-damage-pipeline.md#L248), Medium hit reactions are redirected to `Ignore` or `Weak` (additive, no stagger) **until `State.PoiseBroken` is active**.
- **Poise Reset**: When poise hits 0, `GE_PoiseBroken` triggers a true stagger, clears `GE_BlockPoiseRegen`, and immediately refills `Poise` to 100%. If not broken, poise fully refills 5.0 seconds after the last hit.

### 5.2 Break Meter (`BreakResistance`) & Riposte Vulnerability
- Standard weapon swings deal **0 Break Damage**.
- The yellow bar is only depleted by Parries (55), Perfect Hardens (25), Slayer Punches (100), Trauma DoT (2/s), or Execution status (25%/hit).
- At 0 BreakResistance:
  - `GA_ParryResistanceHandler` applies `GE_StateBroken` (`State.Broken`).
  - Enemy drops their weapon, plays `Event.Reaction.Parried`, and enters an extended vulnerability stance.
  - Light Attack near the broken enemy triggers [`GA_Parry_Riposte`](./combat-pipeline-resolutions.md#L30).
- **Break Resistance Tiers**:
  - Fodder: **25** (broken in 1 parry)
  - Regulars: **50 – 75** (broken in 1–2 parries)
  - Elites / Mini-Bosses: **100 – 150** (broken in 2–3 parries)
  - Major Story Bosses: **220 – 250** (requires 4–5 parries or a dedicated break build)

### 5.3 Stone Stun Time (Harden Counter-Stun)
- Set by attribute `SpartaHealthSet.StoneStunTime`.
- When an enemy strikes a hardened player (`GE_Harden_Active`), `GA_StoneStun_Handler` reflects 50 poise damage and locks the enemy in stone stun for this exact duration:
  - Fodder (Brigand Light, Cultist Base, Babka): **8.0 seconds**
  - Regulars (Heavy Brigands, Spear Ladies, Blade Slaves): **5.0 seconds**
  - Elites (Aristocrat, Tarred Vestige): **3.5 – 5.0 seconds**
  - Tar Golem: **3.0 seconds**
  - Major Bosses (Final Boss, Hexapod, Moth Knight, Dwarf Boss, Monolith): **0.0s / None** (Bosses recoil slightly but **cannot be stone-stunned**).
