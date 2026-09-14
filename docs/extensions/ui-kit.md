# CSSX UI Kit

Development contract for CSS 0.3.0. The kit belongs to CSSX. The Cheat Menu uses it like any other extension.

Declare controls in `menu.json`, then supply their values and actions from C++ or Lua. CSSX renders the menu with Mortal Shell II's fonts, navigation textures and input glyphs. Extension authors do not copy the Cheat Menu's rendering code or ship another UI library.

## Components

| Type | Purpose | Required fields beyond `id`, `type`, `label` |
| --- | --- | --- |
| `button` | Run an action | None |
| `toggle` | Boolean setting | `value`: boolean |
| `number` | Numeric stepper | `value`, `min`, `max`, `step` |
| `slider` | Drag or step a numeric setting | `value`, `min`, `max`, `step` |
| `choice` | Cycle through named options | `value`: option ID, `options`: list of `{id, label}` |
| `radio` | Show mutually exclusive options together | Same as choice, up to eight options |
| `text` | Edit a short string | `value`: string, at most 256 UTF-8 bytes |
| `progress` | Read-only completion bar | `value`: number from zero to one |
| `loading` | Indeterminate work indicator | `value`: boolean, true while loading |
| `label` | Read-only information | None |

Each control also accepts `description`, `enabled` and `busy`. Disabled and busy controls use muted text and reduced opacity. Their rows remain selectable so players can read the description, but their actions and adjustment prompts are unavailable. Use `description` to explain why a control is disabled. A busy control cannot be activated. A button can include `confirm` with the text for the shared confirmation dialog. Radio labels are shown to players; their IDs stay in extension code.

The library supplies banner cards, a 3 by 3 grid, page navigation, empty states and unavailable-extension states. Menus share panels, headings, dividers, selected-row markers, section tabs, input prompts and entry transitions. Descriptions wrap inside a bounded scroll area. The secondary action opens a centered description dialog. Up / Down scrolls its body, and Back returns to settings. Confirmation uses the same frame with separate Cancel and Confirm buttons. Dialogs block the controls beneath them. Single-line labels and values use ellipsis rather than overlapping adjacent controls. The host keeps the selected list item visible, supplies page buttons and a position indicator, and accepts mouse-wheel scrolling in the full-width layout.

## Searchable options

A `choice` supports up to 512 `{id, label}` options. Confirm or **Browse options** opens a searchable picker. Left / Right on the setting still cycles options directly. The picker shows eight results at a time; Up / Down moves through them, the section bindings move eight results, and the mouse wheel scrolls. Confirm applies the highlighted option. Back cancels without changing the value. Empty results cannot be applied.

Search matches words against option labels and IDs without case sensitivity. CSSX caches normalized options when the picker opens and filters only when the query changes. Updating results preserves the text field and caret. Typing requires a keyboard; controller navigation and Back remain available while the search field has focus.

## A radio group

```json
{
  "id": "quality",
  "type": "radio",
  "label": "Quality",
  "description": "Choose the simulation quality.",
  "value": "balanced",
  "options": [
    {"id": "quiet", "label": "Quiet"},
    {"id": "balanced", "label": "Balanced"},
    {"id": "strong", "label": "Strong"}
  ]
}
```

Lua receives `{id = "quality", value = "strong"}`. A native extension receives the same JSON through its `event` callback. The runtime rejects unknown IDs, disabled or busy controls, invalid option IDs and values outside declared bounds before calling extension code. Confirmation is a UI safeguard, not a security boundary against trusted native code.

## Values and updates

With a static menu, return bindings from `model`:

```lua
model = function()
    return {
        values = {quality = selected_quality, progress = completed / total},
        enabled = {run = has_player},
        busy = {run = is_running},
        status = status_message
    }
end
```

Call `cssx.request {op = "invalidate"}` when those values change outside an event. Do not invalidate every frame. Native code uses the same `invalidate` request through `CssxHost` or `cssx::Client`.

Progress values represent actual work completed. Use `loading` when the total is unknown. Split long tasks across ticks so navigation remains responsive. The host does not invent progress or make blocking extension code asynchronous.

## Layout and input

Choose `layout: "tabs"` in the manifest for a wide settings panel, or `layout: "inventory"` for a panel beside the game's character display. Both use the same controls and callbacks. Layout coordinates scale with the native Inventory canvas height; long section lists use a moving window of tab titles.

The kit uses the game's remapped navigation bindings. Up / Down selects a setting. Left / Right adjusts numbers and options. Confirm activates buttons and toggles. The section bindings change tabs. Back dismisses a confirmation, returns to the library, then closes Inventory. Mouse users can select rows, click options and action buttons, drag sliders and use list page buttons.

A slider previews its value during a mouse drag and sends a snapped value when released. Keyboard and controller adjustments send one event per step. Text entry currently requires a keyboard; controller text entry is not implemented yet.

## Visual rules

The shared renderer, `native/src/extension_kit.inl`, owns the palette, type hierarchy, spacing and common drawing helpers. `extension_view.inl` assembles the components and routes input. `extension_controls.hpp` owns shared value formatting, stepping and event checks.

Settings use a left-aligned label and right-aligned value on the same row. The detail panel explains the selected setting and contains its actions. Input glyphs come from the game's prompt widget and follow the active input device. Decorative elements never become extra navigation stops.

## Working example

[The UI Kit gallery](../../examples/extensions/ui-kit) includes every control above, a 256-option search example, a long description dialog, a disabled action, a confirmation dialog and a five-second progress example. It makes no engine calls and changes no gameplay state. Package it with:

```sh
python3 tools/cssx_package.py examples/extensions/ui-kit --output dist/extensions
```

The [Pkl schema](../../tools/extensions/pkl/CSSX.pkl) also provides `RadioGroup`, `Slider`, `ProgressBar`, `Loading` and `TextInput`. Pkl is optional and runs during authoring, not in the game.

## Before the stable release

Controller text entry, broader viewport testing, controller focus checks, hover polish and reduced-motion preferences remain unfinished. There is no free-form layout DSL or arbitrary nested panel support yet. The gallery and this document describe the current implementation, not a promise that every UI pattern is supported.

The design follows the game's own Inventory presentation and Microsoft's guidance on consistent [UI navigation](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/112) and [keyboard focus](https://learn.microsoft.com/en-us/windows/apps/design/input/keyboard-interactions). Native widgets use Unreal's [UMG styling](https://dev.epicgames.com/documentation/unreal-engine/umg-styling-in-unreal-engine).
