# Pose diagnostics must preserve authoring fit shapes

2026-09-21: `render_live_weapon_reference.py` previously set every shape key to
zero. The exporter retains authored fit values before adding the six runtime
morphs. Consequently, those earlier renders did not show the exported body.
Their recorded bone transforms remain useful; their skin silhouettes and
finger distances do not establish contact on the fitted game mesh.

The corrected default clears only the six runtime morph controls and preserves
the authored fit values. It records every active fit key in its report.
`--shape-policy basis` explicitly reproduces the historical all-zero behavior.
Both old full-body views reproduce pixel-for-pixel under that option.
`--framing hands` adds measured hand-centered views without changing skinning.

An independent neutral-geometry check compares the corrected fit policy with
the already verified V43 export, covering all 133,066 points in its ten parts.
The maximum position difference is exactly zero. The old basis differs by up
to 3.90444 cm on the body and 0.169754 cm in hand-weighted vertices. Of those
hand vertices, 2,610 differ by more than 0.01 cm. These numbers establish a
diagnostic error, not the cause of the larger arm or finger defects.

Evidence is under `work/grip-grounding-v1/arm-rest-alignment-v1/`:
`render-fit-validation.json`, `verify-fit.result.json`,
`renderer-regression.json`, command manifests and fitted render reports.
The comparison uses `SK_SeduXtress_HandBindV43.mesh.json` and its audit under
SeduXtress `work/exports-nextgen/`. Its cooked readback was verified earlier.

The already-applied
[renderer patch](../../tools/authoring-patches/diagnostic-fitted-pose-render.patch)
follows `diagnostic-alternate-bind-render.patch`. The verification source is
[verify_render_fit.py](../../tools/authoring-probes/arm-rest/verify_render_fit.py).
Run it in the pinned Blender sandbox with `CSS_ARM_REST_AUDIT_DIR` pointing to
the evidence directory. Original blends and installed assets are unchanged.
