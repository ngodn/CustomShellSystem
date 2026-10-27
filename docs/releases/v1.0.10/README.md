# CSS 1.0.10

CSS 1.0.10 and CSS Shared Assets 1.0.10 ship with Commander White 0.1.3 (lighter package, cheaper belt,
working hair dash fix). Package record: `CSS_CommanderWhite_eins0fx_P/work/optimize-013/` (MEASURE.md,
REPORT.md), brief `aaa-prompts/ASTRA_CW_OPTIMIZE_013.md`.

- Core: a toggle control may name `anim_switch`, a bool property on the post-process AnimBP that follows
  the toggle (true while any toggle naming it is on). Written per instance only when the value or the
  instance changes, on the world and wardrobe components. Old cores ignore the field; the graph default
  (true) keeps the rig running. CW 0.1.3 sets `CSSBeltActive` from its belt toggle and its four worn
  ABPs skip `CR_Belt20` at alpha 0 and reseed on resume.
- Hand support: no barrier before the read-only node search; an instance whose graph holds no catalog
  rig is remembered (`original rig not in graph`) instead of scanned every tick; the one remaining
  barrier before writes is timed (`barrier_avg_us`, `barrier_max_us` in status).
- Shader fallback, stale outfits: `catalog/shader-donors.json` gains `stale_outfits` (outfit id -> released
  versions that failed the shader audit on the verified build). In Auto, a worn outfit whose installed
  version is listed is rebuilt on its donors even on a verified game (`active_for_outfit`); the package
  scan now records each manifest's `id` with its `version`, and the core passes the map to the fallback
  after every catalog load. Status gains `outfit_stale`; the log names the outfit and version once.
  Listed: Commander White 0.1.0/0.1.1, Eve 1.2.0/1.3.0/1.4.0/1.5.0, Unholy Genessa 0.1.0-trial.5/0.3.0,
  Profane Proxima 0.1.0/0.2.0/0.2.1. Donor table unchanged (602 materials, 9 donors), regenerated with
  `build_shader_donors.py --include-report/--stale-report` from the cached audit reports.
- Tifi's community patch (`CSS_Eve_ShaderFix_26073152`, sha256 12258146...) is byte-identical to the 66
  Eve materials shipped since Eve 1.5.1 (raw Zen chunk comparison against 1.5.1, 1.5.2 and 1.5.3), so it
  only helps players still on Eve 1.5.0 or older; the stale-outfit rule covers them and the other outfits.
- Live test 27 Oct: Eve 1.4.0 AIO on verified build 26147827, Auto: "Shader fallback on for
  eins0fx.seduxtress 1.4.0", 21 materials rebuilt, 0 skipped; Off turns it off.
- Known: CW's shoulder hold has not bound since 0.1.2 (catalog swaps `CR_BodyB3`, the worn graphs run
  `CR_BodyV11`). Queued in `docs/work-queue.md`.
- `tools/css_controls.py` validates `anim_switch`. Tests: `tests/control_tests.cpp`,
  `tests/test_css_controls.py::test_toggle_anim_switch`.
- Shared Assets 1.0.10: released 1.0.4 containers unchanged; metadata pak adds the Commander White
  0.1.3 animation and facial pins (7dc76d9eb993) and prunes nine superseded CW development revisions,
  keeping released 0.1.2 (a0a3521d015e). 27 animation adapters.
- Live test 27 Oct (M1 build, test core): belt hidden sets `CSSBeltActive` false on the worn
  performance graph and back to true when shown; look and state restored.
- Tests before release: 34/34 host suites, 98/99 Python. The one failure,
  `test_release.ReleaseTests.test_catalog_release_requires_hand_support_payload`, predates this release:
  its fake 1.0.1 archive carries hand-support.json but not native-foot-ik.json, which the verifier has
  required for 1.0.1 and later since native foot IK shipped.
