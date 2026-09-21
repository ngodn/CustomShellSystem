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
{target,function,args,outputs}`; `table.rows {target}`; `map.update
{target,property,key,expected,value}`; `input.keys {target,keys}`.

Host: `menu.status` → `{menu_open, game_menu_open}`; `menu.close`;
`input.focus`.

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
`confirmations`, `status`, `error`, and optional `notice` (`{"text","label",
"action"}`: one pending thing with the control id that resolves it; the menu
draws it as a strip on every section).

## Loader/core ABI 1 (`src/shared/core_abi.h`)

`dlls/main.dll` exposes `CssxLoaderHost` (root paths, log, hook host, frame
ring) and loads the `core.json` selection from `core/`. A core exports
`cssx_core_api` returning `create/tick/stop/destroy`. Core replacement is a
developer path; players restart.

## Settings schema 1 (`settings.json`)

`open_keyboard`, `open_gamepad` (1–4 Unreal key names each, chord),
`ui_scale` (0.75–1.5), `pause_while_open`, `hide_hud_while_open`,
`show_extension_status`. Unknown keys round-trip.
