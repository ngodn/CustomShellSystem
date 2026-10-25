# CSS 1.0.6

CSS 1.0.6 and CSS Shared Assets 1.0.6 ship with Profane Proxima v0.2.2 and Eve (Stellar Blade) v1.5.2.

- Core: body sliders accept morph weights up to 3, so an outfit can author an extended range (Profane Proxima 0.2.2 sculpts chest, glutes, hips, thighs and calves to 3).
- Core: the Genessa doubles table adds Profane Proxima 0.2.2's two rebuilt body skin parents (36 entries); 0.2.0 and 0.2.1 rows stay.
- Core: cache folders no installed package uses are removed after a complete scan.
- Shared Assets 1.0.6: released 1.0.4 containers unchanged; metadata pak adds Eve 1.5.2 (9e17a21d6e55) and Profane Proxima 0.2.2 (dbe84a47ca42) revision pins. Built with `tools/authoring/shared-assets/append_revision_pins.py` and `build_face_rig.py`.
- Game build 26147827: every Eve, Genessa, Shared Assets and Profane material passes `tools/ShaderDependencyAudit` with SM5 and SM6 (evidence `CustomShellSystem-work/sm5-audit-20261026/`).
