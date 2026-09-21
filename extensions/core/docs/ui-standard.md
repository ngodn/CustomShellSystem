# CSSX UI and input standard

One menu for the framework and every extension. Extensions describe controls;
they never draw. This is the contract the menu implements (`src/core/menu.cpp`)
and the visual review checks against.

## Structure

CSSX is a tab of the game's Player Menu (INVENTORY, CSS, **CSSX**, TARSTONES,
MAP; CSS is absent when it is not installed). The page is the switcher area
under the game's top bar. Reference space 1920×1080 scaled to that area, then
by the user's `ui_scale`. Three screens and three overlays:

| Screen | Left | Centre | Right |
| --- | --- | --- | --- |
| Library | List: title, author/version, status summary (green when active); "CSSX settings" as the last row | | Selected extension: banner, description, id, API, average tick cost; framework notice (migration, load errors) |
| Extension | Section rail | Control rows: label left, value right; 9 visible, scroll indicator | Detail: label, effect line, description (scroll), hint, editor, "asks for confirmation" |
| Settings | Rows: menu scale, status in library, open keys | | Explanation and adjust buttons |

Overlays: option picker (search field, 8 results, count), full description,
confirmation (effect line, message, Cancel/Confirm). An overlay blocks input
to what is beneath it; Back closes it.

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
per frame. The menu records build count, widget count and microseconds per
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
