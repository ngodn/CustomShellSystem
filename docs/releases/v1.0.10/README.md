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
