# Vista rest animation, CSS 1.0.1

The October 17 request targets the native **Vista Sit** interaction. The live
actor is `BP_Interactable_Ability_Sit`; its prompt says `Sit`. During the seated
phase the main animation instance plays
`A_Shared_Actions_Vista_Sit_Loop_Montage` under the shared shell `Vista_Sit`
directory. Evidence and executable probes are in `work/sitting-animation/`.

## Behavior

Locomotion > Rest > Vista rest animation uses the same catalog and compatibility
filter as Idle Animation. The `vista` saved choice is independent of `idle`, per
outfit variant, and travels with profiles. An absent choice means Default,
including saves with the legacy global feminine movement setting enabled.

Only the exact native Vista loop permits the override. Entry, exit, other
montages, menu input, movement and aiming retain their existing gates. The
outfit's existing CSS idle postprocess inputs supply the body pose while the
native interaction keeps running. Default releases those inputs. The shared
animation resolver retains and validates the same clips used by normal idle.

This requires a CSS idle layer on the current outfit. The selector disables
custom choices when that layer is absent. It does not insert a new animation
instance into a running native action. In particular, rest selection does not
start the facial-performance adapter, whose reinitialization can interrupt
the main animation instance. Body poses keep their authored height and position;
there is no automatic adjustment to the stone or conversion of dances into
seated animations.

## Verification

- Both the host and production Windows builds pass. Production disables
  `CSS_INVENTORY_DEV` and `CSS_TRANSITION_TESTS`.
- All 28 host suites and 15 runtime release-packaging tests pass. Tests cover
  separate selection, profile persistence, legacy defaults, library performances,
  aliases, and exact native-loop matching with entry/exit negative cases.
- A normal game restart loads the new DLL and preserves the saved Peony look.
  Original DLL/settings and install hashes are in `install-01/`.
- `runtime-01/result.json` checks all 62 live choices: Default, Feminine, and
  the 60 installed Idle Animation entries. Every custom selection enables its
  body clip while the native loop remains active. The saved state differs only
  in the Vista choice. Default releases the override.
- World screenshots verify the native pose, seated recline, Feminine and Iris
  Out. Two Iris Out snapshots differ, confirming animated playback. These are
  representative visual checks, not a claim to have watched every clip in full.
- The production menu exposes the Rest section and full selector at 2560x1440.
  Native keyboard navigation reaches the sixth row and selects Seated Recline
  without changing the normal Idle setting.

The first proof probe used an open menu and therefore captured the preview.
It is retained as `idle-probe-01`, not counted as world visual evidence. The
second probe closed the menu and demonstrated the live override and restoration.

Keyboard checks use Hyprland's documented
[send_key_state dispatcher](https://wiki.hypr.land/Configuring/Basics/Dispatchers/)
with a matched down/up pair directed at the game window. Native interaction
checks call the focused stone's input handler with its declared Interact action.

## Release relationship

CSS stays at the user-requested 1.0.1. Shared Assets 1.0.1 retains its cooked
assets and includes the already-tested Commander White 0.1.1 compatibility
metadata. Commander White 0.1.1 includes the blood repair described in
[the blood/skin investigation](cw-blood-peony-skin.md). Eve 1.5.0 and Unholy
Genessa 0.3.0 retain their frozen archive bytes. BBCode changelogs and the current
delivery index belong under `dist`. No online publication is part of this task.
