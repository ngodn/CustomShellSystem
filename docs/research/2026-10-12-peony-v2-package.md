# Peony V2 combined package

The private candidate is
`work/three-outfit-rig-physics/peony-v2-package2`. It is not installed and is
not an accepted gameplay build. The installed Peony refit2 remains rejected.

## Payload and metadata

`tools/eve-fit/assemble_peony_v2_trial.py` combines the accepted six-outfit
Eve payloads from `eve-final-container1`, shared payloads from
`shared-cloth-native2`, and verified V2 cooks from
`/mnt/eins0fxE/CSS-Development/peony-v2-character2` and `peony-v2-options1`.
Only the V2 namespace and shared trim function are added from those cooks.
Recooked editor stand-ins for accepted dependencies are excluded.

The output owns 435 Eve packages and 270 shared packages. Its metadata replaces
the rejected Peony variant with `peony-v2-customize1/variant.json`, updates the
Eve container revision on all relevant animation/facial adapters, and maps
Peony to `PeonyV2/Meshes/SK_PeonyV2_Face`. The other six outfit variants,
CW/Genessa adapters, shared animation entries and facial performance resources
are preserved. All 13 V2 dye resources match the accepted Eve resources.

The compact Eve manifest is 253,781 bytes, below the existing 256 KiB limit.
No manifest-size increase was needed. Its 78 controls use the previously
implemented 128-control capacity and authored palette settings.

## CSS facial namespace correction

The first assembly, `peony-v2-package1`, failed the native catalog test with
`Invalid or duplicate facial mesh adapter`. Its declared V2 target was valid,
but `facial_adapter_asset` required a literal `Faces/` directory immediately
inside the project. That rejected the shared library and its dependent outfits.

The helper now accepts explicit facial target paths anywhere inside a valid
`/Game/CSS/<author>/<project>/` namespace. Legacy facial paths remain supported.
This changes folder validation only. Catalog matching still requires the exact
source outfit, source mesh, TOC hash and bulk size. Runtime preparation still
checks the asset is a skeletal mesh, uses the declared skeleton and contains
the requested morphs (`engine.cpp`, facial replacement preparation).

Portable tests cover a second author, nested outfit geometry, project-root
meshes, malformed namespaces, non-CSS assets and path traversal. Wrong hash,
bulk size and source mesh continue to reject matching. The facial and animation
test targets pass, as does the C++23 Windows core build.

## Verification

- `assembly.json`: all protected inputs, asset ownership, payload hashes and
  output trio hashes; no installed files changed.
- `catalog.log`: 1,535 native animation/facial checks pass across all ten
  current variant fixtures.
- `imports.json`: 720 packages and 271 object references pass, zero failures.
- `roundtrip-exit.json`: successful IoStore readback; every staged export/bulk
  payload hash matches the independent readback.
- `core-verification.json`, `core-tests.log`, `recipes/`: frozen changed source,
  build/test results and private DLL identity.

Private core SHA-256:
`633ade14e84c297cfa4e341e281ed3c9664dc6ec2bf553d47ce36254540cf141`.

Eve TOC SHA-256:
`cdc466fb65eb58b7ded13d708721a13834852ac1b439b3cc2d39b230c6fff79d`.

Shared TOC SHA-256:
`86ac688cc843fea5a909bc38afa8da4f1cad29e82ecfd30066d45050f01cd468`.

## Remaining work

The existing core's authored Peony astral entries describe the retired material
parents, not V2. Author and verify companions for the five V2 garment parents
and stocking parent, then update the shared package and material catalog.
Preserve source texture/palette values, flowing trim and stocking coverage.
Use the existing shared ghost-authoring and compiled-uniform checks; do not
reuse the old parents by name or claim source material compatibility alone
proves doubles rendering.

After that, complete installation with the game closed and validate native
appearance, fit, garment motion, customization, profiles, facial playback,
doubles and gameplay. Static package success does not accept those behaviors.
Generated duplicate staging/readback data is removed under the October 12
cleanup record; original sources, candidate trios, recipes and evidence remain.

## Installed successor and saved-setting upgrade

The subsequent `peony-v2-package3` includes the V2 astral companions and is
installed with core `94bb1d08bc7d6dd4181e21d4605a3b87cfb7f6046c90f35194f0e090291c423b`.
See [astral and installation evidence](2026-10-12-peony-v2-astral.md).
The earlier remaining-work section records the state of package2.

`work/three-outfit-rig-physics/peony-v2-upgrade1` checks the actual refit2 and
installed package3 metadata through CSS's C++23 customization and state code.
The standalone recipe is `tools/eve-fit/check_peony_v2_upgrade.cpp`. No runtime
change was needed. No player state, profile or installed file was written.

All 774 saved-setting cases pass, covering Original and the five retired
palettes, retained control-channel endpoints, state/profile serialization and
both automatic-selection and remembered-customization restoration. The 33
compatible explicit settings survive, including body shape and flowing glow.
The five obsolete garment color/roughness controls are dropped. Retired palettes
fall back to Original; this does not attempt to recreate their colors or discard
compatible manual overrides. Placement, expression and group tints survive.
Restoring the resulting look a second time leaves it unchanged.

All 256 combinations of the eight old garment toggles use the independently
recorded V2 section order, including boot skin coverage and stockings. The six
pre-existing Eve variants have identical customization recipes before and after
the package upgrade. Deliberately restoring old boot section numbers, removing
the glute-shape control or reusing a retired palette ID makes the checker fail.
`receipt.json` records input/source/library hashes and compiler/command details;
`negative-controls.json` records each rejection. These tests do not exercise
native rendering, input handling or live material/visibility propagation.

The passive runtime check still reports no applied appearance, 22 loaded
packages, zero rejected packages and no catalog errors. The user's fit check
remains pending. Free space before and after this verification is 128G on root
and 85G on theark at `df -h` precision; only small test/evidence files were added.
