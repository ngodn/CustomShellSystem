# Mortal Shell II auto-combos: follow-up chains after parry, guard and harden

Companion to [ms2-combat-system.md](ms2-combat-system.md). Written 15 September 2026 for the mod plan in which a **combo** is a scripted chain of actions that the mod fires automatically after a successful defensive action (parry, perfect guard, guard hit, perfect harden, harden hit, Slayer Punch). This document lists the trigger events the game exposes, the actions that can be chained, the constraints the game data imposes, and recommended chains per weapon, seal and sidearm.

Assumptions (correct me if the plan differs): the player still presses the defensive input themselves; the mod only automates what happens after the game confirms success; the chain can contain melee attacks, weapon abilities, sidearm shots, sidearm abilities and shell abilities; the chain spends the player's real Resolve.

## 1. Trigger events in the game data

| Trigger | Where the game marks it | Tags granted | Window / cooldown |
| --- | --- | --- | --- |
| Parry success (Infinite Seal) | `GA_Parry_Successful` activates (ability tags `Ability.Primary`, `Ability.Parry.Successful.Melee`, cancels `Ability.Parry.Action`); `GA_Parry_Successful_Projectile_Deflect` for projectiles | `State.Parry` (`GE_Parry`, infinite until removed), `GameplayEffect.Immunity.Damage` via `GE_ParrySafety` | parry window widened for consecutive parries on 5 September; Eredrim and Tiel Seal Affinity grant `GE_Unlock_Parry_Medium/Long` |
| Perfect guard (Untarnished Seal) | `GE_ActiveBlock_PerfectBlock` applied by `GA_ActiveBlock` | `State.ActiveBlock.PerfectBlock` for 0.3 s | invulnerability 0.36 s; `GE_ActiveBlock_PerfectBlockCooldown` 0.6 s (`Ability.Cooldown.PerfectBlock`); `RequiredActivationTime 0.3` before a hold counts as a guard |
| Guard hit (normal block) | `GE_ActiveBlock_BlockDamage` | `GameplayEffect.Immunity.Damage`, `State.HyperArmor`, `State.Hit.IgnorePhysReaction` while the block absorbs | guard meter drains; knockback 800 (300 on perfect) |
| Perfect harden (Vatra's Seal) | `GE_Harden_Perfect` (duration set by caller, default `PerfectStoneFormDuration 0.25`) | `State.Special.Harden.Perfect` | perfect cooldown 0.19 s, deals 100 poise and 25 Break; normal harden hit: 50 poise, stone form ends 1.0 s after the hit, cooldown 5.0 s |
| Harden active | `GE_Harden_Active` | `State.Special.Harden.Active`, `State.Ability.Seal`, condition immunity, `BaseDamageReduction 1.0` | current attack resumes when the hardened hit lands (`ANS_BlockHarden`) |
| Slayer Punch hit (Slayer Seal) | `GA_SlayerSeal_Attack` payload 100 damage, 100 poise, 100 Break | riposte is auto-triggered by the game if the target breaks (`GE_State_TimedAutoRiposteOnHit` / `GE_State_TimedAutoRiposteOnDamage` are the game's own auto-riposte states) | charge cooldown 15 s |
| Enemy broken | riposte prompt; riposte applies `GE_Riposte` | `State.Riposte`, `GameplayEffect.Immunity.Break`, `State.Animation.Riposte.Instigator` | riposte damage 100% of weapon, invulnerable through the animation (Week 1) |

Only `GA_Parry_Successful` and the Slayer auto-riposte are abilities you can listen to directly; guard and harden success are effect applications, so the cleanest hook is a tag-added listener on `State.ActiveBlock.PerfectBlock`, `State.Special.Harden.Perfect` and `State.Parry`, plus `GE_ActiveBlock_BlockDamage` for ordinary blocks.

## 2. Actions that can be chained

Input tags the ability sets already bind, so a chain can inject them as if pressed:

| Action | Input tag | Ability | Resolve | Notes |
| --- | --- | --- | --- | --- |
| Light attack | `InputTag.Ability.Attack.Primary.Pressed` | `GA_Attack_Primary` | 0 | also the riposte input when the target is broken |
| Heavy attack | `InputTag.Ability.Attack.Secondary.Pressed` | `GA_Attack_Secondary` | 0 | finisher stones change the last hit of a combo |
| Weapon ability / infusion | `InputTag.Ability.Active.Pressed` (hold for infusion) | `GA_Action_Ability_Weapon_Press`, `GA_WeaponBuffGranter` | 15 to 100 | intrinsic ability per weapon or the equipped Ability Tarstone |
| Sidearm fire | `InputTag.Ability.Sidearm.Primary.Pressed` | `GA_Sidearm_Primary_Press` | 1 to 40 per shot | requires `State.Aiming` (Aim hold, `GA_Aiming_Handler` needs `Character.State.Sidearm`); the chain must enter aim first |
| Sidearm ability | `InputTag.Ability.Sidearm.Secondary.Pressed` | equipped Ability Tarstone | 15 to 80 | same aim requirement |
| Shell ability | `InputTag.Ability.Shell.Press` (hold variants exist) | `GA_Action_Ability_Shell_Press` | 15 to 100 | `InputTag.Ability.ShellSpecial.Pressed` for the secondary (Eredrim Diapason) |
| Dodge | `InputTag.Ability.Dodge` | per-shell dash (`GA_Dash_<Shell>`) | 0 | Tiel's dash before a hit is itself a trigger (Shadow Dash) |
| Parry / guard / harden / Slayer punch | `InputTag.Ability.Parry.Press`, `Action.ActiveBlock.Press/Hold/Release`, `Action.StoneForm.Press/Release`, `Action.SlayerSeal.Hold/Release` | seal abilities | 0 | re-arming defence at the end of a chain |
| Heal / item | `InputTag.Ability.Action.UseItem`, `InputTag.Ability.Resolve.Pressed` | Mether's Pulse, Resolve Conduit charges | | optional last step when health is low |

## 3. Constraints the chain must respect

1. **No animation cancels.** A melee swing runs to completion. A chain step should start on the montage's end or on its notify window, never mid-swing. The only exceptions the game itself allows are harden mid-swing (resume after the hit) and the Infinite Seal parry.
2. **Equip and ability blocks.** While `State.Special.Harden.Active`, `State.ActiveBlock`, `State.Aiming`, `State.Attack.Melee`, `State.Ability.Buff.Weapon` or `State.Ability.Buff.Sidearm` is present, equipment changes are refused and several inputs are blocked by `GE_Block_*` effects (`GE_Block_SealAbility`, `GE_BlockSidearmAttack`, `GE_BlockWeaponBuff`, `GE_Block_ShellAbility`, `GE_Block_Dodge`). Check the tag before each step and skip, do not queue.
3. **Resolve gate.** `BPC_Ammunition.HasEnoughEnergyPrimary()` must be true or the shot dry-fires. Costs: Repeater 1, Hystrix 2.5, Crossbow 17, Cursed Child 20, Lute 20, Naylshotte 28, Trebuchaxe 30, Ballistazooka 40; ability stones 15 to 100; shell abilities 15 to 100. Each chain needs a fallback branch for low Resolve.
4. **Cooldowns.** Perfect guard 0.6 s, harden 5.0 s (0.19 s after a perfect harden), Slayer Punch 15 s, shell abilities 3 to 20 s. A chain that ends by re-arming a defence must respect these.
5. **Red-circle attacks.** Unblockable and unparryable; a chain that starts from a guard hit must not assume the next enemy attack can be answered the same way. Read `ANS_UnparryableAttackWarning` / `GE_UnparryableHit` on the attacker if the chain wants to bail out.
6. **Riposte priority.** If the target is broken, the light attack input becomes a riposte with i-frames and 100% weapon damage. Every chain should check the break state first and take the riposte instead of its scripted first hit.
7. **Groups.** Perfect guard and parry work on one hit; Vatra's harden eats one instance. Chains longer than two steps are for single targets; cap chain length by the number of enemies aggroed (`GA_AttackerTracker_Player` tracks attackers).
8. **Loop safety.** Parry success -> attack -> get hit -> parry success could re-trigger endlessly. Put a per-trigger cooldown (one chain per 2 to 3 s) and never let a chain end in an input that is itself a trigger unless the cooldown is armed.

## 4. Chain templates by trigger

Each template is a branch list; the chain takes the first branch whose condition holds.

### T1 Parry success (Infinite Seal)

1. Target broken: riposte (light attack), then re-arm parry.
2. Resolve >= weapon ability cost: weapon ability (the 3x Break of a parry usually leaves the target one heavy hit from breaking, so a Plummet Strike or Storm Strike both damages and finishes the bar).
3. Otherwise: light combo to the finisher (Stillblade's Stone adds 10/15/25 Break on the last hit) and stop.

### T2 Perfect guard (Untarnished Seal)

1. Target broken: riposte.
2. Heavy attack (finisher stones Clerik's or Tyrant's change its last hit), then a single sidearm shot with Rupturing Stone if Resolve >= shot cost (crit shots deal 10/15/20 Break).
3. Re-arm guard hold after the swing (0.6 s cooldown has passed by then).

### T3 Guard hit, not perfect

1. Guard meter low: release guard, dodge backward, one sidearm shot at range.
2. Otherwise: keep holding, no attack. A normal block gives no Break, so an automatic counter here is a trap against multi-hit strings.

### T4 Perfect harden (Vatra's Seal)

1. The interrupted attack resumes by itself; let it finish.
2. Then weapon ability if Resolve allows, else heavy attack. Perfect harden already dealt 25 Break and 100 poise, so a Break finisher is likely to break.
3. Do not re-arm harden immediately; the perfect cooldown is short but a second incoming hit inside the same string will land during the follow-up swing.

### T5 Harden hit, not perfect

1. Attack resumes; then dodge out. The 5.0 s cooldown means the chain has no defence for the next hit.

### T6 Slayer Punch connects (Slayer Seal)

1. Game auto-ripostes if the break bar fills. After the riposte (health and Resolve refunded by Gloomslayer), enter aim and fire the sidearm ability (Fusillade or Deadshot) while the target is recovering; the riposte refund pays for it.

### T7 Tiel Shadow Dash success

1. Shadow gained: Shadow Strike (40) via the shell ability input, then Critical Role guarantees the next melee crit, so follow with one heavy attack, then one sidearm shot (also crits under Critical Role at level 2).

## 5. Recommended chains per weapon

Timings are qualitative (fast, medium, slow) because per-montage frame data was not extracted. Costs are exact.

| Weapon | Seal fit | After parry / perfect guard | After perfect harden | Sidearm step |
| --- | --- | --- | --- | --- |
| The Iconoclast (35) | any | light, light, Stillblade finisher; Plummet Strike (70/50/50) if the bar allows | resume, heavy | Naylshotte blast (28) if Resolve >= 56 so a second remains; Crossbow bolt (17) otherwise |
| Axe & Dagger (30/20) | Vatra's or Infinite | light x4 with Duality Stone (double hits at 80%), then Deadly Flurry (100) only from a full bar | resume, light x2 | Hystrix burst (2.5 per quill, 8 to 10 quills) |
| Black Needle (35) | Infinite | Storm Strike (100/100/75) when full, else light thrust x3 into Stillblade finisher | resume, heavy sweep | Crossbow bolt (17) or Trebuchaxe charged throw (30) |
| Axatana (25/25 or 35) | Vatra's | Hexapod Core barrage (60/60/80, stagger immunity from L2), else light x6 | morph to axe (Morph Attack, i-frames since Week 1) then heavy | Repeater burst (1 per round, 10 to 20 rounds) |
| Clockwork Scythe (42) | Untarnished | light sweep x3 (Stillblade finisher), chainsaw stone left running | resume, heavy | Repeater only; the chainsaw drain and a 28+ shot do not fit one bar |
| Veteran's Battle Axe (38) | Untarnished | heavy (Clerik's finisher crit 30/35/40%), Weapon Throw (60/60/40) if the target is not broken | resume, heavy | Ballistazooka (40) or Trebuchaxe (30): the axe has no intrinsic ability to compete |
| Great Martyr's Blade (80) | Untarnished | one heavy; Spiral Surge (100/100/75) only from a full bar and only on a single target | resume, heavy | Ballistazooka (40) with Weeping Stone (Trauma, Break over time) |
| Obsidian Hammer (109) | Untarnished | one heavy; Heavy Stomps (80/75/60) on groups | resume | Ballistazooka (40); nothing faster matches its recovery |

## 6. Recommended chains per shell

| Shell | Ability cost, cooldown | Insert after a defensive success | Reason |
| --- | --- | --- | --- |
| Eredrim | Shoulder Bash 70, Diapason 60 | Shoulder Bash after a perfect guard (50 Break on top of the guard's Break); Diapason when 2+ attackers | Executioner then executes low-health targets; Seal Affinity widens the parry window so T1 chains fire more often |
| Tiel | Shadow Strike 40, 8 s | T7 chain; after a parry, Seal Affinity adds Break and Shadow on failed guards, so a failed defence also feeds Shadow Strike | crit-based follow-ups |
| Proxima | Biosampler 50, 7 s | after a perfect guard, Biosampler pulls the target in and applies Lightning, then heavy | Exoshell mitigation while aiming makes the sidearm step safer |
| Gragu | Staggering Blow 50, 15 s, charge 1.4 s | after a perfect harden (target already staggered), charge the blow for 80 to 150 | Hunger crit while the heart is empty |
| Lazlo | Retribution 25, 15 s | after a guard hit with 2+ attackers, Retribution shockwave (poise 50 to 150, radius 500) | cheap, so it fits even after a sidearm shot |
| Sariel | Exodus 15, 3 s | after any defence, Exodus then continue melee to cure the Pain it inflicts | the cheapest ability in the game; chain it every time |
| Smert | Miracle 100, 20 s | only from a full bar after a riposte; fists inflict Chaos during the stop | do not put Miracle inside routine chains |
| Genessa | Faithful Doubles 25, 6 s | after a perfect guard, summon doubles then light combo; doubles copy the weapon attack | doubles also copy sidearm attacks in Stray form |

## 7. Recommended chains per sidearm (the ranged step)

| Sidearm | Cost | Step to insert | Stones that make the step worth it |
| --- | --- | --- | --- |
| Naylshotte | 28 | one point-blank blast right after a heavy | Rupturing (crit Break), Blackblood (splash), Bravado (+25 to 40% damage at short range) |
| Forgotten Crossbow | 17 | one bolt after any melee step; Fusillade (80) instead when the bar is full | Splitting or Myriad (extra bolts at no extra Resolve), Ironpiercer's |
| Caged Hystrix | 2.5 | short burst after every defence; Strange Remnant (15) on groups | Hag or Accursed infusion, Contagion, Pulse Driver |
| Triarch Repeater | 1 | 10 to 20 rounds after a parry to stack Frost and Lightning | Frostshard, Unstable, Rupturing |
| Salvaged Trebuchaxe | 30 | charged throw only when the target is broken or frozen; Tarred Fragment (50) on bosses | Charged Stone (faster charge), Squall, Monarch's Vestige |
| Ballistazooka | 40 | one round after a riposte (Gloomslayer refund pays it) | Weeping Stone (Trauma), Fulminant, Confessor's Keepsake |
| Cursed Child | 20 | not a damage step; Summoning Stone (70) after a kill | Accursed, Enfeebling |
| Troubadour's Lute | 20 | after a guard hit with 2+ attackers, strum to confuse the second enemy | Hand of Rock (80); infusions do not apply |

## 8. Full example chains

Costs assume level 1 stones and a 100 Resolve pool.

**A. Untarnished + Iconoclast + Naylshotte (starter).** Perfect guard -> if broken, riposte -> else heavy (Stillblade finisher on the light string is the alternative) -> if Resolve >= 56, aim and blast (28) -> release aim, re-arm guard. Cost per chain 28, refilled by the two melee hits.

**B. Infinite + Black Needle + Crossbow.** Parry (3x Break) -> if broken, riposte -> else if Resolve >= 100, Storm Strike -> else light, light, Stillblade finisher -> bolt (17) -> re-arm parry after a 2 s trigger cooldown. Cost 17 or 117.

**C. Vatra's + Axatana + Repeater (Proxima).** Perfect harden (25 Break, 100 poise) -> resumed swing finishes -> Hexapod Core (60) -> aim, 20 rounds (20) -> dodge. Cost 80; Proxima's Shellshock refunds some when armor mitigates.

**D. Slayer + Battle Axe + Crossbow (Gloomslayer).** Slayer Punch (100 Break) -> game auto-ripostes, refunds health and Resolve -> aim, Fusillade (80) -> heavy -> Weapon Throw (60/60/40) if still not dead. Every bolt deals Break under Gloomslayer, so the chain can restart from the sidearm alone.

**E. Untarnished + Obsidian Hammer + Ballistazooka (Eredrim).** Perfect guard -> Shoulder Bash (70, 50 Break) -> if broken, riposte (100% of 109) -> else one heavy -> Ballistazooka round (40) only if the target is still standing and Resolve remains. Long chain, single target only.

**F. Vatra's + Axe & Dagger + Hystrix (Sariel).** Perfect harden -> resumed light string with Duality -> Exodus of Thorns (15) -> Hystrix burst (10 quills, 25) -> continue melee to cure Pain. Cost 40, the cheapest full chain.

## 9. Implementation notes for the mod

- Listen for tag additions on the player ASC: `State.Parry`, `State.ActiveBlock.PerfectBlock`, `State.Special.Harden.Perfect`, `State.Special.Harden.Active` removal after a hit, `State.Riposte`. `GA_Parry_Successful` activation and `GlobalEvent_*` are the only event-style hooks.
- Check before each step: target break state, `SpartaHealthSet.Resolve` against the step's cost attribute (`SidearmAttributeSet.PrimaryEnergyCost`, `PlayerAttributeSet.PrimaryMeleeAbilityCost`, `ShellAbilityAttributeSet.PrimaryEnergyCost`), blocking tags from section 3, and the number of attackers.
- Drive steps by sending the input tags in section 2 through the ability system rather than simulating key presses, so `GE_Block_*` effects still apply. Sidearm steps need Aim held for the shot's duration; `GA_Aiming_Handler` requires `Character.State.Sidearm` (granted by `GE_Sidearm_Active`).
- The game already has auto-riposte states (`GE_State_TimedAutoRiposteOnHit`, `GE_State_TimedAutoRiposteOnDamage`) used by the Slayer Punch; reusing them for a break during a chain gives the same behaviour and i-frames as the native path.
- Do not automate the defensive press itself. If a later version does, remember the perfect windows: guard 0.3 s state, harden perfect 0.25 s, and parry has no published frame window.
- Achievement lock: any chain built on the Slayer Seal inherits the save's achievement disable; chains on the other three seals do not.

## 10. Source notes

Trigger tags, costs, cooldowns and input tags are from the game data export described in the combat document (`GA_ActiveBlock`, `GA_Harden_Original`, `GE_ActiveBlock_*`, `GE_Harden_*`, `GE_Parry`, `GE_Riposte`, `GA_SlayerSeal_*`, item ability sets). Stone and shell reasoning follows the press and community builds recorded in `work/research/research-sidearms-builds.md` (Game8 per-shell builds, GamesRadar, GameRant, Steam threads). Per-montage timings and the exact parry window are not in the export; measure them in game before fixing step delays.
