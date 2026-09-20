# CSS checkpoint, 2026-09-20

Changes are committed by context, as requested. Continue that practice for later
CSS work. These are local development commits, not a release or deployment.

| Commit | Context |
| --- | --- |
| `7b4ca18` | Exact source animation samples and separate additive diagnostics |
| `b0b4bcd` | Independent body/hair controls and morph-aware body geometry |
| `59a22f8` | Preview layer coverage, camera restoration and inspection tools |
| `d862e01` | Optional CSSX render-consumer query and bounded FPS profiling |
| `b3f024f` | Steam screenshots, game-world recordings and motion inspection |

Current checks pass: all 12 host CTest cases after rebuilding, 16 Python control
tests, and 14,359 body-geometry checks across 128 fixtures. Fourteen changed
Python files parse successfully. These checks do not replace the live-game
acceptance still listed in the work queue. Existing live evidence is retained
in the topic documents and workspace artifacts.

The hand corrective candidate now imports and cooks offline. Independent decoded
mesh comparison preserves base geometry, normals, weights, skeleton and all six
body morph records exactly. All 16 private finger shapes survive with measured
compression error and small importer-threshold omissions. See the
[corrective export and cooked readback](../../../CSS-Mod-Authoring/docs/next-gen-hand-corrective-export.md).
It has no validated runtime driver and has not been installed.

The accepted state remains V43's weapon-grip improvement, hair 200/24, preserved
body proportions and body dynamics, and disabled CSSX. Broken left fingers,
damage/parry, grounding, heel supports, preview lighting controls, locomotion
and complete weapon/lifecycle/performance acceptance remain open. Do not mistake
asset readback, isolated hand contacts or source decoding for a hand repair.
Read the [hand investigation](../../../CSS-Mod-Authoring/docs/next-gen-left-hand-source-contact.md)
before proposing another transfer or contact-search experiment.

The pre-existing untracked `catalog/dev-items.disabled` marker is preserved.
