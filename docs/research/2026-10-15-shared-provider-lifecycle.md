# Shared Assets removal and reinstall in the game

The actual cold-start missing-provider cycle passes. CSS retains the user's
saved appearance, animation choices, customization and profiles when Shared
Assets is absent. Loading an unavailable profile reports the missing dependency
without partially applying its settings. Restoring the same shared package
recovers Commander White automatically, and the profile loads without resaving.

## Procedure and evidence

Evidence and the resumable recipe are in
`work/three-outfit-rig-physics/shared-provider-lifecycle-20261015/`.
`postcheck.json` records the recipe hash and final live checks.

- Installed development core: `faaf5794308ae8f482e7bd24e793aa1352cc6d8d1c884e8312bdeb88b2d3fe6d`.
- Initial Linux process: 478716. Missing-provider process: 643569.
  Restored-provider process: 645797. The expected core is mapped in each new game.
- Before the first quit, stationary/player-input checks pass. Fourteen existing
  save files are copied and hash-verified for recovery, along with CSS state.
  No backup is written back over a game save during the test.
- A temporary `css-migration-20261015` profile records the current CW appearance.
  Adding it changes only that profile entry in CSS state.
- Native QuitGame exits before any asset move. The complete shared trio moves
  by directory rename from `Content/Paks/~mods/CSS_SharedAssets_P` to
  `Content/CSS-provider-lifecycle-20261015/CSS_SharedAssets_P` on the same drive.
  Hashes match before and after both moves. No full asset copy is created.
- Each launch uses Steam and the existing save's normal ContinueGame action.
  No mounted package is moved, replaced or hot reloaded.

## Missing provider

The loaded pawn is Stray Genessa. Its actual mesh is the game's
`Sk_Sester_Genessa_V4_Corrupted`, and CSS reports no applied custom appearance.
The catalog rejects current Eve, Unholy Genessa and Commander White, each with
`Install CSS Shared Assets to use this package.` All saved CSS state, including
the temporary profile and every existing profile, remains equal to the baseline.

Loading the temporary profile reports:

> Profile outfit is unavailable: eins0fx.commanderwhite. Install the outfit and its required shared assets, then load the profile again.

The native mesh, shell and full saved state remain unchanged. This is actual
in-game rejection after removing the provider, beyond the earlier host fixture.

## Reinstall and restoration

After another normal quit, the trio returns to its original location with
identical hashes. The next launch and Continue recover CW and her exact
customization automatically. Loading the temporary profile succeeds, then its
deletion restores the entire original CSS state exactly.

The final independent read checks the expected core mapping, CW, customization,
advancing Iris Out playback, availability of every currently selected shared
animation and enabled CINE recording/capture controls. Both `recovered.json` and
`postcheck.json` pass. The game remains running with the original settings.

Two diagnostic assumptions failed and were corrected without product changes:
Customize's describe service rejects a missing outfit, so the missing-provider
check now inspects the native mesh directly. After a successful profile load,
the runtime message can already read `Wearing Original.` because the appearance
has applied; restoration is verified from actual state rather than requiring
the earlier transient `Profile loaded` text. The append-only request logs retain
both attempts. No production code or accepted mesh/rig changed in this test.

## Coverage limits

This closes physical missing-current-provider/reinstall recovery on the current
Stray save, including rejection of a dependent profile and restoration of its
availability. It does not stand in for a clean installation, mounting retired
duplicate providers, every schema migration, other saves/forms, CINE playback
or new visual/physics acceptance. Existing offline duplicate/legacy-provider
checks remain recorded separately. Native saving during QuitGame is normal;
the test does not assert byte-identical game saves across restarts or an
unchanged world position after Continue.
