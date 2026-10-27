# CSS 1.0.9

CSS 1.0.9 and CSS Shared Assets 1.0.9 ship with Profane Proxima 0.2.4 (Chest shape: Perky / Juicy). Package record: `CSS_tHornyProxima_eins0fx_P/work/chest-shape-105/` (REPORT.md, CORE-HANDOFF.md).

- Core: shape routes. A Choice with `shape_control` routes that Shape control's slider to the selected option's `morph`, holds the other options' morphs at zero, and selects the option's `body_geometry_property` (a string property cooked into the post-process AnimBP) as the body contact model. Validation: existing Shape target, first option = the shape's own morph, distinct morphs, one route per shape, geometry property on every option or none, no textures. Old saved looks load the default option; templates keep the current route because they only override the keys they set.
- `sync_body_geometry` reloads the model when the selected property changes on the same instance and keeps the first captured inputs for teardown; the preview model check uses the same property.
- `tools/css_controls.py` validates routes and `check_shapes` checks route morphs against the mesh JSON. Tests: `tests/control_tests.cpp`, `tests/test_css_controls.py::test_shape_route`.
- Shared Assets 1.0.9: released 1.0.4 containers unchanged; metadata pak adds the Profane Proxima 0.2.4 animation pin (8578e4b9772e) and rebuilds css.profaneproxima.face for it.
- Live test 27 Oct: Perky and Juicy at Chest size 1/2/3, God Damn applied on Juicy keeps Juicy, switching back clears Juicy, old looks load as Perky, no log errors.
