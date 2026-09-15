# CSSX host API 1

All requests are synchronous JSON on the game thread. Lua calls `cssx.request(table)`, receiving `(result, error)`. Check the error return even if result is nil. C++ calls `CssxHost.request`, or uses [cssx::Client](../../native/include/cssx/client.hpp). A nonzero C return is success; response chunks together form one JSON value. Copy them before returning from the sink.

The canonical implementation is [extension_runtime.cpp](../../native/src/extension_runtime.cpp), [extension_engine.inl](../../native/src/extension_engine.inl), and [extension_hooks.inl](../../native/src/extension_hooks.inl). This API is a checked bridge to the installed game, not an exhaustive gameplay SDK. Discover and validate the actual reflected function before calling it.

## Managed services

| `op` | Other request fields | Result |
| --- | --- | --- |
| `state.load` | None | Your saved JSON object; creates `{}` if missing, recovers a valid backup when available |
| `state.save` | `value`: object | `{saved: true}` after atomic save |
| `log` | `message`, optional `level`, `fields` object | null |
| `output.write` | `file`: relative path, `text`: UTF-8 contents | Absolute output path |
| `asset` | `file`: relative path within your extension | Absolute existing asset path |
| `invalidate` | None | null; marks your menu model for refresh |

You do not supply an extension ID for these operations. The runtime provides the caller's namespace. Save a schema/version field in your own state and migrate it explicitly. Saving settings does not apply gameplay changes for you.

```lua
local saved, err = cssx.request {op = "state.load"}
if err then error(err) end
local result, save_error = cssx.request {
    op = "state.save", value = {schema = 1, strength = 50}
}
if save_error then error(save_error) end
cssx.request {op = "log", level = "info", message = "Settings saved", fields = {strength = 50}}
```

Paths are under `CustomShellSystem/`: `state/extensions/<id>.json`, `logs/extensions/<id>/current.jsonl`, and `output/extensions/<id>/`. Framework errors go to `logs/cssx.jsonl`. Logs use UTC JSONL, rotate at 1 MiB and keep five backups. JSONL is a record format; rotation bounds disk use. Log state transitions and actionable errors rather than every tick.

## Objects and reflection

| `op` | Request fields | Result |
| --- | --- | --- |
| `player` | None | `pawn`, `controller`, gameplay `shell`, player `revision` |
| `valid` | `target`: handle | Boolean weak-handle validity |
| `find` | `path`: full Unreal object path | Existing object handle or null |
| `load` | `path` | Loads an asset and returns its handle; can stall on first load |
| `class_default` | `class`: short class name | Unique class default object or null; ambiguous matches are rejected |
| `get` | `target`, `property` | Decoded reflected value |
| `set` | `target`, `property`, `value` | Checked property write |
| `properties` | `target`, optional `inherited` | Property names, sizes and array dimensions |
| `describe` | `target`, `function` | Named reflected parameters, sizes and enum metadata |
| `call` | `target`, `function`, `args`, optional `outputs` | Named output values, including `ReturnValue` if present |
| `table.rows` | `target`: DataTable handle | Row names |
| `map.update` | `target`, `property`, `key`, `expected`, `value` | Updates one existing entry only if its complete value still matches |

Object handles are opaque JSON objects containing `$object`. Retain the whole value when passing it back. They wrap weak references and can expire after travel or destruction. Validity does not establish player ownership; check possession, ability avatar and current membership before changing gameplay.

`call.args` may be a parameter-name object or an ordered array. Prefer named arguments after inspecting `describe`. The raw result contains all reflected outputs. Do not accidentally use the first const-reference argument as the return value. `cssx::Client::call` unwraps `ReturnValue` when present.

```lua
local player, err = cssx.request {op = "player"}
if err then error(err) end
if not player.pawn then return end
local damageable, read_error = cssx.request {
    op = "get", target = player.pawn, property = "bCanBeDamaged"
}
if read_error then error(read_error) end
```

Maps decode as `{"$map":[{"key":...,"value":...}]}`. Preserve typed keys. `map.update` cannot insert or remove entries. It compares `expected` against the current value, encodes a replacement in temporary storage, then commits one entry. It is not a transaction spanning several game calls. Keep restoration records and check for another writer before rollback.

A `$table_field` value can forward a typed DataTable field into a compatible function parameter, for example `{"$table_field":{"table":handle,"row":"row-name","field":"ItemClass"}}`. The host resolves and validates the source again at the call. It does not reinterpret arbitrary bytes as a different type.

Missing properties, incompatible shapes, stale handles and unsupported reflected types return errors. Default objects and archetypes cannot be edited through property writes. A failed output decode can occur after a game function changed state; never retry a mutation blindly.

## Input and menus

- `input.focus`: true when the foreground window belongs to the game.
- `menu.status`: includes `menu_open`.
- `menu.close`: requests closure through the native menu handler; check status afterward.
- `input.keys`: `target` is the current player controller; `keys` is up to 64 distinct Unreal key names. Returns a name-to-boolean object.

These reads do not consume input. For gameplay shortcuts, require focus, an active player, no menu, no pause and unblocked input. Use release/press edges, not a held-key action every frame. Keep menu navigation in the UI Kit instead of duplicating it in your tick.

## Managed hooks

`hooks.status` reports availability and the caller's rules. An old CSS loader may report unavailable; explain that players need the complete matching CSS package and a restart.

`hooks.add` requires `target`, `pawn`, `controller`, `function` and `mode`. `mode: "bool"` requires a boolean return and `value`. `mode: "after"` requires an `after` function and matching `Owner` object parameters. Optional `seal` restricts a rule to an exact equipped item-definition class name. A successful call returns a rule ID for `hooks.remove`. `hooks.clear` removes the caller's rules.

CSS checks target ownership and possession when dispatching. The permanent loader owns the engine callback; it does not retain unloadable extension callback pointers. Stop must still restore your non-hook changes. Cleanup failure blocks unloading and must remain retryable.

See [the Cheat Menu implementation](../../extensions/cheat-menu/src/combat_tools.cpp) and [verification notes](../development/cheat-menu-parity.md) for measured uses. This is a restricted rule API, not arbitrary function interception.

## Budgets and update cadence

Requests and combined responses are limited to 1 MiB. State objects are limited to 1 MiB. An output write is limited to 8 MiB. Each callback has a 4,096 host-request cap; this is a safety ceiling, not a performance target. Extension ticks are coalesced to approximately 10 Hz. Static menu files are checked about once per second. Call invalidate only when displayed data changes.

Lua runs in its own interpreter with bounded allocation and instruction budgets. Filesystem and native-module access are not exposed; use managed services. Lua semantics follow the bundled [Lua 5.4 manual](https://www.lua.org/manual/5.4/manual.html). Native loading uses [LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw) with an absolute path and restricted dependency search. Native DLLs remain trusted in-process code.
