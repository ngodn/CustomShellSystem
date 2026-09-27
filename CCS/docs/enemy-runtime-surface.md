# Mortal Shell II: Enemy Runtime Surface & Modding Interception

Written 27 September 2026 from the UE4SS reflection dump (`Binaries/Win64/ue4ss/CXXHeaderDump`, build `MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`). Companion to [`combat-runtime-surface.md`](./combat-runtime-surface.md), [`enemy-damage-pipeline.md`](./enemy-damage-pipeline.md), and [`architecture-reference.md`](./architecture-reference.md).

---

## 1. Native Class Hierarchy & Memory Layout for Enemies

All offsets are from the official reflection dump for UE 5.6.1:

### 1.1 Spawner & World Subsystems
| Class (Dump Location) | Parent Chain | Offsets & What Matters for CCS |
| :--- | :--- | :--- |
| `USpartaEnemySubsystem` (`Sparta.hpp:5130`) | `UTickableWorldSubsystem` | `ActiveEnemies` array (active spawned enemy pawns). Delegates: `OnSpawnEffects` 0x98, `OnSpawnAbilities` 0xA8. Central point to inject blanket mod effects/abilities onto all enemies upon spawning without per-frame scans. |
| `USpartaWorldGenSubsystem` (`Sparta.hpp:8570`) | `UWorldSubsystem` | `GetDifficulty(BiomeTag)` 0x68, `SetDifficulty`, `IncreaseDifficulty`, `OnDifficultyChanged`. Controls regional biome difficulty integers driving `GE_Regional_DamageMultiplier`. |
| `USpartaAssistSubsystem` (`Sparta.hpp:4150`) | `UGameInstanceSubsystem` | `GetSubsystem(WorldContext)`, `GetScalesForBoss(BossId) -> FSpartaBossAssistScales {HealthScale, DamageScale, BreakResistanceScale}`, `SetAdaptiveDifficultyEnabled`, `IsBossAssisted`, `IsBossChallenged`. Allows CCS to dynamically scale boss stats. |
| `USpartaAssistSettings` (`Sparta.hpp:4120`) | `UDeveloperSettings` | `Get()`, `GraceDeaths` 0x50, `RampDeaths` 0x54, `MinDamageScale` 0x5C, `MinHealthScale` 0x64, `MaxDamageScale` 0x17C, `MaxHealthScale` 0x180, `MaxBreakResistanceScale` 0x184. |

---

### 1.2 Enemy Character & AI Controller
| Class (Dump Location) | Parent Chain | Offsets & What Matters for CCS |
| :--- | :--- | :--- |
| `ASpartaCharacter` (`Sparta.hpp:1805`) | `ACSCharacter` → `ACharacter` | Shared base for player and enemies. Components: `HealthComponent` 0x6B0, `WeaponsComponent` 0x6C8, `HitTraceComponent` 0x6D0, `MotionWarpingComponent` 0x6E8, `AbilitySystemComponent` 0xAB8 (`USpartaAbilitySystemComponent*`), `CharacterData` 0xAD0 (`USpartaCharacterData*`), `HitReactionAnimSet` 0x6B8. Functions: `IsDead()`, `GetAnimInstance()`. |
| `ASpartaAIController` (`Sparta.hpp:3870`) | `AModularAIController` → `AAIController` | `BaseBehaviorTree` 0x3E0 (`UBehaviorTree*`). Functions: `SetDynamicSubtree(InjectTag, BehaviorAsset)`, `GetDynamicSubtree`. Allows injecting custom behavior trees or changing AI attack decision logic. |
| `USpartaCharacterData` (`Sparta.hpp:4540`) | `UDataAsset` | Static archetype asset (`CD_*`). `BaseDamage` 0x340 (`FBaseDamageData {BaseDamage, BaseRangedDamage}`), `AttributeData` 0x1E0 (`TArray<FSpartaCharacterAttributeData>`), `UpgradeStatValues` map 0x108 (elemental passives), `bIsBoss` 0x48, `CharacterScaleOverride` 0x68. |
| `USpartaHitReactionAnimSet` (`Sparta.hpp:5668`) | `UDataAsset` | `GetHitReactionAnimation(ReactionType, StrikeDirection, ASC, SourceTags, out bAdditive, out bRandomize, out Range, out ChosenTag, out Cooldown)`. Controls flinch, stagger, and redirect rules. |

---

## 2. Enemy Modding Mechanisms: Ranked Matrix

To satisfy the core requirement of **high performance, no lag, and no FPS drop**, here are the ranked options for intercepting and tuning enemies:

### 2.1 Enemy Damage Tuning (Outgoing to Player)
1. **Rank 1: Write `CD_*.BaseDamage` CDO / Asset Defaults (Zero Frame Cost)**:
   - In Unreal, 60 of 88 enemy attack abilities resolve their payload damage directly through `USpartaCharacterData::BaseDamage`.
   - Modifying `USpartaCharacterData.BaseDamage` at game startup or level load permanently tunes enemy damage across the entire game with **0.00 ms runtime overhead**.
2. **Rank 2: Inject Blanket Gameplay Effect via `USpartaEnemySubsystem::OnSpawnEffects` (Zero Tick Cost)**:
   - When any enemy spawns, `USpartaEnemySubsystem` fires `OnSpawnEffects`.
   - Applying a custom GE that modifies `SpartaCombatSet.BaseDamage` additively or multiplicatively adjusts all enemies cleanly through native GAS.
3. **Rank 3: Pre-Hook on `USpartaHitPayload::ApplyPayload` (Native UFunction Hook)**:
   - When an enemy lands a strike, pre-hook `ApplyPayload`, check if `Instigator` is an enemy (`Team != Player`), and scale `HealthDamage.Multiplier` or `FloatValue`.

---

### 2.2 Enemy Stagger & Poise Tuning (Making Enemies Flinch)
1. **Rank 1: Modify `SpartaHealthSet.MaxPoise` on Spawn**:
   - Standard regular enemies have 30–125 poise, and elites have 130–200 poise.
   - Setting `MaxPoise` to `5.0` on spawned enemies causes them to flinch and stagger on **every single player swing** (mimicking Dark Souls 1 poise break).
   - Setting `MaxPoise` to `9999.0` grants them permanent hyperarmor.
2. **Rank 2: Pre-Hook `USpartaHitReactionAnimSet::GetHitReactionAnimation`**:
   - In [`HRAS_*`](./enemy-movesets.md#L180), `Event.Reaction.Hit.Medium` is redirected to `Ignore` unless `State.PoiseBroken` is active.
   - Hooking this native function or replacing the redirect table allows forcing an additive flinch on every hit regardless of poise.

---

### 2.3 Break Meter & Parry Tuning
1. **Rank 1: Modify `SpartaHealthSet.MaxBreakResistance`**:
   - Major bosses have **220 to 250** Break Resistance.
   - Writing `MaxBreakResistance = 55.0` makes any major boss enter the `State.Broken` vulnerability state after **a single successful parry** (since 1 parry deals 55 break damage!).
2. **Rank 2: Scale Parry Break Damage in `GA_Parry_Successful`**:
   - Writing `UGA_Parry_Successful_C::BreakDamage` (0x700) scales how much break meter each parry depletes.

---

### 2.4 Unparryable Attacks & Grabs (Disabling "Red" Attacks)
1. **Rank 1: Strip `GE_UnparryableHit` from Attack Payloads**:
   - Unparryable enemy attacks carry `GE_UnparryableHit_C` in their `AdditionalEffects` list.
   - Removing this effect from the attack's `AbilityHitPayload` allows the player to **parry and harden through any boss attack in the game** (including the Final Boss's fire sword slam and Hexapod's lurk strikes).
2. **Rank 2: Disable Grab Attack Targeting**:
   - Grab abilities derive from `GA_Attack_Base_Melee_GrabAttack`.
   - Clearing `PossibleTargets_Player` prevents enemies from executing unavoidable cinematic grab attacks.

---

### 2.5 Boss Aggression & Attack Cadence
1. **Rank 1: Modify `USpartaGameplayAbility::GlobalCooldownDuration` & `CooldownDuration`**:
   - Every enemy attack ability instance has `CooldownDuration` (0x3AC) and `GlobalCooldownDuration` (0x3CC).
   - Reducing these values eliminates the idle "pacing" delay between boss attacks, creating relentless ultra-aggressive boss encounters.
2. **Rank 2: Tune `USpartaAssistSettings` Challenge Tiers**:
   - Setting `USpartaAssistSettings.MaxChallengeTier` and raising `MaxDamageScale` / `MaxHealthScale` activates the game's built-in challenge scaling without touching raw code.

---

## 3. High-Performance Safety Rules for Enemy Modding

When interacting with dozens of active AI enemies simultaneously, strict memory hygiene is required:

1. **Never Cache Raw `ASpartaCharacter*` Across Frames**:
   - Enemies despawn, die, and get garbage collected frequently.
   - Always resolve enemy pointers through validated `OwnerGuard` handles (Object Array Index + Internal Serial Number) as established in [`architecture-reference.md`](./architecture-reference.md#L1032).
2. **Game Thread Safety**:
   - AI Behavior Trees and character ticking run strictly on the game thread.
   - Any property manipulation or gameplay effect application must occur within `RegisterEngineTickPostCallback`.
3. **No Iterative Scans of `GUObjectArray`**:
   - Never search for enemies via `UObjectGlobals::FindAll` or `FindFirstOf`.
   - Access active enemies strictly through `USpartaEnemySubsystem::ActiveEnemies` (0x68) or listen to `OnSpawnEffects`.
