# Migration and duplicate-host policy

## What "legacy CSSX" means on disk

The pre-1.0 CSSX lived inside CSS:

```text
ue4ss/Mods/CustomShellSystem/
  cssx.json                    selector: {"file": "cssx_core-<x>.dll"}   <- activation
  cores/cssx_core.dll          unversioned fallback                       <- activation
  cores/cssx_core-*.dll        versioned runtimes (inert without a selector)
  extensions/<id>/             extension folders
  state/extensions/<id>.json   extension settings (+ .bak)
  logs/cssx.jsonl, logs/extensions/<id>/
  output/extensions/<id>/
  assets/cssx-logo.png
```

The unchanged CSS v1.0.0-alpha.1 core checks, once per process,
`exists(cores/cssx_core.dll) || exists(cssx.json)` and then loads the selected
runtime and creates its CSSX Inventory tab. Only those two files activate it.
Everything else is data.

## Runtime detection (every CSSX start)

1. Files: does `Mods/CustomShellSystem/cssx.json` or `cores/cssx_core.dll`
   exist? Record as `legacy.activation_present`.
2. Modules: after Unreal init and on the first tick, enumerate loaded modules
   (`EnumProcessModules`) and record any module whose path is under
   `Mods/CustomShellSystem/cores/` and whose name starts with `cssx_core`.
   Record as `legacy.runtime_mapped`. Re-check once more after 30 s, because
   CSS starts its host lazily on its first tick.
3. Extensions: list ids present in both `Mods/CustomShellSystem/extensions/`
   and `Mods/CSSX/extensions/`. Record as `legacy.shared_extension_ids`.

`runtime/status.json` publishes the three fields and the menu shows a notice.

## Policy

| State | CSSX behaviour |
| --- | --- |
| No activation files, nothing mapped | Normal start. |
| Activation files present, nothing mapped yet | Start, but load **no** extensions until the second module check passes. Notice: "Legacy CSSX activation found in CustomShellSystem; run the migration with the game closed." |
| Legacy runtime mapped in this process | Do not load extensions, do not register hooks. UI opens only to show the diagnosis and the exact files to migrate. Never `FreeLibrary` the legacy DLL; CSS owns it. |
| Activation files gone, legacy runtime still mapped (deleted while running) | Same as above until restart. |

Exactly one host may own an extension folder. Extensions are loaded from
`Mods/CSSX/extensions/` only. A folder left in `CustomShellSystem/extensions/`
is inert once the activation files are gone, because CSS no longer starts a
host; the migration tool moves it anyway so there is one copy.

## Migration tool: `tools/cssx_migrate.py` (game closed)

`python3 extensions/core/tools/cssx_migrate.py --game <MortalShell2> [--dry-run]`

1. Refuses if a `MortalShell2-Win64-Shipping.exe` process exists.
2. Creates `Mods/CSSX/backup/<utc-stamp>/` and copies, byte-verified, every
   file it will move or delete, plus a `manifest.json` with hashes.
3. Moves `CustomShellSystem/extensions/<id>/` → `CSSX/extensions/<id>/` (skips
   ids already present in the target; reports them).
4. Moves `CustomShellSystem/state/extensions/<id>.json[.bak]` →
   `CSSX/state/<id>.json[.bak]` and `logs/extensions/<id>/` → `CSSX/logs/<id>/`,
   `output/extensions/<id>/` → `CSSX/output/<id>/`.
5. Deletes only `cssx.json` and `cores/cssx_core*.dll` in CustomShellSystem.
6. Leaves untouched: `core.json`, `cores/css_core*`, `dlls/main.dll`,
   `state/state.json`, `catalog/`, `assets/`, `enabled.txt`, every other mod.
7. Writes `Mods/CSSX/backup/<stamp>/restore.json`; `--restore <stamp>` puts
   every file back byte-for-byte and re-verifies.

Legacy `state/extensions/<id>.json` files keep the same schema as before, so
the ported Cheat Menu reads its saved preferences unchanged (it checks its own
`schema` field).

## Verification checklist for a migration

- After restart, `runtime/status.json`: `legacy.activation_present=false`,
  `legacy.runtime_mapped=[]`, extensions listed.
- `UE4SS.log` shows both `CustomShellSystem` and `CSSX` started from
  `enabled.txt`; `CSS.log` has no "CSSX startup" line.
- CSS Inventory shows the CSS tab and no CSSX tab.
- Restore path tested once on this machine before release.
