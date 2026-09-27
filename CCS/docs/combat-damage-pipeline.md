# Mortal Shell II: the damage, poise, break and Resolve pipeline

Written 27 September 2026 from the installed build (`MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`, UE 5.6.1). It sits on top of `docs/ms2-combat-system.md` (roster, weapon `BaseDamage`, seals, Tarstones) and does not repeat that material. Working files, raw exports, CSVs and the scripts that produced them are in `CCS/work/damage-pipeline/` (see its `README.md`).

Evidence labels used throughout:

- **[data]** read from cooked assets (CUE4Parse + usmap export; class defaults, GameplayEffect modifiers and executions, DataTable rows, DataAssets). Asset names are real and can be re-exported.
- **[native]** read from the UE4SS reflection dump `Binaries/Win64/ue4ss/CXXHeaderDump/Sparta.hpp` (UPROPERTY layout and UFUNCTION signatures of the game's C++ classes). The bodies of native functions are not readable; only their names, parameters and the attributes they own.
- **[inferred]** follows from names, defaults, standard Gameplay Ability System (GAS) semantics or the shape of a signature. Marked wherever it is not a direct read.
- **[unknown]** not recoverable offline. Blueprint graphs are not decompiled by the export tool; where the logic lives in a graph the text says "BP graph".

No em dashes are used in this document.

---

## 1. The melee hit, end to end

Every step names the class or asset and whether it is native C++ or Blueprint (BP). Base classes: `USpartaGameplayAbility : UGameplayAbility`, `USpartaAttackAbility : USpartaGameplayAbility` (owns `AbilityHitPayload` and `AbilityHitPayloadDataModified`), `USpartaAbilitySystemComponent : UAbilitySystemComponent`, `USpartaGameplayEffect : UGameplayEffect`, `USpartaStatusEffect : USpartaGameplayEffect` [native].

1. **Input.** `GA_Attack_Primary` / `GA_Attack_Secondary` / `GA_Attack_Primary_Release` (BP, `Sparta/Core/Player/Ability/Input/`, tag `Ability.Input`) are thin input abilities [data]. Which weapon attack they trigger is BP graph (combo counter `BPC_Player_ComboCounter`, `ResetComboCountOnAbilityEnd` on the attack ability) [inferred].
2. **Attack ability.** `GA_Player_Attack_<Weapon>_<A1..A3|B1..B3|_Hold|_Finisher>` and `GA_Running_Attack_*` all derive `GA_AttackBase_Melee < GA_AttackBase < GA_PlayMontageBase < GA_SpartaBase < USpartaAttackAbility` [data]. Defaults from `GA_AttackBase_Melee`: tags `Ability.Attack.Melee`, `Ability.Primary`; `ActivationOwnedTags = State.Attack.Melee`; blocked by `State.Block.Ability.Attack.Melee`, `State.Block.Ability.Primary`; `CancelAbilitiesWithTag = Ability.Primary`; `InstancingPolicy = InstancedPerExecution`; `ActivationGroup = Exclusive_Replaceable`; `UseMeleeAssist = true` (soft targeting + motion warping, `WarpTranslationStoppingDistance 150`); `HitStopBlendIn/Out 0.2 s`; `HoldAttackDamage = (1.0, 2.0)`; `ResolveMultiplier = 1.0`; `AIGlobalCooldownTime = 3.0` [data]. 175 abilities derive from it (`work/damage-pipeline/attack_payloads.csv`).
3. **Payload.** Each attack ability owns an instanced `BP_SpartaHitPayload` (native base `USpartaHitPayload`) [data/native]. Payload fields [native]: `HealthDamage`, `PoiseDamageOption`, `BreakDamageOption` (each an `FHealthDamageOption {Type, FloatValue, Multiplier}`), `DamageEffect`, `PoiseDamageEffect`, `BreakDamageEffect`, `PoiseBrokenEffect`, `AdditionalEffects[] {Effect, ApplyOrder PreDamage|PostDamage|PostEvent}`, `DamageDataTag`, `PoiseDamageDataTag`, `BreakDamageDataTag`, `ReactionTag`, `StrikeDirection`, `PoiseDamage`, `BreakDamage`, `PayloadTags`, `CustomElementalStacks`, and the results `CalculatedDamage`, `CalculatedPoiseDamage`, `CalculatedBreakDamage`, `DamageDealt`, `ChosenReactionTag`, `CachedSourceTags/TargetTags/TargetTagsPostApply`, `MergedPayloads`. `EHealthDamageType = {Float, InstigatingCharacterData, InstigatingCharacterWeapon}` [native]. Base payload defaults (`BP_SpartaHitPayload`) [data]: `DamageEffect = GE_DamageBase`, `PoiseDamage = 20`, `PoiseDamageEffect = GE_PoiseDamage`, `BreakDamageEffect = GE_BreakDamage`, `PoiseBrokenEffect = GE_PoiseBroken`, `AdditionalEffects = [GE_BlockPoiseRegen PreDamage]`, data tags `DataTag.HealthDamageMagnitude / PoiseDamageMagnitude / BreakDamageMagnitude`, `MotionWarpingTag = MotionWarping.Reaction`. Player attacks override to `HealthDamage.Type = InstigatingCharacterWeapon` (optionally `Multiplier`), `DamageEffect = GE_MeleeDamageBase`, a per-attack `PoiseDamage`, `ReactionTag = Event.Reaction.Hit.Medium` and extra pre-damage effects (section 3.2).
4. **Montage and hit window.** `GA_PlayMontageBase` plays the attack montage (BP). Hit windows are `USpartaAnimNotifyState_HitCheck` / `USpartaAnimNotify_HitCheck` notifies [native] carrying `DamagePayload`, `WeaponSlot`, `TraceSetup (UseWeaponDefaults|Manual)`, `TraceSource (Weapon|Character)`, `TraceType (CollisionShapes|ComponentReferences|PhysicsBodies)`, `TeamRelationship`, `TraceProfile`, `bAllowMultipleHitsOnSameActor`, `MultipleHitOnSameActorDelay`, `bUseSharedHitList/SharedHitListId`, `MotionSubstep`, `UseDeferredHitDispatch`, `EnvironmentalCollisionChannels`. Example: `ANS_ChainsawHitCheck` (multi-hit every 0.115 s, only while `State.Special.ClockworkScythe.Active`) [data]. Montage notify instances are not exported, so hit counts per swing are [unknown].
5. **Trace.** `USpartaHitTraceComponent` on `ASpartaCharacter` runs `USpartaHitTrace` objects (`CreateHitTrace(Params, Source, OptionalWeapon, TraceData)`, `BeginContinuousCheck/CheckOnce`, per-trace `HitList<Actor,float>`, `IgnoreList`, `TraceShapes`, `PrimitiveComponents`); default shapes come from `ASpartaWeapon::GetDefaultCollisionShapes()` (capsule per `FWeaponVariantData.CapsuleCollisionHalfHeight/Radius`); hits are queued in `USpartaHitTraceSubsystem.PendingHits` when deferred and raised through `OnTraceHit(Trace, HitResult)` [native]. The receiving side implements `ISpartaHittable::GetHitResponse(Source, Hit) -> {Hit, Block, Ignore}` and `SpartaApplyHit(HitResult, Instigator, Payload, DamageCauser)`; the attacker side gets `ISpartaHitApplier::SpartaHitApplied` and `ASpartaWeapon::WeaponHit(...)` / `USpartaWeaponComponent_Base::OnWeaponHit` [native].
6. **Payload application** (`USpartaHitPayload::ApplyPayload(Target, ApplyingAbility, HitResult, DamageCauser)` or `ApplyPayloadInstigator(...)`) [native]. Observable sub-steps: (a) hit payload modifiers on the source ASC (`ActivePayloadModifiersArray`, `UHitPayloadModifier.bOutgoingPayloadModifier = true`) and on the target (`ActiveIncomingPayloadModifiersArray`) run `CanApplyPayloadModifier` (tag queries `SourceQuery/TargetQuery`) then `ApplyPayloadModifier`, which can rewrite the payload and append `AdditionalEffects` [native]; these are granted by `USpartaGameplayEffect.HitPayloadModifier / IncomingHitPayloadModifier` on active effects [native/data] (examples: `HPM_Crazed_Melee`, `HPM_Tiel_Behind_Damage/CritChance/CritDamage`, `HPM_Status_Cursed`, `HPM_Slaughterer`, `HPM_KnightLady`, `HPM_Thorn_Passive`, `HPM_ShellSummon_*`). (b) `CalculateDamage(SourceASC, DamageCauser)` resolves `HealthDamage`: `Float` uses `FloatValue`; `InstigatingCharacterData` uses `USpartaCharacterData.BaseDamage.BaseDamage` (the enemy's `CD_*`); `InstigatingCharacterWeapon` uses `ASpartaWeapon.BaseDamage` of the weapon in the payload's slot; the result is scaled by `Multiplier` [native names, semantics inferred]. `CalculatePoiseDamage` and `CalculateBreakDamage` do the same for poise and break (falling back to the plain `PoiseDamage` / `BreakDamage` floats) [inferred]. (c) `PreDamage` additional effects are applied to the target, then the `DamageEffect` spec with SetByCaller `DataTag.HealthDamageMagnitude = CalculatedDamage`, then `PoiseDamageEffect` with `DataTag.PoiseDamageMagnitude`, then `BreakDamageEffect` with `DataTag.BreakDamageMagnitude` when non-zero, then `PostDamage` effects, then the reaction gameplay event `ReactionTag` with `StrikeDirection`, then `PostEvent` effects [inferred from `ESpartaApplyOrder` and the data tags]. (d) `DamageDealt` is written back; `MergePayloads` merges several payloads landing in the same frame (`MergedPayloads`) [native].
7. **Health damage effect.** `GE_MeleeDamageBase < GE_DamageBase` [data]: Instant; one execution `SpartaDamageExecution` with a captured-attribute scoped modifier `SpartaCombatSet.BaseDamage [Source, snapshot] Additive SetByCaller(DataTag.HealthDamageMagnitude)`; application blocked while the target has `GameplayEffect.Immunity.Damage`; asset tag `GameplayEffect.Damage.Melee`. Siblings: `GE_RangedDamage_Base` (`.Ranged`), `GE_AOE_Damage_Base` (`.AOE`), `GE_UnarmedDamage`, `GE_ShellAbilityDamage`, `GE_MeleeDamage_ShellAbility`, `GE_RangedDamage_ShellAbility`, `GE_RiposteDamage` (`.Riposte` + `.Options.DisableCrit`, and no immunity check), `GE_DamageIgnoreImmunity` / `GE_FallDamage`, `GE_DamageOverTimeBase` (`.OverTime` + `DisableCrit`, period 7 s, duration SetByCaller), `GE_ClockworkChainsawDamage` (`DisableCrit`) [data].
8. **Execution.** `USpartaDamageExecution : USpartaExecutionCalculation : UGameplayEffectExecutionCalculation` [native]. Its inputs are visible from the modifier hook it calls: `UDamageExecutionModifier::ApplyDamageExecutionModifier(float& Damage, float& DamageReduction, float& IncomingDamageMultiplier, float& CriticalChance, float& CriticalBonus, SourceASC, TargetASC, Spec)` and `ApplyPostDamageReductionModifier(float& Damage, ...)` [native]. Modifiers come from `USpartaGameplayEffect.ExecutionValueModifiers` on active effects (`USpartaAbilitySystemComponent.ActiveExecutionValueModifiersArray`, `bOutgoingModifier`) and `ExecutionModifiers` (`UExecutionModifierCalculation {TargetTagsQuery, SourceTagsQuery, BaseValue}`, e.g. `UWindedDamageBonus`, `UWindedPoiseDamageBonus` with `UpgradeStat` tag) [native]. The execution writes the meta attribute `SpartaHealthSet.Damage` [native attribute, inferred use]; `USpartaHealthSet` then moves it into `ShellHealth` (while `Character.State.Shell`) or `Health`, clamped by `MinShellHealth` / `MinHealth` [inferred; `GE_EffectMaxHealth_Multiplier` writes `Healing` the same way].
9. **Notifications.** `USpartaHealthComponent.OnDamageReceived(HealthComponent, Value, Instigator, EffectSpec)`, `OnShellHealthDepleted`, `OnDeathStarted/Finished`, `LastDamageTimeMap<Actor,float>`; `ASpartaCharacter.OnHitReceived(InstigatorActor, InstigatorASC, Payload, EffectContext, DamageCauser, HitResult)`, `OnHitApplied(Hit, HitResult, DamagingWeapon, Instigator)`, `OnHealthDamageCaused(Value, Victim, ContextHandle)`, `CharacterHit(DamagingWeapon, Damage, EventData)`; global event `UGEDamageEvent {Instigator, Target, DamagePayload, InstigatorTags, TargetTags, Magnitude}`; `USpartaDamageDebugSubsystem::HandleDamageReceived`; `USpartaTelemetrySubsystem::OnDamageReceived` [native]. Damage numbers are `BPC_DamageNumbers`, suppressed by `UI.Block.DamageNumbers` (`GE_Block_DamageNumbers`) [data].
10. **Poise.** `GE_PoiseDamage` (Instant, `SpartaPoiseExecution`, captures `SpartaCombatSet.BasePoiseDamage [Source, snapshot] + SetByCaller(DataTag.PoiseDamageMagnitude)`, blocked by `GameplayEffect.Immunity.Poise`) writes `SpartaHealthSet.PoiseDamage` which drains `Poise` [data, last step inferred]. `UPoiseExecutionModifier::ApplyPoiseExecutionModifier(float& Damage, ...)` is the hook [native]. When `Poise` reaches 0 the payload's `PoiseBrokenEffect = GE_PoiseBroken` (0.01 s, grants `State.PoiseBroken`) is applied [data, trigger inferred]. See section 4 for regen.
11. **Hit reaction.** The reaction event triggers `GA_HitReaction` (BP over `USpartaAttackAbility`; `AbilityTriggers = Event.Reaction`, `GlobalPreventionTags = State.Parry, GameplayEffect.Immunity.HitReaction`, `DeactivationTags = Event.HitReaction.Cancel`, owned `State.Reaction`, `InstancedPerExecution`, retriggerable) or `GA_HitReaction_Bosses` (adds `BossAddionalPreventionTags = AI.Class.Boss.StartPhase2/3`) [data]. The montage is chosen natively by `USpartaHitReactionAnimSet::GetHitReactionAnimation(ReactionType, StrikeDirection, ASC, SourceTags, out bAdditive, out bRandomizeStartTime, out StartTimeRange, out ChosenReactionTag, out ReactionCooldown)` from the character's `HRAS_*` asset (`ASpartaCharacter.HitReactionAnimSet`) [native]; section 4.3 lists the redirect rules that turn `Medium` into `Ignore`/`Weak` unless poise is broken. `GE_ForceFlybackReaction` (0.1 s `State.Hit.Melee.ForceFlyback`, custom requirement `GECAR_ForceFlybackReaction`, blocked by `State.Freeze`) forces the flyback branch [data]. Physical (ragdoll-bone) reactions are a separate `GA_PhysicalHitReaction` granted to every character by its `CD_*.GrantedAbilitySet`, driven by `USpartaPhysicalHitReactionData` bone chains (`PHS_Humanoid`, `PHS_DwarfBoss`, ...) with `PhysReactionCooldown`, plus `GE_PhysHitState` / `GE_ForcePhysReaction` / `State.Hit.IgnorePhysReaction` [data/native].
12. **Knockback.** `GA_Knockback_Handler` (BP, `Ability.Knockback`) [data]; strength inputs are `SpartaCombatSet.BaseKnockbackStrength` (enemy tables 300 to 2000) and `BaseKnockbackMultiplier` (0 to 2, bosses 0.0 to 0.2) [data], and the seal abilities' own `KnockbackStrength` values (section 7) [data]. The exact combination is BP graph [unknown].
13. **Break and riposte.** `GE_BreakDamage` (Instant, `SpartaBreakExecution`, captures `SpartaCombatSet.BaseBreakDamage [Source, snapshot] + SetByCaller(DataTag.BreakDamageMagnitude)`, blocked by `GameplayEffect.Immunity.Break`, `State.Riposte`, `State.Death`, `AI.State.DeathEnd`) writes `SpartaHealthSet.BreakDamage`, draining `BreakResistance` [data, last step inferred]; `UBreakExecutionModifier::ApplyBreakExecutionModifier(float& Damage, float& DamageReduction, ...)` is the hook and `SpartaCombatSet.BaseBreakDamageReduction` the target-side reduction [native, inferred pairing]. Normal weapon swings carry `BreakDamage = 0`, so the yellow bar is only filled by the sources in section 4.4. At 0 the enemy gets `GE_StateBroken` (`State.Broken`, infinite, `GameplayEffect.State.Broken`) via `GA_ParryResistanceHandler` (BP, granted to enemies; its `StunReactionPayload` applies `GE_BlockPoiseRegen` + `GE_BreakShield` and reaction `Event.Reaction.Parried`) [data, trigger inferred]. Light Attack on a broken enemy raises `Event.Ability.Riposte.Activate`, which triggers `GA_Parry_Riposte` (`Ability.Riposte`, `Ability.Attack.Melee.InfiniteSeal`; blocked by `State.Block.Ability.Riposte` from `GE_BlockRiposteAttack`; requires the unlock tag `Character.Unlocked.Riposte` from `GE_Unlock_Riposte`) [data]. Its `RipostePayload`: `HealthDamage = InstigatingCharacterWeapon, FloatValue 100`, `DamageEffect = GE_RiposteDamage`, `PoiseDamage 0` [data]. The riposte state `GE_Riposte` grants `State.Riposte` + `GameplayEffect.Immunity.Break` and on completion applies `GE_ParryResistanceReset` (`BreakResistance := MaxBreakResistance`) and `GE_RemoveBrokenState` [data]. Paired animations are keyed by `Event.Reaction.Sync.Parry.<Type>` (Human.1..4, Large.1, per-boss); "quick" ripostes for `EnemiesWithQuickRiposte` (Lady of the Woods, Moth Knight, Tar Golem, Head Boss, Offspring, Final Boss, Monolith, Dwarf Boss, Tar Golem Parasite, Parasite Golem, Swordman) [data]. `SpartaCombatSet.RiposteDamageMultiplier` and `SpartaHealthSet.RiposteWeakness` (1.0 in every enemy table) are the scaling attributes for the "riposte scales with enemy health" Week 1 change [native names; use inferred].

Enemy hits on the player follow the same path from step 3 onward: enemy attack abilities (`GA_BrigBase_Attack_*`, `GA_Cultist*`, boss `GA_*`) own `BP_SpartaHitPayload` instances whose `HealthDamage` is left at the native default and therefore resolves through the enemy's `CD_*.BaseDamage` [data: 60 of 88 enemy payloads inherit; `CD_*` has `BaseDamage` on 142 of 193 characters] with per-attack `Multiplier` (0.4, 0.5, 1.5, 1.7 seen) or a fixed `Float`, `DamageEffect = GE_MeleeDamageBase | GE_RangedDamage_Base | GE_AOE_Damage_Base`, reaction `Medium` (33) / `Flyback` (18) / `Heavy` (12) / `Weak` (9), and often `GE_OnHitDamageImmunity` (0.15 s `Immunity.Damage`, multi-hit protection) or `GE_UnparryableHit` as pre-damage effects (`work/damage-pipeline/enemy_attack_payloads.csv`).

---

## 2. Attribute sets

All sets derive `USpartaAttributeSet : UAttributeSet` [native]. Initial values come from `USpartaCharacterData.AttributeData[] {AttributeSet, DefaultStartingTable}` per character [data]; rows are named `<Set>.<Attribute>` with `{BaseValue, MinValue, MaxValue, bCanStack}` (the `MinValue/MaxValue` columns are inconsistent, e.g. `MaxHealth` has `MaxValue 1.0`, so treat them as unused metadata) [data].

### 2.1 `USpartaHealthSet` (offset 0x60 onward) [native]

| Attribute | Player (`DT_PlayerAttributes`) | Notes |
|---|---|---|
| Health / MaxHealth | MaxHealth 50 (Harbinger) | Shell health is separate |
| ShellHealth / MaxShellHealth | 0 (set by `DT_Attributes_<Shell>`: Eredrim 110, Tiel 85) | damage lands here first while `Character.State.Shell` [inferred] |
| MinHealth / MinShellHealth | absent | `TarGolem` table sets `MinHealth 1` (cannot die by damage) |
| Armor | absent everywhere | `FSpartaCharacterAttributeData.Armor` exists but no table sets it; no reader found |
| Resolve / MaxResolve | 100 / 100 | |
| Poise / MaxPoise | 5 / 5 | |
| BreakResistance / MaxBreakResistance | 75 / 75 | player guard meter (section 7.1) and enemy break bar |
| BleedResistance (+Multiplier) | 0 (shells 6 to 10) | |
| FrostResistance (+Increment) | shells 5 to 7 | stack cap for `GE_FrostStack` (`BoundAttribute`) |
| ShockResistance (+Increment) | shells 7 | stack cap for `GE_LightningStack` |
| WaspsResistance (+Increment) | 7 | |
| CurseResistance (+Increment) | shells 6 to 8 | stack cap for `GE_CurseStack` |
| ConfusionResistance (+Increment), ConfusionBreakDamage | absent | |
| CosmicDiseaseResistance / Increment | 35 / 5 | |
| StasisWeakness | 99 | stack limit attribute of `GE_StasisStack` (`SpartaAttributeStackLimitComponent`) |
| RiposteWeakness | absent (enemies 1.0) | |
| StoneStunTime | absent (enemies 3 to 8) | Harden counter-stun length |
| Healing, Damage, PoiseDamage, BreakDamage, ResolveModify | meta | written by executions and modifiers, then folded into the real attributes [inferred] |

Legacy rows `SpartaHealthSet.ParryResistance`, `MaxParryResistance`, `BaseParryResistanceGain` appear in 60 enemy tables but no such attribute exists in the native set; GAS skips unknown rows, so these are dead data left from the rename to BreakResistance [data + native, conclusion inferred].

### 2.2 `USpartaCombatSet` [native]

| Attribute | Player default | Used by |
|---|---|---|
| BaseDamage | no row (0) | captured by every damage execution; scaled multiplicatively by Tarforge, NG+, regional, Weak, Genessa Astral |
| BaseHeal | no row | `SpartaHealExecution` |
| BasePoiseDamage | no row (0) | captured by `SpartaPoiseExecution`; `GE_PoiseSuppression` overrides to 0; `GE_Melee_Generic_PoiseBonus` / `GE_Sidearm_Generic_PoiseDamage` / `GE_Permanent_WeaponPoiseDmg` multiply |
| BaseBreakDamage | 0 | captured by `SpartaBreakExecution`; `GE_ParryResistanceDamageBonus` adds |
| AttackSpeedBonus | 0 | `GE_WarpStack` +0.01 per stack |
| BaseWindedCounters | no row | `SpartaWindedExecution` (`GE_IncreaseWindedCounter` +1, `GE_RemoveWindedCounter` -1) |
| BasePoisonDamage / Duration | 1 / 2 | `GE_PoisonDamage` tick, `GE_PoisonStack` duration |
| BaseBleedDuration | 5 | |
| BaseBurnDamage / Duration | 1 / 2 | `GE_BurnDamageStack` tick, `GE_BurnState_Duration` |
| BaseFrostDuration / BaseFreezeDuration | 2 / 5 | |
| BaseLightningDuration / Damage, BaseShockDamage / Radius | 2 / 1, absent | |
| BaseWaspsDuration / Damage | 2 / 1 | |
| BaseFragileDuration, BasePerforationDuration | 3, 3 | duration of `GE_Fragile`, `GE_Perforation` |
| BasePhantomDuration / HealthDamage / PoiseDamage | 3 / 5 / 2 | |
| BaseChaosDuration / Damage, BaseCurseStackDuration, BaseCursedStateDuration | absent | `CCC_Chaos` default 4 s |
| BaseTraumaDuration / Damage | 1 / 2 | `GE_Trauma_Damage` break tick |
| BaseConfusionDuration, BaseCosmicDiseaseDuration | absent | |
| BaseKnockbackMultiplier / Strength | absent (enemies) | knockback |
| *Mitigation (Poison, Bleed, Lightning, Burn, Frost, Curse, Stasis, Phantom, Chaos, Wasps, Confusion, CosmicDisease) | 0 | PP item passives (`CCC_PPIO_*`), custom application requirement `GECAR_Mitigation_*` |
| RiposteDamageMultiplier | absent | riposte scaling |
| BaseDamageReduction | 0 | `GE_Harden_Active` override 1.0; `GE_Generic_DamageReduction`, `GE_ShellDamageReduction`, `CCC_PPIO_Moonshine` add |
| BaseBreakDamageReduction | absent | break reduction |
| IncomingDamageMultiplayer (sic) | absent | `GE_Fragile` +0.05/stack (melee), `GE_Perforation` +0.05/stack (ranged), `GE_Generic_IncomingDamageIncrease` |

### 2.3 `UPlayerAttributeSet` [native], `DT_PlayerAttributes` [data]

| Attribute | Default |
|---|---|
| CurrentEXP / ModifyEXP | 0 (`GE_GrantEXP` adds to ModifyEXP) |
| CriticalChance / CriticalBonus | 0.1 / 2.0 |
| BaseSynergyGain / Drain / DetachCost | absent |
| BaseResolveGain / BaseResolveLost | 0.1 / -1.0 |
| PrimaryMeleeAbilityCost / SecondaryMeleeAbilityCost | 0 / 0 (initialised from `UpgradeStat.Melee.Buff.Cost`, `.HoldAttack.Cost`, `.SuperMove.Cost` by `GE_InitializeMelee*Cost`) |
| HealCurrentCharges / HealMaxCharges / HealAmountPerCharge | 0 / 3 / 40 |
| GloomMultiplier / GoldMultiplier / GloomBonus / GoldBonus | 1 / 1 / 0 / 0 |
| ShellReviveCurrentProgress / MaxProgress / ProgressGain / MaxProgressMultiplier | 0 / 0 / 1 / 1.25 |
| PPItemResource / MaxResource / Cost, ItemMaxHealthBonus, ItemMaxResolveBonus, Item*MitigationBonus | absent |

### 2.4 Other sets [native]

- `USidearmAttributeSet`: CurrentAmmo, MaxAmmo, ReloadDuration, ActivationCost, PrimaryEnergyCost, MinRequiredEnergy, SecondaryEnergyCost, BuffEnergyCost (all 0 in `DT_DefaultSidearmAttributes`; real values come from the sidearm's `ID_*`/`GE_Override*` effects, see the combat doc).
- `UShellAbilityAttributeSet`: PrimaryEnergyCost, SecondaryEnergyCost, Duration, Cooldown, Damage, DamageBonus, DamageReduction, MinAmount, MaxAmount, CurrentAmount, Stacks, CriticalHitChanceBonus, CriticalHitDamageBonus, AttacksNumber, PoiseDamage, CustomDamage, MaxDamage, MaxPoiseDamage, Cost, Radius, CustomCost, HealingMeter, MaxHealingMeter, CustomHealingFactor, Chance, Threshold, ResolveGainBoost, BreakDamage, MaxBreakDamage, SecondaryDamage, SecondaryPoiseDamage, SecondaryBreakDamage. Example `DT_Attributes_Shell_Eredrim`: PrimaryEnergyCost 70, SecondaryEnergyCost 60, Damage 50, PoiseDamage 80, BreakDamage 50, SecondaryDamage 20, SecondaryPoiseDamage 100, SecondaryBreakDamage 15, Radius 500, Threshold 0.01. `DT_Attributes_Tiel`: PrimaryEnergyCost 40, Duration 3, Cooldown 8, Damage 70, Stacks 2 [data].
- `UEnemyAttributeSet`: WindedCounters, ModifyWindedCounters, TarstoneEXP (5 to 800 per enemy).
- `UDarkPowerAttributeSet`: CurrentAmmo, MaxAmmo, ReloadDuration, ActivationCost (all 1 in `DT_DefaultDarkPowerAttributes`).

### 2.5 Enemy attribute tables (`DT_Attributes_*`, 158 tables; full CSV in `work/damage-pipeline/enemy_attributes.csv`) [data]

Ranges: MaxHealth 10 to 35000 (most common 350, 130, 80, 300); Poise 1 to 1200 (mode 5, then 200, 35, 10); BreakResistance 1 to 250 (mode 25, then 50, 100, 75); StoneStunTime 3 to 8 s (mode 5, light enemies 8); BaseKnockbackStrength 300 to 2000 (mode 400); BaseKnockbackMultiplier 0 to 2 (mode 0.7). Enemy melee damage is not in the tables; it is `CD_*.BaseDamage {BaseDamage, BaseRangedDamage, BasePoiseDamage, BaseBreakDamage}`.

| Enemy | MaxHealth | Poise/MaxPoise | BreakRes | StoneStun | Knockback (str x mult) | CD BaseDamage (melee / ranged / poise / break) |
|---|---|---|---|---|---|---|
| Brigand Light | 125 | 5 / 5 | 25 | 8 | 400 | 50 |
| Brigand Light Crossbow | 100 | 5 / 5 | 25 | 8 | 400 | 30 / 15 |
| Brigand Heavy | 225 | 75 / 75 | 50 | 5 | 800 x 0.7 | 45 / 45 |
| Brigand Knightly | 300 | 40 / 65 | 50 | 5 | 600 x 0.7 | (see CSV) |
| Brigand Elite | 450 | 125 / 125 | 75 | 5 | 1000 x 0.6 | (see CSV) |
| Cultist Sword / Base | 150 | 5 / 5 | 25 | 8 | 400 | 50 (base: 50 / 20) |
| Cultist Sword Purple | 190 | 15 / 15 | 25 | 8 | 400 | 55 / 35 / 35 / 35 |
| Vampire | 120 | 10 / 10 | 35 | 5 | 600 | (none) |
| Sicario | 350 | 80 / 80 | 100 | 5 | 900 x 0.8 | 55 |
| Sicario Miniboss | 3200 | 180 / 180 | 150 | 4 | 900 x 0.8 | 65 / 27 |
| Grisha Miniboss | 2000 | 270 / 270 | 150 | none | 1500 | 35 |
| Cannibal Knight Miniboss | 1750 | 200 / 200 | 150 | 5 | 1300 x 0.5 | (see CSV) |
| Tar Golem / Headless | 2100 / 7000 | 126 / 125, 1 / 400 | 175 / 220 | 3 | 1200 x 0 | (none) / 91 |
| Lady of the Woods | 3850 | 500 / 500 | 220 | none | 400 x 0.5 | 35 / 45 |
| Head Boss | 3690 | 800 / 800 | 220 | none | 1600 | 60 |
| Dwarf Boss | 6900 | 270 / 400 | 220 | none | 600 x 0.2 | 70 / 30 |
| Moth Knight | 7200 | 300 / 300 | 220 | none | 1000 x 0 | 60 |
| Hexapod | 8000 | 1200 / 1200 | 140 | none | 1000 | 81 / 40 / 40 / 40 |
| Final Boss | 35000 | 350 / 500 | 250 | none | 1800 x 0.1 | 104 / 80 / 60 / 60 |

Bosses and minibosses are flagged `bIsBoss` / `bIsMainBoss` on `CD_*`, sometimes with `CharacterScaleOverride` (Final Boss 2.0, minibosses 1.15 to 1.2) and `UpgradeStatValues` that give their melee a status passive (`UpgradeStat.Melee.Passive.Bleed 4` on Sicario Miniboss, `.Burn 2` on Tar Golem, `.Burn 3 + .Bleed 3` on the Headless Golem, `.Burn 1` on torch cultists) consumed by `GA_AI_Melee_InflictStatusEffect` [data].

---

## 3. Damage formula

### 3.1 Confirmed structure

```
raw            = BaseDamageSource * PayloadMultiplier                       [data + native names]
   BaseDamageSource: Float -> FloatValue
                     InstigatingCharacterWeapon -> ASpartaWeapon.BaseDamage of the slot weapon (player)
                     InstigatingCharacterData   -> USpartaCharacterData.BaseDamage.BaseDamage (enemies)
aggregator     = (SpartaCombatSet.BaseDamage_base [0 for player] + raw + additive mods) * product(multiplicative mods)
                                                                              [GAS capture semantics; inferred ordering]
   multiplicative mods on the source's BaseDamage, filtered by the spec's source tags:
     GE_Permanent_WeaponDmg   x CCC_Weapon_Damage(level)   requires Ability.Attack.Melee.Weapon   (Tarforge)
     GE_Permanent_SidearmDmg  x CCC_Sidearm_Damage(level)  requires Ability.Attack.Sidearm
     GE_Status_Weak           x 0.95 per stack (max 14)
     GE_DamageMultiplier      x 2.0 (debug/cheat style, no tag filter)
     GE_NGP_DamageMultiplier  x CCC_NewGamePlusBase_EnemyDamage(cycle)   (enemies)
     GE_Regional_DamageMultiplier x SetByCaller(DataTag.RegionalDamageMultiplier)  (enemies)
     Lazlo WarmedUp / Genessa Astral / DP_ShellAbilityDamage custom MMCs (shell specific)
   additive: GE_DamageBoost (SetByCaller), HPM_Crazed_* payload modifiers rewrite the payload instead
execution      = SpartaDamageExecution(aggregator, DamageReduction, IncomingDamageMultiplier, CriticalChance, CriticalBonus)
                                                                              [native signature]
   DamageReduction          = target SpartaCombatSet.BaseDamageReduction  (Harden 1.0, Slaughterer, Moonshine, ShellDamageReduction)
   IncomingDamageMultiplier = target SpartaCombatSet.IncomingDamageMultiplayer (Fragile/Perforation +0.05 per stack, 10 stacks max)
   CriticalChance / Bonus   = source PlayerAttributeSet.CriticalChance 0.1 / CriticalBonus 2.0
                              (+ GE_Melee_Generic_CritChance/CritBonus, GE_Sidearm_Generic_*, GE_Melee_HoldAttack_Light,
                               GE_ModifyCriticalHitChance, HPM_Tiel_Behind_*, ShellAbilityAttributeSet.CriticalHit*Bonus)
                              skipped when the effect carries GameplayEffect.Damage.Options.DisableCrit
                              (riposte, DoTs, chainsaw grinder, bleed)
   ExecutionValueModifiers: DEM_NegateDamage (State.NegateDamage, incoming), EVM_Genessa_Mirage, DEM_CosmicDisease_Detonation
   ApplyPostDamageReductionModifier(Damage) runs after reduction
final          -> SpartaHealthSet.Damage -> ShellHealth (in shell) else Health                [inferred]
```

The most likely native arithmetic, by the parameter names, is `Damage * (1 + IncomingDamageMultiplier) * (1 - DamageReduction)` with a crit roll `rand < CriticalChance` multiplying by `CriticalBonus` [inferred]. `GE_Harden_Active` overriding `BaseDamageReduction` to exactly 1.0 (and the Slaughterer stack adding `UpgradeStat.Shell.Eredrim.Tank.Reduction`) is consistent with a `1 - reduction` factor.

### 3.2 Per-attack payload multipliers (player) [data, `work/damage-pipeline/attack_payloads.csv`]

All player weapon attacks use `HealthDamage.Type = InstigatingCharacterWeapon` on `GE_MeleeDamageBase`. Multipliers and poise damage by family:

| Attack family | Health multiplier | PoiseDamage | Extra pre-damage effects |
|---|---|---|---|
| A-string (light) most weapons | 1.0 | inherited 20 (Fists/Katanas 30, Battle Axe 50, Martyr's Blade 70 to 80) | Martyr's Blade adds `GE_ForceFlybackReaction` |
| A-string double hits (Axatana katanas, Dagger Axe) | 0.8 | 30 / inherited | |
| B-string (heavy) | 1.5 (Clockwork Scythe B3 2.0) | 25 (Dagger Axe), 40 (Axatana axe), 50 (Battle Axe, Fists), 70 (Scythe), 100 (Martyr's), 120 to 130 (Heavy Hammer) | `GE_BreakShield` (breaks enemy shields), Heavy Hammer adds `GE_ForceDeathWithDismemberment`, finishers add `GE_ForceFlybackReaction` |
| Hold (charged) attacks | 1.0 payload; `HoldAttackDamage (1.0 .. 2.0)` lerp by hold [inferred] | 90 to 150 | `GE_ForceFlybackReaction`, `GE_BreakShield` |
| Running A | 1.35 | 25 to 110 | Battle Axe, Scythe, Martyr's, Hammer add flyback |
| Running B | 1.8 | 50 to 130 | `GE_BreakShield`, flyback |
| Black Needle A hold | Float 25 (flat) | | |
| Slayer punch `GA_SlayerSeal_Attack` | Float 100 | 100, BreakDamage 100 | `GE_ForceDeathWithDismemberment`, `GE_ForceFlybackReaction`, `GE_ForceFlybackDeathWithDismemberment`; reaction Heavy |
| Weapon abilities (`GA_WeaponAbility_*`) | WeaponThrow 1.0 poise 200; StormStrike Float 5; PlummetStrike / SpiralSurge poise 100 | | |
| Smert punches / kicks | Float 0.001 to 0.01 on `GE_UnarmedDamage_ShellAbility` (damage comes from the shell DoT `CCC_Smert_DOT`) | 100 | |

Weapon `BaseDamage` [data, `weapons_player.txt`]: Hallowed Sword 25, Martyr's Blade 80, Black Needle 35, Hadern's Sword 35, Clockwork Scythe 42, Heavy Hammer 109, Battle Axe 38, Axatana axe 35 / katanas 25 each, Dagger Axe axe 30 / dagger 20; sidearms Crossbow 45, Nail Shotgun 7 (per pellet), Machine Gun 3 (poise 1), Parasite Gun 8, Cursed Child 15, Ballistazooka 125, Trebuchaxe 65, Lute 20. `ShellSummonDamage` (0.001 to 0.015) is the fraction of a summon boss's health removed per hit, paired with `DT_ShellSummonBossStats` (`ShellHealthHitsToKillShell` 3 to 6, chill at 0.6 to 0.7 health, vanish at 0.3 to 0.6) [data].

### 3.3 Tarforge level (`UpgradeStat.Weapon.Level`) [data]

`GE_Permanent_WeaponDmg` / `_WeaponPoiseDmg` / `_WeaponGuardEnergy` / `_SidearmDmg` / `_SidearmEnergyCost` multiply `BaseDamage`, `BasePoiseDamage`, `MaxBreakResistance`, `BaseDamage` (sidearm), `SidearmAttributeSet.PrimaryEnergyCost` by `CCC_Weapon_*` / `CCC_Sidearm_*` (native `UWeaponStatModMagnitudeCalc`, `WeaponLevelTag = UpgradeStat.Weapon.Level`, `Stat = EWeaponStatType`, `FallbackValue 1.0`). The level is read from the ASC's `UpgradeStats` map (`GetUpgradeStatValue`) and looked up in the equipped weapon's `USpartaWeaponProgressionData` (`DT_WeaponProgression` maps `Weapon.<Tag>` to `WeaponProgression_<Name>`; Black Needle shares Martyr's Blade, Heavy Hammer shares Clockwork Scythe). Curves (`work/damage-pipeline/weapon_progression.txt`):

| Progression | DamageMultiplier | Other enabled stats |
|---|---|---|
| Melee (Axatana, Battle Axe, Scythe/Hammer, Dagger Axe, Hadern's, Martyr's/Black Needle) | 1.0 at L0, +0.05 per level, 1.8 at L16 (constant steps; cached to 2.0 at L20) | GuardMeterMultiplier 1.0 to 1.25 linear (Martyr's 1.3) |
| Crossbow | cubic 1.0 to 1.8 at L20 | EnergyConsumption 1.0, 0.7 @16, 0.55 @100, 0.35 @9999 |
| Nail Shotgun | cubic 1.0, 1.05 @1, 1.3 @20 | EnergyConsumption 1.0, 0.7, 0.6, 0.45 |
| Cursed Child, Trebuchaxe | 1.0 to 2.0 at L20 | EnergyConsumption 1.0, 0.7, 0.6, 0.45 |
| Ballistazooka | 1.0 to 2.0 | EnergyConsumption 1.0, 0.7, 0.5, 0.4 |
| Machine Gun | 1.0 to 1.7 | CritDamage 1.0 to 2.0, EnergyConsumption 1.0, 0.85, 0.75, 0.6 |
| Parasite Gun | 1.0 to 1.25 | CritDamage 1.0 to 2.0, EnergyConsumption 1.0, 0.9 @20, 0.7 |
| Simple Lute | 1.0 to 2.0 | EnergyConsumption 1.0 to 0.3 (0.035 per level) |

`ElementalEfficiency`, `CritChance`, `EnergyGainMultiplier` curves exist but are disabled everywhere; `ASpartaWeapon.ElementalEfficiency<Tag,int>` (Martyr's Blade Frost 3 / Lightning 3, Heavy Hammer Stasis 4 / Trauma 3, Hadern's Curse 1 / Poison 2, Battle Axe Burn 2 / Curse 1, Scythe Bleed 2 / Lightning 1, Trebuchaxe Phantom 2 / Lightning 2, Ballistazooka Burn 2 / Trauma 2, Crossbow Poison 1, Parasite Gun Curse 3) is a per-weapon stack count for infusion stones instead [data, use inferred].

### 3.4 Other factors

| Factor | Status |
|---|---|
| Player level (`DT_PlayerExperience`, 1500 / 3000 / 5000 / 7000 / 9000 / 12000 ... EXP) | no damage scaling by level found in any effect or MMC [data, absence] |
| Damage type vs material (`Weapon.Edge.Blade/Blunt/Piercing(.Shotgun)`, `Weapon.Material.Metal/Flesh/Wood/Rock/Fire`, `Character.Armor.Leather/Plate/Chainmail/Unarmored/Stone/Ghost/Wood.Vestige`) | tags live on `USpartaWeaponComponent_Melee/Ranged.DamageTags`, `ASpartaProjectile.DamageTags` and `CD_*.ArmorTag` / `UArmorComponent.CurrentArmorTag` [data/native]. The only consumer found is `USpartaImpactTypeHandler::ShouldPlay(DamageTags, ArmorTag)` (VFX/SFX selection) and the audio mapping assets. No resistance table or execution modifier keyed on these tags exists in the exports, so edge/material does not change numbers [data, absence; conclusion inferred]. `ISpartaDamageCauser::GetDamageTags()` exposes them to BP. |
| Guard / Harden reductions | section 7 |
| Status damage | section 8 |
| Shell ability damage | `ShellAbilityAttributeSet.Damage/PoiseDamage/BreakDamage` fed as `Float` payloads or SetByCaller by the shell ability BPs; `GE_ShellAbilityDamage`, `GE_MeleeDamage_ShellAbility`, `GE_AOE_Damage_ShellAbility` [data] |
| Vulnerable | `GE_Vulnerable_State` grants `State.Vulnerable` (`GameplayEffect.Damage.Vulnerability`); multiplier is BP graph [unknown] |
| Damage cap (Adaptive Difficulty) | `USpartaAssistSettings.bCapSingleHitDamage`, `HitCapRegionTags`, `ProtectionHitCapFractions[]`, `ProtectionTierStreaks[]` [native]; values in `DefaultGame.ini` inside the pak (not extracted) |

---

## 4. Poise, stagger and break

### 4.1 Poise (stagger) [data]

- Every hit applies its payload `PoiseDamage` through `GE_PoiseDamage` (default 20; per-attack values in 3.2; enemy attacks mostly inherit 20, some 40 to 120). `BasePoiseDamage` on the source is captured and multiplied by Tarforge `PoiseDamageMultiplier` (disabled on every progression asset, so 1.0), `GE_Melee_Generic_PoiseBonus`, `GE_Sidearm_Generic_PoiseDamage`, `GA_Support_OnKill_BonusStaggerDamage`; `GE_PoiseSuppression` overrides it to 0.
- `GE_BlockPoiseRegen` (5 s, `State.PoiseRegenBlock`, one stack refreshed per hit) is the first pre-damage effect on almost every payload. When it ends (normally, or prematurely because the target gained `State.PoiseBroken`) it applies `GE_PoiseRegen`, which adds `MaxPoise` to `Poise` (full refill). So poise refills 5 s after the last hit, and immediately after a break. `GE_InterruptingHitReaction` (owned `AI.Behavior.Disabled`, `State.Block.Ability.Primary`, `State.Reaction.Interrupting`) removes the regen block and refills poise on application and on completion.
- `GE_PoiseBroken` grants `State.PoiseBroken` for 0.01 s; `GE_IncreasePoise` multiplies `MaxPoise` (SetByCaller), `GE_PoiseImmunity` / `GE_TimedPoiseImmunity` / `GE_RemovePoiseImmunity` toggle `GameplayEffect.Immunity.Poise`, `GE_HyperArmor` grants `State.HyperArmor` (one stack, removed one at a time).
- Player poise is 5 and most enemy attacks deal 20 or more, so the player is poise-broken by any unblocked hit unless immune; the reaction set decides what that looks like.

### 4.2 Harden / stone stun [data]

`SpartaHealthSet.StoneStunTime` (3 to 8 s per enemy) is how long an enemy that hits a hardened player stays stunned (`GA_StoneStun_Handler`, `GE_BlockStoneStunNoDamage`) [data, pairing inferred]. `GA_Harden_Original`: `StoneFormHitPayload` deals `Float 0.01` health, `PoiseDamage 50`, reaction `Event.Reaction.Hit.Medium` facing the instigator; perfect harden `PoiseDamage 100`, `BreakDamage 25`.

### 4.3 Hit reaction selection [data, `work/damage-pipeline/hit_reaction_sets.txt`]

Reaction tags in the 83 `HRAS_*` sets: `Event.Reaction.Hit.{Weak, Medium, Heavy, Flyback, Ignore, Blocked, Back.Medium/Heavy/Flyback}`, `Event.Reaction.Stun.{OnFire, Poisoned, Wasps, Noise, Equipped.Standard/HugeWeapon/DualWeapons}`, `Event.Reaction.Parried`, `Event.Reaction.Parried.Infinite`, `Event.Reaction.Sync.Parry.<Type>`. Each entry: directions (`ESpartaStrikeDirection None/Left/Right/Up/Down/Forward/Backward`), montage list, repetition policy, `bAdditive` (Weak is additive on most sets, so it layers over the current animation), `ReactionCooldown 0.5 s`. Typical enemy redirect rules (`HRAS_Brigand_Light`, fallback `HRAS_Human`):

- `Hit.Medium` with target `State.HyperArmor` and not `State.PoiseBroken` becomes `Hit.Weak`; `Hit.Medium` without `State.PoiseBroken` becomes `Hit.Ignore`; with `State.Hit.Melee.Unarmed` becomes Flyback. So a medium (normal) hit only plays a real stagger once poise is broken.
- `Hit.Weak` with `State.PoiseBroken` becomes `Hit.Medium`.
- `Hit.Flyback` with `State.Hit.Back` becomes `Hit.Back.Flyback` (`GE_BackHitState` is a 0.1 s `State.Hit.Back` tag set by the attacker's angle check, BP).

Player set `HRAS_Player`: Medium/Heavy become Ignore under `State.HyperArmor` or `State.Animation.Severed`; Weak is ignored while `State.Attack.Melee` or `State.Aiming`; `Parried` redirects to `Stun.Equipped.Standard/HugeWeapon/DualWeapons` by the equipped weapon tag; Flyback is ignored while already flying back. Heavy reaction is also the payload tag used by unparryable enemy attacks (`BP_SpartaUnparryableHitPayload`: `GE_UnparryableHit` + Heavy).

### 4.4 Break meter and execution [data]

`BreakResistance` (enemy 25 / 50 / 75 / 100 / 150 / 175 / 220 / 250 tiers) is only reduced by effects that carry `BreakDamage`:

| Source | Break damage |
|---|---|
| Successful parry (`GA_Parry_Successful`) | 55 (`BreakDamage`), plus knockback 100 x 0.8 |
| Perfect harden | 25 |
| Slayer Seal punch (`GA_SlayerSeal_Attack`) | 100 (`GA_SlayerSeal_Handler.BreakDamageMult 0.6` scales Slayer sidearm break; `GA_SlayerSeal_Charge` cooldown 15 s, resolve regen x0.5, health regen x0.5) |
| Trauma (`GE_Trauma_Damage`) | `BaseTraumaDamage` (2) per second via `SpartaBreakExecution` while `GE_Trauma_Stack` lasts (`BaseTraumaDuration` 1 s, refreshed per stack, 100 stacks) |
| Execution status (`GE_ExecutionStack`, 3 s, 25 stacks, `GA_Execution_Handler.BreakAmount 0.25`) | melee deals 25% of the bar per hit [inferred fraction] |
| Havoc (`GE_HavocStack`, `GA_Havoc_Handler.BreakAmount 0.25`) | ranged deals 25% |
| Confusion with no ally in `SearchRadius 1500` | `FallbackBreakDamage 25` |
| `GE_ParryResistanceDamageBonus` | adds SetByCaller `DataTag.DamageMagnitude` to `BaseBreakDamage` (consecutive parry bonus, `DataTag.ConsecutiveBreakDamageMagnitude` also exists) |
| `GA_Sidearm_InflictBreakOnCriticalHit`, `GA_InflictBreak_OnNPC` (upgrade `Melee_Path_Poison_1`) | BP graph |
| Enemy attacks on a guarding player | see 7.1 |

Regen: `GE_BreakDamageRegen` adds 0.5 `BreakResistance` every 0.1 s (5 per second, both player and enemies), suppressed while `State.BreakDamageRegenBlock` (`GE_BreakDamageRegenBlock`, SetByCaller duration). Immunity: `GE_BreakDamageImmunity`, `GameplayEffect.Immunity.Break` (also granted during `GE_Riposte`). Reset: `GE_ParryResistanceReset` sets `BreakResistance = MaxBreakResistance` (after a riposte). Tarforge `GuardMeterMultiplier` raises the player's `MaxBreakResistance` (`GE_Permanent_WeaponGuardEnergy`).

Riposte damage: payload `InstigatingCharacterWeapon` with `FloatValue 100` on `GE_RiposteDamage` (crit disabled). The Week 1 "scales with enemy health" change is not visible in the defaults; candidates are `RiposteDamageMultiplier` / `RiposteWeakness` attributes or the BP graph of `GA_Parry_Riposte` [unknown]. Auto-riposte states: `GE_State_TimedAutoRiposteOnHit` / `OnDamage` (`State.AutoRiposte.*`, only while `State.Attack.Melee`). `GA_AI_Victim_Execution_InstantDeath` and `GA_Player_ExecutionBase` (`Event.Execution`) handle execution kills; `Slaughterer` (`HPM_Slaughterer`, `GE_Slaughterer_Stack`: `Threshold +0.0009` per stack, `DamageReduction + UpgradeStat.Shell.Eredrim.Tank.Reduction`) raises the execution threshold.

Winded (a hidden counter on enemies): `GE_WindedCounter` status (`State.Winded`, duration `BaseBurnDuration`, stacks unlimited) applies `GE_IncreaseWindedCounter` (+1 `BaseWindedCounters` via `SpartaWindedExecution`) and `GE_RemoveWindedCounter` on expiry; `UWindedDamageBonus` / `UWindedPoiseDamageBonus` execution modifiers add damage per counter keyed by an `UpgradeStat` tag [data/native].

---

## 5. Resolve economy [data]

- Gate: `GE_CanGainResolve` grants `Character.Resource.Unlocked.Resolve` (up to 3 stacks); `GE_GainResolve` and `GE_FillResolve` require it.
- Gain: `GE_GainResolve` adds `CCC_ResolveGain` (BP MMC, no captured attributes declared) to `Resolve`. Inputs available to it: `PlayerAttributeSet.BaseResolveGain` (0.1), the attack ability's `ResolveMultiplier` (1.0), `GE_ResolveBoost_Melee` (multiplies `BaseResolveGain` by SetByCaller `DataTag.ModifyResolveMagnitude`, 99 stacks, only for `Ability.Attack.Melee.Weapon` sources), `GE_Melee_Generic_ResolveGain` and `GE_Sidearm_Generic_ResolveGain` (x `UpgradeStat.Melee/Sidearm.Generic.ResolveGain`), `ShellAbilityAttributeSet.ResolveGainBoost`, `CCC_Lazlo_WarmedUp_Resolve`, `GA_SlayerSeal_Charge.ResolveRegenMult 0.5`, dungeon debuff "Resolve Cap" (-25%), "Brand Of The Deserter" (no resolve while detached). With `BaseResolveGain = 0.1` the natural reading is `resolve += damage_dealt * 0.1 * multipliers` [inferred]. Projectiles have `GrantsResolve = true` on `BP_ProjectileBase`, so sidearm hits can also feed it when the BP allows.
- Spend: `GE_ConsumeResolve` subtracts SetByCaller `DataTag.ResolveCost`. Costs: `SidearmAttributeSet.PrimaryEnergyCost x CCC_Sidearm_EnergyCost(level)` (curves in 3.3), `SecondaryEnergyCost` (`UpgradeStat.Sidearm.Attack.Secondary.Cost`), `BuffEnergyCost`, `MinRequiredEnergy`; `PlayerAttributeSet.PrimaryMeleeAbilityCost` (`UpgradeStat.Melee.Buff.Cost`), `SecondaryMeleeAbilityCost` (`UpgradeStat.Melee.HoldAttack.Cost` or `.SuperMove.Cost`); shell abilities `ShellAbilityAttributeSet.PrimaryEnergyCost/SecondaryEnergyCost`; `USpartaGameplayAbility.Cost` and `AdditionalCosts[]` (`USpartaAbilityCost`) on the ability itself [native]. `GE_Free_Abilities` / `GE_Free_Shooting` waive costs.
- Refill: `GE_FillResolve` sets `Resolve += MaxResolve` (effigies). `GE_ModifyShellCurrentAmount`, `GE_RestoreShellHealth`, `GE_RestoreDarkFormHealth` are the healing counterparts. `BaseResolveLost = -1.0` has no consumer in the exports [unknown].

---

## 6. Sidearm and projectile path [data]

1. `GA_Sidearm_Primary_Press` (input) raises `Event.Sidearm.Activate.Primary`.
2. `GA_SidearmActivationBase` (`Ability.Activation.Sidearm.Default`, trigger on that event) spawns the projectile from the sidearm's `BPI_SidearmBase` data (`CrossfireRate 0.05`, `DefaultHitscanQuery`, `PayloadPreventionQuery = ANY(ALL(GameplayEffect.Force.Projectile.Collision), NONE(GameplayEffect.Immunity.Projectile.Collision))`).
3. `GA_<Sidearm>Attack_Primary < GA_SidearmRangedAttackBase` (or the burst/charged bases) requires `State.Aiming`, soft-targets `spine_01..05, neck_01`, listens to `Event.Reaction.Hit`, and owns the payload: `HealthDamage = InstigatingCharacterWeapon`, `DamageEffect = GE_RangedDamage_Base`, per-sidearm `PoiseDamage` (Machine Gun 5, Nail Shotgun 4, Parasite Gun 1, Lute 15, Trebuchaxe 50, Cursed Child 55, Sticky Bomb and Ally Summon 200, Crossbow/Ballistazooka inherit 20). `ShootingPlayRate` 1.0 (Parasite Gun 5.0, Quick Shooting 2.4).
4. `BP_ProjectileBase : ASpartaProjectile` defaults: `DamageTags = Weapon.Edge.Piercing`, `VelocitySize 3000`, `MaxVelocity 5000`, `Width 80`, `MaxBounceCount 1`, `ParriedPower 225` (deflect impulse), `DepthOfEntry 1.0`, `EnableFriendlyFire`, `GrantsResolve`, `TimeToDestroyWhenHitDuringIFrames 0.001`, `InitialLifeSpan 10`, `bIsAffectedByTimeStop`, `ProjectileTag = Projectile.Slot.Primary`, `OwningWeapon` back-reference; `USidearmProjectileCacheComponent` on the player pools them (`bCachableProjectile`) [data/native]. Impact applies the ability payload through the same `USpartaHitPayload::ApplyPayload` path (`OnProjectileDamageCaused`); the target gets `GE_DamageReceived_Ranged` (1 s `State.DamageReceived.Ranged`) [data].
5. Damage: `WP.BaseDamage x CCC_Sidearm_Damage(level) x (1 + Perforation stacks x 0.05) x crit (GE_Sidearm_Generic_CriticalHitChance/Damage, Machine Gun and Parasite Gun Tarforge CritDamage curve)` on the same execution. Sidearm status infusion uses `ElementalEfficiency` and the `GA_ElementalMechanicHandler` (projectile / blast / cloud / shockwave payloads, `BP_SpartaHitPayload_ElementalBlast/Shockwave` on `GE_AOE_Damage_Base` with flyback).
6. Deflection: `GA_Parry_Successful_Projectile_Deflect` (`DeflectVelocitySize 3500`, `DeflectMaxVelocity 5000`); rolling grants `GameplayEffect.Immunity.Projectile.Collision`; `GE_ProjectileImmunity`, `GE_NotDetectableForProjectiles`, `GE_ForceProjectileCollision` are the other switches.

---

## 7. Incoming damage on the player [data unless noted]

### 7.1 Guard (Untarnished Seal, `GA_ActiveBlock`)

- Trigger `Event.Input.Action.ActiveBlock.Press/Hold`; `RequiredActivationTime 0.3 s`; owned `State.ActiveBlock`, `State.Block.Ability`, `State.Ability.Seal`, `State.CanInterruptMontages`; deactivates on attack, aim, death, dodge, broken, parry, riposte, reaction, falling, paired animation, walk.
- While active `GE_ActiveBlock_BlockDamage` grants `GameplayEffect.Immunity.Damage`, `State.HyperArmor`, `State.Hit.IgnorePhysReaction`: blocked hits deal no health damage and no poise stagger. The guard meter is `BreakResistance/MaxBreakResistance` (player 75, Tarforge up to x1.3), drained by the attacker's `BreakDamage` and refilled at 5 per second by `GE_BreakDamageRegen`; holding guard also drains it and slows movement (tutorial text; the drain effect is BP graph) [inferred]. At 0 the player gets `GE_BreakShield` (`State.ShieldBroken`, `Event.ShieldBroken`) and the guard-broken stun [inferred].
- Perfect guard: `GE_ActiveBlock_PerfectBlock` window 0.3 s, `PerfectBlockInvulnerbilityDuration 0.36 s`, `GE_ActiveBlock_PerfectBlockCooldown 0.6 s` (`Ability.Cooldown.PerfectBlock`), knockback to the attacker 300 (normal 800), `PerfectBlockKnockbackReduction 0.7`, hit stop 0.01 s at 0.1 dilation. Perfect guard fills the enemy break bar (tutorial text); the amount is BP graph [unknown].

### 7.2 Parry (Infinite Seal)

- `GA_Parry_Action_Single` (`Ability.Parry.Action`, montages `AM_Shared_Actions_InfSeal_Parry_01_A`, `_Alternative`, `_02_A` for the consecutive-parry window added 5 September) grants `State.Parry`; `GE_Parry` (`GameplayEffect.Parry`) is the state, `GE_Parry_BlockDamage` grants `Immunity.Damage` + `Immunity.HitReaction` during the active window, `GE_ParrySafety` grants both plus `State.ParrySafety` (post-parry safety).
- `GA_Parry_Handler` (spawn-granted) listens for hits during the window and activates its child `GA_Parry_Successful` (`Ability.Parry.Successful.Melee`): enemy takes `BreakDamage 55`, `KnockbackStrength 100` (x0.8 reduction), plays the paired animation from `Event.Reaction.Sync.Parry.*`, then `GE_ParryResistanceDamageBonus` for consecutive parries (SetByCaller). `State.Parry` is a global prevention tag for `GA_HitReaction`, so a parried player never plays a reaction.
- Unparryable (red circle) attacks apply `GE_UnparryableHit` first, which removes `GE_Parry`, `GE_Harden_Active`, `GE_ParrySafety` and `GE_HyperArmor` on the victim, then hit with the Heavy reaction; `GE_UnavoidableAttack` (`State.UnavoidableAttack`) marks grabs.

### 7.3 Harden (Hallowed Seal, `GA_Harden_Original`)

`GE_Harden_Active` overrides `BaseDamageReduction` to 1.0 and grants `GameplayEffect.Immunity.Conditions.Application/Ongoing` (no status effects) and `State.Special.Harden.Active`; `EnterStoneFormInterpSpeed 4.5`, `LeaveStoneFormInterpSpeed 10`, `StoneFormDurationAfterHit 1.0 s`, `StoneFormCooldown 5 s`, self knockback 600; perfect harden window `PerfectStoneFormDuration 0.25 s`, `PerfectStoneFormCooldown 0.186 s`, `GE_Harden_Perfect` state, attacker poise 100 / break 25, self knockback 200. Blocked by `State.Block.Ability.Harden` (`GE_Block_Harden`), `State.Animation.Paired`, `Input.All.Blocked`.

### 7.4 Dodge (`GA_RollBase`)

`InvulnerabilityDuration 0.6 s` (`GE_Invulnerability`: `Immunity.HitReaction`, `Immunity.Damage`, `Immunity.Conditions.Application`), `RollDistance 600` (strafe 660), `RollTravelTime 0.75 s`, owned `GameplayEffect.Immunity.Projectile.Collision` + `State.Roll` for the whole roll, blocked by `State.Block.Ability.Roll` / falling. `GE_TimedInvulnerability` and `GE_Block_Dodge` exist for scripted cases.

### 7.5 Shell and passives

- Damage lands on `ShellHealth` first while `Character.State.Shell`; `GE_ShellDamageReduction` adds `ShellAbilityAttributeSet.DamageReduction` to `BaseDamageReduction` only in shell; `GE_PreventShellDeath`, `GE_PreventDeath`, `GE_PreventDeath_SavedByTheShell/Undergland`, `GE_ShellSeveredState`, `GE_ForceHealShell/Darkform` and the `GE_ShellSummon_IncomingDamage` payload modifier cover the sever and revive path. `GameplayEffect.Damage.Options.ForceShell/ForceDarkform` tags redirect a hit to one pool (12 uses each) [data, semantics inferred].
- Shell passives that modify incoming damage are payload or execution modifiers: `HPM_Smert_Resilience`, `HPM_Eredrim_Apathy`, `HPM_Lazlo_Furnace`, `HPM_Proxima_Reflection`, `HPM_Thorn_Passive` (Pain, blocked while parrying/guarding/hardened), `HPM_KnightLady` (mitigation, same blocking tags), `EVM_Genessa_Mirage`, `DEM_NegateDamage` (`GE_NegateDamage`, `State.NegateDamage`, does not scale with stacks) [data].
- Status resistance on the player: `GECAR_Mitigation_<Status>` custom application requirements read `SpartaCombatSet.<Status>Mitigation` (PP item passives, diminishing `DiminishingReturnStrength 0.00046`) and fire `Event.Status.Mitigation.<Status>`; `GE_*Immunity` effects grant `State.Immunity.Status.<Status>`; `GE_Immunity_Condition_Application/OnGoing` and `GE_Invulnerability` grant the blanket `GameplayEffect.Immunity.Conditions.*` tags.
- Fall damage: `GE_FallDamage` (ignores `Immunity.Damage`, blocked by `State.Skydive`, `State.Traversal`, `State.Immunity.FallDamage`, `State.Attack.Plunging`).

---

## 8. Status effects (`GE_SpartaStatusEffectBase : USpartaStatusEffect`) [data, `work/damage-pipeline/status_effects_condensed.txt`]

Common base: `Period 1 s`, duration from a captured `SpartaCombatSet.Base<Status>Duration` (source, snapshot), stacking `AggregateByTarget`, expiry `RemoveSingleStackAndRefreshDuration` (stacks fall off one at a time), a `GA_<Status>_Handler` granted while the effect is active (the handler does the reactions and damage payloads), `WidgetClass WBP_StatusEffectIcon`, `RelevantUpgradeStat/DurationUpgradeStat` hooks, `Immunity.Conditions` application block, removal on `State.Death`. Enemy melee applies them through `GA_AI_Melee_InflictStatusEffect` using `UpgradeStat.Melee.Passive.<Status>` values on the `CD_*`; player weapons through `GA_Melee_InflictElemental` with the weapon's `ElementalEfficiency` stacks and infusion stones; sidearms through `GA_ElementalMechanicHandler`.

| Status | Stack effect | Stack limit / duration | Damage or payoff |
|---|---|---|---|
| Poison `GE_PoisonStack` | `GA_Poison_Handler` | 100 / `BasePoisonDuration` (player 2 s, default 5) | `GE_PoisonDamage`: `BasePoisonDamage` (1) per second via `SpartaDamageExecution`, plus `SpartaHealExecution 0.5` (leech to source); stun reaction `Event.Reaction.Stun.Poisoned` (Vomit) |
| Burn `GE_BurnState_Duration/Infinite` | `GA_Burn_Handler` | 1 stack / `BaseBurnDuration` (2 s) | `GE_BurnDamageStack`: `BaseBurnDamage` (1) per second per stack, 99 stacks; `Stun.OnFire` reaction; `GE_BlockBurnDamage_Shell` |
| Bleed `GE_BleedStack` | `GA_Bleed_Handler` (`BleedDamage 0.1`, fallback resistance 5 x 0.5) | 99 / `BaseBleedDuration` 5 s | `GE_BleedDamage` (crit disabled), payload poise 100 with Medium reaction: burst when stacks exceed `BleedResistance` [inferred] |
| Frost `GE_FrostStack` | `GA_Frost_Handler`, `BoundAttribute FrostResistance` | 100 (capped by resistance) / `BaseFrostDuration` 2 s | at cap: `GA_Freeze_Handler` freeze for `BaseFreezeDuration` 5 s, `FreezeReactionPayload` 1 health |
| Lightning `GE_LightningStack` | `GA_Lightning_Handler`, bound `ShockResistance` | 100 / 2 s | `GE_LightningDamage` every 0.5 s (`BaseLightningDamage` 1); at cap shock AOE (`FallbackShockRadius 400`, `FallbackShockDamage 25`, `GE_ShockDamage`, `State.Lightning.Propagating`) |
| Curse `GE_CurseStack` | `GA_Curse_Handler`, bound `CurseResistance` (fallback 6, increment 3) | 100 / `BaseCurseStackDuration` | `GE_Status_Cursed` (1 stack): `HPM_Status_Cursed` nullifies and reflects the next melee strike, not for ranged/AOE/DoT/riposte/fall damage, blocked while the victim parries, guards or hardens; `GE_CurseDamage`, poise 999 |
| Fragile `GE_Fragile` | `IncomingDamageMultiplayer +0.05` per stack, melee sources only | 10 / `BaseFragileDuration` 3 s | up to +50% melee damage taken |
| Perforation `GE_Perforation` | `IncomingDamageMultiplayer +0.05` per stack, ranged sources only | 10 / 3 s | up to +50% ranged damage taken |
| Weak `GE_Status_Weak` | `BaseDamage x0.95` per stack | 14 / 5 s | down to about 49% damage dealt |
| Trauma `GE_Trauma_Stack` | `GA_Trauma_Handler` | 100 / `BaseTraumaDuration` 1 s | `GE_Trauma_Damage`: `BaseTraumaDamage` (2) break per second |
| Execution `GE_ExecutionStack` | `GA_Execution_Handler.BreakAmount 0.25` | 25 / 3 s | melee deals break; premature removal `GE_RemoveExecutionStacks` |
| Havoc `GE_HavocStack` | `GA_Havoc_Handler.BreakAmount 0.25` | 25 / 3 s | ranged deals break |
| Chaos `GE_ChaosStack` | `GA_Chaos_Handler`, `DefaultChaosDamage 5` | 100 / `CCC_Chaos` (default 4 s, infinite 99999) | random condition on expiry (BP) |
| Confusion `GE_Status_Confused` | `GA_Confused_Handler` (`SearchRadius 1500`, `FallbackBreakDamage 25`), expiry held during `State.HitCheck`/`State.Attack` (`SpartaHoldExpirationComponent`) | 1 / `BaseConfusionDuration` | attacks allies, else break |
| Stasis `GE_StasisStack` (+`_CustomDuration`) | `GA_StasisHandler`, stack limit attribute `StasisWeakness` (player 99) | unlimited / 4 s | slow; removed by paired animations |
| Phantom `GE_PhantomStack` | `GA_Phantom_Handler` | / 3 s | delayed splash `BasePhantomHealthDamage` 5 + poise 2 |
| Wasps | `GA_Wasps_Handler`, bound `WaspsResistance` (7) | / 2 s | `BaseWaspsDamage` 1, `Stun.Wasps` reaction |
| Cosmic Disease | `GA_CosmicDisease_Handler`, resistance 35 (+5) | / `BaseCosmicDiseaseDuration` | `GE_CosmicDisease_Damage_Default` 1 s tick scaled by `C_CosmicDiseaseDamage` (1.0 at 1 stack to 3.0 at 60, linear), `_Detonation` with `DEM_CosmicDisease_Detonation` |
| Leech `GE_LeechBuff` | `MaxHealth x1.0025`, `MaxShellHealth x1.0025` per stack | 200, infinite | lost on death |
| Warp `GE_WarpStack` | `AttackSpeedBonus +0.01` per stack | 30 / SetByCaller | |
| Winded `GE_WindedCounter` | see 4.4 | unlimited / `BaseBurnDuration` | |
| Guardian | `GA_Guardian_Handler` (`InvulnerbilityDuration 0.01`, stun payload with `GE_BreakShield`, reaction `Parried`) | | |
| Shadow, Slaughterer, Faith, Infect, Pain, Vomit, Bloodcurse | `CCC_ShadowDuration`, `GE_Slaughterer_Stack` (4.4), Thorn `HPM_Thorn_Passive` (Pain); the rest are BP graphs on their handlers [unknown numbers] |

Immunities: `GE_BleedImmunity`, `GE_BurnImmunity`, `State.Immunity.Status.<X>`, `GE_Immunity_Condition_*`, `GE_Grant_TotalPermanentStatusEffectImmunities`. Resistance modifiers: `GE_ModifyBleedResistance`, `GE_ModifyFrostResistance`, `GE_ModifyShockResistance`, `GE_ModifyWaspsResistance`, `GE_ModifyCurseResistance`, `GE_ModifyConfusionResistance`, `GE_ModifyCosmicDiseaseResistance` (SetByCaller `DataTag.Modify<X>Resistance`).

---

## 9. Enemy scaling

| Mechanism | Evidence | What it does |
|---|---|---|
| Regional difficulty | `GE_Regional_DamageMultiplier` (`BaseDamage x SetByCaller DataTag.RegionalDamageMultiplier`), `GE_Regional_HealthMultiplier` (`MaxHealth x DataTag.RegionalHealthMultiplier`) [data]; `USpartaWorldGenSubsystem.GetDifficulty(BiomeTag)`, `SetDifficulty`, `IncreaseDifficulty`, `OnDifficultyChanged(Biome, Old, New)`; encounter recipes carry `MinDifficulty/MaxDifficulty`, spawn overrides `OC_ReqDifficultyLvl`, `OC_TavernArenaDifficultyLvl`, enum `EDifficulty` [native/data] | a per-biome integer difficulty selects multipliers; the applier (spawner or `USpartaEnemySubsystem.OnSpawnEffects`) is not in the exports [unknown]. Night Mode (raises health and damage per the combat doc) most likely drives these two effects [inferred] |
| New Game Plus | `GE_NGP_DamageMultiplier` x `CCC_NewGamePlusBase_EnemyDamage` (curve `C_NewGamePlusBase_EnemyDamage`: 1.0 at cycle 0, 2.0 at cycle 10, cubic, cycles with offset beyond), `GE_NGP_HealthMultiplier` x `CCC_NewGamePlusBase_EnemyHealth` (1.0, 1.5 at 1, 1.8 at 3, 2.5 at 10), refresh event `Event.MMC.NewGamePlus`, `FallbackValue 1.0` [data] | NG+ cycle scales enemy damage and max health |
| Adaptive Difficulty (5 September, opt-in) | `USpartaAssistSubsystem` (game instance): `SetAdaptiveDifficultyEnabled`, `IsPlayerOptedIn`, `IsAssistEnabled`, `IsChallengeEnabled`, `IsBossAssisted(BossId)`, `IsBossChallenged`, `GetScalesForBoss(BossId) -> FSpartaBossAssistScales {HealthScale, DamageScale, BreakResistanceScale}`, `GetDifficultyTier`, `GetChallengeTier`, `GetDeathCount(BossId)`, `GetTotalDeaths`, `GetOverworldDeathStreak`, `GetDeathsPerHour`, `GetReviveEscalationScale`, `GetPlaytimeHours`, `GetPlayerLevel`, `HandleEnemyDeath` [native]. `USpartaAssistSettings` (developer settings): `GraceDeaths`, `RampDeaths`, `bScaleBossDamage/MinDamageScale`, `bScaleBossHealth/MinHealthScale`, `IncludedBosses/ExcludedBosses`, `EngagementTimeoutSeconds`, revive escalation (`ReviveGraceDeaths`, `ReviveRampDeaths`, `MinReviveEscalationScale`, `ReviveStreakDecayMinutes`), `PerBossOverrides`, challenge scaling (`TierPromotionKills`, `MaxChallengeTier`, `MaxDamageScale`, `MaxHealthScale`, `MaxBreakResistanceScale`, `MaxEnemy*Scale`), protection scaling (`MinTier*Scale`, `MinEnemyTier*Scale`, `TierDwellSeconds`), `bCapSingleHitDamage`, `HitCapRegionTags`, `ProtectionHitCapFractions`, `ProtectionTierStreaks` [native]; `GlobalEvent_AdaptiveDifficultySettingChanged`, `WBP_AdaptiveDifficultyIndicator` [data] | deaths per boss and overall lower boss health/damage/break down to the minimum scales (assist), kills promote a challenge tier that raises enemy health/damage/break up to the maximum scales, and a per-hit damage cap protects new players. The numeric defaults are in `MortalShell2/Config/DefaultGame.ini` inside the pak, which the export tool does not extract [unknown values] |
| Dungeon debuffs | `DT_Debuffs*`: "Blessing Of Flesh" heavy enemies +30% HP, "Throws Of Mercy" 20% chance to deal 0, "Resolve Cap" -25% resolve generation, "Hole In The Pocket" lose 2 gold per hit, "Brand Of The Deserter", level 2 "Enemies are Resistant to All (Fire/Frost/Lightning/Poison) Damage" [data] | `Sparta/Core/Player/Debuffs` effects |
| Shell summon bosses | `DT_ShellSummonBossStats`, `ASpartaWeapon.ShellSummonDamage` [data] | see 3.2 |
| `GE_EffectMaxHealth_Multiplier` (x2 MaxHealth + heal), `GE_CharacterSize_*`, `GE_AI_TimedBuff` [data] | scripted buffs | |

---

## 10. Hook points for a native (UE4SS C++) mod

Reflected (safe to call or hook through UE4SS `UObject`/`UFunction` APIs; offsets are from `Sparta.hpp`):

| Goal | Where |
|---|---|
| Read or set any combat number live | `USpartaAbilitySystemComponent::SetFloatAttributeValue(FGameplayAttribute, float)` (UFUNCTION) or the standard `UAbilitySystemComponent` attribute API on the sets above; the sets are plain `FGameplayAttributeData` at fixed offsets (`USpartaHealthSet` Health 0x60, MaxHealth 0x70, ShellHealth 0x80, Resolve 0xD0, MaxResolve 0xE0, Poise 0xF0, MaxPoise 0x100, BreakResistance 0x210, MaxBreakResistance 0x220; `USpartaCombatSet` BaseDamage 0x30, BasePoiseDamage 0x50, BaseBreakDamage 0x60, AttackSpeedBonus 0x70, BaseDamageReduction 0x320, IncomingDamageMultiplayer 0x340; `UPlayerAttributeSet` CriticalChance 0x50, CriticalBonus 0x60, BaseResolveGain 0xA0, PrimaryMeleeAbilityCost 0xC0) |
| Tarforge level, upgrade stats | `USpartaAbilitySystemComponent::AddUpgradeStatValue(Tag, Value)`, `RemoveUpgradeStatValue`, `GetUpgradeStatValue`, `UpgradeStats` map at 0x15F8, delegate `OnUpgradeStatChanged`; `UpgradeStat.Weapon.Level` drives every `CCC_Weapon_*` |
| Damage before it is applied | hook `USpartaHitPayload::ApplyPayload` / `ApplyPayloadInstigator` (pre) and rewrite `HealthDamage.FloatValue/Multiplier`, `PoiseDamage`, `BreakDamage`, `ReactionTag`, `DamageEffect` via the `Set*` UFUNCTIONs; or hook `CalculateDamage / CalculatePoiseDamage / CalculateBreakDamage` (post) and patch the return |
| Damage after it is computed | `USpartaHealthComponent::OnDamageReceived` delegate (value, instigator, spec), `ASpartaCharacter::OnHitReceived` / `OnHitApplied` / `OnHealthDamageCaused`, `UGEDamageEvent` global event, `USpartaDamageDebugSubsystem::HandleDamageReceived` |
| Registering your own modifier | subclass or instantiate `UHitPayloadModifier` (`CanApplyPayloadModifier`, `ApplyPayloadModifier`) or `UDamageExecutionModifier` / `UPoiseExecutionModifier` / `UBreakExecutionModifier` and push it onto `ActivePayloadModifiersArray` / `ActiveExecutionValueModifiersArray` (0x1448 / 0x1568 on the ASC); the tag queries `SourceQuery/TargetQuery` gate them. Native `Apply*` functions are BlueprintNativeEvents, so a BP-implemented subclass shipped as a LogicMod also works |
| Speed | `USpartaGameplayAbility` montage rate (`MontagePlayRate` on `GA_PlayMontageBase`, BP property), `SpartaCombatSet.AttackSpeedBonus`, `GA_SidearmRangedAttackBase.ShootingPlayRate`, `GA_RollBase.RollPlayRate/RollDistance/RollTravelTime` |
| Costs and cooldowns | `USpartaGameplayAbility.Cost` (0x3B4), `CooldownDuration` (0x3AC), `GlobalCooldownDuration` (0x3CC), `AdditionalCosts`, `ApplyLocalCooldown / ClearLocalCooldown / ApplyGlobalCooldown / ClearGlobalCooldown / SetLocalCooldownPaused`, `IsOnAnyCooldown`; resolve costs through `SidearmAttributeSet.PrimaryEnergyCost` etc. or by applying `GE_Free_Abilities` / `GE_Free_Shooting` |
| Immunity and states | apply/remove the tag-only effects listed above (`GE_Invulnerability`, `GE_HyperArmor`, `GE_PoiseImmunity`, `GE_BreakDamageImmunity`, `GE_HitReactionImmunity`, `GE_NegateDamage`, `GE_Vulnerable_State`); `USpartaAbilitySystemComponent::PauseGameplayEffectDurationCountdowns(Query)` freezes timers |
| Hit reactions | `USpartaHitReactionAnimSet::GetHitReactionAnimation` (hookable), swap `ASpartaCharacter.HitReactionAnimSet` (0x6B8), `USpartaCharacterData.PhysicalHitReactionData` |
| Enemy tuning | `USpartaCharacterData` (`BaseDamage` 0x340, `AttributeData` 0x1E0, `UpgradeStatValues` 0x108, `bIsBoss` 0x48) before `InitializeCharacterData`; live via the ASC; `USpartaEnemySubsystem.OnSpawnEffects/OnSpawnAbilities` (0x98 / 0xA8) for blanket effects |
| Adaptive Difficulty | `USpartaAssistSubsystem::GetSubsystem`, `SetAdaptiveDifficultyEnabled`, `GetScalesForBoss`; `USpartaAssistSettings::Get()` exposes every threshold as a UPROPERTY |
| Hit traces | `USpartaHitTraceComponent::DisableHitProcessing/EnableHitProcessing`, `OnTraceHit` delegate, `USpartaHitTrace.HitList` |

Not reflected (pattern scan or vtable only): the bodies of `USpartaDamageExecution::Execute_Implementation`, `USpartaHealthSet::PostGameplayEffectExecute`, `USpartaHitPayload` internals, `CCC_*` BP MMCs (those are BP bytecode, patchable as LogicMods).

---

## 11. Open questions

1. The exact native arithmetic in `USpartaDamageExecution` (order of reduction, incoming multiplier and crit; whether reduction is `1 - x` or divisive) and how `SpartaHealthSet.Damage` is split between `ShellHealth` and `Health`.
2. `CCC_ResolveGain`'s formula (BP MMC with no declared captures) and the meaning of `BaseResolveLost = -1.0`.
3. What `GA_Parry_Riposte` does with `FloatValue 100` on an `InstigatingCharacterWeapon` payload and where the "scales with enemy health" term comes from (`RiposteDamageMultiplier`, `RiposteWeakness`, or graph).
4. Hit counts per montage (hit-check notify instances are not exported) and the `HoldAttackDamage (1..2)` charge curve.
5. The Untarnished guard drain rate while holding, the break amount of a perfect guard, and the guard-broken stun length (BP graphs of `GA_ActiveBlock`).
6. Who applies `GE_Regional_*` / `GE_NGP_*` and the Night Mode multiplier values; the Adaptive Difficulty numbers (`DefaultGame.ini` inside the pak, needs a raw-file extractor).
7. Knockback formula (`BaseKnockbackStrength x BaseKnockbackMultiplier` vs the seal abilities' own strengths) in `GA_Knockback_Handler`.
8. Whether `Weapon.Edge.*` / `Weapon.Material.*` / `Character.Armor.*` ever reach a numeric path (nothing found; treated as cosmetic).
9. The default ability sets (which `GA_*` every enemy and the player receive at spawn) live in the character Blueprints and `USpartaAbilityDataAsset` references, which were not exported.
