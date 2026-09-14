# CSSX 0.3.0 implementation record

Status: in development. No release or gameplay validation yet.

## Requested deliverables

- Inventory / CSS / CSSX / Tarstones / Map navigation.
- CSSX library: responsive 3 by 3 banner cards with title, version, sliding pagination, keyboard/mouse and controller navigation.
- Native C++ and Lua extension API with shared CSS-style and tabbed menu layouts.
- Separate native CSSX Cheat Menu extension ported from MortalShell2Mod, preserving feature behavior and measured recovery safeguards.
- Three downloads: CSS 0.3.0 with optional CSSX detection; separate CSSX ZIP for installation inside CustomShellSystem (DLL under cores); separate CSSX Cheat Menu ZIP for CustomShellSystem/extensions. Each gets its own Nexus page. CSS works without CSSX.
- Fresh-install, Unicode-path, malformed-extension, lifecycle and existing-outfit compatibility checks. Screenshots, no video.

## Architecture

CSS keeps ownership of native Inventory integration and the character view. The CSSX core discovers extension manifests, owns their lifecycle and settings, and supplies declarative menu models. Each native extension has a versioned C ABI; UTF-8 JSON messages cross the boundary with caller-owned buffers/callbacks, without STL ownership crossing DLLs. Windows filesystem access uses native paths and LoadLibraryW. Lua uses an owned interpreter and the same host operations and menu model, not an automatic compatibility layer for arbitrary UE4SS scripts.

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

User also requested host-owned rotated logs and export directories. Planned paths: logs/cssx.jsonl, logs/extensions/<id>/current.jsonl and output/extensions/<id>/.

Naming: user selected CSSX (Custom Shell System Extensions) on 15 September. No CSSE ABI has shipped. The tab, core DLL and Lua API use CSSX. First extension: CSSX Cheat Menu.

## Native integration checkpoint

Renamed unpublished CSSE interfaces to CSSX. Windows css_core.dll and cssx_core.dll compile. The optional Inventory tab and initial library/control renderer are implemented but not yet visually tested. Game was closed at inspection. Existing CSS loader ABI remains unchanged.

Central storage now writes UTC JSONL logs with size rotation and per-extension output paths. Host tests cover escaping, rotation retention, Unicode filenames, traversal and Windows reserved output names.
