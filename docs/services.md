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

- `{"action":"describe"}` → `{shell, outfit, variant, palette, customize, busy,
  palettes:[{id,name}], controls:[{id,name,kind,group,role,scalar,nodes?}]}`. `nodes` is
  there for physics controls only (since 1.0.0-beta.10): the spring bones, or the dynamics
  chain root bones, that the control drives.
- `{"action":"outfits"}` → `{shell, outfits:[{id, name, variants:[{id,name}]}]}`: what the
  worn shell can wear (builds after 1.0.0-beta.11).
- `{"action":"wear","outfit":id,"variant":id,"customize":{...}?,"persist":bool}` wears an
  outfit through the same path as a choice on the CSS page and returns `{shell, outfit,
  variant, busy:true}`. With `customize` (a snapshot from `describe`) the look lands exactly
  as saved; without it CSS uses what it remembers for that outfit. It lands over the next
  ticks: `describe` reports `busy:true` until then, and the old outfit until it has landed
  (builds after 1.0.0-beta.11).
- `{"action":"idle","value":id}` plays that idle on every look worn from then on, where
  the look has it (`describe` lists a look's `idles:[{id,name}]`). It is never saved and
  changes nobody's own choice; `"value":""` ends it. For an extension that films several
  looks in a row and needs one continuous pose (builds after 1.0.0-beta.11).
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
