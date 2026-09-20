# Arm rest-pose alignment probes

These preserve the [V44B/B2 checkpoint](../../../docs/development/arm-rest-alignment.md).
Neither candidate is a finished V44 release. Set `CSS_ARM_REST_AUDIT_DIR` to a
direct child of `CustomShellSystem/work/grip-grounding-v1/`; existing evidence
uses `arm-rest-alignment-v1`.

- `build_candidate.py`: Blender 5.2.2 / Python 3.13.13, creates the isolated B
  blend and bind. Refuses overwrite. Keeps limb lengths and transforms all
  stored coordinates/morphs consistently, with an independent Blender check.
- `verify_saved.py`: reopens source and candidate, verifies every stored
  coordinate by inverse mapping plus morph metadata, weights and topology.
- `preserve_attachments.py`: builds B2 from B, restores five unweighted leaf
  attachment locals and exact canonical quaternions, removes stale hand-frame
  metadata, checks unchanged geometry/morphs. Refuses overwrite.
- `evaluate.py`: UE5.6.1 Python commandlet, requires the imported diagnostic B
  reference and the corrected source/reference assets from earlier probes.
  Writes bounded H2 poses and independent engine bind readback, with unchanged
  production asset hashes. It does not import or cook the full candidate skin.
- `verify_render_fit.py`: Blender, independently compares corrected neutral
  geometry with every point of the previously verified V43 export.
- `validate_full_export.py`: host Python 3.14, checks B2's full Schema 1 export
  against V43. Verifies preserved weights, fit values and UV/color associations,
  and accounts for ten changed quad diagonals without changing their boundaries.

The original evidence directory contains sandbox command manifests for builds,
reference import, evaluation, full mesh JSON export and renders. All scripts,
scratch, logs and caches stay in the workspace. Respect terminal exit codes;
the editor can take time to shut down after completing a probe.

The renderer patch is
[`diagnostic-fitted-pose-render.patch`](../../authoring-patches/diagnostic-fitted-pose-render.patch).
Its default preserves authored fitting shapes and zeros only runtime controls.
Use `--shape-policy basis` only to reproduce historical images, and use
`--framing hands` for the close views. Alternate blends still require explicit
mesh identity and independently read engine bind data.
