# CSSX contracts: extension ABI 3, manifest schema 2, menu schema 2, loader/core ABI 1

Product version 1.0.0 is independent of every number below.

## Extension ABI 3 (`include/cssx/abi.h`, `include/cssx/hud.h`)

Native extensions export `cssx_get_extension` returning a `CssxExtension`
table. ABI 1 and 2 tables from CSS 0.3.x/0.4.x load unchanged: the host stamps
the `CssxHost` it hands over with the ABI the extension declared and never
reads past the size the extension declared. The manifest `api` must match the
binary's ABI (1↔1, 2↔2, 3↔3); a mismatch is rejected before `create`.

| Member | ABI | Contract |
| --- | --- | --- |
| `create(host)` | 1 | Passive. Copy the host table. Null on failure. |
| `tick(instance, seconds)` | 1 | Optional. Coalesced to ~10 Hz. 0 fails and suspends the extension. |
| `model(instance, sink, out)` | 1 | Menu model, or value bindings when the manifest names a `menu` file. |
| `event(instance, json)` | 1 | Validated control event. 0 fails the event; the model's `error` string is shown. |
| `stop(instance)` | 1 | Restore owned changes. 0 keeps the DLL loaded; retried later. |
| `destroy(instance)` | 1 | After a successful stop. |
| `render(instance, frame)` | 2 | Optional per-frame HUD update. Frame `abi` is stamped with the extension's ABI. |
| `status(instance, sink, out)` | 3 | Optional `{"summary": "...", "active": bool}` for the library card, read ≤ 1 Hz. |

Host table: `request` (JSON bridge), `hud` (ABI 2+), `log`, `invalidate`,
`now_us` (ABI 3+). `cssx::Client` (`client.hpp`) wraps all of this and uses the
direct services only when present.

Bounds: requests and responses ≤ 1 MiB; 4096 host requests per callback;
state ≤ 1 MiB; output writes ≤ 8 MiB; Lua memory 64 MiB, 200 budget units per
callback.

### Request operations

Managed: `state.load`, `state.save {value}`, `log {level,message,fields}`,
`output.write {file,text}`, `asset {file}`, `invalidate`, `extension.info`.

Engine (game thread, live reflection): `player` → `{pawn, controller, world,
shell}`; `valid {target}`; `find {path}`; `load {path}`; `class_default
{class}`; `get {target,property}`; `set {target,property,value}`; `properties
{target,inherited}`; `describe {target,function}`; `call
{target,function,args,outputs}`; `table.rows {target}` (sorted row names); `table.row
{target,row}` (one row's fields as an object, `null` when the row is missing; since CSSX 1.3.2); `map.update
{target,property,key,expected,value}`; `input.keys {target,keys}`.

Host: `menu.status` → `{menu_open, game_menu_open}`; `menu.close`;
`frame.brief {seconds?}` → `{hz, median_ms, frames, core_mean_us, core_max_us, core_p99_us}` (last ten
seconds; since 1.3.1 also `mean_ms, p95_ms, p99_ms, max_ms, hitches, seconds`, and `seconds` 1 to 60 picks the window);
`input.focus`.

Photo Mode (CSSX 1.3.2). An extension whose manifest has `"photo_mode": {"entry": "...", "tabs":
[{"section", "icon"}]}` gets a row under the game's Activate Photo Mode on the pause menu's System page.
The row opens the game's Photo Mode with the extension's menu sections added as tabs, built from the
game's own tab, page and row widgets. Text input is available in the Player Menu. Since 1.3.3,
Photo Mode also shows long read-only labels. CSSX measures the value with the original text block's font
and fits three periods into its laid-out column when the full text overflows. The model retains
the complete value, including after resizing or switching tabs. The extension hears
`{"id":"cssx.photo_mode","value":"open"|"parked"|"closed"}`. The owner can request two operations that run on the next tick.
`photo.step_out` closes Photo Mode the way the game's own exit does and parks the session (the
extension hears `parked`, not `closed`), so the player can move or the extension can use its own
camera; `photo.step_in {section?}` reopens Photo Mode into the parked session, on that section's tab.
Event ids starting with `cssx.` are reserved for the host.

Since CSSX 1.3.1: `CSSX_LAYER_SCREEN` as a layer parent puts the layer on the screen
itself instead of inside the game's HUD widget (it keeps drawing while the game HUD is
hidden); an older CSSX returns 0 for it. An object handle is accepted where a function or property expects an
interface (refused with "Object does not implement the interface" when it does not), an
interface value reads back as its object, and `set` takes `"echo": false` to skip
reading the property back (for structs too large to return).

Services (CSSX 1.3.0): `service.list` → `[{name, version, description, module}]`;
`service.call {service, version, request}` → the provider's reply. Any native module in
the game process can offer services by exporting `cssx_services()` (contract and rules in
`include/cssx/service.h`); CSSX knows no service or provider names. `version` is the
oldest format the caller understands (0 accepts any). Errors: "No service named X is
loaded", "X is version N; this caller needs M or newer", "Invalid service name", the
provider's own refusal text, and "X sent no reply / a reply that is not JSON / a reply
larger than 4 MiB". CSSX asks the provider for its table before every call, so a module
that hot-swaps or withdraws its table is never called stale; a missing service is
rescanned at most once a second. Calls run on the game thread, like every op; extensions
should treat a service as optional and keep working without it. The services a mod
offers are documented by that mod (CSS: `docs/services.md` in the CSS repository).

Hooks: `hooks.status`, `hooks.add {target,pawn,controller,function,mode,
value|after, seal}`, `hooks.remove {id}`, `hooks.clear`. A Blueprint-function
rule installs one global script-function interception for as long as any such
rule exists; the frame statistics report its call count and time.

Refused: `hud.minimap.*` (CSS-specific), unknown ops, default-object writes.

## Manifest schema 2 (`extension.json`)

```json
{"schema": 2, "api": 3, "id": "author.name", "title": "...", "author": "...",
 "version": "1.0.0", "description": "...", "kind": "native" | "lua",
 "entry": "name.dll" | "main.lua", "menu": "menu.json", "banner": "assets/x.png"}
```

`id` is a lowercase storage namespace (letters, digits, `.`, `_`, `-`, no
Windows reserved stems). Schema 1 manifests (`api` 1 or 2, `layout` field) are
accepted. Paths are relative, forward slashes, inside the folder.

### Adding tabs to another extension's Photo Mode (CSSX 1.3.3)

In a schema 2, API 3 manifest, name the extension that owns the mode and list one to eight
sections from your own `menu`. Bundle each PNG with your extension. Choose one target per
extension. A manifest can declare its own mode or contribute to another.

```json
{
  "photo_mode_contribution": {
    "target": "example.director",
    "min_version": "2.0.0",
    "tabs": [{"section": "poses", "icon": "tabs/poses.png"}]
  }
}
```

The owner supplies the pause entry. Its tabs follow the game's tabs. Contributors follow the
owner in extension-id order and keep their own manifest order. CSSX binds each tab to its
provider's model and sends row events to that provider. Section and control ids may therefore
match ones used by the owner. CSSX preserves the session when the game rebuilds its panel or
when your menu definition reloads.

Listen for `{"id":"cssx.photo_mode","value":"open"}` in your regular event callback.
The values below apply to contributors.

| Transition | Event values in delivery order |
| --- | --- |
| Join the owner's mode | `open` |
| Step out to gameplay | `step_out`, `parked` |
| Return to the parked session | `step_in`, `open` |
| Player closes the mode | `closed` |
| Runtime reload, player loss, parked session replaced, or contribution withdrawn | `detached` if the callback is still callable |

The owner and its attached contributors can request `photo.step_out` while open and
`photo.step_in` while parked. CSSX runs each request on the next tick. Pass `section` on
step-in to focus one of the caller's tabs. An omitted or unknown section opens the owner's
first tab. Restore session resources on `closed` and `detached`. Keep them across `parked`.

The optional `min_version` compares the target's three unsigned decimal components numerically.
A missing target, an unavailable mode or a version that fails the floor leaves the contribution
idle. CSSX writes one reason to its discovery log. An older CSSX ignores this optional manifest
field. Require CSSX 1.3.3 in your package if it depends on these tabs. The native ABI remains 3.

CSSX removes a contribution for the rest of the runtime after a Photo Mode callback fails or
takes more than 50 ms to return. A reload retries it. The owner keeps its tabs. ABI 3 calls
native code on the game thread, so time spent in a late call affects that frame. A native
DLL that crashes the process or never returns can still stop the game.

## Menu schema 2 (`menu.json` or the `model()` result)

Schema 1 controls unchanged: `button`, `toggle`, `number`, `slider`, `choice`
(≤512 options, searchable), `radio` (≤8), `text` (≤256 bytes), `progress`,
`loading`, `label`; per control `description`, `enabled`, `busy`,
`disabled_label`, `confirm`, `binding`.

Schema 2 adds, all optional: section `description`; control `hint` (one line
under the editor), `unit` (value suffix), `severity` (`info` | `warning` |
`danger`), `effect` (`reversible` | `persistent` | `irreversible`). The menu
colours danger actions, prints the effect above the description and repeats
it in the confirmation dialog.

Model bindings for a static menu: `values`, `options`, `enabled`, `busy`,
`confirmations`, `status`, `error`, optional `disabled` (id → one-line reason
shown instead of "Unavailable" while that control is disabled), and optional `notice` (`{"text","label",
"action"}`: one pending thing with the control id that resolves it; the menu
draws it as a strip on every section).

## Loader/core ABI 1 (`src/shared/core_abi.h`)

`dlls/main.dll` exposes `CssxLoaderHost` (root paths, log, hook host, frame
ring) and loads the `core.json` selection from `core/`. A core exports
`cssx_core_api` returning `create/tick/stop/destroy`. Core replacement is a
developer path; players restart.

## Settings schema 1 (`settings.json`)

`open_keyboard` (1–4 Unreal key names, chord; `open_gamepad` is retired and
dropped from an older file on save: the L3+R3 chord collided with the game's
own stick presses),
`ui_scale` (0.75–1.5), `pause_while_open`, `hide_hud_while_open`,
`show_extension_status`. Unknown keys round-trip.

### Ticking one component while Photo Mode is paused

CSSX 1.3.3 provides three JSON requests through the ABI 3 client. Pass the object handle of a live actor component such as a skeletal mesh. The scope covers that component's primary tick. Its ordinary tick registration, interval and other tick flags keep their existing values.

| Operation | Result |
| --- | --- |
| `component.pause_tick.read` | Current boolean value of `PrimaryComponentTick.bTickEvenWhenPaused`. Available outside Photo Mode too. |
| `component.pause_tick.acquire` | Enables paused ticking and returns the original boolean. An open owner or contributor can acquire during its `open` callback. Repeating the request returns the same original value. A component held by another extension yields an error. |
| `component.pause_tick.release` | Restores the saved bit and returns true if the caller held the component. Repeated release returns false. An expired component is forgotten by its weak handle. |

For example, send `{"op":"component.pause_tick.acquire","target":{"$object":42}}` with the handle obtained for your component. A successful result of `false` means paused ticking was off before acquisition. Release the handle when playback ends. CSSX also restores it on park, close, detach, world change, extension failure and unload. After stepping back in, acquire again from the next `open` callback if playback continues. A stale handle or incompatible component yields an error on read or acquire.

### Loading a set of assets asynchronously

With CSSX 1.3.3, an ABI 3 extension can submit soft object paths from gameplay or Photo Mode. Preparation needs a live world and local player controller. Submit once and keep the returned integer request handle.

```json
{"op":"asset.load_async","paths":["/Game/Example/Walk.Walk","/Game/Example/Look.Look"]}
```

CSSX queues the request immediately. It sends `cssx.asset_load` to the requesting extension's event callback when each path finishes. This example shows the final event for request 7.

```json
{"id":"cssx.asset_load","value":{"handle":7,"state":"complete","finished":2,"total":2,"results":{"/Game/Example/Walk.Walk":{"$object":101},"/Game/Example/Look.Look":{"$object":102}},"errors":{}}}
```

Use the returned object handles directly in bridge calls. Progress is the number of unique paths finished. Intermediate events use `progress`. The last event uses `complete` when all paths loaded, or `failed` when any failed. Results are cumulative. A failed path has a null result and a message in `errors`. Successfully loaded objects stay resident even after a partial failure.

Send `{"op":"asset.release","handle":7}` to cancel pending work or drop the objects when finished. The first release by the owner returns true. An unknown, expired or other extension's handle returns false. Release is safe inside the event callback. CSSX suppresses subsequent events. The engine may finish I/O already in flight. Release your own references if the objects should become collectible.

A handle survives step-out, park and return to the same Photo Mode session. Closing or detaching the participant's session, changing worlds, or unloading or suspending the extension releases its batches. Outside Photo Mode, release explicitly when finished. An extension may hold two batches, including completed batches, with eight across CSSX. Each submission accepts 1 to 32 absolute object paths and deduplicates repeated paths. CSSX starts or polls one path per engine frame across all callers, with up to two loads in flight. A path that has spent 120 seconds waiting for engine completion fails on the next poll.
