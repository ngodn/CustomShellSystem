# CSS 1.0.4

CSS 1.0.4 and CSS Shared Assets 1.0.4 ship together with Profane Proxima v0.2.0 (id `eins0fx.thornyproxima`): Genessa's doubles as ghostly copies of the outfit, and its mouth expressions. Every other outfit keeps 1.0.3 behaviour.

- Core change: `native/src/astral_material_catalog.inl` grows from 23 to 30 companion entries (the seven Profane Proxima source materials). The 23 existing entries are unchanged; `tests/astral_adapter_tests.cpp` checks all 30.
- Ghost materials: authored with `tools/shared-assets/author_profane_astral.py`, checked with `verify_profane_astral.py`, cooked with `cook_profane_materials.py` (stage `/mnt/eins0fxE/CSS-Development/profane-astral1`, Windows cook 0 errors, 0 warnings, SM5 and SM6 shader maps embedded).
- Shared Assets 1.0.4: `tools/shared-assets/assemble_shared_104.py`. The 1.0.3 payload stage reproduces the released 1.0.3 utoc byte for byte before the 14 new packages are added; roundtrip, decode and import closure pass. Metadata: revision pin c3f1eaf869f9 for v0.2.0 beside v0.1.0's 319aa8c1efe8, and face rig `css.profaneproxima.face` (51 morphs, 20 expressions, six dance faces, two adapters) built with `tools/authoring/shared-assets/build_face_rig.py`.
- Live: the release-candidate core and Shared Assets loaded in game on 24 Oct 2026, and the user accepted the doubles in play.
