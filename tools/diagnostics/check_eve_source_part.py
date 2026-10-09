"""Compare a saved garment with its import payload. Run in Blender 5.2.2.

Loads a private in-memory object copy. Does not save the blend or change the
import payload. This checks source provenance, not cooked or posed geometry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
EXPORTER = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'
sys.path.insert(0, str(EXPORTER))
from export_variant_clean import EXPORT_SHAPES, TO_UE, fitted_mesh


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'mesh', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--part', required=True)
    parser.add_argument('--neutral-runtime-morphs', action='store_true',
                        help='Set the six CSS body shape values to zero on the in-memory copy')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists():
        raise FileExistsError(args.output)
    audit_path = args.mesh.with_suffix('.audit.json')
    protected = {str(p.resolve()): digest(p) for p in
                 (args.source, args.mesh, audit_path, Path(__file__), EXPORTER / 'export_variant_clean.py')}
    data, audit = (json.loads(p.read_text()) for p in (args.mesh, audit_path))
    if audit['output_sha256'] != digest(args.mesh):
        raise ValueError('Import payload does not match its audit')
    parts = audit['parts']
    matches = [i for i, part in enumerate(parts) if part['name'] == args.part]
    if len(matches) != 1:
        raise ValueError('Expected one matching source part')
    part_index = matches[0]
    part = parts[part_index]
    start = sum(p['points'] for p in parts[:part_index])
    face_start = sum(p['faces'] for p in parts[:part_index])
    if sum(p['points'] for p in parts) != len(data['points']):
        raise ValueError('Part point ranges do not cover the payload')
    if sum(p['faces'] for p in parts) != len(data['faces']):
        raise ValueError('Part face ranges do not cover the payload')

    bpy.ops.wm.read_factory_settings(use_empty=True)
    with bpy.data.libraries.load(str(args.source.resolve()), link=False) as (source, target):
        if args.part not in source.objects:
            raise ValueError('Saved source object is missing')
        target.objects = [args.part]
    obj = target.objects[0]
    bpy.context.scene.collection.objects.link(obj)
    saved_values = {}
    if obj.data.shape_keys:
        saved_values = {key.name: key.value for key in obj.data.shape_keys.key_blocks
                        if key.name in EXPORT_SHAPES}
        if args.neutral_runtime_morphs:
            obj.data.shape_keys.animation_data_clear()
            for key in obj.data.shape_keys.key_blocks:
                if key.name in EXPORT_SHAPES:
                    key.value = 0
    mesh, shapes, active = fitted_mesh(obj)
    mesh.calc_loop_triangles()
    matrix = TO_UE @ obj.matrix_world
    triangles = []
    for triangle in mesh.loop_triangles:
        a, b, c = (matrix @ mesh.vertices[i].co for i in triangle.vertices)
        if (b - a).cross(c - a).length_squared >= 1e-12:
            triangles.append(triangle)
    used = sorted({i for t in triangles for i in t.vertices})
    if len(used) != part['points'] or len(triangles) != part['faces']:
        raise ValueError('Source topology counts differ')
    compact = {v: start + i for i, v in enumerate(used)}
    position_error = max((matrix @ mesh.vertices[v].co - Vector(data['points'][compact[v]])).length for v in used)
    expected_triangles = [[data['wedges'][i][0] for i in face[:3]]
                          for face in data['faces'][face_start:face_start + part['faces']]]
    actual_triangles = []
    for triangle in triangles:
        ids = [compact[i] for i in triangle.vertices]
        if matrix.to_3x3().determinant() > 0:
            ids.reverse()
        actual_triangles.append(ids)

    exported_weights = {}
    for vertex, bone, weight in data['influences']:
        if start <= vertex < start + len(used):
            name = data['bones'][bone]['name']
            row = exported_weights.setdefault(vertex, {})
            if name in row:
                raise ValueError('Duplicate exported influence')
            row[name] = weight
    weight_error = 0.0
    for v in used:
        source_weights = {obj.vertex_groups[g.group].name: g.weight
                          for g in obj.data.vertices[v].groups if g.weight > 0}
        expected = exported_weights.get(compact[v], {})
        weight_error = max(weight_error, max((abs(source_weights.get(n, 0) - expected.get(n, 0))
                                             for n in source_weights.keys() | expected.keys()), default=0))
    deltas = {m['name']: {row[0]: Vector(row[1:]) for row in m['deltas']}
              for m in data['morph_targets']}
    morph_errors = {}
    for name in EXPORT_SHAPES:
        source_delta = shapes.get(name)
        worst = 0.0
        for v in used:
            actual = matrix.to_3x3() @ Vector(source_delta[v]) if source_delta is not None else Vector((0, 0, 0))
            expected = deltas.get(name, {}).get(compact[v], Vector((0, 0, 0)))
            worst = max(worst, (actual - expected).length)
        morph_errors[name] = worst

    unchanged = all(digest(Path(p)) == h for p, h in protected.items())
    # Float32 coordinates pass through Blender metres and Unreal centimetres.
    passed = (position_error <= 0.0001 and weight_error <= 0.000001
              and max(morph_errors.values()) <= 0.0001
              and expected_triangles == actual_triangles and unchanged)
    report = dict(passed=passed, part=args.part, points=len(used), triangles=len(triangles),
                  position_error_cm=position_error, weight_error=weight_error,
                  morph_errors_cm=morph_errors, triangle_order_equal=expected_triangles == actual_triangles,
                  active_fit_keys=active, protected_unchanged=unchanged, inputs=protected,
                  saved_runtime_morph_values=saved_values,
                  runtime_morphs_neutralized=args.neutral_runtime_morphs,
                  blender_version=bpy.app.version_string,
                  scope='One saved garment part versus its import payload: positions, triangle order, '
                        'named weights and six body morphs. No source skeleton, UV, normal, cooked '
                        'buffer, simulation, visible fit or gameplay claim.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    if not passed:
        raise ValueError('Source part differs from import payload; see report')


if __name__ == '__main__':
    main()
