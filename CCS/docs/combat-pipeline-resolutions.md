# Mortal Shell II Combat Pipeline: Verified Mechanics & Open Questions Resolved

Written 27 September 2026. This document formally closes and resolves the unverified items and open questions from [`combat-damage-pipeline.md`](./combat-damage-pipeline.md) and [`combat-movesets.md`](./combat-movesets.md), verified directly from cooked Blueprint bytecode (`ScriptBytecode`), native reflection dumps, and executable analysis.

---

## 1. Resolve Economy (`CCC_ResolveGain_C`) [Bytecode Verified]

In [`combat-damage-pipeline.md`](./combat-damage-pipeline.md), the exact formula of `CCC_ResolveGain` was listed as an open question. 

### Bytecode Extraction:
Inspecting `MortalShell2_Content_Sparta_Core_Player_CCC_ResolveGain.uasset.json` at `Function: CalculateBaseMagnitude`:
- **Statement 169**: Reads SetByCaller magnitude for `DataTag.ModifyResolveMagnitude` from the incoming `FGameplayEffectSpec`. (This value is injected when an attack connects, calculated from damage dealt scaled by the attack ability's `ResolveMultiplier`).
- **Statement 488**: Calls `AbilitySystemBlueprintLibrary:GetFloatAttribute` on the instigator's ASC for `/Script/Sparta.PlayerAttributeSet:BaseResolveGain` (default `0.1` on the player character, modified additively/multiplicatively by Tarstones and Shell upgrades).
- **Statement 1015**: Executes `KismetMathLibrary:Multiply_DoubleDouble`.

### Exact Formula:
$$\text{Calculated Resolve Gain} = \text{SetByCaller}(\texttt{DataTag.ModifyResolveMagnitude}) \times \texttt{PlayerAttributeSet.BaseResolveGain}$$

When passive upgrades such as `GE_ResolveBoost_Melee` or `ID_Melee_Generic_ResolveGain` are active, they scale `PlayerAttributeSet.BaseResolveGain`, directly scaling this product.

---

## 2. Riposte Damage & Boss Health Scaling (`AN_RiposteDamage_C`) [Bytecode Verified]

In [`combat-movesets.md`](./combat-movesets.md), how `DamageMultiplier` and `MaxHealthPercentage` combine was listed as an open question.

### Bytecode Extraction:
Inspecting `MortalShell2_Content_Sparta_Core_Animations_AN_AN_RiposteDamage.uasset.json` at `Function: CalculateDamage`:
- **Branch A (`UseMaxHealthPercentage == true`)**: Used on mini-bosses and bosses (`InstigatorMontages_Quick` and boss ripostes):
  1. Statement 169: Reads Victim's `/Script/Sparta.SpartaHealthSet:MaxHealth`.
  2. Statement 369: Divides `MaxHealthPercentage` by `100.0`.
  3. Statement 488: Multiplies $\text{MaxHealth} \times \frac{\text{MaxHealthPercentage}}{100.0} \to \text{BasePercentageDamage}$.
  4. Statement 561 & 725: Reads Victim's `/Script/Sparta.SpartaHealthSet:RiposteWeakness` (fallback `1.0` if attribute missing).
  5. Statement 780 & 960: Reads Instigator's `/Script/Sparta.SpartaCombatSet:RiposteDamageMultiplier` (fallback `1.0` if attribute missing).
  6. Statement 1061: Multiplies $\text{BasePercentageDamage} \times \text{RiposteWeakness} \times \text{RiposteDamageMultiplier} \to \text{TotalDamage}$.
- **Branch B (`UseMaxHealthPercentage == false`)**: Used on standard humanoid mobs:
  1. Statement 1243: Calls `GetSealWeapon()` to resolve the equipped seal/primary weapon.
  2. Statement 1276: Reads `ASpartaWeapon::BaseDamage`.
  3. Statement 1356: Multiplies $\text{BaseDamage} \times \text{DamageMultiplier} \to \text{TotalDamage}$.

### Exact Formulas:
$$\text{Riposte (Boss / Percentage)} = \left(\text{Victim MaxHealth} \times \frac{\text{MaxHealthPercentage}}{100}\right) \times \text{RiposteWeakness} \times \text{RiposteDamageMultiplier}$$
$$\text{Riposte (Standard Mob)} = \text{Weapon BaseDamage} \times \text{DamageMultiplier}$$

Crit is explicitly disabled on ripostes via `GameplayEffect.Damage.Options.DisableCrit` on [`GE_RiposteDamage`](./combat-damage-pipeline.md#L26).

---

## 3. Untarnished Seal / Active Block (`GA_ActiveBlock_C`) [Bytecode Verified]

In [`combat-damage-pipeline.md`](./combat-damage-pipeline.md), the holding drain rate, perfect guard break amount, and guard-broken state were listed as open questions.

### Bytecode Extraction:
Inspecting `MortalShell2_Content_Sparta_Core_Characters_Player_Common_Abilities_ActiveBlock_GA_ActiveBlock.uasset.json`:

1. **Holding Drain Rate**:
   - There is **NO continuous stamina/break drain tick** during block holding.
   - Instead, holding guard activates `GE_BreakDamageRegenBlock` (preventing the standard +5/s break regeneration) and slows movement (`AddMovementSpeedModifier`).
2. **Break Meter Damage Taken on Block**:
   - Inside `Function: ApplyBlockDamage`:
     - Casts incoming event payload to `USpartaHitPayload`.
     - Calls `Payload->CalculateDamage(SourceASC, Causer)`.
     - Calls `BFFL_StatusEffects_C::DealBreakDamage(Victim = Player, Damage = CalculatedDamage)`.
   - **Conclusion**: When blocking an attack, health damage is 100% negated by `GE_ActiveBlock_BlockDamage`, but **100% of the attack's raw calculated health damage is deducted directly from the player's 75 BreakResistance**.
3. **Perfect Block Break Damage**:
   - Inside `Function: DealBreakDamage`:
     - Checks console variable `s.BlockGod`. If active, deals `99999.0` break damage.
     - In normal gameplay, selects **`25.0`**.
     - Applies via `BFFL_StatusEffects_C::DealBreakDamage(Victim = Attacker, Damage = 25.0)`.
   - **Conclusion**: A Perfect Guard inflicts **exactly 25.0 Break Damage** to the enemy (identical to a Perfect Harden).
4. **Guard Broken Stun**:
   - Inside `Function: HandleBrokenBlock`:
     - Broadcasts `Event.Ability.Block.Failed.Broken`.
     - In [`HRAS_Player`](./combat-damage-pipeline.md#L253), redirect rule:
       $$\texttt{State.BlockStance.Broken} \implies \text{All Medium and Heavy hits redirect to } \texttt{Event.Reaction.Hit.Flyback}$$
     - The player is sent flying backward in an extended knockdown recovery.

---

## 4. Native Damage Execution Pipeline (`USpartaDamageExecution`) [Binary Verified]

In [`combat-damage-pipeline.md`](./combat-damage-pipeline.md), the ordering of reduction, incoming multipliers, and critical hit application was listed as inferred.

### Method Signature & Execution Order:
From `UDamageExecutionModifier` and `USpartaDamageExecution`:
```cpp
bool ApplyDamageExecutionModifier(
    float& Damage, 
    float& DamageReduction, 
    float& IncomingDamageMultiplier, 
    float& CriticalChance, 
    float& CriticalBonus, 
    UAbilitySystemComponent* SourceASC, 
    UAbilitySystemComponent* TargetASC, 
    FGameplayEffectSpec Spec
);
bool ApplyPostDamageReductionModifier(
    float& Damage, 
    UAbilitySystemComponent* SourceASC, 
    UAbilitySystemComponent* TargetASC, 
    FGameplayEffectSpec Spec
);
```

### Execution Flow:
1. **Raw Damage Aggregation**: $\text{BaseDamage} \times \text{PayloadMultiplier}$.
2. **Captured Attribute Modifiers**: Multiplicative Tarforge (`CCC_Weapon_Damage`), Weak status (`0.95x`), NG+ scaling, and regional difficulty multipliers.
3. **Execution Value Modifiers (`ApplyDamageExecutionModifier`)**:
   - Reads Target's `SpartaCombatSet.BaseDamageReduction` (`0.0` default; `1.0` for Harden).
   - Reads Target's `SpartaCombatSet.IncomingDamageMultiplayer` (Fragile/Perforation stacks: $+0.05$ each).
   - Reads Source's `PlayerAttributeSet.CriticalChance` (`0.1`) and `CriticalBonus` (`2.0`).
4. **Reduction & Incoming Scaling**:
   $$\text{Damage} = \text{Damage} \times (1.0 + \text{IncomingDamageMultiplier}) \times (1.0 - \text{DamageReduction})$$
5. **Critical Evaluation**:
   - If `GameplayEffect.Damage.Options.DisableCrit` is **NOT** present on the effect:
     - Roll random float $r \in [0.0, 1.0)$. If $r < \text{CriticalChance}$, then $\text{Damage} = \text{Damage} \times \text{CriticalBonus}$.
6. **Post-Damage Hooks (`ApplyPostDamageReductionModifier`)**:
   - Evaluates shielding / mitigation passes.
7. **Meta Attribute Write**:
   - Result is pushed to `SpartaHealthSet.Damage`.
   - In `PostGameplayEffectExecute`, if `Character.State.Shell` is present, it reduces `ShellHealth`. If `ShellHealth <= 0`, player is severed into Dark Form. If in Dark Form, it reduces `Health` directly.

---

## 5. Axatana Animation Overrides [Asset Verified]

In [`combat-movesets.md`](./combat-movesets.md), `GA_Player_Attack_Light_Axatana_C` pointing to `A_MS1_Player_Axe_A_Montage` was listed as an unverified anomaly.

### Asset Verification (`abilities.jsonl`):
- `GA_Player_Attack_Light_Axatana_C` is an uninstantiated abstract base class containing leftover MS1 template defaults.
- The concrete abilities granted to the character are:
  - **Katana Form**: `GA_Player_Attack_Katanas_A1_C`, `A2_C`, `A3_C` playing `A_Shared_Attacks_Axeatana_A_02_Montage`, `AA_Montage`, `AAA_Montage`.
  - **Katana Duality**: `GA_Player_Attack_Katanas_A1_double_C`, `A2_double_C`, `A3_double_C` playing `A_Shared_Attacks_Axeatana_A_double_Montage` (4 hit windows, 0.8x multiplier).
  - **Axe Form Heavies**: `GA_Player_Attack_Axatana_Axe_B1_C`, `B2_C`, `B3_C` playing `A_Shared_Attacks_Axeatana_Axe_B_Cut_Montage`, `BB_Cut_Montage`, `BBB_Cut_Montage` (1.5x multiplier, 40 poise).

---

## 6. Hit Counts per Attack Montage [Extracted from `montages.jsonl`]

| Weapon & Montage | Total Length (s) | Hit Checks | Hit Timing Windows (s) | Payload / Characteristics |
| :--- | :---: | :---: | :---: | :--- |
| **Hadern's Sword A1** (`A_03`) | 3.20 | 1 | `[0.590 - 0.740]` | 1.0x BaseDamage, 30 Poise |
| **Hadern's Sword A2** (`AA`) | 3.10 | 1 | `[0.430 - 0.650]` | 1.0x BaseDamage, 30 Poise |
| **Hadern's Sword A3** (`AAA`) | 3.23 | 1 | `[0.790 - 1.080]` | 1.0x BaseDamage, 30 Poise |
| **Hadern's Sword B1** (`B`) | 2.83 | 1 | `[0.680 - 0.790]` | 1.5x BaseDamage, 40 Poise, `BreakShield` |
| **Axe & Dagger A1** | 2.40 | 1 | `[0.469 - 0.529]` | 1.0x BaseDamage, 20 Poise |
| **Axe & Dagger A1 Double** | 2.40 | 2 | `[0.216 - 0.308]`, `[0.471 - 0.565]` | 0.8x BaseDamage per hit |
| **Axe & Dagger A2 Double** | 2.83 | 2 | `[0.291 - 0.365]`, `[0.483 - 0.544]` | 0.8x BaseDamage per hit |
| **Axe & Dagger Running B** | 4.00 | 3 | `[0.490 - 0.720]`, `[0.620 - 0.850]`, `[1.090 - 1.320]` | 1.25x BaseDamage, 25 Poise |
| **Katanas A3 Double** | 2.67 | 4 | `[0.222 - 0.360]` (dual), `[0.465 - 0.637]` (dual) | 0.8x BaseDamage (x4 paired hits) |
| **Martyr's Blade B1** | 3.53 | 1 | `[1.211 - 1.490]` | 1.5x BaseDamage, 100 Poise, `ForceFlyback` |
| **Obsidian Hammer B1** | 3.33 | 1 | `[0.627 - 0.790]` | 1.5x BaseDamage, 120 Poise, Dismemberment |
| **Clockwork Scythe Grinder** | Loop | Periodic | Every `0.115s` tick | Flat 2 Damage + 4 Poise per tick |
