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

## Next deployment

Install during the next normal game restart. The game still uses the accepted
package under its previous filename. Use the existing replacement installer
so it retires the prior package by stable ID instead of installing both:

```sh
python3 tools/css_package.py install \
  work/eve1/CSS_EveStellarBlade_eins0fx_P --replace
```

Before installation, back up the actual current state and core selector. The
installer preserves a verified backup of retired files inside the workspace.
After installation, verify exactly one loaded package for the stable ID, the
new SHELL label and Black Pearl variant, the same selected appearance and
colors, favorite status and saved-profile resolution. Preserve the user's
latest state, rather than copying an older test snapshot over it. Use a normal
restart; DLL hot reload caused earlier animation crashes in this session.

Remaining release scope includes the official-shell visual selector, expanded
locomotion slots and animation assets, broader lifecycle/gameplay coverage,
and final release packaging. The modding kit follows in `CSS-Modding`.
