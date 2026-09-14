# CSSX 0.3.0 implementation record

Status: in development. First native and UI-kit live checks completed. No release yet. Later dated checkpoints supersede earlier pending notes.

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

## Declarative UI and packaging checkpoint

Pkl 0.32.1 installed through mise and the counter example evaluated successfully. Added canonical native validation plus a Python package builder with an explicit file list, Windows path checks, Unicode paths, x64 DLL header validation and reproducible ZIP output. The example ZIP is under dist/extensions and is not a release.

Lua now compiles as C++ so allocation failures unwind host JSON temporaries. Added empty-array support, protected argument marshalling, memory exhaustion and caught-loop checks. Static menu reload validates before replacing the current model; invalid and temporarily missing files retain the working view. Extension IDs are lowercase namespaces to prevent Windows path collisions. Confirmation dialogs block clicks on underlying controls.

Host tests cover Unicode settings/output, restart persistence, script isolation, static UI reload, archive contents and retaining an existing ZIP after a rejected build. Windows DLLs and validator compile. No new build has been deployed or visually validated yet.

## First live native checkpoint, 15 September

The game loaded CSSX and the native Cheat Menu and accepted the five-page navigation. The first native port covers god mode, auto heal, Resolve refill, health actions, movement preferences, shell switching, resource grants and fourteen controller unlock actions. The full original feature set is not ported yet. Cooldown/seal hooks, item and Tarstone tools, shell-specific effects and the exact prologue recovery predicate remain pending.

A live reversible test read bCanBeDamaged=true, enabled native God mode, read false, disabled it and read true again. This verifies that path on the current Genessa pawn. It does not validate combat, the other cheats or coexistence with the still-installed old cheat mod.

The loader now constructs a passive candidate before stopping the running core, so a constructor failure cannot leave the old core stopped. The game was closed for the permanent loader update; its contract and prior DLL were backed up. Current development cores still use ABI 1 and verified live reload. The staging tool now generates a unique CSS core filename even when only an extension DLL changed.

## Shared UI kit checkpoint

User clarified that all extension authors should use a reusable kit, including radio buttons, styling, panels, animation, icons, progress and loading states. Added the shared renderer primitives and control semantics, radio/slider/progress/loading schema types, the Pkl types, event validation and a Lua component gallery. The Cheat Menu uses the kit and native Inventory prompt glyphs.

Live screenshot work/cssx-native/cheat-menu-kit.jpg shows aligned labels and values, native section/button prompts, a list position indicator and detail panel. This is a development screenshot, not release media. Canvas enumeration initially hit the 64-child guard; CSSX's bounded canvas now uses a 256-child limit while native tab enumeration retains its tighter bound. Live construction recovered after that fix.

All nine portable CTest groups pass. Windows DLLs compile and are loaded in-game. Radio and slider provider events are accepted in the live Lua gallery. Automated mouse input was blocked by game window focus; user interaction feedback is pending. The gallery is installed as a separate development extension and must not be bundled with the player release.

User confirmed the gallery looks good and requested **CSSX UI Kit** as a separately packaged technology-preview extension. Its stable namespace is `cssx.ui-kit`, initial preview version 0.1.0. This adds a fourth ZIP alongside CSS, CSSX and the Cheat Menu. The gallery source stays in examples/extensions/ui-kit as a starting point for modders.

## Text input and layout fixes

The user's text-input report was reproduced by tools/cssx_ui_check.py: opening the text control detached CSSX because EditableText does not share TextBlock's Font property layout. The shared text component now uses GetFont/SetFont reflected frames, matching CSS Templates. The regression passes with a Unicode draft, a redraw and a saved-value read-back. Drafts survive redraws; action errors stay on the page.

Added single-line ellipsis for settings, values and buttons, clipped headings, a wrapped description ScrollBox and secondary-button detail scrolling. This cook exposes the WrapTextAt property but not its setter function, confirmed by a live describe/get probe. The kit sets the checked property before attaching the text widget. Rendering exceptions now show an extension error page instead of detaching all custom tabs.

CSSX UI Kit is installed with its stable ID cssx.ui-kit. Preview archive: dist/extensions/CSSX-UI-Kit-v0.1.0-preview.zip. Its generated banner lives in examples/extensions/ui-kit/assets/banner-v1.png. No preview archive has been published. Searchable option pickers are the next requested UI capability.
