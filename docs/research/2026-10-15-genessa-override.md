# Temporary Genessa override: native restrictions and persistence

The requested confirmed Faithful override is not implemented. The native
blocker message is installed and verified separately. A temporary CSS preference
alone cannot establish that a real form change has no game-save impact.

## Current evidence

Evidence lives in
`work/three-outfit-rig-physics/genessa-revival-block-20261015/`.
The installed core is `4a4402a210f4d1e8`, with developer probing enabled.
Only getters, reflected metadata, cooked-package extraction and static executable
analysis were used for this investigation.
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

The first REA/Ghidra native search for `SaveSession` timed out at the MCP
client's 300-second limit without returning analysis evidence. A later session
check still identifies the selected executable, but reports no active analysis;
the previously observed Ghidra process and its temporary directory are gone.
This attempt establishes no native call flow. The executable SHA-256 was
`363322022a0fdfd0cf87c494619da91e0985e6435d105a37def8a89cb3e29922`.
Recover through a retained analysis or a supported longer-running analysis
path before repeating the query. The Blueprint persistence findings above
remain valid; native scheduling and loading remain open.

## Conditional form restoration on load

`controller-class-path.json` resolves the loaded controller's class default
object to `/Game/Sparta/Core/Player/BP_PlayerController`. Independent cooked
readback in `controller-packages.json` establishes the following Blueprint path:

- `LoadCharacter` calls the separate Darkform and shell item-definition loaders
  outside its editor-default branch. Those read the player save's selected item
  definitions; they do not decide which form becomes active.
- `ActivateShellOrDarkform` gets the native subsystem's `GetSessionSave`, casts
  it to `BPSO_PlayerSession`, then combines `ShouldResumeSession` with its
  `IsInDarkform` flag. If both are true, it calls `ActivateDarkform(true, true)`.
  Otherwise it activates the shell when `ActiveShellItemDefinition` is valid,
  falling back to Darkform when it is not. A failed session cast also takes
  that valid-shell/fallback branch.
- `BPFL_SaveLoad.ShouldResumeSession` returns the native game mode's
  `ShouldResumeRun`. The game-settings default `ResumeLastRun=false` alone
  therefore does not establish the result of this runtime getter.
- The controller's post-load graph separately gates loading saved Resolve on
  the same resume-session predicate. Masking the whole resume decision would
  consequently affect more than form selection.

Full simplified bytecode, with branch offsets, is retained in
`ActivateShellOrDarkform.json`, `LoadCharacter.json`, `LoadDarkFormFromSave.json`,
`LoadShellFromSave.json` and `ExecuteUbergraph_BP_PlayerController.json`.
`ReceiveOnLoaded` on the session Blueprint calls its native parent and starts
the transform-check timer; it does not itself select the player's form.

Additional bounded leads are retained in `load-packages.json` and
`spawn-packages.json`. The Darkform load item is a definition with a display
name; the skin-handler and spawn Blueprint functions inspected there do not
read `IsInDarkform` or call `GetSessionSave`. This does not exclude inherited
or native behavior. Class-path lookups were read-only; none of these loading,
activation or saving functions was invoked.

This confirms a consumer of the saved form flag, not that the current game
session will resume or that Alteration permits the resulting form. Native
save scheduling, `GetSessionSave` selection and `ShouldResumeRun` remain to
be traced before a persistence interception is justified.

`resume-live-readonly.json` additionally resolves the current world game mode
through its reflected `GetInstance` signature and reads `ShouldResumeRun`:
it returns false. This describes this already-loaded world only, not the next
load or all save slots. The getter's signature was inspected before invocation.
No resume flag, session object or gameplay form was changed.

The replacement native analysis runs through the installed REA 6.4.0 CLI,
avoiding the MCP request timeout. Its reproducible command is
`native-save1/recipe.sh`, with a 3,600,000 ms startup deadline and an 8 GiB Java
heap verified in the owned JVM arguments. It requests a `SaveSession` trace
and retained snapshot. While `trace.json` is empty and the job is running,
there is no native result to cite. Follow the existing process rather than
starting another import. These are per-command settings; no global tool or
game configuration changed. The supported controls are documented in
[REA's installation guide](https://github.com/morluto/rea/blob/main/docs/installation.md#ghidra).

## Current executable and live session getter

The older `CXXHeaderDump/Sparta.hpp` is not authoritative for the current native
save ABI. `native-save1/save-signatures.json` captures the live reflected
`SaveToDisk` signature with only `bSaveAsync`, whereas the older header declares
three arguments. No save function was invoked to discover this difference.
Use live metadata for calls and verify native layouts against this executable.

`native-save1/locate_tables.py` identifies candidate UE name/function-pointer
pairs in the exact SHA-256-pinned PE above. It retains duplicate candidates,
neighboring entries and a negative-name control with no matches in
`native-table-candidates.json`. Class attribution cannot follow from a name pair
alone. Bounded GNU objdump captures corroborate the save wrappers and their
argument handling:

- The `SaveSession` wrapper at `0x145b10260` branches to `0x145cce440`, which
  sets both byte arguments to one and branches to `0x145cce600`.
- `SaveToDisk` at `0x145b10280` reads one boolean, then calls `0x145cce5f0`.
  That sets the third argument to one and branches to the same routine.
- `AutoSave` at `0x145b0e8e0` branches through `0x145cc4600`. After object
  checks, it reaches the same routine with both byte arguments set to one.
- `GetSessionSave` at `0x145b0fa60` calls `0x142101b90`, which returns the
  pointer at subsystem offset `0x100`. The older header uses different offsets.

The independent read-only receipt `session-getter-readonly.json` compares the
actual reflected getter result with both named properties. It matches
`RuntimeSessionSaveObject`, not `SessionSaveObject`, on this live subsystem.
The common routine's retained window contains a conditional virtual call on
that offset's object followed by passing it to `0x145c32ba0`. The further
attribution below identifies pre-save dispatch and `UpdateFrom`, but does not
yet establish a verified interception boundary. Shared entry paths do not prove when the
autosave timer fires or whether every call writes a session record.

All addresses are static image virtual addresses for this exact build, not
runtime hook addresses. `read_function.py` retains the command, hash, range and
stderr beside each assembly file. PE exception-table entries can describe only
function fragments; explicit windows are labeled as such, including the
3,300-byte window beginning at `0x145cce600`. The helpers use the documented
[PE section and RVA mapping](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
and [GNU objdump address bounds](https://sourceware.org/binutils/docs/binutils/objdump.html).
These are static artifact findings, separate from the still-running REA import.
No override, save hook, effect mutation or game configuration change was made.

### Registration validation and pre-save ordering

`read_save_registration.py` follows the owning registration routines rather
than treating every adjacent name/code pointer as a native entry. This matters:
`FNameNativePtrPair` stores name then function, while `FClassFunctionLinkInfo`
stores constructor then name. The initial candidate scan also matches across
neighboring generated-link entries and can associate a name with the next
constructor. Those generated-link candidates must not be used as named native
functions. The original candidate files remain as discovery evidence.

The corrected class-table read identifies `SpartaSaveSubSystem`, its native
registration at `0x145b0e720`, and 63 native entries at `0x149a40ea0`.
`native-registration-live-crosscheck.json` compares that complete name set
with live reflection: 63 on each side, no missing or extra functions. The
previously traced SaveSession, SaveToDisk, AutoSave and GetSessionSave entry
points all agree. `HasValidSessionSave` calls `0x145cc9840`, identifying the
previously unnamed validity check in `ShouldResumeRun`.

`verified-save-registration-with-event.json` also retains the save-object
class's 12 generated function links and eight native entries. Its constructor
chain leads to the vtable written at `0x145c32522`. Slot `0x2b8` points to
`0x145c32ad0`, which conditionally dispatches `ReceiveSpartaPreSave` through
`0x145b0e530`. The event name is independently tied to the dispatcher's FName
global by its initializer at `0x141072320`. The event's generated parameter
descriptor confirms a 48-byte context, whereas the adjacent ReceiveTick has
a four-byte parameter. Both descriptors are retained to expose the earlier
candidate-table ambiguity explicitly.

The common save routine tests context byte `0x2e` before calling that virtual
slot on the runtime session object, then passes the same object to
`0x145c32ba0`. The separately registered `UpdateFrom` wrapper directly calls
that address. Thus this static branch collects pre-save data before copying
from the runtime object. The context starts with that byte zero, and the
save-object dispatch itself also checks it for session objects. This can explain
the unchanged cached form, but is not proof that all session writes are skipped:
other callers, scheduling and actual save/load behavior remain to be tested.

No hardcoded address from this investigation has been installed as a hook.
The next runtime observation should determine whether the session pre-save event
actually executes during normal saving before attempting any interception.

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
