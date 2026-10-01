# Socket fit and helmet review, October 2

Scope: the other agent's uncommitted CSS work against `0b243bb`, plus corrections
from this review. The separate `work/cloth-core` worktree is not included.
This is a source checkpoint for beta.7, not a release or installation.

## Standards review

Two correctness and safety findings were corrected:

- A repeated game re-stow retained CSS's old collision-push ownership and could
  cancel the new fit. `StowedPose` now tracks location and rotation, drops old
  push ownership on game changes, and preserves the base of an unchanged
  transform component. Tests include rebasing after a rotation-only change.
- Helmet cleanup could destroy an unattached helmet owned by another live
  actor. It now requires the exact Gragu helmet class, no live owner and no
  attachment parent.

One additional lifetime/performance risk was addressed: fit data now comes
from its copied table cache before any blocking asset load. On a cache miss,
the worn data stays retained and component/data identity is checked after load.
No new asset load runs in the per-frame attachment pass.

## Spec review

Three findings were corrected:

- Applying to a replacement player component restored the previous pawn and
  erased the just-selected fit. Configuration now follows successful apply.
- Orphan cleanup exceeded the documented ownerless-only rule, corrected above.
- The helmet exception matched any class containing `helmet`. Both enumeration
  and cleanup now match the specific game class.

The extracted game tables also contain non-stowed sockets, including Gragu's
`Socket_Prop_L_02` adjustment. Automatic fitting now only uses `_Stowed` entries.
The earlier assertion that no shell adjusted other prop sockets was corrected.

## Validation and release gates

Use the repository's C++23 configuration and pinned UE4SS runtime
`97b7e501`, with retained header SDK `d7e7826d`. UE reflection layouts remain
checked at runtime. Transform conventions were compared with the local
UE 5.6.1 source and [Epic's transform composition reference](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/TTransform/Multiply).

The registered host suite covers 14 test executables. Socket tests include
repeated stows, independent location/rotation changes, wrapped angles,
borrowing gear, and held-socket exclusion. Parser tests cover fit precedence,
invalid tags and NPC fallback. These do not execute Unreal's live actor code.

Helmet queries run on display-character changes and menu close, not on each
periodic refresh. This limits the class-wide query described in
[Epic's actor query reference](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Actor/GetAllActorsOfClass).

Before beta.7 ships, verify the exact candidate DLL in the game:

- Sariel and Gragu wearing Genessa, including gun, seal and stowed melee gear.
- Auto to Default and back, outfit switches, original appearance, respawn and
  shell/component replacement. Restores must preserve later game writes.
- Held weapons, parry follow-up, trap cameras and gear borrowed by abilities.
- Repeated menu opens/closes, helmet rules, outgoing previews and other owners.
- Match the baseline scene, resolution, lighting and movement sequence, then
  compare frame times and CSS's existing frame-profile counters. Record initial
  load separately from steady gameplay and repeated menu use.

No zero-bug or zero-FPS-impact claim follows from a successful compile or host
suite. Live acceptance and performance measurements remain pending.

## Executed checks

- `cmake --build build/host -j 4` and
  `ctest --test-dir build/host --output-on-failure`: exit 0, all 14 tests pass.
- `cmake --build build/inventory-native -j 4`: exit 0, Windows build with
  `CSS_INVENTORY_DEV=ON` and transition probes off.
- `cmake --build build/beta5 -j 3`: exit 0, Windows build with both developer
  options off. The existing build-directory name is historical; the compiled
  version still comes from `VERSION` (beta.6), with beta.7 release work pending.
- `git diff --check`: pass.

Both Windows builds report the existing unused `row` variable in
`extension_data.cpp:140`. No new warnings were reported in the reviewed files.
Build/test logs are retained in `work/socket-fit/review`. No DLL was installed
and no release ZIP was changed during this review.
