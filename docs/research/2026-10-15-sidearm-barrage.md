# Barrage and Emberseed visibility

The October 15 live check uses collected level-3 Barrage and Emberseed on
Naylshotte, with CSS sidearm visibility set to Only when in use. The installed
core remains `dc0a609e74750815084130278354de76f754643326c8ec3829393d7439cead67`.
No production code or assets changed.

Evidence and recipes are in
`work/three-outfit-rig-physics/sidearm-barrage-20261015/`.
The passing capture is `projectile-readback/result.json`, `observation.log` and
`verification.json`. Run its `verify.py` to check the retained evidence without
further game input.

## Result

One normal secondary-fire click consumes 40 Resolve. The native cluster parent
renders, followed by three activations of eight child projectiles. All 24 child
projectiles remain visible after Naylshotte becomes hidden and its attached Burn
effect stops rendering. The children subsequently become hidden/unrendered through
the native projectile lifecycle. They are pooled actors, so reuse of an existing
object identity is not evidence that no projectile fired.

The 160-sample, 100 ms observer records the native owner, actor visibility,
recent-render state, position, sidearm socket, attached glow, input and Resolve.
All projectile owners are the current player. A positive control verifies the
Lua actor-array binding against the current player before any shot. The observer
runs on the game thread, limits each class query to 64 results and explicitly
cancels its timer after the bounded capture. Temporary script/request files
restore byte-for-byte.

Native Tarstone builds, levels/experience/stacks, support slots, durability and
pending breaks match their original snapshots. The original Quickcharge/Bleed/
Deadshot build and active Bleed infusion restore, along with equipment, unlocks,
CSS settings, Health, Resolve, position, camera and released inputs. The owned
40-point test cost is refunded through the native Resolve method. Actor rotation
allows only the existing 1e-9-degree floating-point tolerance.

This closes Barrage/Emberseed firing and stowing visibility for the reported
combination. No enemy was hit, so this is not a damage verification. It also does
not isolate the original natural owner-visibility drift trigger.

## Retained failed instrumentation

The root attempt mistakenly sends X11 button 2, which is middle-click. It records
no secondary input, expenditure or projectile. Its result only establishes
restoration; the independent root `verification.json` explicitly rejects a firing
pass despite the original fixture's insufficient success condition.

`right-button/` sends button 3 correctly and records native ClusterBomb activation
and 40 Resolve expenditure. Its observer stops when it treats a reflected array
output as a direct UObject. The installed UE4SS implementation returns
RemoteUnrealParam entries for this output; each must be unwrapped with `get()`.
The failure and full restoration receipt remain. The final capture corrects the
binding and verifies it against a known actor before firing. Both failed attempts
are instrumentation failures, not evidence of a CSS projectile defect.
