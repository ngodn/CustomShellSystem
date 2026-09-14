# CSSX extension contract (in development)

CSSX supplies shared Inventory integration, menu controls, settings, logs and game-thread callbacks. It uses UE4SS for engine access. Existing UE4SS scripts need a CSSX adapter; this does not automatically import arbitrary mod menus.

## Three downloads

CSS 0.3.0 adds optional CSSX detection. CSSX installs inside CustomShellSystem and supplies cores/cssx_core.dll. Extension folders install inside CustomShellSystem/extensions. CSS remains usable without CSSX.

## UI authoring

Declare sections, controls, action IDs and value bindings in menu.json. Pkl is an optional authoring tool that emits that JSON during packaging. Native C++ and Lua callbacks provide values, dynamic options, enabled state and status messages. The runtime supplies spacing, fonts, focus and input behavior. An inventory layout leaves room for the character; a tabs layout uses a wider control panel.

A menu definition has schema 1 and sections. Each control's ID is its default value binding and event ID. A model with values, options and enabled maps overrides the corresponding control fields. Authors may write JSON directly. The Pkl schema and Lua example are under tools/extensions/pkl and examples/extensions/lua-counter.

## Standard storage

All paths are relative to CustomShellSystem:

- logs/cssx.jsonl: framework diagnostics.
- logs/extensions/<id>/current.jsonl: extension diagnostics.
- state/extensions/<id>.json: settings, with backup recovery.
- output/extensions/<id>/: exported text or data.

The manifest ID owns the namespace. Titles and author display names never become directory names. Log records contain UTC time, severity, extension ID, message and structured fields. Each log rotates at 1 MiB and keeps five backups, current.jsonl.1 through current.jsonl.5. The framework log uses the same policy. No log or settings files belong in release ZIPs.

Lua calls cssx.request with op="log", level, message and optional fields. C++ uses the same request through CssxHost. Output uses op="output.write", file (a relative filename) and text. The host checks traversal, Windows reserved names and Unicode paths. Modders do not implement their own rotation or write into the game installation root.

## Lifecycle

Native callbacks use native/include/cssx/api.h. Requests and callbacks are synchronous on the game thread. Do not retain callback pointers, response buffers or engine pointers beyond their allowed lifetime. Native DLLs are trusted executable code; CSSX is not process isolation.

Start passively. Resolve the current player when needed. Restore reversible changes in stop. A failed stop prevents unloading. Lua runs in a separate interpreter with bounded memory and instruction budgets, without native modules or filesystem libraries. Use host storage and engine APIs instead.

## Research

- https://pkl-lang.org/main/current/pkl-cli/index.html
- https://pkl-lang.org/main/current/language-bindings.html
- https://www.lua.org/manual/5.4/manual.html
- https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw
