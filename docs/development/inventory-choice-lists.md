# Choice lists and contextual helpers

The UI rule requested on 2026-09-21 applies across CSS, not only to the official
shell row. Keep browsing and Back helpers in the left footer, preview controls
in the center, and current-option helpers in the right footer. Labels and
keyboard/controller glyphs follow the current context; placement stays stable.
Modal dialogs keep their own controls inside the dialog.

The main page's `direction_hint` helper now owns its footer position. Callers
provide the direction and context label, not arbitrary coordinates. This covers
shells, outfit variants, templates, visibility, color swatches, scalar controls,
secondary-motion settings and locomotion. Clickable actions such as Wear and
Reset remain controls within their detail panel.

## Shared choice renderer

`choice_list` in `native/src/inventory_view.inl` serves official shells and
multi-variant outfits. New inline lists should reuse it. It provides:

- Fixed 48-unit rows with native styling and an equipped marker.
- A clipped ScrollBox with a scrollbar when its content exceeds the viewport.
- Stable scroll position across redraws within the same choice context.
- Automatic reveal when keyboard/controller selection changes.
- Independent left-list and right-list scroll state.

The existing pointer bounds restrict preview wheel zoom to the center region,
so a wheel event over the right list scrolls its content. No new global input
mapping or per-frame object scan was added. The list's temporary widget handles
are cleared on detach and extension-page transitions.

## Live verification

Both development and shipping builds pass. The installed development core is
`css_core-choices1.dll`, confirmed through the actual process map. CSSX stays
disabled. `work/stock1/ui-live/` contains the deployment receipt and backups.

`tools/authoring-probes/release/check_choice_ui.py` exercises wheel scrolling,
unchanged preview transforms, scroll preservation across a rebuild, clicking
the final shell, keyboard wraparound and revealing the selected option. The
second consumer was MoreBeauteGenessa's Original/Lite variant list. Steam
screenshots of shell choices, variants, Customize and Locomotion were reviewed
for helper alignment and readable rows.

The initial close driver clicked Back, then CSS closed before its mouse-up
request arrived. The guarded test hook rejected that request. The input was
released, the menu reopened and the unchanged saved state verified. The driver
now uses Escape because its key-up hook remains valid after CSS closes. A fresh
keyboard close/open check passed without restarting the game. Retain this
distinction: the initial failure was in the test driver, not a failed menu
teardown. Do not use its mouse-click helper to close CSS.

Evidence: `work/stock1/ui-check/result.json`, `reentry-key.jsonl`, before/after
state snapshots and screenshots. Eve's selection, custom values, favorites,
profiles and gameplay identity are preserved. Controller bindings share the
same selection path, but this automated run used mouse and keyboard, not a
physical controller. Broader release acceptance remains open.
