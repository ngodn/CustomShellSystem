# Sidearm firing report, 14 September 2026

The inspected firing failure is consistent with depleted Resolve. No CSS recovery code or gameplay-unlock bypass was added.

## Live findings

The equipped Nail Shotgun reported these values through its own ammunition component:

| Check | Result |
| --- | --- |
| Current energy (Resolve) | 0 |
| Maximum energy | 100 |
| Minimum required energy | 28 |
| `HasEnoughEnergyPrimary()` | false |
| `IsReadyToFire` | true |
| Aiming ability active | false |
| Aiming/stow requested/stow blocked | false |
| Player input blocks | empty |
| Current player montage | none |

The cooked `BPC_Ammunition` implementation reads `SpartaHealthSet.Resolve` for energy. The player and component belonged to the same current pawn. The user confirmed firing remained unavailable, then identified insufficient Resolve or an inactive Infinite Resolve toggle as the likely explanation.

An earlier capture had Ballistazooka equipped. The weapon subsequently changed to Nail Shotgun. These are separate observations, not proof of a permanent equipment lock.

## Corrected initial lead

`GE_Sidearm_Active` grants `Character.State.Sidearm`. Its presence while the weapon is stowed is not proof of a stuck ability. The cooked `GA_Aiming_Handler.K2_CanActivateAbility` requires that tag. Removing it as a blanket recovery would interfere with normal aiming.

The nearby MortalShell2Mod stores toggle preferences but deliberately starts cheats off each process launch. Its `ToggleSaved()` returns false. A saved `infiniteResolve: true` or `noAbilityCooldown: true` does not establish that either feature is active in the current session.

## What remains unconfirmed

The occasional Inventory restriction was not reproduced independently. The basic sidearm equip behavior rejects equipping the already selected sidearm, but that alone does not explain every reported Inventory lock. Investigate again only if a different sidearm cannot be selected, with the exact menu state captured.

No claim is made that every possible sidearm failure is resource depletion. This session did not demonstrate a CSS mesh/animation fault, a cheat-hook fault, or a progression-lock fault. No Resolve refill, tag removal, ability cancellation, or save/progression edit was executed during this investigation. Temporary diagnostic additions were removed afterward.

Local captures and decoded Blueprint evidence are excluded from Git under `work/sidearm-inspection/`.
