# Completed-intro restriction recovery

CSS owns this watcher so its recovery does not depend on installing CSSX or Cheat Menu. The portable implementation lives in `native/shared/player_recovery.cpp`. Cheat Menu's explicit check uses the same class.

The live failure observed on Steam build 25265616 was an equipped Clockwork Scythe with `CanPutInHand=false`, no weapon in hand during scythe attack montages, and an active `GA_Player_Prologue_EggStrandingCustom_C` after both completion checks passed. Its `WeaponPutInHandBlock` handle resolved to `GE_State_Block_Weapon_PutInHand_Primary_C`.

The watcher checks once per accumulated second. It requires one active instance owned by the current player's ability component, both completion checks, exactly one weapon-drawing restriction, zero or one of each other known restriction, and no current montage. Nine consecutive observations of the same effect must pass before calling the game's `ResetPlayerState` once. The watcher reads the counts afterward and logs the actual result. It does not end the ability, change progression, directly remove effects or clear tags.

Each effect token is attempted once per observed player. A new effect on the same ability starts a fresh observation period. A player change resets the candidate. Missing or incompatible interfaces defer observation with a backoff, and unchanged errors are not repeatedly logged. The attempted-token set is bounded. Core reload starts a new watcher, so a failed cleanup may receive another guarded attempt after an explicit reload.

The effect lookup returns a read-only definition, not the active instance. CSS only uses it to verify the class behind the ability's stored handle. See [Epic's API reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/UAbilitySystemBlueprintLibrary/GetGameplayEffec-_2).

Validation: the draw-only regression failed before the change. Nine portable checks pass after it, including new-effect recurrence, unchanged-effect suppression, stacked locks, incomplete intro, animation, duplicate instances and player changes. The live watcher cleared the measured restrictions and the same scythe returned to the hand. This establishes recovery for that observed fault, not a general explanation or fix for every gameplay lock.
