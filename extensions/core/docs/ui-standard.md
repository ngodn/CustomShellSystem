# CSSX UI and input standard

One menu for the framework and every extension. Extensions describe controls;
they never draw. This is the contract the menu implements (`src/core/menu.cpp`
for hosting, input and the model flow, `src/core/menu_page.cpp` for the page)
and the visual review checks against.

Since 1.2.0 the page is built from Mortal Shell II's own widget blueprints, the
way the CSS tab is, so both read as one family of game menus. CSSX still does
not depend on CSS: the game's widgets are loaded by path from the game's own
paks, and the page works with CSS absent.

## Structure

CSSX is a tab of the game's Player Menu (INVENTORY, TARSTONES, MAP, [CSS],
**CSSX**). The page is laid out in the game's own 3840x2160 design units inside
a ScaleBox, like the Inventory, so the native widgets keep their proportions at
every resolution. The side columns are 1136 wide, as the Inventory's are. The
Settings page's *Menu scale* (75% to 150%) scales the whole design canvas.

| Band | Design y | Content |
| --- | --- | --- |
| Header | 100–360 | The CSSX emblem (`assets/logo.png`, the user's artwork) and the page title in Trajan with its subtitle under it, like the game's optional list header. Library and Settings: "Extensions / CSSX x.y.z". Extension page: its title / "by author / version". |
| Tab strip | 390–540 | The Inventory filter-strip recipe: `T_UI_Nav_TitleBG` frame, a bumper prompt (`WBP_Prompt`) each side, and `WBP_NB_Menu` tabs styled from the Inventory's own filter tabs inside a clipped horizontal scroll with the Inventory's edge fade. The selected tab slides into view, so a long section list never truncates. Library page: LIBRARY and SETTINGS. Extension page: its sections. |
| List | 590 to the prompt bar | Change Shade rows (`WBP_NB_SkinOption`) under category headers (`WBP_Skin_Category`) in the Inventory's faded scroll with its scrollbar brush. Each row shows a name and a state word under it (the value, "Working...", or why it is unavailable); the game's E badge marks what is on or in use (an active extension, a toggle that is on); unavailable rows fade. |
| Details window | right column, from 150 | The Inventory details window (`WBP_Equipment_Description`): title, effect line as the sub header, description, then the editor and the prompt list. |
| Status line | under the window | Error text in red, otherwise the extension's `status`. |
| Prompt bar | bottom of the left column | Where the Inventory has "Esc Close / WASD Navigate": Close or Library, and Browse. |

The details window holds, per selected control:

| Control | Editor in the window | Prompts |
| --- | --- | --- |
| toggle | options-menu selector row (`WBP_NB_Option`) "State < On >" | Turn on / Turn off (Confirm), A D Adjust |
| number | selector row "Value < 12 >" | A D Adjust |
| slider | options-menu slider row (`WBP_NB_Option_Slider`), the bar draggable with the mouse, commits on release | A D Adjust |
| choice | selector row cycling the options; more than eight options add a searchable inline picker (search field, match count, every match as a list row, E on the current one) | Browse and search options / Next option, A D Adjust |
| radio | one list row per option, E on the chosen one | A D Adjust |
| text | the game's resource frame with an editable text field | Save text |
| button | | its label (Confirm) |
| label, progress, loading | description; progress and loading show their value as a plain row | |

Descriptions longer than about 360 characters move into the window's scrolling
part so all of it stays readable; `hint` is a muted paragraph under it;
`effect` is the sub header; a control with `confirm` says "Asks for
confirmation". The extension's `notice` (unapplied edits, cleanup) sits at the
top of the window on every section as a warning line plus its action, run by
the secondary key (F / X) or a click.

Confirmations use the game's own `WBP_ConfirmationPrompt_Default` with the
effect line above the message and glyphs on Confirm and Cancel; every live menu
input listener sleeps while it is up so Q/E cannot switch tabs underneath. The
dialog widget is created once and kept between confirmations.

Library: a "Framework" group with the CSSX entry (banner, open keys,
performance line, framework notices) and an "Extensions" group with one row per
extension (title; author and version, or the extension's status summary when
that setting is on). The window shows the selected extension's banner,
description, id, kind, API level and average tick cost, or why it is
unavailable.

## Input

Navigation uses the game's own menu actions read from its Enhanced Input
mapping context (`IA_Menu_Up/Down/Left/Right`, `Left/Right_Tertiary`,
`Confirm_Primary/Secondary`, `Back`), so remapped keys and controller glyphs
follow the game. Held Up/Down/Left/Right repeat after 400 ms at 90 ms. One
action per frame. While a text field has the keyboard, only Escape and the
controller act.

| Action | Library | Extension | Picker / dialog |
| --- | --- | --- | --- |
| Up/Down | move selection | move row | move result / focus |
| Left/Right | | adjust number, slider, choice, radio, toggle | focus Confirm / Cancel |
| Bumpers | Library / Settings | previous/next section | ±8 results |
| Confirm | open (the CSSX entry opens Settings) | activate button/toggle, browse, save text | select / confirm |
| Secondary | | run the notice action | |
| Back | close the Player Menu | library | cancel |

Mouse: every game widget's transparent navigation button is polled for a press
while a mouse button is down (never per frame otherwise); rows, tabs, arrows,
prompts, picker rows and dialog options click; a click on a prompt does what
its key does and plays the glyph's key flash; slider bars drag. The mouse
wheel scrolls the lists natively and walks the picker results.

Opening: the game's own Player Menu key, then bumper/Q/E to the CSSX tab; or
the hotkey chord (`settings.json`, default F6 or L3+R3), sampled at 30 Hz only
while a player controller exists. Pause, cursor, HUD and the menu input
context are the game's: CSSX never toggles them. Back on the library closes
the Player Menu the way CSS does (`HandleGameMenu(0, true)`).

## Rendering rules

Pooled game widgets. The skeleton (columns, strip, list scroll, details
window, prompt bar) is built once per layout size. Every list position keeps
one widget per kind it has ever shown and switches visibility, so a build
never creates, removes or reparents a widget once each shape has been seen.
A build runs immediate-mode page code that takes the next pooled widget of
its kind and writes only what differs (text, badge, state, opacity, glyph).
Creating a game widget costs about a millisecond, so a build creates at most
four and continues next frame.

The menu warms up while the Player Menu is open on another tab (the game is
paused, the page is not showing): skeleton, texture imports and the first
pooled rows. Imported textures are rooted for the menu's lifetime; the game's
blueprints drop their brushes on every reopen, and an unrooted texture would
be collected and imported again.

Every reopen of the Player Menu re-runs the game blueprints' Construct, which
restores their designer text, badges and box sizes. On activation the page
forgets what it believes is on screen and redoes the layout Construct undid.

Rebuilds happen on model revision, selection change, section change, picker
or dialog change, viewport or scale change and input-device change, never per
frame. Idle cost with the page open is the key poll (about 0.05 ms); with the
Player Menu closed the only per-frame cost is one cached `bOpen` read. The
menu records build count, microseconds per build and the worst build;
`frame.stats` and `menu.diagnostics` expose them.

Measured live (2026-09-27, Cheat Menu with 15 rows): a selection move
rebuilds in 0.1 to 0.6 ms; opening a section that creates new pooled kinds
1.5 to 5 ms once; the dialog 4 ms first, 2.4 ms after; the skeleton 30 ms once
per session (normally during warm-up). Before 1.2.0 every move rebuilt the
whole page from hand-drawn widgets in 12 to 30 ms.

## Authoring rules for extensions

- Describe every control; the description is the help text.
- Use `effect` on anything that touches the save: `persistent` for grants and
  unlocks, `irreversible` for bulk or destructive changes; pair with `confirm`.
- Use `severity: "danger"` only for irreversible actions.
- Use `hint` for the one thing the player needs to know before pressing.
- Keep `status` short: what is active now.
- Call `invalidate` when displayed data changed outside an event; never every tick.
