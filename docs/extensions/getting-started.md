# Build a CSSX extension

This is the development API for CSS 0.3.0. The runtime and packaging checks work locally; gameplay integration and the Cheat Menu port are still being tested. Do not treat this as a released compatibility contract yet.

CSSX owns the menu, input navigation, settings, logs and exported files. An extension supplies its manifest, controls and behavior. It does not need a separate UE4SS menu or its own filesystem code.

## Start with the Lua example

Copy [the counter example](../../examples/extensions/lua-counter) into a new authoring directory. Set a unique lowercase `id`, title, author and version in `extension.json`. Use an ID such as `author.mod-name`; keep it unchanged across releases so existing settings remain associated with the extension.

The example targets the Lua 5.4.9 interpreter bundled with CSSX. It shows:

- A numeric control whose value persists across sessions.
- Structured messages in the extension's own rotated log.
- A button that exports a text file to the managed output directory.
- A separate menu definition that can reload without restarting gameplay code.

`main.lua` returns a table with `model` and `event` functions. `start`, `tick` and `stop` are optional. `cssx.request` returns a result, or `nil, error`. Successful operations can also return `nil`, so check the second return value. Use `cssx.array()` for an empty JSON array; ordinary empty Lua tables represent JSON objects.

Menu controls have stable IDs. The `model` callback supplies `values`, `options`, `enabled` and a status message. Events contain the control ID and its new value when applicable. The renderer handles spacing and keyboard/controller focus. Confirmed actions also receive `confirmed = true`; check that in behavior code before irreversible operations.

## Write JSON or Pkl

Authors can edit `menu.json` directly. [The Pkl definition](../../tools/extensions/pkl/CSSX.pkl) is optional and uses Pkl 0.32.1. Pkl runs during authoring or packaging, never inside the game.

With mise:

```sh
mise install pkl@0.32.1
mise exec pkl@0.32.1 -- pkl eval --format=json \
  examples/extensions/lua-counter/menu.pkl \
  --output-path examples/extensions/lua-counter/menu.json
```

CSSX checks menu changes once per second. A valid edit replaces the displayed definition; a rejected or temporarily missing file preserves the previous menu. Errors go to the extension log. This reload path changes the UI definition only.

## Package

Build the native validator, which uses the same manifest and menu rules as the runtime. On a Linux authoring host with CMake, Ninja, a C++23 compiler and OpenSSL development files:

```sh
cmake -S native -B build/cssx-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cssx-host --target cssx_validate
python3 tools/cssx_package.py examples/extensions/lua-counter \
  --name 'CSSX_{name}_{author}_v{version}'
```

To evaluate Pkl as part of the package build:

```sh
mise exec pkl@0.32.1 -- python3 tools/cssx_package.py \
  examples/extensions/lua-counter --compile-pkl
```

Use `--validator` to select a validator built elsewhere, including `cssx_validate.exe` on Windows. The Python tool targets Python 3.14. The optional `--pkl` argument selects a Pkl executable outside PATH.

The packager includes `extension.json`, the manifest's entry/menu/banner files, explicit additional `files`, and any `LICENSE`, `LICENSE.txt` or `CREDITS.txt`. It does not recursively copy the authoring directory. Generated state, logs, caches and build directories cannot be packaged. Native entry points must be x64 Windows DLLs. Packaging validates data without executing the extension.

The ZIP contains one folder named after the manifest ID. Extract that folder into:

```text
ue4ss/Mods/CustomShellSystem/extensions/
  author.mod-name/
    extension.json
    main.lua
    menu.json
```

CSSX itself is a separate installation inside `CustomShellSystem`. Do not bundle CSS, CSSX or UE4SS with your extension.

## Native C++

The versioned ABI is [api.h](../../native/include/cssx/api.h). Export `cssx_get_extension` and return a `CssxExtension` table. The host's request callback uses the same JSON operations as Lua. No STL object ownership crosses the DLL boundary.

Copy response data before the callback returns. Catch exceptions inside exported callbacks. Resolve current objects through host operations instead of retaining raw Unreal pointers. Keep gameplay changes passive until selected. Restore reversible changes in `stop`; returning zero prevents unloading if cleanup failed.

Native extensions are trusted game-process code. The framework can report callback failures, but it cannot contain an access violation inside a third-party DLL.

If startup or a tick fails, CSSX suspends the extension and calls `stop` immediately. Make cleanup repeatable: return zero (Lua: `false`) if it cannot finish, retain the information needed to restore your changes, and let CSSX retry during shutdown. Failed cleanup blocks unloading. A suspended extension never receives another tick. Do not rely on unloading the DLL to undo values already written into the game.

### Reflected data and game updates

Use `describe` to check a required function before the first gameplay mutation. Missing or incompatible interfaces should disable the affected action with an explanation, rather than stop the whole extension or CSS. Do not replace a previously supported game adapter when adding another build. The [game compatibility record](../development/game-build-compatibility.md) tracks the native adapters separately from outfit and extension formats.

`get` returns localized `FText` as a UTF-8 string using the game's text conversion. Map reads return `{"$map":[{"key":...,"value":...}]}` so object and struct keys retain their type. Maps use reflected key/value layouts, check sparse indices and reject storage larger than 4,096 slots. Whole-map replacement is not supported. See Epic's [map key](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/FScriptMapHelper/GetKeyPtr) and [value](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/FScriptMapHelper/GetValuePtr) interfaces for the engine's pair layout.

Calls return all reflected outputs by default. An optional `outputs` array selects which outputs to decode, for example `"outputs":["Completed"]`. Use `"outputs":[]` when a function returns an opaque soft reference that your extension does not need. Output names are validated before the call. This does not skip input validation or change the function's behavior. Never retry a gameplay mutation merely because reading its result failed; the mutation may already have happened.

To update an existing map entry, send `map.update` with `target`, `property`, `key`, `expected` and `value`. Supply `key` exactly as returned by `get`, including object-handle metadata. `expected` must match the complete current value. The host encodes and decodes the replacement in temporary storage before committing, rejects missing or ambiguous keys, and never inserts, removes or rewrites keys. Struct values can specify just the fields to change; other fields are copied from the current entry. Default objects and archetypes cannot be edited.

This check applies to one map entry in one synchronous game-thread call. It is not a transaction across several objects or game functions. The Cheat Menu prepares its complete level edit first, restores earlier entries with another expected-value check if a later map write fails, and reports a partial update if a game-owned equipped-item refresh fails. It does not silently retry a mutation.

Host responses have a 1 MiB limit, including all callback chunks. CSSX rejects overflow instead of accepting a truncated JSON prefix.
