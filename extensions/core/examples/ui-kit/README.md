# CSSX UI Kit gallery

A working Lua example of the shared controls. It changes only its own example values.

Package it with `python3 tools/cssx_package.py examples/extensions/ui-kit --output dist/extensions`, then extract the ZIP into `CustomShellSystem/extensions`. It appears as **CSSX UI Kit** in the library. The technology preview has its own ZIP and library card. It is not bundled into CSS, CSSX or the Cheat Menu.

Open **Inputs** for toggles, radio groups, sliders, number steppers, option selectors and text input. Open **Feedback** for progress, loading, confirmation, disabled and information states. Run the five-second example to see progress updates without blocking navigation.

Change `layout` in `extension.json` from `tabs` to `inventory` to use the character layout. The same menu definition and action callbacks work in both layouts. Restart the extension host after changing a manifest; editing `menu.json` reloads its layout automatically.

Text entry currently requires a keyboard. All other examples use the game's menu bindings or the mouse. The gallery deliberately has no engine calls, unlocks or persistent gameplay effects.
