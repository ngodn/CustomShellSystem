# Mortal Shell II player movesets: montages, notify windows and per-attack damage

Written 27 September 2026 from the installed build (`MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241`, Steam, 15 September hotfix). Companion to `CustomShellSystem/docs/ms2-combat-system.md` (weapon roster, BaseDamage, Tarstones, seals); this file answers that document's open question 1: which montage each light, heavy, running, hold, ability, plunge and riposte input plays per weapon, and what damage, poise, timing and cancel windows are stored on it.

Evidence rules:

- **Game data** means read from cooked assets with CUE4Parse (montage properties, notify objects, Blueprint class defaults, bytecode literals) or from the UE4SS reflection dump (`Binaries/Win64/ue4ss/CXXHeaderDump`). Every such fact names the asset or class it came from.
- **Inferred** means a reading of Blueprint or native behaviour that cannot be confirmed from data alone (the bytecode is not decompiled here). Inferred statements are marked.
- Times are seconds inside the montage at play rate 1.0. Every player attack ability keeps `MontagePlayRate = 1.0` (no per-attack override found in any `GA_Player_*` default); `GA_Player_Attack_Light/Heavy::GetAbilityMontagePlayRate` exists and can scale it at runtime (Warp stacks are the obvious candidate, inferred).
- Raw listings: `CCS/work/movesets/montage-listing.md` (every notify of every exported montage), `weapon-tables.md` and `weapon-summary.md` (generated joins), `abilities.jsonl`, `montages.jsonl`, `selectors.json`. `CCS/work/movesets/README.md` lists the commands and files.

## 1. From input to montage

### 1.1 Naming convention (game data, folder `Sparta/Characters/Shells/_Shared/Animation/Attacks/<Weapon>/`)

- `A` = light chain, `B` = heavy chain; the letter count is the position: `A`, `AA`, `AAA` = light 1, 2, 3; `B`, `BB`, `BBB` = heavy 1, 2, 3. Lower-case prefixes mean the same thing (`AM_Shells_AxeDagger_3Hit_aaA` is light 3).
- `_Hold` = the charged variant of that step. `_Cut` (Black Needle `_CUT`) = the same animation trimmed to start after the charge pose, used as the ordinary press on weapons whose input plays the hold montage first (section 1.3).
- `_Finisher` = the combo finisher that replaces step 3 when the finisher is unlocked (Stillblade's, Zealot's, Clerik's, Tyrant's Stone effects in `GA_Player_Attack_Light/Heavy`).
- `_double` / `_Double` = Duality Stone variants: two hit windows at `HealthDamage.Multiplier 0.8`.
- `Running_A` / `Running_01` = sprint light, `Running_B` = sprint heavy. `Super`, `Ability_*`, `NeedleStorm_*`, `Stomps`, `Throw`, `Whirlwind` = weapon abilities.
- `A_MS1_Player_*` = Mortal Shell 1 animations reused (Axe & Dagger heavies use the MS1 hammer set, Great Martyr's Blade lights use the MS1 Martyr's Blade set). `_V2`, `_02`, `_03`, `_60fps` are iteration numbers; the ability's `Montage` property says which one ships.
- `A_*` = AnimSequence, `A_*_Montage` or `AM_*` = AnimMontage. All are on `SKEL_Human_Skeleton`, `DefaultSlot`, one segment, blend in 0.3 / out 0.4 cubic inertialization on most attacks.

### 1.2 The chain (game data unless marked)

1. **Input.** Light Attack = `InputTag.Ability.Attack.Primary.Pressed`, Heavy = `InputTag.Ability.Attack.Secondary.Pressed`. `GA_Attack_Primary` / `GA_Attack_Secondary` (`Sparta/Core/Player/Ability/Input/`, tag `Ability.Input`, functions `OnInputPressed`, `ResetInputs`) are thin input abilities with no tags or montages in their bytecode literals. The weapon actor's `GrantedAbilitySet` (`ASpartaWeapon::GrantedAbilitySet`, e.g. `WP_HadernsSword`) grants four selectors and binds two of them to those input tags. Which two differs by weapon (section 1.3).
2. **Selector.** `GA_Player_AttackSelector_Light_<Weapon>` / `_Heavy_<Weapon>` (and `_Hold` twins) derive from `GA_Player_AttackSelectorBase` (`Sparta/Core/Player/Ability/GA_Player_AttackSelectorBase`). Defaults: `ActivationRequiredTags Character.State.PrimaryWeapon`, `ActivationBlockedTags State.Block.Ability.Attack.Selector, State.Block.Ability.Primary` (hold twins add `State.Unarmed`, `Character.State.Darkweapon`), `CancelAbilitiesWithTag Ability.Reaction.Hit, Ability.Primary, Ability.Animation.Paired`, `PlungingAttackTags Ability.Attack.Melee, Ability.Primary, Ability.Attack.Plunging`, `PlungingTraceLength -350`. Its functions decide in this order (function names from the header; order inferred): `CanActivatePlungingAttack` / `IsFalling` (trace 350 cm down while falling, sends `Event.Ability.Attack.Plunging.Activate`), `IsRunning` then `RunningAttackList[0]`, else `ShouldTriggerComboFinisher` then `GetSpecialFinisherAttack` from `AdditionalAttacks`, else `ComboAttackList[CurrentComboCount]`. The count lives in `BPC_Player_ComboCounter` (`MaxComboCount 3`, `ResetDelay -1`); `GA_AttackBase_Melee.ResetComboCountOnAbilityEnd = true`, so the count survives only while the previous attack ability is still active, which is why the next press must land inside the montage's InputQueue window to chain (inferred from those two defaults).
3. **Attack ability.** One class per step, e.g. `GA_Player_HadernsSword_A1` (`Sparta/Core/Player/Ability/Weapons/<Weapon>/`), deriving `GA_Player_Attack_Light` or `GA_Player_Attack_Heavy` -> `GA_AttackBase_Melee` -> `GA_AttackBase` -> `GA_PlayMontageBase` -> `GA_SpartaBase` -> `GA_SpartaAbility` -> native `USpartaGameplayAbility`. The step class holds `Montage`, `AbilityHitPayload` (a `BP_SpartaHitPayload` subobject), `WarpTranslationDistance`, `WarpTranslationStoppingDistance`, `SoftTargetingSettings` (`BP_STS_Melee_<Weapon>`), hit-stop settings and tags. `GA_PlayMontageBase` plays it with `UAbilityTask_PlayMontageAndWaitWithNotifies` (`MontagePlayRate 1.0`, `MontageStartSection Default`, `StopMontageWhenAbilityEnds`, `EndAbilityOnMontageEnd`, `ActivationGroup Exclusive_Replaceable`).
4. **Hit detection.** `GA_AttackBase_Melee::RegisterForHitTraces` collects the montage's `SpartaAnimNotifyState_HitCheck` notifies (`ANS_HitCheckNotifies`) and a `USpartaHitTrace`; `OnTraceHitEvent` -> `OnAttackHit(Victim, HitResult, DamagePayload, DamageCauser)` -> `ModifyAttackPayload` -> `USpartaHitPayload::ApplyPayload`. The payload applies `DamageEffect` (`GE_MeleeDamageBase`, execution `SpartaDamageExecution` capturing the `BaseDamage` attribute), `PoiseDamageEffect` (`GE_PoiseDamage`, `SpartaPoiseExecution`, captures `BasePoiseDamage`), `BreakDamageEffect` (`GE_BreakDamage`, `SpartaBreakExecution`, ignored while the target has `GameplayEffect.Immunity.Break`, `State.Death` or `State.Riposte`), `PoiseBrokenEffect` (`GE_PoiseBroken`, grants `State.PoiseBroken` for 0.01 s) and the `AdditionalEffects` list.
5. **Hold attacks.** Two mechanisms, section 1.3.
6. **Riposte, plunge, dash** are separate abilities (sections 4 and 5, and the riposte part of section 3).

### 1.3 Two hold-attack designs (game data)

Every montage that can be charged carries a hold-handler notify state that watches `Input.Attack.Primary.Hold` (light) or `Input.Attack.Secondary.Hold` (heavy), requires `Character.Unlocked.HoldAttack.Light/Heavy` on the character, and defines `TimeThresholdMinMax` and `HoldingPlayrate`. While the button is held inside the window the montage plays at `HoldingPlayrate`; the threshold is measured by `BPC_Player_ComboCounter` (`HoldAttackTickRate 0.005`, `HoldAttackTimeThreshold`, `HoldAttackHeldTime`, `GrantHoldAttackState` applies `GE_State_InHoldAttackCharge` = `State.Attack.Hold.Charge`, `PayHoldAttackCost` reads `PlayerAttributeSet.SecondaryMeleeAbilityCost` initialised by `GE_InitializeMeleeHoldAttackCost`). The two designs:

| design | weapons | input-bound selector plays | handler on that montage | on release before the min threshold | on hold past the threshold |
| --- | --- | --- | --- | --- | --- |
| normal-first | Iconoclast, Axe & Dagger, Great Martyr's Blade | the normal montage (`GA_Player_AttackSelector_Light_<W>` is `Ability.Attack.Selector.Main`) | `ANS_HoldAttackHandler` with `AttackTriggerEventTag Event.Attack.Selector.Light.Hold` (or `.Heavy.Hold`, `.Heavy.Hold.A/.B`, `.Light.Hold.Finisher`), `HoldingPlayrate 0.2` (Martyr's 0.185), window roughly 0.24 to 0.52 s | nothing, the normal swing continues | the `_Hold` selector (`AbilityTriggers` = that event) starts the `_Hold` ability and montage |
| hold-first | Veteran's Battle Axe, Obsidian Hammer, Black Needle, Clockwork Scythe, Axatana (axe and katanas) | the `_Hold` montage (`GA_Player_AttackSelector_Light_<W>_Hold` is the `Main` selector) | `ANS_HAH_Custom` with `AttackTriggerEventTag None`, `FailTriggerEvent Event.Attack.Selector.Light.Normal` (step 3: `.Normal.Finisher`), `HoldingPlayrate 0.85` (some 1.0, 0.7, 0.8), window roughly 0.1 to 1.4 s | fires the fail event, the normal selector (trigger `Event.Attack.Selector.Light.Normal`) starts the `_Cut` montage | the same `_Hold` montage keeps playing into its `Attack` section |

Thresholds are `0.5 to 1.05 s` almost everywhere (Axe & Dagger lights `0.4 to 0.7`, light 3 `0.4 to 0.85`; Axe & Dagger heavy 2 `0.5 to 1.0`). Damage on a hold attack: `GA_AttackBase_Melee::ApplyHoldAttackMultiplier` uses `HoldAttackDamage` (a min/max pair) through `BPC_Player_ComboCounter::GetHoldAttackDamage(Vector2D)`; `GA_Player_Attack_Light.HoldAttackDamage = 1.25 to 1.75`, `GA_Player_Attack_Heavy.HoldAttackDamage = 1.75 to 2.5`, `GA_AttackBase_Melee` default `1.0 to 2.0`. That the pair is interpolated by held time between the thresholds is inferred (the component tracks `ChargeRatio`, `HoldAttackHeldTime` and the threshold pair; the lerp itself is bytecode). Hold abilities set `IsHoldAttack = true`, `bUseHitStop = false` and their montages block every input until the queue window (`InputBlock` with an empty allow list), so a charged swing cannot be dodged out of once it starts.

### 1.4 Damage per hit (game data)

`USpartaHitPayload::HealthDamage` is an `FHealthDamageOption {Type, FloatValue, Multiplier}` with `EHealthDamageType = Float | InstigatingCharacterData | InstigatingCharacterWeapon`. `GA_Player_Attack_Light` and `_Heavy` both set `Type = InstigatingCharacterWeapon`, so a hit deals the equipped weapon's `ASpartaWeapon::BaseDamage` (35 Iconoclast, 30/20 Axe & Dagger, 38 Battle Axe, 80 Martyr's Blade, 109 Hammer, 35 Axatana axe, 25 per katana, 35 Black Needle, 42 Scythe, 40 Harbinger fist, 60 kick) times `Multiplier` (default 1.0, native default; every attack that serialises no multiplier and still deals damage confirms it) times the hold multiplier when charged, before `SpartaDamageExecution` applies Tarforge level, Harbinger level, crit and status (native, not readable here). The multipliers actually stored on player attacks are few:

| multiplier | where |
| --- | --- |
| 1.0 | every light chain hit, every hold attack base, katanas running, Axatana axe running, Martyr's running heavy stab, Smert, plunge (separate flat 100) |
| 1.5 | every heavy chain hit on every weapon (payload on `GA_Player_Attack_Heavy` subclasses), Axatana axe chain, Harbinger fist heavy right hand |
| 2.0 | Clockwork Scythe heavy 3 (`GA_Player_Attack_ClockworkScythe_B3`) |
| 1.35 | running light on Iconoclast, Battle Axe, Black Needle, Scythe, Hammer, Martyr's Blade |
| 1.25 | Axe & Dagger running light (three hits) |
| 1.8 | running heavy on Iconoclast, Axe & Dagger, Battle Axe, Black Needle, Scythe, Hammer |
| 0.8 | Duality Stone `_double` variants (two hits each) |
| 0.5 + 1.0 / 0.5 + 1.5 | Harbinger fists: left and right hand trace at once (`AM_Shared_Attacks_Fists_A/B`) |
| flat 25 x3 | Black Needle light hold (`Type Float`, `FloatValue 25`) |
| flat 5 | Needle Storm (`GA_WeaponAbility_StormStrike`) |
| flat 2, poise 4 | Clockwork Chainsaw ticks (`GA_Player_Attack_ClockworkScythe_Grinder`, `GE_ClockworkChainsawDamage`) |

Poise damage is `PoiseDamage` on the payload (base class default 20). Break damage: no player attack payload sets `BreakDamage` or `BreakDamageOption`, so ordinary swings deal 0 Break; Break comes from Perfect Guard / Harden / Parry, the Slayer Seal, `GA_Player_Attack_Light::Finisher_BreakDamage` (Stillblade's Stone) and abilities. Heavy payloads add `GE_BreakShield` (breaks an enemy block stance, `PostDamage`) and several add `GE_ForceFlybackReaction` (knock-down) or `GE_ForceDeathWithDismemberment` (Hammer). `StrikeDirection` on the payload picks the victim's directional hit reaction; `ReactionTag` is `Event.Reaction.Hit.Medium` on every ordinary swing (Needle Storm uses `Heavy`, some finishers `Flyback`).

## 2. Notify glossary (classes seen on player attack, dodge and riposte montages)

Fields come from the reflection headers (`headers-notify-classes.txt`); behaviour is from the class defaults and the effects they reference, marked inferred where the Blueprint graph was not read.

| class | fields (header) | role on player montages |
| --- | --- | --- |
| `SpartaAnimNotifyState_HitCheck` (native, notify name "Hit Check") | `DamagePayload`, `WeaponSlot`, `TraceSetup UseWeaponDefaults/Manual`, `TraceSource Weapon/Character`, `TraceType CollisionShapes/ComponentReferences/PhysicsBodies`, `CollisionShapes`, `bAllowMultipleHitsOnSameActor`, `MultipleHitOnSameActorDelay`, `bUseSharedHitList`, `SharedHitListId`, `MotionSubstep`, `TriggerTagQuery` | the hit window. 305 on exported montages; 229 carry only `WeaponSlot` (weapon default capsule, ability payload), 81 carry a `DamagePayload` override (multiplier, poise, direction, reaction, extra effects). With `UseWeaponDefaults` the shape is the weapon actor's `DefaultCollisionShape` capsule (section 6). Several hits in the same window on one montage = both hands / two weapons (Axe & Dagger, katanas, fists) or two shapes (Scythe uses paired windows). |
| `ANS_ChainsawHitCheck` (BP) | as HitCheck | Clockwork Scythe chainsaw tick trace while `State.Special.ClockworkScythe.Active`. |
| `AnimNotifyState_InputBlock` (native) | `Block {BlockBehavior BlockAll/BlockSpecified/AllowSpecified, InputTag[]}` | commitment. Normal-first lights: 0 to about 0.3 to 0.5 s allow only `Dodge` and `Parry.Press` (you can dodge-cancel the wind-up), then only `Parry.Press` until the queue closes. Hold and ability montages: empty allow list = all input blocked. |
| `AnimNotifyState_InputQueue` (native) | none | buffers the next attack/dodge press; the queued input fires when the window ends (inferred from the fact that combo continuation timing matches window end). Every attack has one; dashes and ripostes too. |
| `ANS_InterruptWithMovement` (BP) | none (references `BPI_PlayerController`) | from its start, a movement input ends the montage (recovery cancel). Present on every attack, dash, riposte, stun montage. |
| `ANS_HyperArmor` (BP) | `RequiredTag` | applies `GE_HyperArmor` (grants `State.HyperArmor`; `HRAS_Player` redirects Medium and Heavy reactions to `Ignore` while that tag is present). Unconditional on heavies, running attacks, hold attacks. |
| `ANS_AddGameplayEffectConditional` + `BP_CO_CheckUpgradeStatValue` (BP) | `GameplayEffectClass`, `ConditionObject.UpgradeStat`, `TriggerTagQuery` | same hyper armor on light chains only when `Character.Unlocked.Unyielding` is present and `UpgradeStat.Melee.Unyielding` says which steps (Unyielding Stone). |
| `ANS_HoldAttackHandler` (BP) | `RequiredTag`, `InputTagToCheck`, `AttackTriggerEventTag`, `FailTriggerEvent`, `ReleasePlayrate`, `HoldingPlayrate`, `TimeThresholdMinMax` | normal-first charge window (section 1.3). Class defaults: `RequiredTag Character.Unlocked.HoldAttack`, `HoldingPlayrate 0.4`, `ReleasePlayrate 1.0`. |
| `ANS_HAH_Custom`, `ANS_HAH_Generic` (BP, child classes) | same | hold-first charge window with `FailTriggerEvent` (section 1.3). |
| `ANS_SpartaMotionWarping_Translation` (BP) + `RootMotionModifier_SkewWarp` | `RootMotionModifier` | lunge toward the soft target: root motion in the window is skew-warped to `WT_DesiredEndLocation`, clamped by the ability's `WarpTranslationDistance` (min, max cm) and `WarpTranslationStoppingDistance` (stop this far from the target). |
| `ANS_RotateToFaceTarget` (BP) | `bUseWeaponInterpSpeed`, `FixedInterpolationSpeed`, `PlayerTargetUpdateFrequency`, `AlignmentMethod` | turn toward the target during wind-up at `ASpartaWeapon::FaceToTargetInterpSpeed` (Hammer 3, Martyr's 4, Scythe 5, Iconoclast/Axe 6, Black Needle/Axe & Dagger 8, katanas 9). |
| `CSAnimNotifyState_TimeDilation` (native) | `TimeDilation`, `Type`, blends | speeds or slows the animation for feel (x0.5 to x1.8 windows around the contact frame). |
| `CSAnimNotify_SendGameplayTagEvent` (native) | `EventTag` | `AI.Event.EarlyOut.Attack` (AI users of the same montage may leave), `Event.Ability.End` (dash ability end), `Event.Ability.Riposte.End`, `Event.Ability.PlummetStrike.BreakAreaDamage`. |
| `AN_TriggerElementalMechanic` (BP) | `Mechanic`, `WeaponSlot`, `OverridePayload`, `ForceTrigger` | fires the infusion element mechanic at the impact frame of hold and ability attacks. |
| `ANS_WeaponFX`, `SpartaAnimNotify_WeaponEvent`, `AN_CameraShake`, `AN_GroundImpact`, `AN_SpartaNiagara`, `SpartaAnimNotify_Footstep` | audio/VFX | whoosh and trail per `AttackType Light/Medium/Heavy/Special`, WW audio events, shakes. No gameplay effect. |
| `ANS_AddGameplayEffect` (BP) | `GameplayEffectClass` | `GE_Invulnerability` on weapon abilities and ripostes (immunity to damage, hit reactions and conditions), `GE_State_Animation_Riposte_Instigator`, `GE_ProjectileImmunity`. |
| `ANS_AddGameplayTags` (BP) | `Tags`, `ApplyToAIOnly` | `State.Hit.IgnorePhysReaction` on plunge, `State.ActiveBlock.Broken` on parried stun. |
| `ANS_BlockHarden` (BP) | none (applies `GE_Block_Harden`) | no Harden during weapon abilities. |
| `ANS_ModifyWeaponCollisionSize` (BP) | `WeaponSlot`, `SizeMultiplier` (default 2.0) | Deadly Flurry doubles the trace capsule. |
| `AN_RiposteDamage` (BP) | `DamageMultiplier` (default 1.0), `UseMaxHealthPercentage`, `MaxHealthPercentage`, `MaxHealthPercentage (MiniBoss)` | the damage ticks of a riposte (section 3, riposte). |
| `AN_Axatana_Transform_ToAxe/ToKatanas` (BP) | none | Axatana morph at montage start. |
| `AnimNotify_PlayMontageNotify` (engine) | `NotifyName` | Blueprint callbacks: `Shockwave` (Heavy Stomps), `AOE`, `Activate`, `FindSoftTarget`, `ClearWarpTargets`, `ResetWeaponState`. |
| `ANS_AbyssCheckForRM_Disable` (BP) | `DistanceToCharacter 200`, `TraceDownLength 200` | on dashes: cuts root motion at a ledge. |

## 3. Per-weapon tables

Column key: hit windows = `SpartaAnimNotifyState_HitCheck` begin-end; damage = `HealthDamage` source and multiplier (W = the weapon's BaseDamage), plus the hold multiplier range for charged attacks; poise = payload `PoiseDamage`; queue = `InputQueue` window; dodge ok = `InputBlock` windows that still allow `InputTag.Ability.Dodge`; move cancel = `ANS_InterruptWithMovement` start; hyper armor = `ANS_HyperArmor` window (Unyielding = conditional on the stone); hold handler = charge window, thresholds, holding play rate and the event it fires; warp = `WarpTranslationDistance` min-max and stopping distance. Break damage is 0 on every row (section 1.4). Full notify lists per montage are in `work/movesets/montage-listing.md`.

### The Iconoclast (HadernsSword, BaseDamage 35, locomotion H1)

Light chain, selector `GA_Player_AttackSelector_Light_HadernsSword`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `HadernsSword_A1` | `A_Shared_HadernSword_A_03_Montage` 3.20 | 0.59-0.74 | Wx1.0 | 30 | Left | 0.50-0.90 | 0.00-0.50 | 0.90 |  (Unyielding: 0.00-1.20) | 0.24-0.52 hold 0.5-1.05s @x0.2 -> Light.Hold | 50-200 / 80 |
| Light 2 | `HadernsSword_A2` | `A_Shared_HadernSword_AA_Montage` 3.10 | 0.43-0.65 | Wx1.0 | 30 | Right | 0.35-0.77 | 0.00-0.30 | 0.77 |  (Unyielding: 0.00-0.77) | 0.02-0.30 hold 0.5-1.05s @x0.2 -> Light.Hold | 50-200 / 80 |
| Light 3 | `HadernsSword_A3` | `A_Shared_HadernSword_AAA_Montage` 3.23 | 0.79-1.08 | Wx1.0 | 30 | Down | 0.80-1.40 | 0.00-0.40 | 1.40 |  (Unyielding: 0.00-1.13) | 0.15-0.43 hold 0.5-1.05s @x0.2 -> Light.Hold.Finisher | 50-200 / 80 |
| running | `HadernsSword_RunningAttack` | `A_Shared_Attacks_HadernSword_Running_A_Montage` 4.50 | 0.58-0.72 | Wx1.35 | 45 | Left | 0.59-1.13 | - | 1.13 | 0.00-1.13 | - | 50-550 / 100 |
| finisher / extra | `HadernsSword_A_Finisher` | `A_Shared_HadernSword_AAA_Finisher_Montage` 3.37 | 0.29-0.45; 0.73-0.86 | Wx1.0 | 30 | Left | 0.50-0.86 | 0.00-0.30 | 0.89 | - | 0.01-0.20 hold 0.5-1.05s @x0.2 -> Light.Hold.Finisher | 50-200 / 80 |

LightHold chain, selector `GA_Player_AttackSelector_Light_HadernsSword_Hold`, event-triggered by Event.Attack.Selector.Light.Hold.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_HadernSword_A1_Hold` | `A_Shared_HadernSword_A_Hold_Montage` 3.82 | 0.16-0.47; 0.77-1.14 | Wx1.0, hold x1.25-1.75 | 20 | Down | 0.60-1.40 | - | 1.40 | - | - | 125-255 / 80 |
| LightHold 2 | `Attack_HadernSword_A2_Hold` | `A_Shared_HadernSword_AA_Hold_Montage` 3.94 | 0.20-0.52; 0.74-0.99 | Wx1.0, hold x1.25-1.75 | 20 | Down | 0.70-1.35 | - | 1.35 | - | - | 125-255 / 80 |
| LightHold 3 | `Attack_HadernSword_A3_Hold` | `A_Shared_HadernSword_AAA_Hold_Montage` 3.54 | 0.30-0.62; 0.93-1.23 | Wx1.0, hold x1.25-1.75 | 20 | Down | 0.80-1.55 | - | 1.55 | - | - | 125-255 / 80 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_HadernsSword`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `HadernsSword_B1` | `A_Shared_HadernSword_B_Montage` 2.83 | 0.68-0.79 | Wx1.5 | 40 | Forward BreakShield | 0.50-1.00 | - | 1.00 | 0.00-0.90 | 0.24-0.52 hold 0.5-1.05s @x0.2 -> Heavy.Hold | 80-250 / 100 |
| Heavy 2 | `HadernsSword_B2` | `A_Shared_Attacks_HadernSword_BB_02_Montage` 2.97 | 0.44-0.72 | Wx1.5 | 40 | Forward BreakShield | 0.50-1.00 | - | 1.00 | 0.00-0.90 | 0.11-0.39 hold 0.5-1.05s @x0.2 -> Heavy.Hold | 100-255 / 100 |
| Heavy 3 | `HadernsSword_B3` | `A_Shared_HadernSword_BBB_Montage` 2.70 | 0.83-1.07 | Wx1.5 | 40 | Down BreakShield | 1.00-1.60 | - | 1.60 | 0.00-1.40 | 0.18-0.41 hold 0.5-1.05s @x0.2 -> Heavy.Hold.Finisher | 125-230 / 100 |
| running | `HadernsSword_RunningAttack_B` | `A_Shared_Attacks_HadernSword_Running_B_Montage` 4.03 | 0.85-1.01 | Wx1.8 | 50 | Right BreakShield | 1.00-1.37 | - | 1.37 | 0.00-1.37 | - | 75-800 / 75 |
| finisher / extra | `HadernsSword_B_Finisher` | `A_Shared_HadernSword_BBB_Finisher_Montage` 4.63 | 0.89-1.26 | Wx1.5 | 40 | Forward BreakShield | 1.00-2.00 | - | 2.00 | 0.00-2.00 | 0.32-0.60 hold 0.5-1.05s @x0.2 -> Heavy.Hold.Finisher | 80-250 / 100 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_HadernsSword_Hold`, event-triggered by Event.Attack.Selector.Heavy.Hold.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_HadernsSword_B1_Hold` | `A_Shared_HadernSword_B_Hold_Montage` 2.77 | 0.09-0.27; 0.58-0.74 | Wx1.0, hold x1.75-2.5 | 40 | Down BreakShield | 0.35-0.86 | - | 0.86 | - | - | 150-400 / 80 |
| HeavyHold 2 | `Attack_HadernsSword_B2_Hold` | `A_Shared_HadernSword_BB_Hold_Montage` 3.04 | 0.14-0.27; 0.54-0.70 | Wx1.0, hold x1.75-2.5 | 40 | Down BreakShield | 0.35-0.95 | - | 0.95 | - | - | 150-400 / 80 |
| HeavyHold 3 | `Attack_HadernsSword_B3_Hold` | `A_Shared_HadernSword_BBB_Hold_Montage` 4.81 | 0.36-0.57; 1.19-1.35 | Wx1.0, hold x1.75-2.5 | 40 | Down BreakShield | 0.40-1.52 | - | 1.54 | - | - | 150-400 / 80 |

### Axe & Dagger (DaggerAxe, axe 30 / dagger 20, locomotion M)

Light chain, selector `GA_Player_AttackSelector_Light_AxeDagger`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_AxeDagger_A1` | `AM_Shells_AxeDagger_3Hit_A` 2.40 | 0.47-0.53 | Wx1.0 | 20 | Left | 0.32-0.67 | 0.00-0.30 | 0.67 |  (Unyielding: 0.00-0.60) | 0.11-0.27 hold 0.4-0.7s @x0.2 -> Light.Hold | 0-250 / 100 |
| Light 2 | `Attack_AxeDagger_A2` | `AM_Shells_AxeDagger_3Hit_aA` 2.40 | 0.49-0.56 | Wx1.0 | 20 | Right | 0.23-0.63 | 0.00-0.30 | 0.63 |  (Unyielding: 0.00-0.60) | 0.07-0.29 hold 0.4-0.7s @x0.2 -> Light.Hold | 0-250 / 100 |
| Light 3 | `Attack_AxeDagger_A3` | `AM_Shells_AxeDagger_3Hit_aaA` 2.67 | 0.42-0.66 | Wx1.0 | 20 | Forward | 0.27-0.67 | 0.00-0.30 | 0.67 |  (Unyielding: 0.00-0.60) | 0.15-0.37 hold 0.4-0.85s @x0.2 -> Light.Hold | 0-250 / 100 |
| running | `Running_Attack_AxeDagger_A` | `A_Shared_Attacks_AxeDagger_Running_B_Montage` 4.00 | 0.49-0.72; 0.62-0.85; 1.09-1.32 | Wx1.25 | 25 | Right | 0.95-1.55 | - | 1.55 |  (Unyielding: 0.00-1.45) | - | 120-550 / 100 |
| finisher / extra | `Attack_AxeDagger_A1_Double` | `AM_Shells_AxeDagger_3Hit_A_dble` 2.40 | 0.22-0.31; 0.47-0.57 | Wx0.8 | 20 | Left | 0.17-0.67 | 0.00-0.30 | 0.67 | - | 0.03-0.20 hold 0.4-0.7s @x0.2 -> Light.Hold | 90-200 / 50 |
| finisher / extra | `Attack_AxeDagger_A2_Double` | `AM_Shells_AxeDagger_3Hit_aA_dble` 2.83 | 0.29-0.36; 0.48-0.54 | Wx0.8 | 20 | Right | 0.33-0.77 | 0.00-0.33 | 0.77 | - | 0.00-0.26 hold 0.4-0.7s @x0.2 -> Light.Hold | 40-315 / 75 |
| finisher / extra | `Attack_AxeDagger_A3_Double` | `AM_Shells_AxeDagger_3Hit_aaA_dble` 2.50 | 0.22-0.35; 0.46-0.64 | Wx0.8 | 20 | Forward | 0.30-0.67 | 0.00-0.30 | 0.67 | - | 0.00-0.19 hold 0.4-0.85s @x0.2 -> Light.Hold | 140-350 / 80 |

LightHold chain, selector `GA_Player_AttackSelector_Light_AxeDagger_Hold`, event-triggered by Event.Attack.Selector.Light.Hold.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_AxeDagger_A1_Hold` | `A_Shared_Attacks_AxeDagger_Hold_A_02_MontageFull` 2.97 | 0.31-0.46; 0.59-0.72; 1.12-1.29 | Wx1.0, hold x1.25-1.75 | 20 | Down | 1.30-1.80 | - | 1.80 | 0.00-1.38 | - | 125-255 / 90 |
| LightHold 2 | `Attack_AxeDagger_A2_Hold` | `A_Shared_Attacks_AxeDagger_Hold_AA_02_MontageCut` 2.82 | 0.21-0.40; 0.72-0.87; 1.34-1.46 | Wx1.0, hold x1.25-1.75 | 20 | Down | 1.50-2.15 | - | 2.15 | 0.00-2.15 | - | 125-255 / 90 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_AxeDagger`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_AxeDagger_B1` | `A_Shells_Attacks_AxeDagger_B_Montage` 3.17 | 0.74-0.93 | Wx1.5 | 25 | Up BreakShield | 0.66-1.20 | - | 1.20 | 0.00-1.10 | 0.25-0.55 hold 0.5-1s @x0.2 -> Heavy.Hold.A | 300-375 / 80 |
| Heavy 2 | `Attack_AxeDagger_B2` | `A_MS1_Player_Hammer_bB_Montage_02` 3.33 | 0.63-0.79 | Wx1.5 | 25 | Right BreakShield | 0.30-0.98 | - | 0.98 | 0.00-0.92 | 0.12-0.42 hold 0.5-1s @x0.2 -> Heavy.Hold.A | 60-340 / 80 |
| Heavy 3 | `Attack_AxeDagger_B3` | `A_MS1_Player_Hammer_bbB_Montage` 2.47 | 0.87-1.00 | Wx1.5 | 25 | Down BreakShield | 0.97-1.50 | - | 1.50 | 0.00-1.22 | 0.15-0.45 hold 0.5-1.05s @x0.2 -> Heavy.Hold.A | 115-300 / 90 |
| running | `Running_Attack_AxeDagger_B` | `A_MS1_Player_Hammer_Run_Attack5_Montage` 2.30 | 0.80-0.98 | Wx1.8 | 60 | Down BreakShield | 0.50-1.30 | - | 1.30 | 0.00-1.20 | - | 0-450 / 100 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_AxeDagger_Hold`, event-triggered by Event.Attack.Selector.Heavy.Hold.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_AxeDagger_B1_Hold` | `AM_Shells_Attacks_AxeDagger_Hold_B` 3.62 | 0.20-0.39; 0.58-0.76; 0.81-0.99 | Wx1.0, hold x1.75-2.5 | 20 | Up BreakShield | 0.40-1.27 | - | 1.39 | 0.00-1.05 | - | 80-600 / 60 |
| HeavyHold 2 | `Attack_AxeDagger_B2_Hold` | `AM_Shells_Attacks_AxeDagger_Hold_BB` 3.33 | 0.28-0.43; 0.63-0.75 | Wx1.0, hold x1.75-2.5 | 20 | Up BreakShield | 0.55-1.20 | - | 1.20 | 0.00-0.83 | - | 80-600 / 60 |

### Veteran's Battle Axe (BattleAxe, 38, H1)

Light chain, selector `GA_Player_AttackSelector_Light_BattleAxe`, event-triggered by Event.Attack.Selector.Light.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_BattleAxe_A1` | `A_Shared_Attacks_BattleAxe_A_04_Montage` 4.21 | 0.34-0.47 | Wx1.0 | 50 | Left | 0.30-0.80 | 0.00-0.80 | 0.80 | 0.00-0.70 | - | 125-300 / 120 |
| Light 2 | `Attack_BattleAxe_A2` | `A_Shared_Attacks_BattleAxe_AA_01_Cut_Montage` 2.83 | 0.29-0.38 | Wx1.0 | 50 | Up | 0.20-0.70 | 0.00-0.70 | 0.70 | 0.00-0.60 | - | 125-400 / 75 |
| Light 3 | `Attack_BattleAxe_A3` | `A_Shared_Attacks_BattleAxe_AAA_02_Montage` 3.37 | 0.48-0.71 | Wx1.0 | 50 | Right | 0.45-0.90 | 0.00-0.35 | 0.90 | 0.00-0.85 | - | 125-400 / 150 |
| finisher / extra | `Attack_BattleAxe_A_Finisher` | `A_Shared_Attacks_BattleAxe_AAA_Finisher_Montage` 3.70 | 0.48-0.71 | Wx1.0 | 50 | Forward ForceFlybackReaction | 0.97-1.33 | 0.00-0.35 | 1.33 | 0.00-1.33 | - | 125-400 / 50 |

LightHold chain, selector `GA_Player_AttackSelector_Light_BattleAxe_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_BattleAxe_A1_Hold` | `A_Shared_Attacks_BattleAxe_A_Hold_04_Montage` 6.40 | 1.65-1.78; 2.39-2.57 | Wx1.0, hold x1.25-1.75 | 50 | Down Flyback | 2.75-3.40 | 0.00-0.32 | 3.40 | 0.00-3.30 | 0.37-1.35 hold 0.5-1.05s @x1.0, release early -> Light.Normal | 125-300 / 80 |
| LightHold 2 | `Attack_BattleAxe_A2_Hold` | `A_Shared_Attacks_BattleAxe_AA_01_Hold_Montage` 6.10 | 1.22-1.35; 2.05-2.14 | Wx1.0, hold x1.25-1.75 | 50 | Down Flyback | 2.15-2.80 | 0.00-0.20 | 2.80 | 0.00-2.70 | 0.23-1.01 hold 0.5-1.05s @x0.85, release early -> Light.Normal | 125-300 / 75 |
| LightHold 3 | `Attack_BattleAxe_A3_Hold` | `A_Shared_Attacks_BattleAxe_AAA_01_Hold_Montage` 5.37 | 1.09-1.27; 1.91-1.97 | Wx1.0, hold x1.25-1.75 | 50 | Down; Right Flyback | 3.20-3.80 | 0.00-0.13 | 3.78 | 0.00-3.50 | 0.10-0.57 hold 0.5-1.05s @x0.85, release early -> Light.Normal.Finisher | 125-300 / 125 |
| running | `Running_Attack_BattleAxe` | `A_Shared_Attacks_BattleAxe_Running_01_Montage` 4.40 | 0.80-0.91 | Wx1.35 | 80 | Down BreakShield/ForceFlybackReaction | 0.73-1.50 | 0.00-0.17 | 1.50 | 0.00-1.50 | - | 120-600 / 100 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_BattleAxe`, event-triggered by Event.Attack.Selector.Heavy.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_BattleAxe_B1` | `A_Shared_Attacks_BattleAxe_B_01_Cut_Montage` 5.10 | 0.73-0.82 | Wx1.5 | 50 | Down BreakShield | 0.90-1.47 | 0.00-0.90 | 1.47 | 0.00-1.47 | - | 125-400 / 50 |
| Heavy 2 | `Attack_BattleAxe_B2` | `A_Shared_Attacks_BattleAxe_BB_03_Cut_Montage` 4.32 | 0.42-0.55 | Wx1.5 | 50 | Left BreakShield | 0.47-0.90 | 0.00-0.47 | 0.90 | 0.00-0.90 | - | 125-400 / 50 |
| Heavy 3 | `Attack_BattleAxe_B3` | `A_Shared_Attacks_BattleAxe_BBB_01_Cut_Montage` 4.53 | 0.74-1.03 | Wx1.5 | 50 | Up BreakShield | 0.85-1.40 | 0.00-0.85 | 1.40 | 0.00-1.35 | - | 125-400 / 50 |
| finisher / extra | `Attack_BattleAxe_B_Finisher` | `A_Shared_Attacks_BattleAxe_BBB_Finisher_01_Montage` 3.80 | 0.92-1.04 | Wx1.5 | 70 | Left BreakShield/ForceFlybackReaction | 1.25-1.85 | 0.00-0.70 | 1.85 | 0.00-1.80 | - | 125-400 / 50 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_BattleAxe_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_BattleAxe_B1_Hold` | `A_Shared_Attacks_BattleAxe_B_Hold_01_Montage` 6.60 | 1.52-1.70; 2.30-2.40 | Wx1.0, hold x1.75-2.5 | 100 | Down BreakShield | 2.23-2.88 | 0.00-0.46 | 2.88 | 0.00-2.84 | 0.45-1.17 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 150-400 / 100 |
| HeavyHold 2 | `Attack_BattleAxe_B2_Hold` | `A_Shared_Attacks_BattleAxe_BB_Hold_03_Montage` 6.70 | 1.50-1.69; 2.37-2.44 | Wx1.0, hold x1.75-2.5 | 100 | Down; Left BreakShield | 2.00-3.03 | 0.00-0.45 | 3.03 | 0.00-3.03 | 0.46-1.28 hold 0.5-1.05s @x1.0, release early -> Heavy.Normal | 100-400 / 120 |
| HeavyHold 3 | `Attack_BattleAxe_B3_Hold` | `A_Shared_Attacks_BattleAxe_BBB_Hold_01_Montage` 5.47 | 1.53-1.82; 2.35-2.44 | Wx1.0, hold x1.75-2.5 | 100 | Right BreakShield | 2.72-3.23 | 0.00-0.25 | 3.23 | 0.00-3.23 | 0.30-1.34 hold 0.5-1.05s @x1.0, release early -> Heavy.Normal.Finisher | 150-300 / 120 |
| running | `Running_Attack_B_BattleAxe` | `A_Shared_Actions_BattleAxe_Running_B_Montage` 4.20 | 0.73-1.08; 1.24-1.53 | Wx1.8 | 80 | Down BreakShield/ForceFlybackReaction | 1.35-2.00 | 0.00-0.20 | 2.00 | 0.00-1.90 | - | 120-600 / 100 |

### Great Martyr's Blade (MartyrsBlade, 80, H2)

Light chain, selector `GA_Player_AttackSelector_Light_MartyrsBlade`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_MartyrsBlade_A1` | `A_MS1_Player_MartyrsBlade_Attack_A_Montage` 2.50 | 0.65-0.86 | Wx1.0 | 70 | Right ForceFlybackReaction | 0.62-1.15 | 0.00-0.55 | 1.15 | 0.00-1.07 | 0.30-0.55 hold 0.5-1.05s @x0.185 -> Light.Hold | 125-255 / 80 |
| Light 2 | `Attack_MartyrsBlade_A2` | `A_MS1_Player_MartyrsBlade_Attack_AA_Montage` 3.20 | 0.97-1.19 | Wx1.0 | 70 | Forward ForceFlybackReaction | 0.99-1.55 | 0.00-0.55 | 1.55 | 0.00-1.34 | 0.40-0.68 hold 0.5-1.05s @x0.2 -> Light.Hold | 125-255 / 80 |
| Light 3 | `Attack_MartyrsBlade_A3` | `A_MS1_Player_MartyrsBlade_Attack_AAA_Montage` 1.87 | 0.61-0.85 | Wx1.0 | 80 | Forward ForceFlybackReaction | 0.47-0.97 | 0.00-0.37 | 0.97 | 0.00-0.94 | 0.33-0.58 hold 0.5-1.05s @x0.185 -> Light.Hold | 125-255 / 80 |
| running | `Running_Attack_MartyrsBlade` | `AM_Shells_MartyrsBlade_Run_Attack` 3.00 | 0.74-0.92 | Wx1.35 | 80 | Up BreakShield/ForceFlybackReaction | 0.70-1.50 | - | 1.50 | 0.00-1.20 | - | 120-550 / 100 |
| finisher / extra | `Attack_MartyrsBlade_A_Finisher` | `A_Shared_Attacks_MartyrsBlade_AAA_Finisher_Montage` 3.47 | 0.55-0.76 | Wx1.0 | 80 | Forward ForceFlybackReaction | 1.16-1.67 | 0.00-0.40 | 1.67 | 0.00-1.50 | 0.23-0.48 hold 0.5-1.05s @x0.185 -> Light.Hold | 125-255 / 50 |

LightHold chain, selector `GA_Player_AttackSelector_Light_MartyrsBlade_Hold`, event-triggered by Event.Attack.Selector.Light.Hold.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_MartyrsBlade_A2_Hold` | `A_Shared_MartyrsBlade_HoldAttacks_AA_Blast_Montage` 3.93 | 0.27-0.63 | Wx1.0, hold x1.25-1.75 | 90 | Down ForceFlybackReaction | 1.15-1.80 | - | 1.80 | 0.00-1.70 | - | 125-255 / 80 |
| LightHold 2 | `Attack_MartyrsBlade_A1_Hold` | `A_Shared_MartyrsBlade_HoldAttacks_A_Blast_Montage` 4.53 | 0.85-1.21 | Wx1.0, hold x1.25-1.75 | 90 | Down ForceFlybackReaction | 1.85-2.35 | - | 2.35 | 0.00-2.34 | - | 125-255 / 80 |
| LightHold 3 | `Attack_MartyrsBlade_A2_Hold` | `A_Shared_MartyrsBlade_HoldAttacks_AA_Blast_Montage` 3.93 | 0.27-0.63 | Wx1.0, hold x1.25-1.75 | 90 | Down ForceFlybackReaction | 1.15-1.80 | - | 1.80 | 0.00-1.70 | - | 125-255 / 80 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_MartyrsBlade`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_MartyrsBlade_B1` | `AM_Shells_MartyrsBlade_B1` 3.53 | 1.21-1.49 | Wx1.5 | 100 | Down BreakShield/ForceFlybackReaction | 1.48-2.00 | - | 2.00 | 0.00-1.85 | 0.37-0.70 hold 0.5-1.05s @x0.2 -> Heavy.Hold.A | 300-375 / 100 |
| Heavy 2 | `Attack_MartyrsBlade_B2` | `AM_Shells_MartyrsBlade_B2` 3.43 | 1.21-1.49 | Wx1.5 | 100 | Down BreakShield/ForceFlybackReaction | 1.20-1.80 | - | 1.77 | 0.00-1.70 | 0.37-0.70 hold 0.5-1.05s @x0.2 -> Heavy.Hold.A | 60-340 / 70 |
| Heavy 3 | `Attack_MartyrsBlade_B3` | `AM_Shells_MartyrsBlade_B3` 3.63 | 1.61-1.89 | Wx1.5 | 100 | Right BreakShield/ForceFlybackReaction | 1.82-2.30 | - | 2.30 | 0.00-2.10 | 0.77-1.10 hold 0.5-1.05s @x0.2 -> Heavy.Hold.B | 115-300 / 100 |
| running | `Running_Attack_MartyrsBlade_B` | `A_Shared_Attacks_MartyrsBlade_Running_B_Stab_Montage` 4.27 | 0.45-0.77; 1.10-1.42 | Wx1.0 | 80 | Up | 1.20-1.85 | - | 1.85 | 0.00-1.80 | - | 120-550 / 100 |
| finisher / extra | `Attack_MartyrsBlade_B_Finisher` | `A_Shared_Attacks_MartyrsBlade_BBB_Finisher_Montage` 4.93 | 0.93-1.51 | Wx1.5 | 100 | Left BreakShield/ForceFlybackReaction | 1.88-2.50 | - | 2.50 | 0.00-2.20 | 0.61-0.93 hold 0.5-1.05s @x0.2 -> Heavy.Hold.A | 125-255 / 50 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_MartyrsBlade_Hold`, event-triggered by Event.Attack.Selector.Heavy.Hold.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_MartyrsBlade_B1_Hold` | `AM_Shell_Attacks_MartyrsBlade_Hold_B` 5.03 | 0.81-1.01; 2.64-2.78 | Wx1.0, hold x1.75-2.5 | 120 | Down BreakShield/ForceFlybackReaction | 3.00-3.50 | - | 3.50 | 0.00-3.00 | - | 150-400 / 100 |
| finisher / extra | `Attack_MartyrsBlade_B3_Hold` | `AM_Shell_Attacks_MartyrsBlade_Hold_BBB` 5.07 | 1.01-1.12; 2.64-2.78 | Wx1.0 | 120 | Down BreakShield/ForceFlybackReaction | 3.00-3.50 | - | 3.50 | 0.00-3.00 | - | 150-400 / 100 |

### Obsidian Hammer (HeavyHammer, 109, H1)

Light chain, selector `GA_Player_AttackSelector_Light_HeavyHammer`, event-triggered by Event.Attack.Selector.Light.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_HeavyHammer_A1` | `A_Shared_HeavyHammer_Overhead_01_Cut_Montage` 4.94 | 0.46-0.65 | Wx1.0 | 100 | Forward BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 0.83-1.35 | 0.00-0.35 | 1.35 | 0.00-1.25 | - | 125-450 / 130 |
| Light 2 | `Attack_HeavyHammer_A2` | `A_Shared_HeavyHammer_Overhead_02_Cut_Montage` 3.64 | 0.63-0.76 | Wx1.0 | 100 | Forward BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 0.47-1.05 | 0.00-0.35 | 1.05 | 0.00-0.95 | - | 125-400 / 150 |
| Light 3 | `Attack_HeavyHammer_A3` | `A_Shared_HeavyHammer_Overhead_03_Cut_Montage` 4.91 | 0.56-0.96 | Wx1.0 | 110 | Forward BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 0.91-1.50 | 0.00-0.35 | 1.50 | 0.00-1.40 | - | 125-400 / 150 |
| finisher / extra | `Attack_HeavyHammer_A_Finisher` | `A_Shared_HeavyHammer_Overhead_03_Finisher_Cut_Montage` 6.24 | 0.61-0.74; 1.85-1.95 | Wx1.0 | 50 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 2.95-3.75 | 0.00-0.30 | 3.75 | 0.00-3.70 | - | 125-400 / 50 |

LightHold chain, selector `GA_Player_AttackSelector_Light_HeavyHammer_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_HeavyHammer_A1_Hold` | `A_Shared_HeavyHammer_Overhead_01_Hold_Montage` 7.57 | 2.07-2.20 | Wx1.0, hold x1.25-1.75 | 110 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 2.26-2.80 | 0.00-0.52 | 2.80 | 0.00-2.70 | 0.51-1.26 hold 0.5-1.05s @x0.7, release early -> Light.Normal | 125-255 / 80 |
| LightHold 2 | `Attack_HeavyHammer_A2_Hold` | `A_Shared_HeavyHammer_Overhead_02_Hold_Montage` 6.63 | 2.07-2.20 | Wx1.0, hold x1.25-1.75 | 110 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 2.23-2.80 | 0.00-0.68 | 2.80 | 0.00-2.70 | 0.69-1.31 hold 0.5-1.05s @x0.85, release early -> Light.Normal | 125-255 / 80 |
| LightHold 3 | `Attack_HeavyHammer_A3_Hold` | `A_Shared_HeavyHammer_Overhead_03_Hold_Montage` 6.60 | 2.07-2.20 | Wx1.0, hold x1.25-1.75 | 120 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 2.23-2.80 | 0.00-0.33 | 2.80 | 0.00-2.70 | 0.37-1.27 hold 0.5-1.05s @x0.85, release early -> Light.Normal.Finisher | 125-255 / 80 |
| running | `Running_Attack_HeavyHammer` | `A_Shared_Attacks_HeavyHammer_Running_03_Montage` 4.40 | 0.80-0.91 | Wx1.35 | 110 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 0.75-1.78 | 0.00-0.20 | 1.78 | 0.00-1.78 | - | 120-600 / 100 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_HeavyHammer`, event-triggered by Event.Attack.Selector.Heavy.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_HeavyHammer_B1` | `A_Shared_Attacks_HeavyHammer_B_Cut_Montage` 5.17 | 0.34-0.73 | Wx1.5 | 120 | Forward BreakShield/ForceDeathWithDismemberment | 0.70-1.25 | 0.00-0.60 | 1.25 | 0.00-1.15 | - | 125-400 / 50 |
| Heavy 2 | `Attack_HeavyHammer_B2` | `A_Shared_Attacks_HeavyHammer_BB_Cut_Montage` 4.73 | 0.70-1.11 | Wx1.5 | 120 | Forward BreakShield/ForceDeathWithDismemberment | 0.95-1.55 | 0.00-0.55 | 1.55 | 0.00-1.45 | - | 125-400 / 50 |
| Heavy 3 | `Attack_HeavyHammer_B3` | `A_Shared_Attacks_HeavyHammer_BBB_Cut_Montage` 5.30 | 0.34-0.76 | Wx1.5 | 130 | Forward BreakShield/ForceDeathWithDismemberment | 2.05-2.85 | 0.00-0.35 | 2.85 | 0.00-2.05 | - | 125-400 / 50 |
| finisher / extra | `Attack_HeavyHammer_B_Finisher` | `A_Shared_Attacks_HeavyHammer_BBB_Finisher_Cut_Montage` 6.05 | 0.71-1.07; 1.24-1.60 | Wx1.5 | 130 | Left BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 2.85-3.60 | 0.00-2.45 | 3.60 | 0.00-3.50 | - | 125-400 / 50 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_HeavyHammer_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_HeavyHammer_B1_Hold` | `A_Shared_Attacks_HeavyHammer_B_Hold_Montage` 7.53 | 2.04-2.18; 3.01-3.15 | Wx1.0, hold x1.75-2.5 | 130 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 3.20-4.50 | 0.00-0.51 | 4.50 | 0.00-4.50 | 0.51-1.26 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 150-400 / 100 |
| HeavyHold 2 | `Attack_HeavyHammer_B2_Hold` | `A_Shared_Attacks_HeavyHammer_BB_Hold_Montage` 7.77 | 2.10-2.29; 3.09-3.28; 4.52-4.67 | Wx1.0, hold x1.75-2.5 | 130 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 3.33-4.10 | 0.00-0.36 | 4.10 | 0.00-4.10 | 0.42-1.17 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 150-900 / 50 |
| HeavyHold 3 | `Attack_HeavyHammer_B3_Hold` | `A_Shared_Attacks_HeavyHammer_BBB_Hold_Montage` 7.03 | 2.10-2.29; 2.94-3.13; 4.45-4.57 | Wx1.0, hold x1.75-2.5 | 150 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 3.50-4.90 | 0.00-0.51 | 4.90 | 0.00-4.65 | 0.51-1.26 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal.Finisher | 150-900 / 50 |
| running | `Running_Attack_HeavyHammer_B` | `A_Shared_Attacks_HeavyHammer_Running_02_Montage` 3.40 | 1.20-1.33 | Wx1.8 | 130 | Down BreakShield/ForceDeathWithDismemberment/ForceFlybackReaction | 1.15-2.15 | 0.00-0.50 | 2.15 | 0.00-2.15 | - | 120-600 / 120 |

### Axatana, axe form (Axatana_Axe, 35, H1)

Heavy chain, selector `GA_Player_AttackSelector_Heavy_Axatana_Axe`, event-triggered by Event.Attack.Selector.Heavy.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_Axatana_Axe_B1` | `A_Shared_Attacks_Axeatana_Axe_B_Cut_Montage` 4.42 | 0.37-0.65 | Wx1.5 | 40 | Forward BreakShield | 0.35-0.90 | 0.00-0.35 | 0.90 | 0.00-0.85 | - | 80-250 / 100 |
| Heavy 2 | `Attack_Axatana_Axe_B2` | `A_Shared_Attacks_Axeatana_Axe_BB_Cut_Montage` 3.93 | 0.61-0.92 | Wx1.5 | 40 | Forward BreakShield | 0.60-1.20 | 0.00-0.60 | 1.20 | 0.00-1.10 | - | 100-255 / 100 |
| Heavy 3 | `Attack_Axatana_Axe_B3` | `A_Shared_Attacks_Axeatana_Axe_BBB_Cut_Montage` 4.83 | 0.29-0.61 | Wx1.5 | 40 | Down BreakShield | 0.40-0.90 | 0.00-0.40 | 0.90 | 0.00-0.85 | - | 125-230 / 100 |
| running | `Attack_Katanas_RunningAttack_Axe` | `A_Shared_Attacks_Axeatana_Running_Axe_02_Montage` 3.93 | 0.75-1.11; 1.50-1.60 | Wx1.0 | 30 | Left BreakShield | 1.20-2.05 | 0.00-0.49 | 2.05 | 0.00-1.80 | - | 50-550 / 100 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_Axatana_Axe_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_Axatana_Axe_B1_Hold` | `A_Shared_Attacks_Axeatana_Axe_B_Hold_Montage` 5.53 | 1.75-2.01 | Wx1.5, hold x1.75-2.5 | 40 | Forward BreakShield | 1.65-2.40 | 0.00-0.40 | 2.40 | 0.00-2.20 | 0.76-1.41 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 80-280 / 120 |
| HeavyHold 2 | `Attack_Axatana_Axe_B2_Hold` | `A_Shared_Attacks_Axeatana_Axe_BB_Hold_Montage` 5.93 | 1.78-1.97 | Wx1.5, hold x1.75-2.5 | 40 | Forward BreakShield | 1.80-2.50 | 0.00-0.15 | 2.50 | 0.00-2.20 | 0.15-1.05 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 100-280 / 120 |
| HeavyHold 3 | `Attack_Axatana_Axe_B3_Hold` | `A_Shared_Attacks_Axeatana_Axe_BBB_Hold_03_Montage` 5.24 | 1.51-1.83; 1.97-2.27 | Wx1.5, hold x1.75-2.5 | 40 | Down BreakShield | 1.50-2.70 | 0.00-0.38 | 2.70 | 0.00-2.60 | 0.38-0.98 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal.Finisher | 125-280 / 125 |
| running | `Attack_Katanas_RunningAttack_Axe` | `A_Shared_Attacks_Axeatana_Running_Axe_02_Montage` 3.93 | 0.75-1.11; 1.50-1.60 | Wx1.0 | 30 | Left BreakShield | 1.20-2.05 | 0.00-0.49 | 2.05 | 0.00-1.80 | - | 50-550 / 100 |

### Axatana, twin katana form (Katanas, 25 each, M)

Light chain, selector `GA_Player_AttackSelector_Light_Katanas`, event-triggered by Event.Attack.Selector.Light.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_Katanas_A1` | `A_Shared_Attacks_Axeatana_A_02_Montage` 2.43 | 0.20-0.32 | Wx1.0 | 30 | Left | 0.20-0.50 | 0.00-0.17 | 0.50 | - | - | 100-200 / 100 |
| Light 2 | `Attack_Katanas_A2` | `A_Shared_Attacks_Axeatana_AA_Montage` 2.78 | 0.22-0.35 | Wx1.0 | 30 | Right | 0.17-0.49 | 0.00-0.17 | 0.49 | - | - | 100-200 / 100 |
| Light 3 | `Attack_Katanas_A3` | `A_Shared_Attacks_Axeatana_AAA_Montage` 2.43 | 0.33-0.47; 0.33-0.47 | Wx1.0 | 30 | Down | 0.35-0.63 | 0.00-0.35 | 0.63 | - | - | 100-200 / 100 |
| running | `Attack_Katanas_RunningAttack` | `A_Shared_Attacks_Axeatana_Running_Katanas_Montage` 3.23 | 0.23-0.36; 0.55-0.66; 0.75-0.87; 1.01-1.14 | Wx1.0 | 30 | Left | 0.70-1.23 | 0.00-0.20 | 1.23 | - | - | 50-550 / 100 |
| finisher / extra | `Attack_Katanas_A1_double` | `A_Shared_Attacks_Axeatana_A_double_Montage` 2.80 | 0.16-0.26; 0.37-0.51 | Wx0.8 | 30 | Left | 0.25-0.63 | 0.00-0.25 | 0.63 | - | - | 100-200 / 100 |
| finisher / extra | `Attack_Katanas_A2_double` | `A_Shared_Attacks_Axeatana_AA_double_Montage` 3.60 | 0.22-0.31; 0.41-0.54 | Wx0.8 | 30 | Right | 0.35-0.60 | 0.00-0.35 | 0.60 | - | - | 100-200 / 100 |
| finisher / extra | `Attack_Katanas_A3_double` | `A_Shared_Attacks_Axeatana_AAA_double_Montage` 2.67 | 0.22-0.36; 0.22-0.36; 0.43-0.56; 0.43-0.56 | Wx0.8 | 30 | Down | 0.45-0.80 | 0.00-0.45 | 0.80 | - | - | 100-200 / 100 |

LightHold chain, selector `GA_Player_AttackSelector_Light_Katanas_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_Katanas_A1_Hold` | `A_Shared_Attacks_Axeatana_A_Hold_Montage` 2.93 | 0.55-0.66; 0.78-0.88; 1.13-1.27; 1.13-1.27 | Wx1.0, hold x1.25-1.75 | 30 | Left; Right | 0.80-1.40 | 0.00-0.14 | 1.40 | - | 0.14-0.34 hold 0.5-1.05s @x0.2, release early -> Light.Normal | 100-350 / 100 |
| LightHold 2 | `Attack_Katanas_A2_Hold` | `A_Shared_Attacks_Axeatana_AA_Hold_Montage` 3.77 | 0.30-0.41; 0.58-0.68; 0.89-1.02; 0.89-1.02 | Wx1.0, hold x1.25-1.75 | 30 | Left; Right | 0.60-1.20 | 0.00-0.08 | 1.20 | - | 0.08-0.28 hold 0.5-1.05s @x0.2, release early -> Light.Normal | 100-200 / 100 |
| LightHold 3 | `Attack_Katanas_A3_Hold` | `A_Shared_Attacks_Axeatana_AAA_Hold_Montage` 3.57 | 0.73-0.84; 1.01-1.11; 1.33-1.43; 1.33-1.43 | Wx1.0, hold x1.25-1.75 | 30 | Right | 1.25-1.85 | 0.00-0.12 | 1.85 | - | 0.12-0.32 hold 0.5-1.05s @x0.2, release early -> Light.Normal.Finisher | 100-200 / 100 |
| running | `Attack_Katanas_RunningAttack` | `A_Shared_Attacks_Axeatana_Running_Katanas_Montage` 3.23 | 0.23-0.36; 0.55-0.66; 0.75-0.87; 1.01-1.14 | Wx1.0 | 30 | Left | 0.70-1.23 | 0.00-0.20 | 1.23 | - | - | 50-550 / 100 |

### Black Needle (BlackNeedle, 35, H1)

Light chain, selector `GA_Player_AttackSelector_Light_BlackNeedle`, event-triggered by Event.Attack.Selector.Light.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `BlackNeedle_A1` | `A_Shared_Attacks_BlackNeedle_A_CUT_Montage` 2.31 | 0.18-0.31 | Wx1.0 | 15 | Forward | 0.13-0.40 | 0.00-0.15 | 0.40 |  (Unyielding: 0.00-0.38) | - | 50-200 / 180 |
| Light 2 | `BlackNeedle_A2` | `A_Shared_Attacks_BlackNeedle_AA_CUT_Montage` 1.62 | 0.13-0.25 | Wx1.0 | 15 | Forward | 0.05-0.35 | 0.00-0.08 | 0.35 |  (Unyielding: 0.00-0.30) | - | 50-350 / 180 |
| Light 3 | `BlackNeedle_A3` | `A_Shared_Attacks_BlackNeedle_AAA_CUT_Montage` 2.21 | 0.17-0.27 | Wx1.0 | 15 | Down | 0.05-0.35 | 0.00-0.06 | 0.35 |  (Unyielding: 0.00-0.30) | - | 50-255 / 180 |
| finisher / extra | `Attack_BlackNeedle_A_Finisher` | `A_Shared_Attacks_BlackNeedle_AAA_CUT_Finisher_Montage` 3.23 | 0.13-0.26; 0.63-0.81 | Wx1.0 | 20 | Forward ForceFlybackReaction | 0.80-1.03 | 0.00-0.80 | 1.03 | - | - | 125-255 / 180 |

LightHold chain, selector `GA_Player_AttackSelector_Light_BlackNeedle_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_BlackNeedle_A1_Hold` | `A_Shared_Attacks_BlackNeedle_Hold_A_02_Montage` 4.50 | 1.40-1.53; 1.59-1.66; 1.91-2.00 | flat 25.0, hold x1.25-1.75 | 20 | None | 1.80-2.25 | 0.00-0.81 | 2.27 |  (Unyielding: 0.00-2.20) | 0.34-1.17 hold 0.5-1.05s @x1.0, release early -> Light.Normal | 125-255 / 230 |
| LightHold 2 | `Attack_BlackNeedle_A2_Hold` | `A_Shared_Attacks_BlackNeedle_Hold_AA_02_Montage` 3.80 | 1.25-1.38; 1.47-1.54; 1.79-1.88 | flat 25.0, hold x1.25-1.75 | 20 | None | 1.90-2.27 | 0.00-0.87 | 2.27 |  (Unyielding: 0.00-2.27) | 0.35-1.17 hold 0.5-1.05s @x1.0, release early -> Light.Normal | 125-255 / 230 |
| LightHold 3 | `Attack_BlackNeedle_A3_Hold` | `A_Shared_Attacks_BlackNeedle_Hold_AAA_02_Montage` 4.40 | 1.28-1.41; 1.50-1.59; 1.82-1.90 | flat 25.0, hold x1.25-1.75 | 20 | Forward | 1.75-2.35 | 0.00-1.00 | 2.35 |  (Unyielding: 0.00-2.25) | 0.22-1.17 hold 0.5-1.05s @x1.0, release early -> Light.Normal.Finisher | 125-255 / 220 |
| running | `Running_Attack_BlackNeedle` | `A_Shared_Attacks_BlackNeedle_Running_01_Montage` 2.36 | 0.36-0.54 | Wx1.35 | 50 | Down | 0.30-0.80 | - | 0.80 | 0.00-0.75 | - | 120-750 / 100 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_BlackNeedle`, event-triggered by Event.Attack.Selector.Heavy.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `BlackNeedle_B1` | `A_Shared_Attacks_BlackNeedle_B_CUT_Montage` 3.79 | 0.36-0.57 | Wx1.5 | 20 | Left BreakShield | 0.31-0.81 | - | 0.81 | - | - | 125-400 / 200 |
| Heavy 2 | `BlackNeedle_B2` | `A_Shared_Attacks_BlackNeedle_BB_CUT_Montage` 3.40 | 0.36-0.58 | Wx1.5 | 20 | Right BreakShield | 0.14-0.78 | - | 0.79 | - | - | 125-255 / 200 |
| Heavy 3 | `BlackNeedle_B3` | `A_Shared_Attacks_BlackNeedle_BBB_CUT_Montage` 2.57 | 0.24-0.46 | Wx1.5 | 20 | Down BreakShield | 0.25-0.85 | - | 0.85 | - | - | 125-255 / 280 |
| finisher / extra | `Attack_BlackNeedle_B_Finisher` | `A_Shared_Attacks_BlackNeedle_BBB_CUT_Finisher_Montage2` 3.53 | 0.40-0.58 | Wx1.5 | 70 | Down BreakShield/ForceFlybackReaction | 0.40-1.05 | - | 1.05 | - | - | 125-255 / 180 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_BlackNeedle_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_BlackNeedle_B1_Hold` | `A_Shared_Attacks_BlackNeedle_Hold_B_03_Montage` 5.30 | 1.28-1.49; 1.73-1.94 | Wx1.0, hold x1.75-2.5 | 100 | Down BreakShield | 2.00-2.80 | 0.00-0.29 | 2.80 | 0.00-2.25 | 0.30-1.20 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 125-600 / 200 |
| HeavyHold 2 | `Attack_BlackNeedle_B2_Hold` | `A_Shared_Attacks_BlackNeedle_Hold_BB_03_Montage` 5.03 | 1.39-1.57; 1.70-1.96 | Wx1.0, hold x1.75-2.5 | 100 | Down BreakShield | 1.85-2.65 | 0.00-0.64 | 2.17 | 0.00-2.25 | 0.67-1.14 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 125-600 / 200 |
| HeavyHold 3 | `Attack_BlackNeedle_B3_Hold` | `A_Shared_Attacks_BlackNeedle_Hold_BBB_03_Montage` 5.60 | 1.13-1.34; 1.50-1.72; 1.95-2.11 | Wx1.0, hold x1.75-2.5 | 100 | Down BreakShield | 1.85-3.35 | - | 3.35 | 0.38-2.45 | 0.37-1.02 hold 0.5-1.05s @x0.8, release early -> Heavy.Normal.Finisher | 125-600 / 240 |
| running | `Running_Attack_BlackNeedle_B` | `A_Shared_Attacks_BlackNeedle_Running_B_Montage` 4.47 | 0.93-1.03 | Wx1.8 | 50 | Down BreakShield | 0.75-1.40 | - | 1.40 | 0.00-1.52 | - | 120-750 / 100 |

### Clockwork Scythe (ClockworkScythe, 42, H1)

Light chain, selector `GA_Player_AttackSelector_Light_ClockworkScythe`, event-triggered by Event.Attack.Selector.Light.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_ClockworkScythe_A1` | `A_Shared_Attacks_Scythe_A_02_V2_Montage` 4.40 | 0.28-0.70; 0.28-0.70 | Wx1.0 | 50 | Forward | 0.30-0.85 | 0.00-0.30 | 0.85 | 0.00-0.80 | - | 125-180 / 100 |
| Light 2 | `Attack_ClockworkScythe_A2` | `A_Shared_Attacks_Scythe_AA_02_Montage` 3.80 | 0.45-0.80; 0.45-0.80 | Wx1.0 | 50 | Forward | 0.69-1.20 | 0.00-0.30 | 1.20 | 0.00-1.10 | - | 125-255 / 100 |
| Light 3 | `Attack_ClockworkScythe_A3` | `A_Shared_Attacks_Scythe_AAA_03_Montage` 3.93 | 0.30-0.52; 0.30-0.52 | Wx1.0 | 50 | Forward | 0.45-0.90 | 0.00-0.35 | 0.83 | 0.00-0.85 | - | 125-255 / 100 |
| finisher / extra | `Attack_ClockworkScythe_A3_Finisher` | `A_Shared_Attacks_Scythe_AAA_03_Finisher_Montage` 4.72 | 0.39-0.64; 0.39-0.65; 1.12-1.38; 1.13-1.38 | Wx1.0 | 50 | Forward | 0.62-1.62 | 0.00-0.61 | 1.62 | 0.01-1.63 | - | 125-255 / 100 |

LightHold chain, selector `GA_Player_AttackSelector_Light_ClockworkScythe_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LightHold 1 | `Attack_ClockworkScythe_A1_Hold` | `A_Shared_Attacks_Scythe_A_02_Hold_V2_Montage` 4.80 | 1.92-2.20; 1.92-2.20; 2.43-2.68; 2.43-2.68 | Wx1.0, hold x1.25-1.75 | 50 | Down ForceFlybackReaction | 2.15-3.15 | 0.00-0.51 | 3.15 | 0.00-3.00 | 0.51-1.26 hold 0.5-1.05s @x0.85, release early -> Light.Normal | 125-400 / 80 |
| LightHold 2 | `Attack_ClockworkScythe_A2_Hold` | `A_Shared_Attacks_Scythe_AA_Hold_V2_Montage` 5.77 | 1.97-2.22; 1.97-2.22; 2.44-2.71; 2.44-2.77 | Wx1.0, hold x1.25-1.75 | 50 | Down ForceFlybackReaction | 2.95-3.60 | 0.00-0.35 | 3.60 | 0.00-3.50 | 0.36-1.40 hold 0.5-1.05s @x0.85, release early -> Light.Normal | 125-400 / 80 |
| LightHold 3 | `Attack_ClockworkScythe_A3_Hold` | `A_Shared_Attacks_Scythe_AAA_03_Hold_60fps_V2_Montage` 5.63 | 1.57-1.83; 1.58-1.83; 2.05-2.30; 2.05-2.31 | Wx1.0, hold x1.25-1.75 | 50 | Down ForceFlybackReaction | 2.60-3.60 | 0.00-0.30 | 3.60 | 0.00-3.50 | 0.35-1.21 hold 0.5-1.05s @x0.85, release early -> Light.Normal.Finisher | 125-400 / 80 |
| running | `Running_Attack_ClockworkScythe` | `A_Shared_Attacks_Scythe_Running_Montage` 3.70 | 0.77-1.24; 0.80-1.21; 1.45-1.66; 1.49-1.61 | Wx1.35 | 80 | Down BreakShield/ForceFlybackReaction | 1.20-2.00 | 0.00-0.33 | 2.00 | 0.00-1.76 | - | 120-600 / 100 |

Heavy chain, selector `GA_Player_AttackSelector_Heavy_ClockworkScythe`, event-triggered by Event.Attack.Selector.Heavy.Normal.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_ClockworkScythe_B1` | `A_Shared_Attacks_Scythe_B_02_Montage` 3.63 | 0.50-0.77; 0.50-0.77 | Wx1.5 | 70 | Down BreakShield | 0.35-0.95 | 0.00-0.35 | 0.95 | 0.00-0.85 | - | 300-500 / 100 |
| Heavy 2 | `Attack_ClockworkScythe_B2` | `A_Shared_Attacks_Scythe_BB_03_Montage` 3.79 | 0.81-0.95; 0.82-0.94 | Wx1.5 | 70 | Down BreakShield | 0.99-1.50 | 0.00-0.65 | 1.50 | 0.00-1.40 | - | 60-900 / 100 |
| Heavy 3 | `Attack_ClockworkScythe_B3` | `A_Shared_Attacks_Scythe_BBB_10_Montage` 3.15 | 0.31-0.43; 0.31-0.43 | Wx2.0 | 70 | Right BreakShield | 0.65-1.15 | 0.00-0.35 | 1.15 | 0.00-1.05 | - | 115-900 / 100 |
| finisher / extra | `Attack_ClockworkScythe_B3_Finisher` | `A_Shared_Attacks_Scythe_BBB_Finisher_01_Montage` 4.07 | 0.45-0.61; 0.45-0.61; 1.12-1.18; 1.12-1.18 | Wx1.5 | 70 | Right BreakShield | 1.31-2.27 | 0.00-1.29 | 2.30 | 0.02-1.47 | - | 115-400 / 100 |

HeavyHold chain, selector `GA_Player_AttackSelector_Heavy_ClockworkScythe_Hold`, input-bound (Ability.Attack.Selector.Main).

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HeavyHold 1 | `Attack_ClockworkScythe_B1_Hold` | `A_Shared_Attacks_Scythe_B_02_Hold_V2_Montage` 6.23 | 2.35-2.63; 2.35-2.61; 3.00-3.26; 3.00-3.25 | Wx1.0, hold x1.75-2.5 | 100 | Up BreakShield/ForceFlybackReaction | 3.42-4.00 | 0.00-0.91 | 4.00 | 0.00-3.90 | 0.99-1.82 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 150-400 / 80 |
| HeavyHold 2 | `Attack_ClockworkScythe_B2_Hold` | `A_Shared_Attacks_Scythe_BB_03_Hold_02_V2_Montage` 5.37 | 1.65-1.90; 1.65-1.90; 2.53-2.71; 2.53-2.71 | Wx1.0, hold x1.75-2.5 | 100 | Down; Up BreakShield/ForceFlybackReaction | 2.82-3.40 | 0.00-0.35 | 3.40 | 0.00-3.30 | 0.34-1.06 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal | 150-900 / 80 |
| HeavyHold 3 | `Attack_ClockworkScythe_B3_Hold` | `A_Shared_Attacks_Scythe_BBB_10_Hold_V2_Montage` 4.97 | 2.14-2.25; 2.14-2.25; 2.80-2.98; 2.80-2.98 | Wx1.0, hold x1.75-2.5 | 100 | Down; Up BreakShield/ForceFlybackReaction | 2.87-3.40 | 0.00-0.60 | 3.40 | 0.00-3.30 | 0.64-1.82 hold 0.5-1.05s @x0.85, release early -> Heavy.Normal.Finisher | 150-400 / 80 |
| running | `Running_Attack_ClockworkScythe_B` | `A_Shared_Attacks_Scythe_Running_B_02_Montage` 3.93 | 0.81-1.61; 1.01-1.32; 1.90-2.16; 1.93-2.14 | Wx1.8 | 80 | Down BreakShield/ForceFlybackReaction | 1.75-2.55 | 0.00-0.31 | 2.55 | 0.00-2.45 | - | 130-600 / 50 |

### Smert Fight Stance (fists and kicks, flat payloads)

Light chain, selector `GA_Smert_AttackSelector_Light`, no trigger in data.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Light 1 | `Attack_Smert_Punch_A1` | `A_Smert_Attacks_Punch_A_Montage` 1.27 | 0.33-0.43 | flat 0.001 | 100 | Forward Null | 0.30-0.43 | - | 0.43 | - | - | 40-900 / 40 |
| Light 2 | `Attack_Smert_Punch_A2` | `A_Smert_Attacks_Punch_AA_Montage` 1.83 | 0.33-0.43 | flat 0.001 | 100 | Forward Null | 0.30-0.50 | - | 0.50 | - | - | 40-900 / 40 |
| Light 3 | `Attack_Smert_Punch_A3` | `A_Smert_Attacks_Punch_AAA_02_Montage` 1.80 | 0.21-0.27; 0.36-0.43; 0.61-0.70 | flat 0.001 | 100 | Forward Null | 0.63-0.77 | - | 0.77 | - | - | 60-650 / 40 |
| running | `Attack_Smert_Running_A` | `A_Smert_Attacks_Running_A_03_Montage` 2.30 | 0.57-0.70 | flat 0.001 | 20 | None BreakShield/ForceFlybackReaction/Null | 0.87-1.01 | - | 1.00 | - | - | 0-500 / 0 |

Heavy chain, selector `GA_Smert_AttackSelector_Heavy`, no trigger in data.

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Heavy 1 | `Attack_Smert_Kick_B1` | `A_Smert_Attacks_Kick_B_Montage` 2.20 | 0.40-0.50 | flat 0.01 | 20 | None BreakShield/ForceFlybackReaction/Null | 0.20-0.53 | - | 0.53 | 0.00-0.53 | - | 50-650 / 50 |
| Heavy 2 | `Attack_Smert_Kick_B2` | `A_Smert_Attacks_Kick_BB_Montage` 2.70 | 0.49-0.63 | flat 0.01 | 20 | None BreakShield/ForceFlybackReaction/Null | 0.25-0.63 | - | 0.63 | 0.00-0.63 | - | 50-650 / 80 |
| Heavy 3 | `Attack_Smert_Kick_B3` | `A_Smert_Attacks_Kick_BBB_Montage` 1.73 | 0.50-0.65 | flat 0.01 | 20 | None BreakShield/ForceFlybackReaction/Null | 0.20-0.67 | - | 0.67 | 0.00-0.67 | - | 50-650 / 80 |
| running | `Attack_Smert_Running_B` | `A_Smert_Attacks_Running_B_04_Montage` 1.40 | 0.60-0.74 | flat 0.01 | 20 | None BreakShield/ForceFlybackReaction/Null | 0.46-0.90 | - | 0.90 | 0.00-0.90 | - | 50-500 / 80 |

### Other attack abilities with a montage (not in a selector list)

| step | ability | montage, length s | hit windows s | damage | poise | strike dir, extras | queue | dodge ok | move cancel | hyper armor | hold handler | warp cm |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| - | `GenessaTrainer_BlackNeedle_B1` | `A_Shared_Attacks_BlackNeedle_B_GenessaTrainer_Montage` 4.13 | 0.75-0.96 | Wx1.0 | 15 | Forward | - | - | - | - | - | 50-350 / 250 |
| - | `GenessaTrainer_BlackNeedle_B2` | `A_Shared_Attacks_BlackNeedle_BB_GenessaTrainer_Montage` 3.97 | 0.90-1.11 | Wx1.0 | 15 | Right | - | - | - | - | - | 50-350 / 300 |
| - | `GenessaTrainer_BlackNeedle_B3` | `A_Shared_Attacks_BlackNeedle_BBB_GenessaTrainer_Montage` 3.37 | 0.90-1.13 | Wx1.0 | 15 | Forward | - | - | - | - | - | 50-255 / 300 |
| - | `Attack_Fists_A1` | `AM_Shared_Attacks_Fists_A` 1.77 | 0.24-0.40; 0.24-0.40 | Wx0.5; Wx1.0 | 30 | Left | 0.20-0.60 | 0.00-0.20 | 0.60 | - | - | 80-495 / 75 |
| - | `Attack_Fists_A1_SmertMemory` | `AM_Shared_Attacks_Fists_A1_SmertMemory` 1.77 | 0.24-0.40; 0.24-0.40 | Wx0.5; Wx1.0 | 30 | Left | 0.20-0.60 | 0.00-0.20 | 0.60 | - | - | 80-495 / 75 |
| - | `Attack_Fists_A2` | `AM_Shared_Attacks_Fists_aA` 1.80 | 0.21-0.38; 0.21-0.38 | Wx0.5; Wx1.0 | 30 | Right | 0.20-0.50 | 0.00-0.20 | 0.50 | - | - | 80-495 / 75 |
| - | `Attack_Fists_A2_SmertMemory` | `AM_Shared_Attacks_Fists_aA_SmertMemory` 1.80 | 0.21-0.38; 0.21-0.38 | Wx0.5; Wx1.0 | 30 | Right | 0.20-0.50 | 0.00-0.20 | 0.50 | - | - | 80-495 / 75 |
| - | `Attack_Fists_B1` | `AM_Shared_Attacks_Fists_B` 2.23 | 0.50-0.60; 0.50-0.62 | Wx0.5; Wx1.5 | 50 | Up BreakShield/UnarmedAttack | 0.36-0.96 | - | 0.96 | - | - | 65-410 / 75 |
| - | `Attack_Fists_B2` | `AM_Shared_Attacks_Fists_bB` 2.43 | 0.34-0.48; 0.38-0.46 | Wx0.5; Wx1.5 | 50 | Right BreakShield/UnarmedAttack | 0.25-0.80 | - | 0.80 | - | - | 65-410 / 75 |
| - | `Attack_Smert_Punch_Ground` | `A_Smert_Attacks_GroundPunch_02_Montage` 3.97 | 1.12-1.22 | flat 0.001 | 100 | Forward Null | 1.45-2.65 | - | 2.65 | 0.00-1.90 | - | 60-800 / 40 |
| - | `WeaponAbility_DeadlyFlurry` | `A_MS1_Player_Hammer_Super_A_DarkForm_Montage` 2.93 | 0.64-1.74; 0.64-1.74 | Wx1.0 | 20 | Forward | 2.00-2.50 | - | 2.50 | - | - | 140-400 / 45 |
| - | `WeaponAbility_Katanas_A1` | `A_Shared_Attacks_Katanas_Super_A1_Montage` 3.60 | 0.63-0.75; 0.98-1.11; 0.98-1.11; 1.25-1.35; 1.50-1.61; 1.51-1.62; 1.70-1.81; 1.70-1.81 | Wx1.0 | 20 | Left | 1.50-2.50 | - | 2.50 | - | - | 200-600 / 70 |
| - | `WeaponAbility_Katanas_A2` | `A_Shared_Attacks_Katanas_Super_A2_Montage` 3.33 | 0.20-0.33; 0.20-0.33; 0.43-0.56; 0.43-0.56; 0.70-0.80; 0.70-0.80; 0.88-0.99; 0.88-0.99; 1.13-1.26; 1.13-1.26 | Wx1.0 | 20 | Forward | 1.25-2.25 | 0.00-2.25 | 2.25 | - | - | 200-600 / 70 |
| - | `WeaponAbility_SpiralSurge` | `AM_A_Shared_MartyrsBlade_Super_02` 8.97 | 1.17-1.49; 1.51-1.78; 1.80-2.12; 3.23-3.55 | Wx1.0 | 100 | Forward ForceFlybackReaction | 5.80-6.80 | - | 6.80 | - | - | 200-600 / 80 |

### Weapon abilities (game data, `Sparta/Core/Tarstones/Melee/Abilities/GA_WeaponAbility_*`)

All are `Ability.Primary, Ability.Attack.Melee.Super`, `ActivationGroup Exclusive_Blocking`, `Cost 2.0` on the class (the Resolve price shown in the UI comes from the Tarstone item, see the combat doc section 6.2; the relation between this `Cost` and Resolve is not in data). The montage is chosen per weapon inside `GetAbilityMontage` (bytecode literals).

| ability | weapon montage(s) | length | invulnerable | hits (payload) | notes |
| --- | --- | --- | --- | --- | --- |
| Plummet Strike `GA_WeaponAbility_PlummetStrike` | `A_Shared_Attacks_HadernSword_Ability_Full_Montage` (Iconoclast), `A_Shared_Attacks_Axeatana_Axe_Super_Montage` (Axatana) | 5.70 / 7.00 | 0 to 3.4 / 0 to 4.63 (`GE_Invulnerability`), Harden blocked | Iconoclast: 0.37-0.58 (Wx1, poise 100, Down, +ForceFlybackReaction) and 2.56-2.70 (Flyback reaction); Axatana: 1.32-1.62, 1.92-2.13, 3.08-3.20 | `Event.Ability.PlummetStrike.BreakAreaDamage` at the landing (2.63 / 3.14) drives the shockwave splash; `FindSoftTarget` re-targets mid-air; warp 100-200, stop 100 |
| Heavy Stomps `GA_WeaponAbility_HeavyStomps` | `A_Shared_Attacks_HeavyHammer_Ability_Stomps_Montage` (default) or `A_Shared_Attacks_HeavyHammer_Stomps_02_Montage` | 8.30 / 6.30 | Stomps: hyper armor 0 to 6.05, no invulnerability; Stomps_02: `GE_Invulnerability` 0 to 5.3 | Stomps: no HitCheck, four `Shockwave` montage notifies at 1.40, 2.11, 2.95, 4.09 (splash from the Tarstone values); Stomps_02: four HitChecks 1.42, 2.32, 3.02, 4.24 (Wx1, poise 50, Down) | payload on the class: poise 100, Down; input fully blocked until 5.15 s |
| Weapon Throw `GA_WeaponAbility_WeaponThrow` (Lost Clotstone) | `A_Shared_Attacks_HeavyHammer_Ability_Throw_Montage`, `A_Shared_Attacks_BattleAxe_Ability_Throw_Montage` | 5.97 | 0 to 3.65 | 0.75-1.49 and 1.50-3.70 (the thrown weapon traces), payload poise 200, +ForceFlyback, +ForceDeathWithDismemberment | `Show` at 3.0 re-shows the weapon; targeting `BP_STS_Melee_WeaponThrow` 3000 cm |
| Spiral Surge `GA_WeaponAbility_SpiralSurge` | `AM_A_Shared_MartyrsBlade_Super_02` | 8.97 | 0 to 6.5 | 1.17-1.49, 1.51-1.78, 1.80-2.12, 3.23-3.55 (Wx1, poise 100, +ForceFlyback) | queue 5.8-6.8; warp 200-600, stop 80; time dilation x1.5 over the recovery |
| Deadly Flurry `GA_WeaponAbility_DeadlyFlurry` | `A_MS1_Player_Hammer_Super_A_DarkForm_Montage` | 2.93 | 0 to 2.15 | one long window 0.64-1.74 on both weapons with `bAllowMultipleHitsOnSameActor` (multi-hit), collision x2 (`ANS_ModifyWeaponCollisionSize`) | warp 140-400, stop 45, warps even with no target |
| Needle Storm `GA_WeaponAbility_StormStrike` | `A_Shared_Attacks_BlackNeedle_NeedleStorm_Short/Med/Long_Montage` (`Montage_Short/Medium/Long`, chosen by stone level, inferred) | 2.93 / 2.87 / 3.33 | 0 to 1.55 / 1.85 / 2.3 | Short 3 hits, Med 4, Long 7 at 0.61 to 2.26 (Wx1 on the notifies, poise 10, reaction Heavy; class payload flat 5) | `A_Shared_Attacks_HadernSword/AxeDagger_StormStrike_Temp_Montage` are the same 7-hit layout on other weapons, unused by any ability default |
| Katana barrage `GA_WeaponAbility_Katanas_A1` -> child `Combo` `_A2` (Hexapod Core) | `A_Shared_Attacks_Katanas_Super_A1_Montage`, `_A2_Montage` | 3.60 / 3.33 | 0 to 2.5 / 2.25 | A1: 8 windows 0.63 to 1.81 (Wx1, Left), A2: 10 windows 0.20 to 1.27, both hands | A2 `bSkipCost`; queue 1.5-2.5 chains into A2 |
| Clockwork Chainsaw / Grinder `GA_WeaponAbility_ClockworkChainsaw`, `_ClockworkGrinder` | additive `A_Shared_Scythe_Activate_Montage`, `A_Shared_Scythe_ShakeAdditive_Montage`, idle `A_Shared_Locomotion_Scythe_Ability_Idle_Montage`; grinder draw/stow montages | loop | hyper armor GE on grinder | ticks through `GA_Player_Attack_ClockworkScythe_Grinder`: flat 2 damage, poise 4, `GE_ClockworkChainsawDamage`, reaction Ignore, no hit stop; drain `GE_ClockworkScythe_Chainsaw_Drain` | `Cost 1.0`, tags `Ability.Secondary` |
| Homing Throw `GA_WeaponAbility_HomingThrow` (Infused Stone) | projectile, no montage default | | | class payload Wx1 + 5 flat, poise 20 | `MaxThrowDistance 3000`, `ThrowTotalTime 0.65` |
| Black Needle spin (`A_Shared_Attacks_BlackNeedle_Spins_Full_Montage`) | played by `GA_WeaponBuff_BlackNeedle_Activation` | 2.77 | `GE_ProjectileImmunity` 0.25 to 1.85 | none | this is the infusion activation, not an attack |

### Riposte (game data)

`GA_Parry_Riposte` (`Sparta/Core/Player/Ability/Parry/`, trigger `Event.Ability.Riposte.Activate`, tags `Ability.Attack.Melee.InfiniteSeal, Ability.Riposte`, blocked by `State.Block.Ability.Riposte` which `GE_BlockRiposteAttack` grants for 0.4 s). `RipostePayload`: `HealthDamage Type InstigatingCharacterWeapon, FloatValue 100`, `DamageEffect GE_RiposteDamage` (tags `Damage.Melee, Damage.Riposte, Damage.Options.DisableCrit`), poise 0. `InstigatorMontages` map the victim's sync tag to the player montage; humans use `Event.Reaction.Sync.Parry.Human.1/2/3` -> `AM_Shells_InfSeal_Riposte_Brig_01/02/03_Shell` (`Sparta/Characters/Shells/_Shared/Animation/Parry/`), the victim plays the matching `_Brig` montage from `HRAS_Human`. Bosses in `EnemiesWithQuickRiposte` (Lady of the Woods, Moth Knight, Tar Golem, Head Boss, Offspring, Final Boss, Monolith, Dwarf Boss, Parasite Golem, Swordman) use `InstigatorMontages_Quick` boss-specific montages.

| player montage | length | invulnerable | damage ticks `AN_RiposteDamage` (multiplier, % of victim max health, mini-boss %) | queue | move cancel |
| --- | --- | --- | --- | --- | --- |
| `AM_Shells_InfSeal_Riposte_Brig_01_Shell` | 1.90 | 0 to 1.9 | 0.40: x0.625 / 65% / 18%; 0.92: x0.375 / 35% / 7% | 0.5-1.15 | 1.2 |
| `AM_Shells_InfSeal_Riposte_Brig_02_Shell` | 2.10 | 0 to 2.1 | 0.16: x0.25 / 25% / 5%; 0.53: x0.5 / 50% / 15%; 1.01: x0.25 / 25% / 5% | 0.85-1.45 | 1.6 |
| `AM_Shells_InfSeal_Riposte_Brig_03_Shell` | 1.80 | 0 to 1.8 | 0.31: x1.0 / 100% / 25% | 0.75-1.4 | 1.45 |

`UseMaxHealthPercentage = true` on every tick and the percentages sum to 100 (mini-boss 25): this is the Week 1 "riposte damage scales with enemy health" rule in data. The `DamageMultiplier` values sum to 1.0, matching "100% of the instigating weapon's damage". Which of the two is used, or how they combine, is native (`SpartaDamageExecution` on `GE_RiposteDamage`) and not readable; the victim's `AM_*_Brig` montage grants `AI.Event.GrantDeathReward` then `AI.Event.ApplyDeath.NoReward`, so an enemy that dies during the riposte dies from the ticks, not from a scripted kill. `Event.Ability.Riposte.End` fires at 1.14 / 1.44 / 1.39 s.

### Plunge (game data)

`GA_Player_Attack_Plunging` (trigger `Event.Ability.Attack.Plunging.Activate` from the selector): `BaseDamage 100`, `PoiseDamage 100` (own fields, flat, not weapon-scaled), `LoopMontage A_Shared_Attacks_Plunge_Fall_Loop_Montage` (sections `Default` 0 to 3.47 = `A_Shared_Attacks_Plunge_Start`, `Loop` from 3.47 = `A_Shared_Attacks_Plunge_Fall_Loop`, `State.Hit.IgnorePhysReaction`), `LandingMontage A_Shared_Attacks_Plunge_Land_Montage` (2.0 s, no HitCheck: the hit is applied by the ability on landing, `Activate` montage notify at 0.16, all input blocked to 1.68, queue 0.93-1.68, move cancel 1.68). `WarpingDuration 0.3`, `WarpDistanceZ`, `TimeForFullWind 2.0` (wind VFX ramps with fall time), `WarpTranslationDistance 100 to 1000`, stop 190, warps with no target, targeting `BP_STS_Melee_Plunging` (550 cm sphere, camera-based). Owned tags while plunging: `State.Attack.Plunging`, `State.Invisible`, blocks sidearm and aiming.

## 4. Dodge and dash per shell (game data)

`GA_DashBase` (`Sparta/Core/Player/Ability/Dash/`): `DashPlayRate 1.0`, `DashDistance 375`, `StrafeDashDistance 415`, `DashTravelTime 0.5`, `InvulnerabilityDuration 0.33`, `GE_Invulnerability` handle, `DashVFXCue`, montage maps `DefaultDashMontages` (M and L locomotion), `H1DashMontages`, `H2DashMontages` (8 directions each, `Sparta/Characters/Shells/_Shared/Animation/Locomotion/Dash/{,H,H2}`), `GetUpDashMontage`. Tags: `Ability.Primary, Ability.Dodge`; owned `State.Dodge`, `GameplayEffect.Immunity.Projectile.Collision`; blocked while falling, transformed, or `State.Block.Ability.Dodge/Primary`; `CancelAbilitiesWithTag Ability.Charges`; `DeactivationTags Event.Ability.Dash.Cancel`. The montage set is picked by the weapon's `LocomotionStyle` tag (`LocomotionType.H1`: Iconoclast, Battle Axe, Black Needle, Scythe, Hammer, Axatana axe; `H2`: Martyr's Blade; `M`: Axe & Dagger, katanas; `L`: fists).

| shell ability | i-frames s | distance / strafe cm | travel s | play rate | notes |
| --- | --- | --- | --- | --- | --- |
| `GA_Dash_Harros`, `_Genessa`, `_Eredrim`, `_Gragu`, `_Proxima`, `_Necrophage` (Lazlo), `_Sariel`, `_Smert`, `_StrongOne` | 0.33 | 375 / 415 | 0.5 | 1.0 | only `DashVFXCue` differs |
| `GA_Dash_Tiel` | 0.433 | 375 / 415 | 0.5 | 1.0 | Shadow Dash: traces every 0.05 s during the dash, `DefaultShadowDash 0.233`, `DefaultStacks 1`, hit payload `Type Float` (Shadow damage), `GE_NotDetectableForProjectiles` |
| `GA_Dash_CorruptedGenessa` (Stray form) | 0.12 | 425 / 465 | 0.35 | 1.25 | cooldown disabled |
| `GA_RollBase` (legacy roll, `A_Player_OneHand_Roll_*`), `GA_Roll_Smert` | 0.6 | 600 / 660 (Smert 630 / 680) | 0.75 | 1.0 | roll montage 1.50 s: input block to 1.0 (Aiming.Release, Parry.Press allowed), queue 0.45-1.0, `Event.Ability.End` at 1.0 |

The dash montages carry no invulnerability notify: `A_Shells_Shared_Locomotion_Dash_Fwd_Montage` (1.63 s, H 1.63 s, H2 1.13 s) has `InputBlock` 0 to 0.60 allowing only `Aiming.Release` and `Parry.Press`, `InputQueue` 0.30-0.60, `Event.Ability.End` at 0.60, `ANS_InterruptWithMovement` from 0.60 and `ANS_AbyssCheckForRM_Disable` 0 to 0.70. So the i-frame window is the ability's `InvulnerabilityDuration` applied from activation (0 to 0.33 s, inferred from the handle name), the dash commits for 0.60 s, and the rest of the montage is free recovery.

## 5. Hit reactions and stagger on the player (game data)

`BP_PlayerCharacter.HitReactionAnimSet = HRAS_Player` (`Sparta/Core/Characters/Player/Common/HRAS_Player`). `GA_HitReaction` (trigger `Event.Reaction`, owned `State.Reaction`, `GlobalPreventionTags State.Parry, GameplayEffect.Immunity.HitReaction`, `HitReactionCooldown` from the set) picks a montage by `ReactionTag` and `StrikeDirection` from the attacker's payload.

| reaction tag | additive | cooldown | montages (direction: montage, length) |
| --- | --- | --- | --- |
| `Event.Reaction.Hit.Weak` | yes | 0.5 | `A_Player_1H_HitReaction_Weak_CU/CL/R/L_Montage` (1.0 s, Legacy/Characters/Humans/Player/Animations/HitReactions/Weak) |
| `Event.Reaction.Hit.Medium` | no | 0.5 | `A_Player_1H_HitReaction_Mid_CU_01..03`, `_CL_01..02`, `_L_01..02`, `_R_*` (1.06 to 1.18 s) |
| `Event.Reaction.Hit.Back.Medium` | no | 0.5 | `A_Shared_HitReactions_Mid_Back_Montage` 1.35 s |
| `Event.Reaction.Hit.Heavy` | no | 0.5 | `A_Player_1H_HitReaction_Strong_CU/CL/R_01..02/L_01..03` (1.56 to 1.62 s) |
| `Event.Reaction.Hit.Back.Heavy` | no | 0.5 | `A_Shared_HitReactions_Heavy_Back_Montage` 3.37 s |
| `Event.Reaction.Hit.Flyback` | no | 0.5 | `A_Shells_HitReaction_Flyback_01` 7.2 s (sides), `_02` 4.53 s, `_03` 4.0 s |
| `Event.Reaction.Hit.Back.Flyback` | no | 0.5 | `A_Shared_HitReactions_Flyback_Fwd_01/02` 4.07 / 3.47 s |
| `Event.Reaction.Stun.Equipped.Standard/HugeWeapon/DualWeapons` (parried) | no | 0.5 | `A_Shared_Actions_ParriedStun__H1_01_Montage` 6.0 s, `_H2_Montage` 6.17 s, `_M_Montage` 5.67 s: `State.ActiveBlock.Broken`, all input blocked 4.2 s (M 4.0), queue then movement cancel |

Redirect rules on the set: Medium and Heavy become `Ignore` while the player has `State.HyperArmor` (hyper armor windows in section 3) or `State.Animation.Severed`, and become `Flyback` while `State.BlockStance.Broken` (guard meter empty); Flyback is ignored while already flying back; Weak is ignored while `State.Attack.Melee` or `State.Aiming` (a weak hit does not interrupt a swing); `Event.Reaction.Parried` maps to the stun by weapon class (Standard for H1 weapons and the Axatana axe, HugeWeapon for the Martyr's Blade, DualWeapons for katanas and Axe & Dagger).

Poise: the player has `Poise 5` and `BreakResistance 75` (`DT_PlayerAttributes`, combat doc). Enemy swings carry `PoiseDamage` on their payloads the same way; `GE_PoiseDamage` runs `SpartaPoiseExecution` and `GE_PoiseBroken` marks `State.PoiseBroken`. The threshold logic that turns poise damage into the choice between Weak, Medium and Heavy (and `ChosenReactionTag` on the payload) is inside the native executions and `USpartaHitReactionAnimSet::GetHitReactionAnimation`; it is not in the cooked data.

## 6. Hit shapes and targeting per weapon (game data)

With `TraceSetup UseWeaponDefaults` the HitCheck traces the weapon actor's `DefaultCollisionShape` capsule (SCS component on `WP_*`), swept with motion substepping:

| weapon actor | capsule radius / half height cm | extra shapes | soft targeting `BP_STS_Melee_*` sphere / light cone |
| --- | --- | --- | --- |
| `WP_HadernsSword` | 20 / 68 | `AdditionalTraceCollision` 8 / 32 near the hilt | 500 cm, light cone +-30 deg, sprint +-50 |
| `WP_DaggerAxe_Axe` / `_Dagger` | 25 / 60 and 12 / 40 | | 450 cm, +-20; sprint 1300 cm, +-30, 800 distance threshold |
| `WP_BattleAxe` | 8 / 71 plus `HeadCollision` | | 600 cm, +-90 / light +-30 |
| `WP_MartyrsBlade` | 12 / 120 | | 600 cm, +-90 / +-70 |
| `WP_HeavyHammer` | 12.9 / 42.3 plus `CollisionHead` and `CollisionHandle` 6.9 / 45.4 | | 600 cm, +-90 / +-30; sprint +-80 / +-60 |
| `WP_Axatana_Axe` | 17 / 65.3 | | 700 cm, +-90 / +-40; sprint +-55 |
| `WP_Axatana_Katana_Left/Right` | 8 / 41.5 each | | same |
| `WP_BlackNeedle` | 16 / 175 | | 700 cm, +-90 / +-40; sprint 1000 cm, +-55 |
| `WP_ClockworkScythe` | 15 / 39 | | 650 cm, +-90 / +-35; sprint 1000 cm, +-55 |
| `WP_Fist_L/R`, `WP_Kick_L/R` | 14 / 40 and 25 / 25 | | fists light 650 cm, heavy 550 cm |

## 7. What a native mod can change

Plain `UPROPERTY` fields on live objects (patchable from a UE4SS or native mod without touching pak files; class defaults change every future activation, instanced abilities are `InstancedPerExecution` so patch the CDO):

- `ASpartaWeapon` on the spawned `WP_*` actor: `BaseDamage`, `BasePoiseDamage`, `BaseBreakDamage`, `FaceToTargetInterpSpeed`, `ResolveGainMultiplier`, `ShellReviveEfficiency`, `UpgradeStatValues`, `ElementalEfficiency`, `GrantedAbilitySet` (before equip).
- Attack ability CDOs `GA_Player_<Weapon>_A1_C` and parents: `Montage` (swap a montage), `MontagePlayRate`, `MontageStartSection`, `HoldAttackDamage` (X, Y), `bUseHitStop`, `HitStopDuration`, `HitStopTimeDilation`, `WarpTranslationDistance`, `WarpTranslationStoppingDistance`, `bWarpTranslation*`, `AutoRipostePolicy`, `CamShakeClass`, `SoftTargetingSettings`, and the `AbilityHitPayload` subobject (`HealthDamage.Multiplier`, `HealthDamage.Type`, `PoiseDamage`, `BreakDamage`, `StrikeDirection`, `ReactionTag`, `AdditionalEffects`, `KnockbackPower`).
- Selector CDOs: `ComboAttackList`, `RunningAttackList`, `AdditionalAttacks` (`TArray<TSubclassOf<UGA_AttackBase_C>>`), so chains can be re-ordered or lengthened, but `BPC_Player_ComboCounter.MaxComboCount` (3) caps the index and `HoldAttackTimeThreshold`, `HoldAttackHoldingPlayrate`, `HoldAttackReleasePlayrate`, `HoldAttackDamageMultiplier` on the component are also live.
- Montage objects: `UAnimMontage::Notifies` is a `TArray<FAnimNotifyEvent>` on the loaded montage; each event's link time, `Duration`, `bNotifyEnabled` and the notify object's fields (`SpartaAnimNotifyState_HitCheck::DamagePayload`, `bAllowMultipleHitsOnSameActor`, `CollisionShapes`, `ANS_HoldAttackHandler::TimeThresholdMinMax`, `HoldingPlayrate`, `AnimNotifyState_InputBlock::Block`, `CSAnimNotifyState_TimeDilation::TimeDilation`) are editable in memory, so hit windows, queue windows and dodge windows can be moved without a recook. The notify begin time is `FAnimNotifyEvent::LinkValue` (absolute link) with `TriggerTimeOffset` 0.
- `GA_DashBase_C` and shell dash CDOs: `InvulnerabilityDuration`, `DashDistance`, `StrafeDashDistance`, `DashTravelTime`, `DashPlayRate`, montage maps.
- `GA_Parry_Riposte_C`: `InstigatorMontages`, `RipostePayload`, `EnemiesWithQuickRiposte`; `AN_RiposteDamage` fields on the riposte montages.
- `GA_Player_Attack_Plunging_C`: `BaseDamage`, `PoiseDamage`, `WarpTranslationDistance`.

Baked into assets (needs a cooked replacement pak or a new montage built at runtime): the animation sequences and their root motion, montage length and slot tracks, blend settings, the SCS capsule sizes on `WP_*` (component templates; the spawned component's `CapsuleRadius`/`CapsuleHalfHeight` are live though), and the Blueprint logic (combo finisher selection, hold lerp, `GetAbilityMontagePlayRate`).

Native-only, not readable or patchable through reflection: `SpartaDamageExecution`, `SpartaPoiseExecution`, `SpartaBreakExecution` (how BaseDamage, Tarforge, level, crit and status combine), the poise-to-reaction thresholds, and `USpartaHitTrace` sweep internals.

## 8. Open questions

1. `GetAbilityMontagePlayRate` on `GA_Player_Attack_Light/Heavy` is overridden but its inputs are bytecode; Warp stacks (attack speed status) are the likely input. Measure in game.
2. The hold-damage lerp: `HoldAttackDamage` min/max are certain, the mapping from held time (0.5 to 1.05 s) to the multiplier is inferred.
3. `Cost 2.0` on `GA_WeaponAbility_*` versus the 50 to 100 Resolve shown on Ability Tarstones: which attribute converts it (`PrimaryMeleeAbilityCost`, initialised by `GE_InitializeMeleeAbilityPrimaryCost`) is likely but unverified.
4. Riposte: `DamageMultiplier` and `MaxHealthPercentage` both sum to a full riposte; how `GE_RiposteDamage` combines them is native.
5. Poise thresholds for Weak / Medium / Heavy reactions and how `ChosenReactionTag` is picked are native.
6. Whether `InputQueue` releases the buffered input at window end or immediately when the current ability allows it is inferred from timing, not read from code.
7. The Axatana `GA_Player_Attack_Light_Axatana` / `_Heavy_Ataxana` defaults still point at MS1 montages (`A_MS1_Player_Axe_A/B_Montage`) with `AttackMontageSectionName Attack`; the per-step subclasses override `Montage`, so those defaults look like leftovers, unverified.
8. `GA_WeaponAbility_HeavyStomps` references two stomp montages; which one plays under what condition (Colossus Stone level, inferred) was not decoded.
