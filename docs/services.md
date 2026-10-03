# CSS services for other mods

CSS (1.0.0-beta.9 and newer) publishes services through the neutral CSSX service contract:
the core DLL exports `cssx_services()` (layout in `native/src/cssx_service.h`, a copy of
CSSX's `include/cssx/service.h`). CSS never calls out and does not need CSSX; a CSSX
extension reaches these with `service.call`. The table is withdrawn while no CSS core is
live, so a core left behind by a hot swap is never called.

## css.customize, version 1

Read and change the current look. Game thread only; refused while CSS's own tick is
running (for example from a hook CSS triggered), with "CSS is busy; call again from your
own tick".

- `{"action":"describe"}` → `{shell, outfit, variant, palette, customize,
  palettes:[{id,name}], controls:[{id,name,kind,group,role,scalar}]}`.
- `{"action":"apply","commands":[...],"persist":bool}` applies 1-256 commands atomically
  and returns the new `{palette, customize, ...}`; CSS puts it on the model on its next
  pass. Commands: `palette {palette}`, `control {control, channel, value | rgb | delta}`,
  `reset_control {control}`, `color` / `reset_color` (as on the CSS page), and
  `restore {customize}` (a snapshot from `describe`; values that do not fit the worn
  outfit are dropped).
- `persist:true` saves within a second, like a change on the CSS page. `persist:false`
  applies the look but holds the save back for 30 minutes, for transient looks such as a
  CINE take; restore the snapshot with `persist:true` when done.
- Choosing the `original` palette clears every custom value, as on the CSS page.

Errors: "CSS is not running", "CSS is busy; call again from your own tick", "css.customize
must be called on the game thread", "Invalid customize request", "No CSS appearance is
worn", and CSS's validation messages (unknown palette, control or channel, value out of
range).
