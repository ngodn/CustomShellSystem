# Eve motion preservation in the installed shading candidate

The October 14 installed Eve candidate preserves all twelve payloads for these
six packages from the accepted October 12 motion build:

- `CR_Body`
- `CR_Hair`
- `CR_Hand`
- `ABP_Secondary`
- `Fit/ABP_BikiniFeet2`
- `Fit/ABP_KnitFeet1`

Evidence: `build/eve-surface-package1/motion-preservation.json`. The audit pins
48 retained files, including the current installed Eve trio, and checks their
hashes again before writing its result. The installed trio matches the combined
candidate's verified package hashes. No game files or settings changed.

## Body source and accepted limits

The editable `peony-v2-body-limits1` body rig still matches its authoring receipt,
fresh reload and Windows cook input. The cook completed with unchanged sources.
Its two cooked payloads match both the accepted motion assembly and the new
shading assembly. The saved body graph digest is unchanged by limit authoring.

The decoded limits are 6, 6, 10, 10, 3, 3 and 5 degrees, with travel limits
1.5, 1.5, 1, 1, 0.5, 0.5 and 0.5. The historical live capture has the same
limits and records the user's acceptance of Peony Firm/Natural while walking
and playing Iris Out. This is historical acceptance, not a new game test.

The first audit draft required exact equality between decoded decimal JSON
and reflected float values. The largest observed difference was
`5.372047418017445e-09` radians; travel values were exact. The comparison now
uses an absolute tolerance of `1e-7`, while separately checking the intended
angular limits against degrees converted to radians. A second draft used the
wrong fixture output filename, `response.json`; the retained response is
`graph-inputs.json`. Neither draft produced a success report or changed assets.

## Supporting graphs and preset evidence

The five supporting editable assets match both the saved namespace migration
and the input hashes of the earlier six-outfit preset fixture. That fixture's
response and checker hashes still match its 36-case, 20,196-frame result.
The body source used by the fixture is also the accepted tighter source.
See [the original test and its limits](2026-10-12-eve-existing-motion-regression.md).

The shipping `ABP_Secondary` remains the preserved compiled graph verified by
`eve-secondary-namespace1`. Its editor asset is a separately retained source;
this audit does not claim recompiling it reproduces the shipping bytecode.
The other supporting payloads still match `eve-rig-pack2`. A complete fresh
source-to-cook proof for those supporting graphs remains separate work.

## Reproduction and limits

Host Python is 3.14.7; mise does not pin Python in this project.

```sh
python3 tools/diagnostics/audit_eve_motion_preservation.py \
  --candidate build/eve-surface-package1 \
  --output build/eve-surface-package1/motion-preservation-recheck.json
```

Use a new output filename. The check is read-only except for its report and
requires the recorded candidate still to be installed. It does not inspect a
running game, prove current live bindings, or establish visual shading,
collision, lifecycle or performance acceptance. Those full-goal gates remain.
