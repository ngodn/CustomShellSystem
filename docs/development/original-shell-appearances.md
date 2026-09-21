# Official shell appearances

CSS adds **Use Original Shell** directly beneath **Original appearance** in
SHELL. Original appearance restores the current gameplay shell. Use Original
Shell offers another official shell's visuals while keeping the current pawn,
CharacterId, abilities, progress and animation class.

## Source and boundaries

The source is the running game's `SpartaGameSettings.GetShellNames` and
`GetShellItemDefinition`. Ignore its `Load From Save` sentinel. Follow the
item CDO's `ItemFragment_SetShellClass.ShellClass` to the shell CDO and read
`DefaultMesh`. Use the item CDO's localized `DisplayName` for the UI.

Independent cooked reads confirm these ten default appearances in CL93241:

| Display name | Stable variant ID | Default mesh leaf |
| --- | --- | --- |
| Tiel | tiel | SK_Tiel |
| Lazlo | necrophage | SK_Necrophage_ArmoredOn |
| Genessa | genessa | SK_Sester_Genessa_V6 |
| Smert | smert | SK_Shell_Smert |
| Harros | harros | SK_Harros |
| Eredrim | eredrim | SK_Eredrim |
| Sariel | thorn | SK_ThornBoi |
| Gragu | gragu | SK_Shell_Gragu |
| Proxima | knightlady | SK_Shell_KnightLady_V04 |
| Solomon | solomon | SK_Solomon |

Discovery reads definitions once after a playable character exists, and again
only after a catalog rescan. No object-array scans or gameplay shell-equipping
calls are used. Reflected soft references are copied into matching engine
conversion parameters, with type and size checks. Do not cast them to the old
UE4 soft-reference layout in the retained SDK. Temporary roots protect loaded
CDOs during discovery; the resulting catalog holds strings, not actor pointers.

Loading happens outside recurring per-frame work. Epic documents that
[blocking asset loads can hitch](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Kismet/UKismetSystemLibrary/LoadAsset_Blocking?application_version=5.5).
Meshes are loaded only when selected through the existing appearance path.
Its skeleton checks, rollback, menu mirroring and restoration still apply.

The reserved outfit ID is `css.original_shells`. Packages cannot claim it.
Selections and profiles use the existing state format, keyed by the actual
gameplay shell. The dedicated row is excluded from ordinary outfit ordering
and search, so favorites retain their existing ordering without a duplicate
official-shell entry. These choices use default mesh materials. They do not
grant shell-specific armor mechanics, powers or progression.

## Evidence and current status

`work/stock1/items.jsonl` captures all ten game item definitions.
`items-decoded.json` and `classes-decoded.json` independently decode the cooked
item and character packages using AssetReadback. `expected.json` derives the
expected names, IDs and meshes from that readback. `conversions.jsonl` verifies
the live conversion signatures before implementation.

Portable data tests pass: selection/profile persistence, current gameplay-shell
key preservation, enemy rejection, reserved-ID rejection and unchanged favorite
ordering. Development and shipping DLL builds pass. The development DLL and
metadata-only Eve rename are installed through a normal restart; the loaded
module is confirmed in the process map. CSSX remains disabled.

The first live run rejected valid constrained soft references because
`SameType` also compares their target classes. The correction permits checked
subclass-to-base widening within the same soft-reference kind. Sizes and
property ownership still come from reflection. This passed on all ten shells.

`tools/authoring-probes/release/check_original_shells.py` then passed all ten
actual menu hit bindings, world and preview mesh equality, unchanged pawn,
CharacterId, CharacterData, ability component, capsule, movement component and
animation class. Steam screenshots of every choice were reviewed. Eve's
selection, customization, ground offset, favorites and profiles were restored.
The only new state is an empty remembered customization for the built-in group.
Evidence: `work/stock1/check/result.json`, `choices.json` and the screenshots.

The first successful module was `css_core-stock2.dll`; deployment receipts and
backups are in `work/stock1/fix-live/`. The subsequent
[shared scrolling selector](inventory-choice-lists.md) is installed as
`css_core-choices1.dll` and passes menu re-entry. Combat, travel and every
gameplay shell are not covered by this idle appearance trial.
