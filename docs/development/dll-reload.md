# DLL reload support

Checked against the current sources during the CSSX FPS investigation on 2026-09-20.

| Change | Existing mechanism | Restart requirement |
| --- | --- | --- |
| CSS core DLL | `tools/css.py reload` builds a uniquely named DLL and changes `core.json`. The permanent loader detects it and replaces the core on the game thread. | No restart by design, subject to successful cleanup and the animation issue below. |
| CSSX runtime or native extension DLL | A whole CSS core replacement stops and destroys CSSX, then the new core discovers the selected runtime and extensions. Changing `cssx.json` alone is not watched. | No independent CSSX-only reload command exists yet. A whole-core reload or restart is currently needed. |
| Extension menu JSON | Each entry checks its menu file timestamp about once per second and validates a replacement before adopting it. | No restart or DLL reload. |
| Extension Lua source or manifest | Read during extension discovery/startup. | Same lifecycle as the CSSX runtime; menu JSON reload does not reload Lua logic. |
| Permanent loader, hook service or UE4SS binary | Loaded outside the replaceable CSS core. The build tool checks the installed loader contract. | Close the game before replacement, then restart. |

`native/src/loader.cpp` constructs a passive candidate before stopping the active core. A failed constructor retains the old core. A refused cleanup cancels replacement. After a successful stop, it destroys the old core and calls `FreeLibrary`. This is real DLL replacement, not simply changing the selector file. `runtime/loader.json` identifies the acknowledged core; process mappings confirm which DLLs are resident.

The current `Core::stop` also detaches Inventory, releases locomotion and restores the original appearance. The profiling reload on September 20 crashed in parallel animation asset ticking after replacement. The exact lifetime fault is unresolved. Therefore this incident does not establish that DLL reload is unsupported, but game-thread dispatch and successful cleanup acknowledgments alone do not prove animation workers are safe across replacement.

For the FPS comparison we used normal quits and restarts to ensure CSSX and its extensions were actually absent and to avoid repeating that crash. Routine menu editing should continue using the existing JSON reload. A CSSX-only stop/reload path should preserve the character and animation state, close any active extension UI, release owned hooks/widgets and refuse to unload on incomplete cleanup. That path is queued after FPS validation; it has not been implemented or tested yet.

On September 21 the official-shell selector and shared choice-list revisions
used normal restarts because the animation-worker lifetime fault is still
unresolved. These DLL-only changes do not inherently require restarting.
Installing the renamed mounted Eve containers did require a restart. Before
the next routine DLL replacement, explain which reason applies. Verify the
existing core cleanup against animation-worker lifetimes before calling hot
reload safe; a selector acknowledgement alone is insufficient.
