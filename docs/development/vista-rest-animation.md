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

Only the exact native Vista loops (common and Lazlo) permit the override. Entry, exit, other
montages, menu input, movement and aiming retain their existing gates. The
outfit's existing CSS idle postprocess inputs supply the body pose while the
native interaction keeps running. Default releases those inputs. The shared
animation resolver retains and validates the same clips used by normal idle.

Native bodies use the existing Shared Assets idle BlendSpace carriers, with the
same skeleton checks as normal Idle. All 60 shared idle entries have native-human
carriers in the installed manifest. The player's DefaultSlot output temporarily
bypasses the sitting pose while that carrier is active. This changes one reflected
SlotName input on the current ABP_Player instance, after GetCurveValue synchronizes
pending animation evaluation. It restores the original name on release, exit,
menu, mesh/instance changes and errors, provided CSS still owns the value. Shared
montages and their ability tasks are unchanged. The postprocess path remains in
use for outfits that already provide CSSIdle inputs.

Rest selection does not insert a new animation instance into a running native
action. In particular, it does not
start the facial-performance adapter, whose reinitialization can interrupt
the main animation instance. Body poses keep their authored height and position;
there is no automatic adjustment to the stone or conversion of dances into
seated animations.

## Verification

The form-compatibility revision used for the live body/form checks has SHA-256
`d9df18ba210534341415c5fa9d1b9b4f2f52608feb58c2a0116c415683a17531`.
Its production build and all 28 host suites pass. `forms-live-02/result.json`
verifies all 62 choices on native Genessa, including exact active carrier asset
paths and restoration of DefaultSlot during native Stand. Steam and the game
closed during the following Harbinger exit, so that interrupted exit is not
counted. The restarted `forms-live-03` batch completes four choices and native
sit/stand on 16 additional bodies: all eight Harbinger variants (including Dark
Bro), Lazlo, Tiel, Smert, Harros, both Unholy Genessa variants, Commander White
and Peony. The wider body sweep stopped after the user's timing complaint;
Eredrim's partial case and the unvisited bodies are not counted as live passes.
Settings were restored, and testing narrowed to actual gameplay forms.

`gameplay-forms-02/result.json` completes six cases with Default, Seated Recline
and Iris Out: actual Lazlo with native body and Peony, actual Harbinger with
mirror off and mirrored Peony, and actual Stray Genessa with mirror off and
mirrored Peony. Each verifies the active asset and native exit. Lazlo uses its
specific Loop and Stand; Harbinger and Stray use the common mappings. The batch
returns to Faithful Genessa and reloads the original profile successfully.

The final shipping DLL adds only the search-field focus fix to that tested
runtime, SHA-256 `00e8bc5fdbd17939400d08d23df2a7cab169f3f760abd55e4d55bae7d3646e49`.
EditableText normally clears keyboard focus on Enter before CSS polls the key.
CSS text inputs now retain it. `ui-final-05/result.json` verifies native keyboard
navigation to Vista, filtering for `iris`, Enter selecting Iris Out, unchanged
normal Idle selection, and profile restoration. `ui-final-selected.png` shows
the selected result and unobstructed search hint at 2560x1440. The normal restart
and unchanged settings are recorded in `install-search-01`.

The user authorized removing nearby enemies: eight non-boss Team.Hostile actors
within 50 metres of the Vista were killed through their native health component,
with dead-state readback in `vista-enemies-cleared.json`. The same local group
respawned after restart and was cleared again in `vista-enemies-cleared-03.json`.
NPCs were excluded.

`audit_vista_sets.py` follows every CSAnimSet/CSAnimSubset dependency from the 25
player character definitions and common animation sets. Its 41-package closure
has no unresolved set references. All Vista mappings live in
`AS_Player_Interactions`: common fallback plus Lazlo
(`CharacterId.Player.Shell.Necrophage.Default`), each with Start, Loop and End.
The predicate accepts both Loop paths and rejects all four transition paths.
`vista-mapping-audit.json`, `vista-assets.json`, `player-assets.json` and
`human-skeleton.json` retain the decoded evidence from the installed game.

Source ordering was checked against the matching local UE 5.6.1
`AnimNode_Slot.cpp`, `AnimInstanceProxy.cpp`, and `AnimInstance.h`: a slot without
a matching montage passes its source pose, and GetProxyOnAnyThread on the game
thread drains the component's pending parallel evaluation. No node structure or
pose link is copied or rewritten.

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

## Final local delivery

The runtime archive is `dist/v1.0.1/MSII-CSS-v1.0.1.zip`, built from annotated
tag `v1.0.1` at `2e672498ad7c52c60675ede14cb1ac5d3058d52c`. Its SHA-256 is
`bda1335f6d766e653351911fcfe990116d05eca72f33e9cba18169968a115a68`.
The installed loader and core match that archive. The save loads with Peony;
position/camera are restored and CSS state remains byte-identical. Evidence is
`work/sitting-animation/final-install-confirmed/` and
`work/sitting-animation/install-release-01/receipt.json`.

An earlier automation timeout was incorrectly described as a loading failure.
The user reported that loading had finished before the agent closed the game.
During the final launch, shader compilation was observed on screen and allowed
to finish. The native menu functions were used after a black main-menu screen,
and the loaded world was checked visually. This records successful save loading,
not a claim that the black menu screen was diagnosed or repaired.

`dist/DELIVERY.md` links all five packages and their BBCode changelogs. The two
user-provided Vista images are preserved in `dist/v1.0.1/media/`. At the user's
subsequent request, both were uploaded on branch `docs/vista-rest-1.0.1-media`.
The BBCode uses verified raw image URLs pinned to image commit `9b85d80`, placed
directly beneath the Vista feature description. Both responses are HTTP 200 and
byte-identical to the supplied images. Release ZIPs remain local.
