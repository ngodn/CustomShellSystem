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
| Thorough unlock-all-shells | Implemented, confirmed, preflight and ownership read-back | Actual grants on a separate test save, streamed pickups and summons |
| Shell switching | Implemented: close menu, send once, verify identity | Every shell, Dark Form and interrupted travel |
| Completed-intro recovery | Shared CSS watcher, nine observations, tracks each owned effect | Further natural recurrence and travel checks |
| Smert stance, Genessa clones, Lazlo shockwaves | Implemented; clone and stance lifecycle verified live | Upgraded Lazlo positive check, travel and interrupted activation |
| No cooldown | Implemented; owned-instance edits and resident hook service | In-game cooldown calls, reload and cleanup on both game builds |
| Matching-seal parry, block and harden | Implemented; exact seal and player-instance guards | In-game result overrides, seal changes and combat behavior |
| Max Shell Points 100 | Implemented with owned map values and exact restoration | Travel with the toggle active |
| Pickup selection, add, remove and give all | Implemented; 77 entries and soft-class resolution verified live | Grant/remove read-back on a test save |
| Tarstone selection and grants | Implemented, localized catalog and duplicate-ownership guard | Grant/read-back on a separate test save, native Inventory refresh |
| Individual and bulk Tarstone levels | Implemented, selected/category/all-owned scope, preserves both maps and refreshes equipped instances | Actual level changes and equipped effects on a separate test save |
| Saved hotkeys and controller binds | Implemented; native keyboard dispatch, persistence and menu suppression verified | Physical controller dispatch and final menu presentation |

## Settings contract

Toggle, numeric and shortcut edits are drafts. Apply settings updates the active session and saves numeric preferences and shortcuts. Discard changes restores the applied values. Cheats start off at launch. Pending settings block one-shot actions so an action cannot quietly use an older amount. Resource grants, progression, health reduction and gameplay-shell changes use confirmations. Saving a health-reduction or shell-switch shortcut also requires confirmation. Apply settings itself does not perform those one-shot actions.

A failed preference save rolls back changed session settings. If restoration fails, periodic cheats stop and the user is directed to Turn off all cheats to retry cleanup. The extension refuses unload when owned values cannot be restored. Values changed by another owner are preserved. Controller changes stop cheats rather than carrying them into another save.

## Recovery contract

CSS watches for the exact active Egg Stranding ability after both get-up and map completion. Cleanup requires its own matching weapon-block effect, no active montage and unstacked restriction counts over nine consecutive observations. Attack selection may already be unlocked while drawing the weapon remains blocked. CSS never sets progression flags or removes arbitrary tags. Each effect receives at most one cleanup attempt. A newly applied effect on the same ability gets a fresh observation period; changed players discard the candidate. The Cheat Menu also exposes an explicit diagnostic check using the same implementation.

Low Resolve is a valid firing requirement, not a stuck-state diagnosis. Do not add a generic force-unlock action to bypass that or legitimate story locks.

## Shell unlock contract

Unlock all shells resolves the game's shell catalog, save interface and current-world pickups before writing. It validates each required function signature, deduplicates tags and actors, and checks the current pawn, controller and runtime save throughout the operation. Catalogs and actor batches are bounded. Saved equipment ownership must confirm every requested shell before streamed pickups are marked collected and summons refreshed.

The controller's `S_UnlockAllShells` handles its debug list. The port also grants every catalogued shell tag and tags found on streamed shell pickups. It does not replay the original Lua script's hard-coded debug indices 0 through 14: the live `S_UnlockShell` parameter is an integer without enum metadata, so that range cannot establish compatibility with another game build. This difference needs checking against actual shell availability on the test save.

Actor lookup uses the player's world through [GetAllActorsOfClass](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameplayStatics/GetAllActorsOfClass). Unloaded pickups are not traversed. A partial failure reports the completed step count, warns that even the failed call may have saved changes, and is never retried automatically. Progression writes are not presented as reversible settings.

Portable tests cover confirmations, pending settings, duplicate tags and actors, pickup-only shell tags, invalid catalogs, oversized batches, signature mismatches before mutation, failed save writes, ownership verification failures and changed players. Live read-only probes verified the ten-tag catalog, `EquipmentUnlockState` map and pickup/summon interfaces on build 25265616. There were no streamed shell pickups or summons in the inspected area. Local evidence: `work/cssx-native/unlock-shells-preflight.json`, `unlock-shells-actors-preflight.json`, `unlock-save-properties.json` and `unlock-equipment-state.json`. These probes do not verify fresh progression grants.

## Evidence

Portable tests cover draft isolation, discard, apply, save failure rollback, integer and step validation, confirmations, original-value restoration, pending shell identity, cleanup retry and the intro observer's positive and negative conditions. `tools/cssx_cheat_check.py` checks draft isolation, Apply, Discard and Turn off all cheats on the running player and restores the original damage flag.

These checks do not establish full feature parity. Do not publish the Cheat Menu as a fully verified port while the remaining live checks are incomplete.

Tarstone grant tests cover confirmation, checking all required interfaces before mutation, preserving owned stones, avoiding duplicate registration after the item manager's receive event, typed soft references and no automatic retry after a partial failure. These are portable tests, not proof that a real save grant and its UI refresh work.

On Steam build 25265616, the live catalog returned 77 localized names. Refreshing it did not change either ownership map. Adding the already-owned Justiciar's Stone also left both maps byte-for-value unchanged, including level, experience, stacks and durability. Evidence: `work/cssx-native/tarstone-live-check.json` and `tarstone-maps.json` (local only).

Level-edit tests cover staged settings, confirmations, both map copies, preserving all non-level fields, equipped payloads, rollback after a rejected map write and no retry after an equipped refresh fails. The live map bridge accepts a no-op update and rejects a stale expected value, missing key, invalid numeric field and unknown struct field without changing either map. The level action also recognizes an already-matching level without rewriting data. These checks do not replace gameplay testing of a real level change.

Shell-point limit checks passed on the updated game: eight limits rose to 100 after Apply and returned to their exact originals after Disable all. Portable tests also cover passive drafts, keeping higher or newer values, failed preference saves, and cleanup retry.

Combat port: Windows DLLs compile and portable tests cover passive drafts, duplicate instance filtering, preserving other writers' cooldown values, failed hook installation and preference-save rollback, changed seals and failed-unregister retry. The live probe found 110 player-owned ability instances and checked Apply/Clear cooldown signatures plus the three parry boolean returns. The active seal is controller.ActiveSealItemHandle.ItemDef, where the handle is an item-instance object on build 25265616.

The updated permanent loader was installed and tested after a restart. The three parry functions returned the intended overrides, then their exact original results after disabling. Cooldown registered 220 instance rules, both native Apply callbacks dispatched successfully, and sampled values restored exactly. Genessa's valid duration sentinel of -1 is preserved. Reloading with cooldown enabled removed all rules and restored sampled fields before unloading the old core. Evidence: work/cssx-native/parry-live-check.json, cooldown-live-check.json and combat-reload-check.json. These checks do not replace actual combat testing of all three seals or a fresh run on the older executable.

On 15 September, the equipped scythe played scythe attack montages but remained stowed. The completed Egg Stranding ability still owned effect handle 321, weapon drawing was blocked, and attack selection was already unblocked. The old predicate rejected that combination. The regression test failed before the change. With the shared CSS watcher installed, the game cleanup cleared all six measured restrictions and the same scythe returned to the hand. CanPutInHand changed from false to true. Evidence: work/cssx-native/unarmed-{attack-samples,restrictions,intro-completion,can-draw-before,can-draw-after}.json. Portable tests cover fresh effects on the same ability, no repeated attempt on an unchanged effect and resuming observation after animation. No blanket lock removal was added. The reason the game leaves this ability active across these save loads is still unknown.

## Shell powers and shortcuts

Powers resolve the current player's ability instance and verify its avatar, gameplay-shell identity and continued membership in the player's ability list. Outfit appearance does not qualify a different shell for these powers. Activation waits for gameplay, positive health, game-window focus, no menu, no pause and no ignored movement/look input. Repeating shockwaves use gameplay time without replaying missed intervals after a pause.

Genessa's cooked `SpawnPrimaryClone` schedules `SpawnSecondaryClone` on the next game tick. `HasSecondaryClone` requires `UpgradeStat.Shell.Genessa.Mirage.Level >= 2`. On the tested save it returned false, which explained the missing second clone. The port temporarily overrides that return only on the current owned ability, calls the primary spawn once, and waits for both actor references. It does not write shell upgrades. Cleanup preserves newer actors in those slots and restores the exact original SpawnCount. A pending creation remains owned and can block unload until the game resolves it; no blind retry is sent.

Live checks on build 25265616 verified draft isolation, both clone actors, removal of both actors, SpawnCount restoration from 9999 to its original 1, and restoration of HasSecondaryClone to false. Reloading the active pair passed the same cleanup. Evidence: `work/cssx-native/clone-upgrade-gate-check.json`, `clones-pair-live-check.json`, `clones-reload-check.json`; cooked control flow is recorded under `clone-blueprint/`. The original Lua implementation called both spawn functions and counted accepted calls, which did not establish actor creation.

Smert owns an active gameplay-effect handle, not a UObject pointer. Cleanup removes the stance only while that exact handle remains active and is still the ability's recorded handle. A newer game-owned stance is preserved. Failed cleanup remains retryable and prevents unload.

Shortcuts share the existing native menu entry point. Open Inventory using the game's configured binding, then select CSSX. There is no second F6 menu overlay. Configurable action shortcuts offer F1 through F24 excluding Steam's F12 default, Ctrl combinations and R3 plus D-pad chords. Conflicting or overlapping assignments are rejected. Keys supplement the game's bindings rather than consuming them. The menu advises choosing unused combinations.

Polling stops when no shortcuts are assigned. Assigned shortcuts sample at most 60 times per second, batch key reads, require a fresh release and press after focus/menu/settings changes, and dispatch at most one action per sample. Portable tests cover short presses, held-key suppression, controller chords, Apply/Discard, conflicts, saved preferences, confirmation for shell/health shortcuts and cleanup after delayed power creation. `input.keys` was verified live for keyboard and controller key names; that read-only check alone does not prove dispatched shortcuts work.

Smert live activation created effect handle 1068; disabling removed that exact active effect even though the ability retained its old numeric handle. Lazlo on this save has no Temperament ability instance. The missing-upgrade Apply request was rejected without leaving an active cheat, and the gameplay shell returned to Genessa. The positive repeating-shockwave test still needs a save with Temperament. Evidence: `smert-stance-live-check.json`, `lazlo-missing-upgrade-check.json`, and the cooked `SoftSkill` reference under `lazlo-blueprint/`.

The live shortcut check used native F8 key input: draft ignored, held key fired once, release/press toggled God off with exact damage-flag restoration, Inventory suppressed the shortcut, and the host closed native Inventory successfully. The saved binding was read back from the extension state, then cleared. No tested cheats or test shortcuts were left enabled. Evidence: `bindings-live-check.json`; repeatable local driver: `tools/cssx_binding_check.py`.
