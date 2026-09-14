# CSSE 0.3.0 implementation record

Status: in development. No release or gameplay validation yet.

## Requested deliverables

- Inventory / CSS / CSSE / Tarstones / Map navigation.
- CSSE library: responsive 3 by 3 banner cards with title, version, sliding pagination, keyboard/mouse and controller navigation.
- Native C++ and Lua extension API with shared CSS-style and tabbed menu layouts.
- Separate native CSSE Cheat Menu extension ported from MortalShell2Mod, preserving feature behavior and measured recovery safeguards.
- CSS 0.3.0 runtime ZIP including CSSE core under cores; Cheat Menu ZIP for CustomShellSystem/extensions.
- Fresh-install, Unicode-path, malformed-extension, lifecycle and existing-outfit compatibility checks. Screenshots, no video.

## Architecture

CSS keeps ownership of native Inventory integration and the character view. The CSSE core discovers extension manifests, owns their lifecycle and settings, and supplies declarative menu models. Each native extension has a versioned C ABI; UTF-8 JSON messages cross the boundary with caller-owned buffers/callbacks, without STL ownership crossing DLLs. Windows filesystem access uses native paths and LoadLibraryW. Lua uses an owned interpreter and the same host operations and menu model, not an automatic compatibility layer for arbitrary UE4SS scripts.

All engine requests run on the game thread. No extension callback may escape the host lifetime. Shutdown must restore reversible changes before unloading. An extension error is attributed to that extension. Native DLLs remain trusted code; this is not process isolation or crash containment.

## Port review

Source: ../MortalShell2Mod/Scripts/main.lua, ShellSwitch.lua, PrologueRecovery.lua. The original includes an MIT license by Matthew arvidson; retain it with the port.

Source modules extracted for review under work/csse-0.3.0/source-review (ignored build notes).

Preserve passive defaults, player identity checks, one shell request with read-back, and the nine-observation completed-prologue lock predicate. Do not clear generic gameplay/story tags to recover unrelated locks. Bulk unlocks retain explicit confirmation and achievement/save effects in their UI.

## Research

- UE4SS scripting/native capabilities: https://docs.ue4ss.com/
- Lua protected calls, interpreter lifecycle and loading: https://www.lua.org/manual/5.4/manual.html

## Pending

Framework and ABI; menu integration; full feature port; automated tests; live verification; screenshots; release packaging and documentation.

## UI authoring decision

User proposed a UI DSL on 15 September. Use a versioned declarative JSON UI contract with optional Pkl build-time authoring. C++/Lua provide state and actions; the runtime owns layout and navigation. Do not require players to install or run Pkl. Static layouts should reload independently from gameplay code.

Pkl currently lists Java, Kotlin, Swift and Go bindings, not C++/Lua. Its CLI exports JSON, so no native binding is needed for this workflow. Sources: https://pkl-lang.org/main/current/language-bindings.html and https://pkl-lang.org/main/current/pkl-cli/index.html . A schema, Pkl definitions, packaging adapter and example still need implementation and validation.

## Foundation checkpoint

Implemented manifest discovery, bounded menu validation, the C ABI, isolated Lua 5.4.9 interpreter lifecycle, per-extension state, native loading and 3 by 3 library navigation state. Host CTest checks pass. A real Lua interpreter test covers Unicode state paths, actions, persistence, runaway-script isolation and shutdown. Native game integration, Windows verification and the cheat port remain pending.

User also requested host-owned rotated logs and export directories. Planned paths: logs/csse.jsonl, logs/extensions/<id>/current.jsonl and output/extensions/<id>/.
