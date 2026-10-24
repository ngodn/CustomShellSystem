# CSS 1.0.5

CSS 1.0.5 and CSS Shared Assets 1.0.5 ship with Profane Proxima v0.2.1, whose glow now compensates the game's exposure (EyeAdaptationInverse, reference 0.05) so it reads the same in every lighting condition.

- Core: the Genessa doubles table matches a material by path and graph identity together, so one outfit can carry several revisions; Profane 0.2.1's four glow parents are added beside 0.2.0's (34 entries). `tests/astral_adapter_tests.cpp` checks all 34.
- Core: transient wears (persist false) no longer overwrite `remembered_custom`, and custom-only applies keep the selected animations (CINE lane change, included with the user's approval).
- Shared Assets 1.0.5: released 1.0.4 containers unchanged; metadata adds revision 28e0b5dbe13c to the animation adapters and the `css.profaneproxima.face` adapters (0.1.0 and 0.2.0 rows kept). Built with `tools/authoring/shared-assets/append_revision_pins.py`.
- Research and live measurements: `work/research/lighting/`, Astra's `work/glow-lighting-01` and `work/glow-exposure-95`/`96` reports in the Profane package.
