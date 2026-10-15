# Shade chooser and direct facial expressions

The user's final correction is explicit: no Choose expression button; show the
expression list directly. Shell shades need Choose shade for controller use.

Implemented in `native/src/inventory_view.inl`. The expression row lists available
expressions immediately and cycles with the existing left/right bindings.
Accept restores Original. The redundant expression picker handler is removed.
Use Original Shell now offers Choose shade through the secondary action. Its
list starts on the worn shade, includes Original and unlocked shades for that
shell, and uses the existing picker selection/cancellation route. Core selection
still validates native unlock requirements independently.

## Installed evidence

Receipts: `work/three-outfit-rig-physics/shade-picker-20261015/`.

- C++23 development and shipping core builds passed. Installed development SHA256:
  `60c97f59159a24b04210ef5b78f29ee7e9000179a847a5ff46f959eaae59a4c8`.
- `activation/result.json`: stationary reload passed with new DLL mapped and old
  DLL unmapped, exact settings, equipment flags, geometry, resources and CINE
  model restored. Developer mode remains enabled.
- `ui/expressions-direct.json` and `ui/unholy-genessa.jpg`: all 22 expression
  actions exist directly, with no picker or Choose expression action. D/A cycle,
  and Space restores Original. Screenshot was visually inspected.
- `shades/result.json`: F opens Choose shade on Obsidian Eredrim; W changes the
  highlighted choice; Escape cancels without changing the appearance. Selecting
  Original works, reopening starts on Original, and selecting Obsidian restores
  the original appearance. Screenshot was visually inspected.
- Live bindings include secondary = Gamepad_FaceButton_Left, up/down = D-pad,
  accept = Gamepad_FaceButton_Bottom and back = Gamepad_FaceButton_Right. Expressions
  use D-pad left/right. The automated checks used their keyboard equivalents;
  physical controller input was not exercised.

The UI assertions pass, but the two capture scripts report overall failure:
their exact geometry guard detected approximately 4.9 and 3.6 Unreal units of
position change after menu close. The first also detected a small rotation
change. No position/camera correction was forced. This is retained evidence,
not a geometry-restoration pass. Separate postchecks confirm exact saved CSS
state and original appearance restored, menu closed, keys released, and desktop
focus restored. The first capture's temporary UG settings were explicitly
restored through normal customization before the separate shade check.

The earlier incorrect expression-picker capture used an internal UI action as
a top-level command, producing Unknown CSS command. That screenshot was rejected.
The subsequent actual picker worked, but the user explicitly requested removing
that extra step. No previous screenshot is evidence for the current UI.

## Scope

No outfit assets, physics or save unlocks changed. Stable delivery remains gated
by `docs/development/three-outfit-release-gates.md`. This UI correction does not
complete the broader release goal.

## Follow-up: clipped shell list

The user supplied a Harros screenshot with earlier shell entries clipped and
an embedded Shade section beneath the shell list. Both calls to `choice_rows`
wrote `panel_focus`; the second call targeted Original shade instead of Harros.
The new chooser did not remove this older embedded list, so the initial fix was
incomplete.

The first follow-up removes the embedded shade rows and their extra note. Its
Harros screenshot then exposed a second issue: automatic centering still cut off
Tiel at the top, and the scrollbar was hidden. The final correction uses
IntoView rather than Center for a newly opened shell list, and makes its
scrollbar visible. Other panels retain their existing scroll behavior.

Final receipts: `work/three-outfit-rig-physics/shade-list-scroll-20261015/`.
Both core builds pass. Installed development SHA256:
`cd942c57df307581f31312c17436014154c596389f966ff9a203165f1528875b`.
Guarded activation passes exact settings, equipment flags, geometry, resources
and CINE restoration. `ui/result.json` verifies ten base-shell choices, no
embedded shade variants, Choose shade still present, and exact restoration.
The screenshot was inspected: Tiel is fully visible, Harros highlighted, and
the scrollbar indicates the remaining entries. The list contains ten base
shells; the earlier eight-shell count referred to shells with added shades.

Earlier receipts remain in `shade-list-20261015/`. One preflight stopped on
movement, and two stopped because Iris Out facial playback remained active.
No DLL mutation occurred in those failed preflights. After the user's explicit
idle-window response, the normal performance stop and temporary Original idle
allowed safe activation. The first capture's incorrect eight-base-shell
assertion failed with full restoration; the corrected ten-shell capture passed.
`restoration.json` in the final directory confirms the user's CW appearance,
Iris Out selection, exact pre-window settings and closed menu. No gameplay or
camera input was sent by the Harros captures.
