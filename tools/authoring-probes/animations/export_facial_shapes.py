"""Export Eve's expression shapes without changing its saved assembly or body fit.

Run with Blender 5.2.2, --disable-autoexec, and an unused output directory.
The output is an authoring candidate, not an installed outfit replacement.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
MOD = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import fitted_mesh, TO_UE, LEFT_HAND_CORRECTIVES


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--expressions', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists():
        raise ValueError('Use a new output directory')
    sources = {str(p.resolve()): digest(p) for p in (args.source, args.baseline, args.expressions)}
    records = json.loads(args.expressions.read_text())
    poses = []
    names = set()
    for path, exports in records.items():
        sequences = [entry for entry in exports if entry['Type'] == 'AnimSequence']
        if len(sequences) != 1:
            raise ValueError(f'{path}: expected one expression sequence')
        curves = sequences[0]['CompressedCurveData']['FloatCurves']
        weights = {}
        for curve in curves:
            name = curve['CurveName']
            values = {key['Value'] for key in curve['FloatCurve']['Keys']}
            if len(values) != 1 or name in weights:
                raise ValueError(f'{path}: animated or duplicate expression curve {name}')
            weight = values.pop()
            if not math.isfinite(weight) or not -1 <= weight <= 1:
                raise ValueError(f'{path}: unexpected expression weight')
            weights[name] = weight
        if not weights:
            raise ValueError(f'{path}: empty expression')
        names.update(weights)
        poses.append(dict(source=path, name=sequences[0]['Name'], weights=weights))
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()))
    body = bpy.data.objects.get('Eve Body')
    if not body or not body.data.shape_keys:
        raise ValueError('Eve Body has no expression shapes')
    keys = body.data.shape_keys.key_blocks
    folded = {}
    for key in keys:
        if key.name.casefold() in folded:
            raise ValueError('Ambiguous case-insensitive shape name')
        folded[key.name.casefold()] = key.name
    mapping = {name: folded[name.casefold()] for name in sorted(names)}
    if len(set(mapping.values())) != len(mapping):
        raise ValueError('Expression names alias the same shape')
    baked = {name: float(keys[target].value) if not keys[target].mute else 0.0
             for name, target in mapping.items()}
    mesh, deltas, _ = fitted_mesh(body, (*LEFT_HAND_CORRECTIVES, *mapping.values()))
    baseline = json.loads(args.baseline.read_text())
    mesh.calc_loop_triangles()
    world = TO_UE @ body.matrix_world
    triangles = []
    for triangle in mesh.loop_triangles:
        a, b, c = (world @ mesh.vertices[i].co for i in triangle.vertices)
        if (b-a).cross(c-a).length_squared >= 1e-12:
            triangles.append(triangle)
    used = sorted({index for triangle in triangles for index in triangle.vertices})
    points = np.array([tuple(world @ mesh.vertices[index].co) for index in used])
    reference = np.array(baseline['points'][:len(points)])
    if points.shape != reference.shape:
        raise ValueError('Body topology differs from accepted export')
    error = float(np.linalg.norm(points - reference, axis=1).max())
    if error > 0.0002:
        raise ValueError(f'Body fit changed: {error} cm')
    transform = (TO_UE @ body.matrix_world).to_3x3()
    shapes = []
    for source, target in mapping.items():
        values = []
        for index, vertex in enumerate(used):
            delta = deltas[target][vertex]
            converted = transform @ Vector(delta)
            if converted.length > 1e-6:
                values.append([index, *converted])
        if not values:
            raise ValueError(f'Expression shape {source} has no deltas')
        shapes.append(dict(name=source, source_shape=target, baked_value=baked[source], deltas=values))
    # Existing body and finger corrections must retain the same deltas.
    old = {entry['name']: entry['deltas'] for entry in baseline['morph_targets']}
    retained_error = 0.0
    for name, delta in deltas.items():
        if name not in old:
            continue
        expected = np.zeros((len(points), 3))
        for index, x, y, z in old[name]:
            if index < len(points):
                expected[index] = (x, y, z)
        actual = np.array([tuple(transform @ Vector(delta[index])) for index in used])
        retained_error = max(retained_error, float(np.linalg.norm(actual-expected, axis=1).max()))
    if retained_error > 0.0002:
        raise ValueError(f'Existing body/hand shape changed: {retained_error} cm')
    for path, sha in sources.items():
        if digest(Path(path)) != sha:
            raise ValueError('An input changed during facial export')
    args.output.mkdir(parents=True)
    payload = dict(schema=1, body_points=points.tolist(), morph_targets=shapes, expressions=poses)
    target = args.output / 'facial-shapes.json'
    target.write_text(json.dumps(payload, separators=(',', ':'), allow_nan=False) + '\n')
    report = dict(sources=sources, blender=bpy.app.version_string, body_points=len(points),
                  shapes=len(shapes), expressions=len(poses), mapping=mapping, baked_values=baked,
                  max_body_difference_cm=error, max_retained_morph_difference_cm=retained_error,
                  output_sha256=digest(target), installed=False)
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'FACIAL_EXPORT_PASS: {len(shapes)} shapes, {len(poses)} expressions')


if __name__ == '__main__':
    main()
