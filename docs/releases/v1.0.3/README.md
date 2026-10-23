# CSS 1.0.3

CSS 1.0.3 and CSS Shared Assets 1.0.3 ship the opt-in foot IK leg-length correction and the support for eins0fx's new Proxima outfit v0.1.0 (id `eins0fx.thornyproxima`). Every other outfit keeps 1.0.2 behaviour.

- Core change: `native/src/native_foot_ik.*`, opt-in list `packaging/catalog/native-foot-ik.json`. Research and live measurements: memory note and the Astra walk reports under `CSS_tHornyProxima_eins0fx_P/work/walk-84` to `walk-88`.
- No-regression pass: `work/footik-regression/check.py`, 19 looks (CSS outfits, ports, original shells, NPC and enemy looks), passed on the release candidate core.
- Shared Assets 1.0.3: released 1.0.2 utoc and ucas unchanged; metadata pak rebuilt with `tools/authoring/shared-assets/append_revision_pins.py` (final pin 319aa8c1efe8, superseded candidate pins removed, 22 of 32 adapter rows).
