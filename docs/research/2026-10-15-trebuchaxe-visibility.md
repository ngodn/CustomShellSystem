# Trebuchaxe hovering axe

The hovering axe is `SM_Projectile_Preview`, an intentionally hidden component
of the native Trebuchaxe. CSS exposed it by setting every owned mesh's
`bHiddenInGame` to false when revealing the sidearm. This is a visibility defect,
not a detached mesh or a transform that needs correcting.

## Evidence

All receipts and recipes are under
`work/three-outfit-rig-physics/trebuchaxe-20261015/`.

- `read2/initial.json` identifies the equipped weapon and its owned components.
- `package.json` and `preview-template.json` read the shipped
  `WP_Trebuchaxe.uasset` using the current game's matching mappings. The preview
  template has `bHiddenInGame=true`.
- `aim-controls/result.json` records an A/B/A comparison during normal Ctrl
  aiming. The floating axe is present, disappears when only the preview is
  hidden, and returns when that flag is restored. The main weapon stays intact.
- The same capture hides only the owner actor while leaving all component flags
  false. Every owned mesh stops rendering. This establishes that hiding the
  owner can suppress these meshes without overwriting their individual flags.
- Those controls restore settings, CINE, resources, component flags, player/view
  geometry, released inputs and prior desktop focus.

The reported Tarstone combination has not yet been compared with an unequipped
loadout. The demonstrated CSS visibility error does not depend on changing a
Tarstone or a projectile transform.

## Repair and installation

`set_item_hidden` in `native/src/misc_visibility.inl` now changes only the direct
attachment component and its owner actor. Nested components retain the game's
individual visibility decisions. The existing owner-visibility drift repair is
preserved, including its role in suppressing effects on hidden sidearms.

Windows core build and all 26 host suites pass. Installed development core:

`5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`

`install-idle-paused/result.json` verifies the mapped replacement, unchanged
player/view, settings and CINE. `idle-pause-restoration.json` verifies that the
temporary native idle selection is reversed to the original `eve_idle_702` and
the entire saved CSS state matches. The initial installer stopped at its
active-animation guard without changing the core.

The old core's teardown revealed nested meshes once more. The installer restored
only the identified preview's authored hidden flag on the existing weapon after
replacement. Fresh weapon instances already have that native template value.
The old core is retained for rollback. No cooked assets, game saves, progression,
ammo, resources or transforms were modified by installation.

## Installed visibility checks

`fixed-visibility/result.json` passes 13 observed phases: three draw/stow cycles,
Default, Hidden and Only when in use while drawn or stowed, and an induced
owner-only visibility drift. The preview remains hidden and never renders. The
main weapon renders when shown; all owned meshes stop rendering when hidden.
Nested hidden/visible flags, including the independently hidden third projectile,
remain unchanged throughout. Captures show the floating axe gone.

The fixture restores full CSS state, CINE, resources, player/view geometry,
component flags, released inputs and prior desktop focus.

`equipped-drift-complete/result.json` also passes induced actor-only drift on
Trebuchaxe, Infinite Seal and Genessa's Catalyst. Default leaves the sidearm
shown, and returning to Only when in use restores the original hidden state.
Original flags, full saved state, player/view and process identity match after
cleanup. This checks the retained enforcement path on three current items; it
does not substitute for another sidearm's infusion test. Two earlier fixture
errors are retained: a local `inspect.py` shadowed Python's standard module
(renamed to `inspect-owned-components.py`), and the first copied drift recipe
still named Naylshotte in its Default-control lookup. Neither is a passing test;
the latter restored its injected flags and settings before stopping.

## Charged firing

`firing-resolve-samples/result.json` passes with the user's original Peony
`eve_idle_702` selected. The actual mouse press reaches `HasMaxCharge=true`;
release produces a recorded Resolve change from 100 to 70. It returns to 100
without diagnostic resource writes. The fully charged, post-shot and stowed
captures keep the preview hidden. The real weapon is visible while drawn and
all its meshes stop rendering after stow. Settings, CINE, health, player/view,
released inputs and prior desktop focus restore.

Two preceding failed fixtures are retained:

- `firing/result.json` reached maximum charge but did not observe expenditure.
  Its capture interval does not establish that no shot occurred.
- `firing-native-idle/result.json` failed because it calculated expenditure
  from the delayed value after Resolve had already recovered. Raw requests
  contain 100, 70, then 100. `independent-verification.json` records this
  distinction without changing the failed receipt. The temporary native idle
  restored to the user's saved selection with the entire CSS state matching.

The final fixture records the minimum sampled Resolve instead of using a
delayed final value. It passes on the original idle, so these observations do
not establish an idle-related firing defect.

Other sidearms, Tarstone combinations, uncharged firing, damage and a fresh
launch remain separate acceptance checks. The charged-shot pass does not close
that wider matrix.
