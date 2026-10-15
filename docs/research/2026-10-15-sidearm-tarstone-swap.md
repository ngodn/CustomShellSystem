# Naylshotte Tarstone replacement

Evidence: `work/three-outfit-rig-physics/sidearm-tarstone-swap-20261015/`.
Installed core and assets are unchanged. The test uses the already collected
level-3 Emberseed and Viletongue Tarstones, without granting items or levels.
The native level field is zero-based and reads 2 for both.

## Native equip contract

`WBP_MGT_Tarstones_Single.CacheCurrentEquipmentTags` obtains the sidearm tag
from `BPFL_Player.GetCurrentSidearmID`. `WBP_Tarstone_Slot_Base` passes that
weapon ID as `EquipmentSlot` to native `EquipTarstone` and `UnequipTarstone`.
For this weapon the value is `Weapon.NailShotgun`. In contrast,
`GetCurrentTarstoneBuildForSlot` takes `Weapon.Slot.Sidearm` and resolves the
currently equipped weapon. These two tag contracts are not interchangeable.

The UI unequips the old item, equips the replacement and broadcasts the
sidearm elemental Removed/Equipped events. The corrected diagnostic follows
that order. The UI also calls SaveGame; the diagnostic does not call SaveGame,
but it changes runtime loadout data temporarily and cannot prevent normal game
saving. The original Tarstone build is restored through the native equip path.
Raw UI graphs and signatures are retained beside the recipe.

The decoded item names identify `ID_Sidearm_ClusterBomb` as Barrage Stone and
`ID_Sidearm_InflictBurn` as Emberseed Stone. Bombardment is Fulminant Stone,
not Barrage. This avoids testing the wrong projectile ability later.

## Retained failed preparation and recovery

The first preflight stops on a camera change before mutation or gameplay input.
After renewed idle authorization, the initial fixture incorrectly passes the
slot tag to EquipTarstone. The native method creates a separate build entry and
grants the Burn item; the original Naylshotte build remains unchanged. The
receipt is a failed test, not a visibility failure.

Recovery unequips that exact added item, clears its two null array elements via
the guarded reflected map-update operation, then removes only the owned empty
entry through the installed UE4SS Lua TMap Remove API. The temporary Lua script
and request file are restored byte-for-byte, with their originals retained.
The map has no surviving diagnostic entry. Native unequip also adds a Default
weapon-effect tag; removing that extra tag through RemoveWeaponEffectTag
restores the original active Bleed asset and tags. No raw save-file edits, core
reloads or production changes are used. The newer user position/view is kept.

`resume/recovery.json` records the intermediate missing effect before its repair;
`resume/infusion-recovery.json` then verifies the restored effect. Keep both.
The corrected native-path trial subsequently confirms exact Tarstone builds,
levels/experience/stacks, durability, support build, resources, settings,
unlocks and infusion restoration in `native-ui-path/result.json`.

That native-path trial successfully creates a new hidden Burn effect while
stowed, but stops on an incorrect parent assertion. The actual parent is
`SM_Weapon`, the native effects component's AttachComponent, rather than the
skeletal root WeaponMesh. Its owner is the equipped weapon. The next recipe
checks that native attachment and owner explicitly.

## Completed native replacement check

`settled-focus/result.json` and its independent `verification.json` pass six
phases: new Burn while stowed, Burn drawn, Burn restowed, restored Bleed while
stowed, Bleed drawn and Bleed restowed. Both effects remain active, render when
drawn and stop rendering when stowed. Their owner is the equipped Naylshotte;
the native static-mesh attachment is preserved. Replacement produces a new
Niagara component, rather than merely retaining the previous effect.

All Tarstone builds, levels/experience/stacks, support slots, durability and
pending break events exactly match the baseline afterward. The diagnostic
entry from the rejected preparation is absent. Original infusion asset/tags,
equipment, CSS settings, unlocks, Health/Resolve and released inputs restore.
Position, camera and view match the post-focus baseline; actor-angle comparison
allows only 1e-9 degrees of floating-point rounding. No shot was fired.

The preceding `completed/` directory contains another failed preflight, despite
its directory name. Its guard observed a camera change during focus, before any
mutation. The final recipe first waits for a stable post-focus view and only
then captures its test baseline. It retains the earlier view separately and
never overwrites it. Movement guards remain enabled throughout native changes
and draw/stow input. This does not assign a cause to prior view movement.

This closes active attached-effect replacement for the reported hidden-sidearm
issue. It does not prove Barrage projectile damage or isolate the original
natural owner-visibility drift sequence. Those remain separate requirements.
