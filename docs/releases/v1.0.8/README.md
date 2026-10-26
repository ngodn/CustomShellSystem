# CSS 1.0.8

CSS 1.0.8 and CSS Shared Assets 1.0.8 ship with Eve (Stellar Blade) 1.5.3: the slimmed AIO (664 MB, same id and paths) and seven standalone outfits (`eins0fx.eve.<variant>`, assets under `/Game/CSS/CSS_Eve<Name>_eins0fx_P/`). Build record: `CSS_SeduXtress_eins0fx/work/standalone-100/` (PLAN.md, REPORT.md).

- Core: `playback_adapter_asset` also accepts `/Game/CSS/<stem>/Animations/Adapters/` (package-stem namespace); without it Shared Assets with standalone adapters was rejected. Test: `tests/asset_namespace_tests.cpp`.
- Core: 13 Astral root aliases (each standalone's `Hair/M_Hair1`, Peony's six cloth roots) so Genessa doubles copy the standalones; graph StateId checks unchanged.
- Catalog: hand-support bindings for the Black Pearl and Peony standalones; shader donors regenerated (602 materials, 142 for the standalones).
- Shared Assets 1.0.8: released 1.0.4 containers unchanged; metadata pak adds 24 adapter rows (Eve 1.5.3 AIO 88b450457720 and seven standalones), keeps Eve 1.5.2 9e17a21d6e55 and prunes Eve 1.5.1 and older so the animation list (30) and css.eve.face (21) stay within the 32 limit the core enforces.
- Live test 26 Oct: all eight Eve packages load (32 outfits, 0 rejected) and every standalone applies textured.
