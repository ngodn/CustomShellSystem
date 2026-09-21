# Eve package identity

The public package name is `CSS_EveStellarBlade_eins0fx_P`. The SHELL entry is
`Eve (Stellar Blade)` and its first variant is `Black Pearl`. Future outfits
belong under Eve as additional variants. The accepted color baseline is
colors4 with the colors2 core; user acceptance is recorded in
[authored colors](authored-color-swatches.md#user-acceptance).

## Saved-look compatibility

Keep the existing stable outfit ID `eins0fx.seduxtress` and variant ID
`black_pearl`. Saved selections, favorites, remembered customization and
profiles reference these IDs. Public labels and container filenames can change
without rewriting user state. Historical source paths and cooked
`/Game/CSS/SeduXtress/` references remain internal compatibility names.

This rename changes only the manifest/catalog display name and description,
container filenames and their declared manifest names. The mesh, materials,
physics, animation, thumbnail, dye resources, controls and palettes remain
identical to the accepted candidate. No recook is needed for a public label.

## Prepared candidate

`tools/authoring-probes/release/prepare_eve.py` produces the renamed trio from
the accepted colors4 trio. It checks portable paths, preserves every asset
payload, reverses its declared metadata edits to prove exact equivalence,
and verifies the finished package. Output:

- `work/eve1/CSS_EveStellarBlade_eins0fx_P/`
- `work/eve1/CSS_EveStellarBlade_eins0fx_P.zip`
- `work/eve1/verification.json`

The native package, cache repair and corruption tests pass. ZIP creation
verifies the source snapshot, member names, CRCs, extracted package and hashes.
The version remains `1.0.0-candidate`. This is a prepared candidate, not a
published release or a declaration that CSS v1.0.0 is ready.

## Installed candidate

The rename was installed with the official-shell feature using the normal
restart and replacement installer. The old stable-ID package was retired,
with verified workspace backups. `work/stock1/live/deployment.json` and
`install.log` record the operation; the subsequent reference correction keeps
the same package. Both installed cooked containers retain their accepted
hashes.

Steam screenshots confirm **Eve (Stellar Blade)**, **Black Pearl**, the equipped
marker and favorite star. The ten-shell trial restores the original Eve
selection and customization and verifies favorites and saved profiles remain
unchanged. Evidence: `work/stock1/check/result.json` and `restored.jpg`.

Remaining release scope includes the official-shell visual selector, expanded
locomotion slots and animation assets, broader lifecycle/gameplay coverage,
and final release packaging. The modding kit follows in `CSS-Modding`.
