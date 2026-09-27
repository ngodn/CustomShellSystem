# Mortal Shell II: Enemy Movesets, Boss Abilities & AI Pipeline

Written 27 September 2026 from cooked assets (`MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`, UE 5.6.1). Companion to [`combat-movesets.md`](./combat-movesets.md) and [`enemy-damage-pipeline.md`](./enemy-damage-pipeline.md). Working files, ability dumps, and behavior tree references reside in [`CustomShellSystem/CCS/work/`](file:///home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/).

---

## 1. Enemy AI Decision & Ability Activation Chain

Enemy attack selection is driven by **Unreal Engine 5 Behavior Trees** (`BT_*`), coordinated through [`ASpartaAIController`](file:///home/eins0fx/development/mods/msII/CustomShellSystem/CCS/work/runtime-surface/native-core-classes.hpp) and the gameplay ability system:

```
[Behavior Tree Execution: `BT_<Enemy>_Base`]
       │
       ├─ Environment Query System (EQS: `UBTTask_SyncedEQS`)
       │    ├─ Distance check to player (Melee range < 300cm, Mid 300-800cm, Ranged > 800cm)
       │    ├─ Facing angle check (Front, Flank, Behind)
       │    └─ Navmesh traversal constraint (`ConstrainRootMotionToNavmesh = true`)
       │
       ├─ Decision Branches:
       │    ├─ Player Aiming Sidearm? -> `GA_PredictSidearmAttacks` -> `GA_Enemy_Evade_Projectile`
       │    ├─ Player Outside Melee Range? -> `GA_GapCloser` (Sprint Lunge / Jump-in)
       │    ├─ Repositioning? -> `GA_StrafeBase` (Circle player, maintain combat spacing)
       │    └─ Attack Selected -> Dispatches Gameplay Tag to `UBTTask_SpartaActivateAbility`
       │
       ▼
[Enemy Gameplay Ability: `GA_<Enemy>_Attack_*`]
       │
       ├─ Instanced per execution (`InstancingPolicy = InstancedPerExecution`)
       ├─ Applies Owned Tag `State.Attack.Melee` / `State.Attack.Ranged`
       ├─ Evaluates Motion Warping (`ANS_SpartaMotionWarping_Translation` to `WT_DesiredEndLocation`)
       ├─ Rotates toward target (`ANS_RotateToFaceTarget`)
       ├─ Traces Hit Windows (`SpartaAnimNotifyState_HitCheck`)
       └─ Injects `BP_SpartaHitPayload` into player on contact
```

---

## 2. Core Enemy Archetype Movesets

### 2.1 The Brigands (`Characters/Enemies/Brigands/`)
The baseline humanoid combatants of Mortal Shell II. Share [`HRAS_Human`](./combat-damage-pipeline.md#L248) fallback.

| Ability | Class / Base | Motion / Delivery | Multiplier | Poise Dmg | Reaction | Special Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **Swipes (2-hit)** | `GA_BrigBase_Attack_Swipes_2hit` | Dual horizontal slashes | 1.0x | 20 | Medium | Standard opener at close range (<250cm) |
| **Frenzy Swipes** | `GA_BrigBase_Attack_Frenzy_Swipes` | Rapid 4-hit flurry | 1.0x | 20 | Medium | High commitment; windup dodgeable |
| **Frenzy Fast** | `GA_BrigBase_Attack_Frenzy_Swipes_Fast` | Accelerated flurry | 1.0x | 20 | Medium | Used by Elite and Winter Brigands |
| **Gap Swing (2-hit)**| `GA_BrigBase_Attack_Gap_Swing_2hit` | Forward stepping vertical slice | 1.0x | 35 | **Heavy** | Lunge range 500cm; breaks neutral guard |
| **Juke Left / 2-hit** | `GA_BrigBase_Attack_Juke_Left` | Feint sidestep into thrust | 0.5x, 1.0x | 20 | Medium | Punishes early player attacks |
| **Run-up Stab** | `GA_BrigBase_Attack_Runup_Stab` | Sprinting thrust | 1.0x | 30 | Medium | Long tracking; motion warped |
| **Stabs (3-hit)** | `GA_BrigBase_Attacks_Stabs_3hit` | Rapid triple spear/sword thrust | 1.0x | 35 | **Heavy** | Heavy reaction guarantees player flinch |
| **RunUp 02** | `GA_BrigBase_RunUp_02` | Sprinting gap closer | — | — | — | Closes distance up to 1200cm |
| **Crossbow Shoot** | `GA_Brigbase_Crossbow_Shoot` | Aim and fire bolt projectile | Flat 25 | 10 | Weak | Homing bolt projectile; grants Resolve to player if parried |
| **Crossbow Reload**| `GA_Brigbase_Crossbow_ReloadShoot` | Crouch reload into instant shot | Flat 25 | 10 | Weak | Punishes player attempting to heal |
| **Switch Weapon** | `GA_Brigbase_Crossbow_SwitchToMelee` | Holsters bow, draws shortsword | — | — | — | Triggered when player closes to < 350cm |

---

### 2.2 Cultists & Spear Ladies (`Characters/Enemies/CultistSpearLady/`)
Agile, acrobatic zealots specializing in multi-hit spins, long-range lunges, and sidearm evasion.

| Ability | Class / Base | Motion / Delivery | Multiplier | Poise Dmg | Reaction | Special Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **3-Spins Attack** | `GA_CultistSpearLady_3Spins_Attack` | 360-degree spear whirlwind | 1.0x | 30 | Medium | 3 distinct hit windows; hits in full circle |
| **5-Hit Attack** | `GA_CultistSpearLady_5Hit_Attack` | Acrobatic 5-hit spear dance | 1.0x | 25 | Medium | Fast combo; ends with overhead slam |
| **Stable Leap** | `GA_CultistSpearLady_StableLeap_Attack` | Jump-in downward thrust | 1.35x | 45 | **Heavy** | Pierces neutral guard; 600cm jump |
| **Gap Thrust** | `GA_CultistSpearLady_Gap_Thrust_Attack` | Straight linear spear lunge | 1.0x | 30 | Medium | Narrow collision capsule, high forward velocity |
| **GapClose Grab** | `GA_CultistSpearLady_GapClose2_Attack` | Lunge into impalement grab | 1.5x | 50 | **Heavy** | `FirstHitWhichGrabs = true`; unavoidable grab |
| **Predict Sidearm**| `GA_CultistSpearLady_PredictSidearmAttacks` | Passive state monitoring player | — | — | — | Triggers `GA_CultistSpearLady_Evade` on shot |
| **Evade** | `GA_CultistSpearLady_Evade` | Cartwheel backflip | — | — | — | Grants i-frames (`GE_Invulnerability`) against projectiles |

---

### 2.3 The Sicario (`Characters/Enemies/Sicario/`)
Lethal dual-dagger assassins with active parry stances and bleed application.

| Ability | Class / Base | Motion / Delivery | Multiplier | Poise Dmg | Reaction | Special Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **Rush Flurry** | `GA_Sicario_Attack_Rush` | 4-hit running dual-blade cross | 1.0x | 30 | Medium | Inflicts Bleed 4 on hit; very fast queue |
| **Counter-Parry** | `GA_Enemy_Parry_Attack` | Enters defensive parry stance | — | — | `Blocked` | If player strikes during window, player gets staggered! |
| **Counter Strike** | `GA_Sicario_Counter_Attack` | Riposte slash following parry | 1.8x | 60 | **Heavy** | Punishes player spamming light attacks |
| **Backstab Teleport**| `GA_Sicario_Vanish_Behind` | Smokescreen into behind lunge | 1.5x | 50 | **Flyback** | Triggers `State.Hit.Back` on player |

---

### 2.4 The Grisha (`Characters/Enemies/Grisha/`)
Brutal four-armed beasts with high poise, shockwaves, and bone-crushing grab attacks.

| Ability | Class / Base | Motion / Delivery | Multiplier | Poise Dmg | Reaction | Special Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **Double Swipe** | `GA_Grisha_Attack_DoubleSwipe` | Wide horizontal claw sweeps | 1.2x | 50 | Medium | Hits in 180-degree frontal arc |
| **Triple Claw** | `GA_Grisha_Attack_3HitCombo` | Left, Right, Overhead slam | 1.5x | 70 | **Heavy** | Overhead slam cracks the ground; AOE radius 250cm |
| **Head Smash** | `GA_Grisha_Attack_HeadSmash` | Two-handed skull crush | 1.7x | 90 | **Flyback** | Knocks player completely down; breaks shield |
| **Roar Shockwave** | `GA_Grisha_Roar_Handler` | Intimidating roar impulse | Flat 10 | 100 | **Heavy** | Knocks player back 800cm; interrupts montages |
| **Bite Grab** | `GA_Grisha_Grab_Attack` | One-handed scoop into head bite | 2.5x | 100 | **Flyback** | `FirstHitWhichGrabs = true`; unparryable; strips shell |

---

### 2.5 The Vampire (`Characters/Enemies/Vampire/`)
Wall-crawling, ceiling-stalking horrors with lethal grab ambushes.

| Ability | Class / Base | Motion / Delivery | Multiplier | Poise Dmg | Reaction | Special Notes |
| :--- | :--- | :--- | :---: | :---: | :---: | :--- |
| **Claw Combo** | `GA_Vampire_Attack_Combo` | 3-hit slashing string | 1.0x | 25 | Medium | Applies Bleed stacks |
| **Bite Grab** | `GA_Vampire_Attack_Bite` | Pounce grab onto player neck | 2.0x | 80 | **Heavy** | Pins player for 4.8s; drains health to heal vampire |
| **Sky Spawn Dive**| `GA_Vampire_SkySpawn` | Drops 5000cm from ceiling | 2.0x | 90 | **Flyback** | Surprise ambush from shadows; unavoidable grab |

---

## 3. Major Story Boss Movesets & Phase Transitions

### 3.1 The Final Boss (`Characters/Bosses/FinalBoss/`)
- **Health Pool**: **35,000 HP** | **Poise**: **500** | **Break**: **250** | **Base Damage**: **104**
- **22 Gameplay Abilities**:

| Ability Class | Montage / Animation | Multiplier | Poise | Reaction | Special Properties |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `GA_FinalBoss_Attack_Close_180_Swipe` | `A_FinalBoss_Attack_Close_180_Swipe_Montage` | **1.5x** (156 dmg) | 45 | **Flyback** | Broad 180 sweep with fire sword |
| `GA_FinalBoss_Attack_Close_Stomps` | `A_FinalBoss_Attack_Close_Stomps_Montage` | 1.0x (104 dmg) | 40 | Medium | Left foot into right foot ground stomp |
| `GA_FinalBoss_Attack_Close_Sword_Stomps` | `A_FinalBoss_Attack_Close_Sword_Stomps_Montage` | **1.5x** | 50 | **Heavy** | Combined sword slash with ground stomp |
| `GA_FinalBoss_Attack_Fire_Sword_Ground_Hit`| `A_FinalBoss_Attack_Fire_Sword_Ground_Hit_Montage` | **1.7x** (176.8 dmg) | 80 | **Flyback** | Massive overhead slam; shoots flame shockwave |
| `GA_FinalBoss_Attack_SpitFire` | `A_FinalBoss_Attack_SpitFire_Montage` | Flat 2.0 / tick | 20 | Weak | **Unparryable** (`GE_UnparryableHit`); stream of fire DoT |
| `GA_FinalBoss_Attack_Stab_1Hit` | `A_FinalBoss_Attack_Stab_1Hit_Montage` | **1.5x** | 45 | **Flyback** | Forward lunging impale |
| `GA_FinalBoss_Attack_Stab_2Hit` | `A_FinalBoss_Attack_Stab_2Hit_Montage` | **1.5x** | 45 | **Flyback** | Double thrust |
| `GA_FinalBoss_Attacks_Grab` | `A_FinalBoss_Attacks_Grab_Montage` | 2.5x (260 dmg) | 100 | **Heavy** | Unparryable choke grab followed by sword run-through |
| `GA_FinalBoss_Attack_Balls_Close` | `A_FinalBoss_Attack_Balls_Close_Montage` | **1.5x** | 45 | **Flyback** | Conjures homing dark energy spheres |
| `GA_FinalBoss_Attack_Defense_End` | `A_FinalBoss_Attack_Defense_End_Montage` | Flat 2.0 | 20 | Weak | **Unparryable** counter-blast when defense stance ends |
| `GA_FinalBoss_Attack_JumpBack` / `JumpFwd` | Jump montages with ground impact | 1.0x | 40 | **Flyback** | Repositions across the arena with impact radius |
| `GA_FinalBoss_StunPhaseChange` | Phase transition animation | — | — | — | Invulnerable; ignites arena in flames; alters BT tree |

---

### 3.2 The Hexapod (`Characters/Bosses/Hexapod/`)
- **Health Pool**: **8,000 HP** | **Poise**: **1,200** (Highest in game) | **Break**: **140** | **Base Damage**: **81**
- **29 Gameplay Abilities**:

| Ability Class | Montage / Animation | Multiplier | Poise | Reaction | Special Properties |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `GA_Hexapod_Attacks_Attack_01` | Front claw crush | **1.7x** (137.7 dmg) | 70 | **Flyback** | Pre-damage: `GE_OnHitDamageImmunity` (multi-hit grace) |
| `GA_Hexapod_Attacks_Attack_02` | Alternating mandible swipes | 1.0x (81 dmg) | 40 | **Flyback** | Fast tracking |
| `GA_Hexapod_Attacks_Attack_03_Combo` | 4-hit scythe-leg barrage | 1.0x | 40 | **Flyback** | Overwhelms player stamina / guard meter |
| `GA_Hexapod_Attacks_Attack_03_Left/Right`| Lateral sweep attacks | 1.0x | 40 | **Flyback** | Covers flanks |
| `GA_Hexapod_Attacks_Attack_04` | Overhead body slam | 1.5x | 90 | **Flyback** | Massive area shockwave |
| `GA_Hexapod_Attacks_Attack_06` | Tail sting pierce | 1.2x | 60 | **Flyback** | Inflicts Poison stacks |
| `GA_Hexapod_Attacks_Attack_07_AoE_Shooting`| Acid mortar barrage | 0.8x / pellet | 80 | Medium | **Unparryable** (`GE_UnparryableHit`); acidic puddles |
| `GA_Hexapod_Attacks_Dive` | Burrows underground | **1.7x** | 70 | **Flyback** | Completely untargetable while submerged |
| `GA_Hexapod_Attacks_Lurk` | Underground stalk | **1.5x** | 80 | **Heavy** | **Pierces Invulnerability** (`GE_DamageIgnoreImmunity`)! |
| `GA_Hexapod_Grab_Attack` | Mandible pin & chew | 2.0x | 100 | **Heavy** | Unparryable grab |

---

### 3.3 The Dwarf Boss (`Characters/Bosses/Dwarf/`)
- **Health Pool**: **6,900 HP** | **Poise**: **400** | **Break**: **220** | **Base Damage**: **70**
- **17 Gameplay Abilities**:

| Ability Class | Montage / Animation | Multiplier | Poise | Reaction | Special Properties |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `GA_DwarfBoss_Attack_3HitCombo` | Triple hammer swing | 1.2x | 60 | **Heavy** | Wide horizontal sweeps |
| `GA_DwarfBoss_Attack_DoubleSwing_Overhead` | 2 horizontal swings into vertical slam | 1.5x | 80 | **Flyback** | Final slam cracks arena floor |
| `GA_DwarfBoss_Attack_TripleOverhead` | Three consecutive jumping overhead slams | 1.5x | 90 | **Flyback** | Each slam generates 300cm shockwave |
| `GA_DwarfBoss_Attack_Spins` | Continuous spinning hammer whirlwind | 1.0x | 40 | **Heavy** | Can deflect incoming player projectiles |
| `GA_DwarfBoss_Attack_Stomp` | Ground stomp | 1.0x | 50 | Medium | Quick defensive check when player is behind |
| `GA_DwarfBoss_Attack_WeaponBuff` | Buffs hammer with molten tar | — | — | — | Grants Burn passive to all subsequent attacks |
| `GA_DwarfBoss_Attack_HeadExplode` | Head detonates in molten fire | 2.0x (140 dmg) | 120 | **Flyback** | Massive explosion; triggers Phase 2 frenzy |

---

## 4. Hit Reaction Sets (`HRAS_*`) & Quick Ripostes

### 4.1 Hit Reaction Redirect Rules
In [`HRAS_*`](./combat-damage-pipeline.md#L248), enemies do not play flinch animations on normal hits due to conditional redirect logic:
1. `Event.Reaction.Hit.Medium` (Standard swing from player):
   - If target has `State.HyperArmor` and **not** `State.PoiseBroken`: Redirects to `Event.Reaction.Hit.Weak` (additive wobble; does not interrupt attack).
   - If target does **not** have `State.PoiseBroken`: Redirects to `Event.Reaction.Hit.Ignore` (no animation played, attack continues uninterrupted!).
   - Only when **`State.PoiseBroken`** is active does `Hit.Medium` actually interrupt and stagger the enemy!
2. `Event.Reaction.Hit.Weak`:
   - If target has `State.PoiseBroken`: Promoted to `Hit.Medium` (guaranteed flinch).
3. `Event.Reaction.Hit.Flyback` (Heavy Hammer, Martyr's Blade heavies, finishers):
   - Overrides poise! Forces the enemy into ragdoll displacement or flying knockdown unless immune.

### 4.2 Quick Riposte Synchronization (`EnemiesWithQuickRiposte`)
Standard humanoid enemies trigger extended paired animations (`Event.Reaction.Sync.Parry.Human.1..4`). However, elite enemies and bosses carry the tag in [`GA_Parry_Riposte_C::EnemiesWithQuickRiposte`](./combat-pipeline-resolutions.md#L30):
- `CharacterId.Enemy.LadyOfTheWoods`
- `CharacterId.Enemy.MothKnight`
- `CharacterId.Enemy.TarGolem` / `TarGolemParasite` / `ParasiteGolem`
- `CharacterId.Enemy.HeadBoss`
- `CharacterId.Enemy.Offspring`
- `CharacterId.Enemy.FinalBoss`
- `CharacterId.Enemy.Monolith`
- `CharacterId.Enemy.DwarfBoss`
- `CharacterId.Enemy.Swordman`

For these bosses, the game skips the long paired cinematic grab and executes the **Quick Riposte** variant (`InstigatorMontages_Quick`), dealing health percentage damage and immediately returning control to the player and boss.
