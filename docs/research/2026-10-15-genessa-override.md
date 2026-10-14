# Temporary Genessa override: native restrictions and persistence

The requested confirmed Faithful override is not implemented. The native
blocker message is installed and verified separately. A temporary CSS preference
alone cannot establish that a real form change has no game-save impact.

## Current evidence

Evidence lives in
`work/three-outfit-rig-physics/genessa-revival-block-20261015/`.
The installed core is `4a4402a210f4d1e8`, with developer probing enabled.
Only getters and cooked-package extraction were used for this investigation.
No upgrade, gameplay effect, save object or autosave setting was changed.

`override-packages.json` independently decodes five packages using the existing
.NET 10 AssetReadback executable, CUE4Parse patch and the matching game mappings.
It establishes:

- `GE_Genessa_ReviveBlock` is infinite, aggregate-by-target, stack limit one. Its
  granted tag is `State.ReviveBlock`.
- Alteration stores its own active blocker and severed-effect handles. Its
  `RemoveReviveBlock` method removes the stored blocker handle. Ending the
  ability calls that method and then native `ForceReviveShell`.
- Native revival does more than select a mesh. `ReviveShell` activates the
  shell, can heal it, broadcasts revival events and temporarily blocks the shell
  ability. `RemoveSeveredState` removes effects carrying
  `GameplayEffect.State.Severed`. Bypassing the activation predicate therefore
  still has gameplay consequences that need a round-trip check.

The Alteration graph and revival bytecode are retained in the preceding
`doubles-secondary-20261015` evidence. Its live Alteration effect handles were
active and executed. Those observations do not prove that deleting and
recreating an effect preserves the owning ability's bookkeeping.

## Session-save behavior

`session-config.json` reads the actual `SpartaGameSettings` default object:
the configured class is `/Game/Sparta/SaveObjects/BPSO_PlayerSession`,
`AutoSaveFrequency` is 300 seconds and `ResumeLastRun` is false. The frequency
alone does not prove that a session save runs on each world autosave.

`session-packages.json` decodes the configured session class and BPFL_SaveLoad.
The session class declares `IsInDarkform` with the `SaveGame` property flag and
sets `bIsSessionSaveData=true`. Its `ReceiveSpartaPreSave` graph validates player
zero and its ability system, then calls the resource, revival-progress and form
save functions. `SaveIsDarkForm` writes the result of
`HasMatchingGameplayTag(Character.State.Darkform)` into `IsInDarkform`.

The following simplified bytecode extracts retain statement offsets and the
complete control flow for review:

- `SaveIsDarkForm.json`
- `ReceiveSpartaPreSave.json`
- `ExecuteUbergraph_BPSO_PlayerSession.json`
- `ReceiveOnLoaded.json`

`session-live-readonly.json` resolves the current save subsystem through its
reflected `GetInstance` signature. Both `SessionSaveObject` and
`RuntimeSessionSaveObject` currently report `IsInDarkform=false`, although the
actual pawn is Stray. Their captured revival progress and shell health are zero.
This is a read of cached objects, not a save/load experiment or proof that form
is never persisted. The exact native session-save scheduling and load behavior
remain unverified.

## Implementation constraints

Normal native rules remain the default. Selecting an override must first show
Confirm/Cancel with the Alteration restriction, actual gameplay consequences
and verified persistence behavior. Cancel performs no form or effect mutation.
The override is not a profile setting and must not remove an upgrade or modify
progression. Do not disable game saving to satisfy that requirement.

Two implementation routes need further evaluation:

1. A tightly scoped activation-predicate override for the current pawn's native
   revival ability, valid only during the explicitly confirmed request. This
   could leave Alteration's blocker handle intact, but must verify the native
   revive/severed cleanup, health effects, other abilities and return to Stray.
2. A targeted persistence boundary for a proven active temporary override, if
   required to preserve the native saved form. Do not suppress the whole save
   or mask legitimate changes after a new player, save, death or travel event.
   Establish the session writer and loader behavior before implementing this.

The existing loader hook service supports game-thread post callbacks for native
and Blueprint functions with teardown. That establishes available plumbing,
not correctness of either proposed hook. No override hook has been installed.
The source and exact callback frame/return layouts need review before a trial.

The installed UE 5.6.1 `AbilitySystemComponent.h` also exposes native effect
inhibition, but its API requires transferring the handle and checking the
returned handle. The old overload is deprecated specifically because the input
handle may no longer be valid. The game's reflected ability-system dump does
not expose that function. Treating inhibition as a harmless reflected toggle
would therefore be unsupported by the inspected binding and ownership evidence.
Source:
[UE 5.6.1 AbilitySystemComponent.h](../../../CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemComponent.h).

Epic documents that persistent Gameplay Effects live on the ability system and
can alter tags or remove other effects. This supports treating effect removal
as a lifecycle operation, not an interchangeable boolean toggle. The game's
extracted graph is the authority for Alteration's actual ownership.
[Epic Gameplay Effects documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-effects-for-the-gameplay-ability-system-in-unreal-engine).

Required follow-up: verify native save scheduling and loading; prototype bounded
activation with readback and restoration; test Confirm/Cancel and ordinary form
requests; test return-to-native behavior, save/load, world/player replacement,
death, CSS disable and core teardown. Do not advertise no save impact until
those checks support it.
