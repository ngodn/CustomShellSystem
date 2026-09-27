# War Aegis fitting baseline

Source: Gemini `exports/SK_Eve_WarAegis.mesh.json`, SHA256 `9c58a1b5dec47adc14ed1fa8a1a8e4af00753a5ba25febc2c5504399d84b6a67`. Original untouched.

While Alice's bow pipeline runs, baseline renders `work/eve26/aegis-base1` and `aegis-max1` completed exit 0 and front views were inspected. Default has broad chest, shoulder and arm intersections. All six supported sliders at 1 cause severe inflated and folded garment deformation. Do not accept this export or hide the body to cover the problem. Verify intended suit coverage against the original authoring reference before changing any apparent openings.

The suit is one source part, 38,242 points and 76,044 triangles, material slots 16/17/18. Body is 36,787 points; hair occupies slots 19/20/21. No separate shoe part is present in this export. Source garment has no baked fit keys. Check the original source fit settings and geometry first, then reuse validated body-corresponding morph and weight transfer where appropriate. No fitting edits or import have been performed yet. Alice remains the active outfit.


## Original author source inspected

`inspect_knit_motion_source.py --outfit aegis --output work/eve26/aegis-author1.json` completed exit 0, confirms the original blend hash is unchanged, and records shape values, modifiers and rig dependencies. Original suit has the same 38,242 points and active `Fix=1` and `VagShape=1`; the export audit lists no baked garment fit keys. This discrepancy needs an evaluated-coordinate comparison before attributing clipping to omitted keys. It is not yet a proven cause.

A separate original `Eve War Aegis` mesh has 44 vertices, a Cloth modifier with pin group `Pin` and quality 5, followed by the Eve armature. Preserve and inspect this authored motion source instead of inventing a solver from the export alone. No War Aegis source asset was modified.
