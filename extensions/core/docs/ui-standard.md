# CSSX UI and input standard

One menu for the framework and every extension. Extensions describe controls;
they never draw. This is the contract the menu implements (`src/core/menu.cpp`)
and the visual review checks against.

## Structure

CSSX is a tab of the game's Player Menu (INVENTORY, CSS, **CSSX**, TARSTONES,
MAP; CSS is absent when it is not installed). The page is the switcher area
under the game's top bar. Reference space 1920×1080 scaled to that area, then
by the user's `ui_scale`. The layout mirrors the CSS tab so both feel like one
family:

| Band | y (reference) | Content |
| --- | --- | --- |
| Artwork and title | 80–260 | `assets/banner.png` (the CSSX artwork) top-left, 400 wide; page title in Trajan beside it with the subtitle under it (library: "Extensions / CSSX 1.0.0"; extension: its title / by author / version). The notice strip sits top-right. |
| Tab strip | 270–322 | Left bumper glyph, tabs in Trajan capitals (selected bright with a gold underline), right bumper glyph, then a rule. Library page: LIBRARY and SETTINGS. Extension page: its sections. The strip measures itself and shrinks the face to fit the width. |
| Content | 340–936 | Left: list rows, half the width (library entries, settings rows, or the section's controls; 58 px rows, as many as fit, scroll bar and "n of m" under it). Right: full-height detail panel (selected entry or control: title, effect line, description, hint, editor, confirmation note). |
| Status line | 952 | Error text in red, otherwise the extension's `status`. |
| Footer | 1010 | Navigation hints on the left, contextual actions on the right. |

Overlays: option picker (search field, 8 results, count), full description,
confirmation (effect line, message, Cancel/Confirm). An overlay blocks input
to what is beneath it; Back closes it.

Framework pages (library, settings) carry the CSSX emblem (`assets/logo.png`)
left of the title; the library's "CSSX" entry shows `assets/banner.png`, the
open keys and the performance line. Extension pages carry the extension's
title and banner only.

Notice strip: an extension model may carry `notice` (`text`, `label`,
`action`). The extension page draws it at the top right under the title
(gold edge), clickable, and the `run` action activates that control by id
from any section, with the control's confirmation if it has one. The Cheat
Menu uses it for "N unapplied edits > Apply settings" and for cleanup.

Reserved bands: header (title, subtitle, divider at y=208 under the game's
top bar), status line (y=1080−128, error text in red, otherwise the
extension's `status`), footer hints (y=1080−70): navigation on the left,
contextual actions on the right. Nothing else draws in those bands.

## Input

Navigation uses the game's own menu actions read from its Enhanced Input
mapping context (`IA_Menu_Up/Down/Left/Right`, `Left/Right_Tertiary`,
`Confirm_Primary/Secondary`, `Back`), so remapped keys and controller glyphs
follow the game. Held Up/Down/Left/Right repeat after 400 ms at 90 ms. One
action per frame.

| Action | Library | Extension | Overlay |
| --- | --- | --- | --- |
| Up/Down | move selection | move row | scroll description / move result |
| Left/Right | | adjust number, slider, choice, radio | |
| Bumpers | | previous/next section | ±8 results |
| Confirm | open | activate button/toggle, browse choice, save text | select / confirm |
| Secondary | | full description | close description |
| Back | close menu | library | cancel |

Mouse: rows, section tabs, buttons, radio options, picker rows and modal
buttons are click targets; sliders drag and commit on release; the mouse
position is read once per frame and hover tests run only on a click. Text
fields take keyboard input directly (Slate).

Opening: the game's own Player Menu key, then bumper/Q/E to the CSSX tab; or
the hotkey chord (`settings.json`, default F6 or L3+R3), sampled at 30 Hz only
while a player controller exists, which opens the Player Menu through the
game's `HandleGameMenu` and selects the CSSX tab, or closes the menu when the
CSSX page is showing. Pause, cursor, HUD and the menu input context are the
game's: CSSX never toggles them. Back on the library closes the Player Menu
the way CSS does (`HandleGameMenu(0, true)`).

Left stick moves the selection as well as the D-pad (the page has no
character preview to rotate).

## Rendering rules

Retained widgets. A rebuild happens on model revision, selection change,
section change, overlay change, viewport change or input-device change, never
per frame. Until the game's key bindings are known (first ticks after the tab
opens) footer hints are drawn as text without a glyph, so the prompt widget's
default mouse icon never shows for a keyboard action. The menu records build count, widget count and microseconds per
build; `frame.stats` exposes them. While the CSSX tab is not showing, the page is not rebuilt and no input is polled; with the Player Menu closed the only per-frame cost is one `bOpen` read.

Unavailable controls stay selectable so the player can read why
(`disabled_label`, description). Long labels and values use ellipsis, never
overlap. Empty sections and empty libraries say so in words.

## Authoring rules for extensions

- Describe every control; the description is the help text.
- Use `effect` on anything that touches the save: `persistent` for grants and
  unlocks, `irreversible` for bulk or destructive changes; pair with `confirm`.
- Use `severity: "danger"` only for irreversible actions.
- Use `hint` for the one thing the player needs to know before pressing.
- Keep `status` short: what is active now.
- Call `invalidate` when displayed data changed outside an event; never every tick.
