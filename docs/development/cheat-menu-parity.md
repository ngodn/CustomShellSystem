# Native Cheat Menu port

Working checklist for CSSX 0.3.0. The source reference is the supplied MortalShell2Mod Scripts/main.lua, Scripts/ShellSwitch.lua and Scripts/PrologueRecovery.lua. The C++ implementation is in [extensions/cheat-menu](../../extensions/cheat-menu).

| Original feature | Native status | Verification still needed |
| --- | --- | --- |
| God, Auto Heal, Infinite Resolve | Implemented | Combat and coexistence with other cheat mods |
| Health, Resolve and revival actions | Implemented | Gameplay checks for each action |
| Movement multiplier | Implemented, restores owned CharacterData | Death, shell restart and character-data replacement |
| Harbinger level | Implemented, confirmed, UI level minus one | Separate test save |
| Ten resource grants | Implemented, confirmed | Separate test save |
| Fourteen controller unlock actions | Implemented, confirmed | Separate test save |
| Thorough unlock-all-shells | Pending | Debug shell list, tags and streamed pickups |
| Shell switching | Implemented: close menu, send once, verify identity | Every shell, Dark Form and interrupted travel |
| Completed-intro recovery | C++ port with nine observations and exact predicate | Live positive reproduction when the lock occurs |
| Smert stance, Genessa clones, Lazlo shockwaves | Pending | Current-player ownership and cleanup |
| No cooldown | Pending | Owned-instance edits and safe hook lifecycle |
| Matching-seal parry, block and harden | Pending | Seal gating and safe hook lifecycle |
| Max Shell Points 100 | Pending | Map values and exact restoration |
| Pickup selection, add, remove and give all | Implemented; 77 entries and soft-class resolution verified live | Grant/remove read-back on a test save |
| Tarstone selection and grants | Implemented, localized catalog and duplicate-ownership guard | Grant/read-back on a separate test save, native Inventory refresh |
| Individual and bulk Tarstone levels | Implemented, selected/category/all-owned scope, preserves both maps and refreshes equipped instances | Actual level changes and equipped effects on a separate test save |
| Saved hotkeys and controller binds | Pending | Focus, menu suppression and binding conflicts |

## Settings contract

Toggle and numeric edits are drafts. Apply settings updates the active session and saves numeric preferences. Discard changes restores the applied values. Cheats start off at launch. Pending settings block one-shot actions so an action cannot quietly use an older amount. Resource grants, progression, health reduction and gameplay-shell changes use confirmations. Apply settings itself does not perform those one-shot actions.

A failed preference save rolls back changed session settings. If restoration fails, periodic cheats stop and the user is directed to Turn off all cheats to retry cleanup. The extension refuses unload when owned values cannot be restored. Values changed by another owner are preserved. Controller changes stop cheats rather than carrying them into another save.

## Recovery contract

The intro observer is explicitly started from Recovery. It requires the exact active Egg Stranding ability, finished get-up and map checks, the matching weapon-block effect, no active montage and the measured tag counts. Nine observations must match. It never sets progression flags or removes arbitrary tags. The same ability receives at most one cleanup call. Changed players cancel the check.

Low Resolve is a valid firing requirement, not a stuck-state diagnosis. Do not add a generic force-unlock action to bypass that or legitimate story locks.

## Evidence

Portable tests cover draft isolation, discard, apply, save failure rollback, integer and step validation, confirmations, original-value restoration, pending shell identity, cleanup retry and the intro observer's positive and negative conditions. `tools/cssx_cheat_check.py` checks draft isolation, Apply, Discard and Turn off all cheats on the running player and restores the original damage flag.

These checks do not establish full feature parity. Do not publish the Cheat Menu as a complete port while the pending rows remain.

Tarstone grant tests cover confirmation, checking all required interfaces before mutation, preserving owned stones, avoiding duplicate registration after the item manager's receive event, typed soft references and no automatic retry after a partial failure. These are portable tests, not proof that a real save grant and its UI refresh work.

On Steam build 25265616, the live catalog returned 77 localized names. Refreshing it did not change either ownership map. Adding the already-owned Justiciar's Stone also left both maps byte-for-value unchanged, including level, experience, stacks and durability. Evidence: `work/cssx-native/tarstone-live-check.json` and `tarstone-maps.json` (local only).

Level-edit tests cover staged settings, confirmations, both map copies, preserving all non-level fields, equipped payloads, rollback after a rejected map write and no retry after an equipped refresh fails. The live map bridge accepts a no-op update and rejects a stale expected value, missing key, invalid numeric field and unknown struct field without changing either map. The level action also recognizes an already-matching level without rewriting data. These checks do not replace gameplay testing of a real level change.
