# Facial authoring and preservation checks

These are offline tools for UE 5.6.1 (C++20) and Blender 5.2.2. They do not
change the installed game or establish live compatibility. Stage the matching
`CSSFacialLibrary.h` and `.cpp` into the existing CSSAuthoring editor module.
Build only after any editor using the previous module has exited.

## Interchange

Both formats contain `body_points` in Unreal centimeters and `morph_targets`.
Each morph has a unique name and sparse `[local_point_index, dx, dy, dz]`
deltas. Local indices address `body_points`, not arbitrary mesh vertices.

- Schema 1 maps points to the source mesh's initial contiguous vertices. It
  rejects `point_indices` and remains compatible with the existing Eve export.
- Schema 2 requires `point_indices`, one unique source mesh vertex ID per
  point. This supports separated facial objects such as CW's eyelashes.

The schema must be the JSON number 1 or 2. An optional `diagnostic_only` must
be the boolean false; true and non-booleans are rejected. Every point must
match the candidate's source position within 0.0002 cm. Counts, indices,
finite values, duplicate names/vertices and the 20 cm delta bound are checked
before replacing the candidate description. Import is commandlet-only and
restricted to `/Game/CSS/AnimLab/SK_Face_...` candidates. Original packages are
not valid import targets.

## Commander White source evaluation

1. Run `sample_cw_facial_controls.py` with Blender `--background
   --disable-autoexec`, the preserved C1 source, a new output directory and
   repeated `--control` names from the verified original rig. Defaults sample
   quarter strengths; `--levels` accepts independent validation strengths.
   `--combined` additionally evaluates the requested controls together.
   Embedded scripts remain disabled, source files are hashed and never saved,
   and neutral restoration is checked after each control and combination.
2. `bake_cw_facial_shapes.py` requires B9, the samples, the current assembly
   JSON gzip and the recorded topology receipt. It validates source/fitted
   correspondence and the actual assembly point positions before exporting
   sparse schema 2. Only quarter-weight samples are accepted by this bake.
3. Run `trial_cw_facial_shapes.py` in the isolated editor with
   `CSS_FACE_TRIAL_MODE=create`, then `verify` in a fresh process. The current
   fixture is C4 to CWJ1; it is a jaw/blink experiment, not a general face map.
4. `render_cw_facial_samples.py` produces front/quarter geometry reviews. Its
   Workbench view omits optical cornea/moisture polygons only from the neutral
   diagnostic scene, not the source or imported asset.
5. Run `audit_cw_facial_interpolation.py` against independent source samples
   with an explicit `--max-error-cm`. It measures individual interpolation and
   combined-control interaction separately. Source approximation and cooked
   morph filtering are different checks; do not combine their error claims.

## Native preservation

`VerifyExpressionShapes` compares original morph attributes, new source deltas,
built render deltas and omitted vertices against the original build threshold.
`VerifyExpressionPreservation` removes new morph attributes only from temporary
copies, then requires identical serialized base mesh descriptions. This covers
topology, positions, normals, UVs, source skin weights and old morph attributes.
It also compares built indices, import/bone maps, per-section vertex geometry,
UVs, colors, weights, material/visibility settings and cloth mapping data.
The stored clothing property is compared recursively through UE reflection.
These checks do not simulate cloth, verify every derived render buffer, or
replace a Windows cook and live playback test.

`verify_facial_schema.py` tests invalid schema types/values and diagnostic flags
on saved schema-1 and schema-2 candidates, then repeats valid readback and
preservation checks. `verify_facial_shapes.py` repeats morph, preservation and
binding checks across all six existing Eve candidates. All input and asset
hashes must remain unchanged during readback.

Evidence lives in `work/cine-animation-port/cw-facial-*`. No generated samples,
large assets or source archives belong in the code commit. Remaining work
includes additional CW controls, expression mappings, cooked comparisons,
catalog binding, runtime interruption/restoration and live outfit acceptance.
