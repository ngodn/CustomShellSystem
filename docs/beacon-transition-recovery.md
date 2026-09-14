# Beacon return recovery

## Report

After cleansing a beacon, returning to Marrow Keep or the outer world can leave
the original shell visible and CSS unable to open. The report includes returns
with and without a story scene. This is distinct from the earlier beacon-cancel
material reset. The full cleansing sequence was not broken in the initial live
session, so the investigation first reproduced the constituent failures.

## Findings

CSS only scheduled automatic appearance application when the player's full name
or CharacterId changed. The game can restore the stock mesh on the same player.
The existing material repair explicitly skipped a different mesh, so neither
path recovered that case. A developer-only reset reproduced this in the running
game: Seductress became SK_Sester_Genessa_V6 and stayed there after two seconds.
Explicitly enabling CSS restored it.

A new player could also be observed before its mesh was ready. The pending apply
flag was cleared before attempting the change. A failed or deferred attempt was
not retried unless identity changed again.

The wardrobe previously used cursor visibility as its only game-menu exclusion.
A cursor is not proof that a menu is open. Conversely, the live game pause menu
had a hidden cursor, IsInGameMenu=true and PauseGameCounter=1. Its real UI handler
provides the more relevant state.

Inventory preview maintenance ran before player refresh and file requests.
A repeated cleanup exception could prevent those later steps from running. This
was a code-path finding, not a reproduced exception in the reported cleanse.

MortalShell2Mod's PrologueRecovery repairs one identified, completed prologue
ability. Its conditions do not establish a beacon fault. CSS does not call that
repair, remove gameplay tags, reset abilities, unlock equipment or edit game saves.

## Changes

- Track player, controller and mesh-component identities with serial-checked weak
  handles, in addition to the shell tag.
- Recover a return to the stock mesh previously captured for that component.
  Leave unknown mesh replacements alone.
- Keep automatic recovery pending while the player, mesh or possession is not
  ready. Respect the saved automatic-restore setting and Original selection.
- Defer recovery during movement/look locks, active montages, loading widgets and
  temporary material effects. Retry failures with a 500 ms to 8 second backoff.
  The normal observation interval remains 250 ms; no global actor scans.
- Exclude world subobjects and transient-package materials from asset-path
  rollback snapshots. Revalidate weak references after blocking asset loads.
- Use game menu state, UI pause ownership and loading/read-text widgets when
  opening CSS. A stray cursor flag alone does not block it.
- Close CSS if its widget is detached, its camera is replaced, its player changes,
  or the game takes UI ownership. Pause cleanup targets the world CSS paused.
  Input cleanup does not overwrite a different controller, world or active game UI.
- Catch and throttle inventory maintenance failures separately from CSS input
  processing. Clear stale inventory ownership before attempting restoration.
- Treat extra empty override slots as equivalent during restoration. SetMaterial
  can clear a slot without shrinking the override array.

These checks preserve genuine story and game-menu locks. They do not attempt to
repair an unknown lock owned by the game or another mod.

## Verification

The controlled live test passed in the same Windows game process (PID 372):

1. Reset the applied mesh to the captured stock mesh, then observe automatic
   restoration of Seductress on the same player.
2. Repeat with a temporary dynamic material. Confirm the stock mesh stays while
   the effect is present and Seductress returns after the effect is removed.
3. Set a stray cursor flag with no game menu active. Open and close CSS. Confirm
   preview and pause ownership are released and the player is visible.
4. Compare gameplay animation-instance identity before and after these steps.

Four native CTest targets pass, including the new recovery lifecycle cases.
All 31 Python tests pass using the original checkout's extraction fixtures.
The isolated checkout initially lacked those ignored fixtures; its two fixture
failures were not runtime or conversion regressions.

Full manual cleansing/story/travel verification is recorded separately below.
Do not treat the controlled mesh/material test as proof of every narrative path.

## Repeating the checks

C++23 uses the existing clang-cl/MSVC ABI toolchain and pinned UE4SS SDK.

```sh
cmake -S native -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j 4
ctest --test-dir build/linux --output-on-failure
```

`tests/live_transition_check.py` requires a safe, loaded game and a core built
with `-DCSS_TRANSITION_TESTS=ON`. It stages the built core, temporarily changes
cosmetics/cursor state, and restores them. Run it from the repository root.
Developer reset commands are compiled out when that option is OFF, the default.
Build and reload the normal core after using it. Do not run another request
client concurrently.

For a read-only recording while the player performs the actual cleanse:

```sh
python3 tools/check_transition.py --seconds 240 --output work/beacon-return.jsonl
```

This records player/mesh/controller readiness, menu and input-lock state, plus
CSS recovery and preview ownership. Engine timeouts during loading are recorded
as observations. It does not activate a beacon, alter a quest, or skip a scene.

The initial evidence is in the isolated worktree's `work/baseline-reset.json`
and `work/fixed-transition-live.json`. Runtime DLL acknowledgements and the
manual recording are in `work/final-loader.json` and
`work/beacon-cleanse-final.jsonl` there.

## Sources

Runtime signatures and field names were checked against the game's UE4SS
CXXHeaderDump: BP_PlayerController, BPC_UserInterfaceHandler, Engine and UMG.
Epic's [RemoveFromParent documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UWidget/RemoveFromParent)
explains widget detachment; object validity alone does not establish viewport
membership. The shipped reflection dump, rather than a newer online API version,
is the ABI authority for this adapter.

## Installed build and manual status

Normal core: `css_core-844e02d6366239e5-1789345448127435733.dll`, live reload acknowledged by Windows PID 372.
SHA-256: `844e02d6366239e5b268cb25ec07ed88e89c5de963247f389f76966db406ed88`. Test commands are compiled out.

The first four-minute read-only recording remained in ordinary gameplay with
Seductress selected and no game UI or input lock. It did not capture a cleanse
return. The full beacon/story path still needs the user's requested re-test.
