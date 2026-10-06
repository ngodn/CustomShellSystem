"""Blender 5.2.2: audit retained Eve controls beneath Unholy's native mask.

This only writes diagnostic interchange, never an installable face. Matching
shape names on the body do not establish control over the visible native mask.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'
sys.path.insert(0, str(HELPER))
from export_seduxtress_eve import fitted_mesh, TO_UE


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--template', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists():
        raise FileExistsError('Use a new output directory')
    protected = {str(p.resolve()): digest(p) for p in
                 (args.source, args.template, HELPER / 'export_seduxtress_eve.py')}
    template = json.loads(args.template.read_text())
    if template['schema'] != 1:
        raise ValueError('Unsupported facial template')
    mapping = {s['name']: s['source_shape'] for s in template['morph_targets']}
    if len(mapping) != len(template['morph_targets']) or len(set(mapping.values())) != len(mapping):
        raise ValueError('Ambiguous facial mapping')
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()))
    body = bpy.data.objects['Eve Body']
    keys = body.data.shape_keys
    if not keys or not keys.use_relative:
        raise ValueError('Expected relative Eve facial shapes')
    hidden = set()
    masks = 0
    for modifier in body.modifiers:
        if modifier.type == 'MASK':
            masks += 1
            if masks > 1 or not modifier.invert_vertex_group:
                raise ValueError('Unexpected body visibility mask')
            group = body.vertex_groups[modifier.vertex_group].index
            hidden = {v.index for v in body.data.vertices
                      if any(g.group == group and g.weight > modifier.threshold for g in v.groups)}
        elif modifier.type != 'ARMATURE':
            raise ValueError(f'Unexpected modifier: {modifier.type}')
    baked = {name: float(keys.key_blocks[target].value)
             if not keys.key_blocks[target].mute else 0.0 for name, target in mapping.items()}
    if any(abs(value) > 1e-6 for value in baked.values()):
        raise ValueError('Source already has an active facial expression')
    mesh, deltas, active = fitted_mesh(body, tuple(mapping.values()))
    mesh.calc_loop_triangles()
    world = TO_UE @ body.matrix_world
    used = set()
    materials = {}
    triangles = 0
    for triangle in mesh.loop_triangles:
        if hidden.intersection(triangle.vertices):
            continue
        a, b, c = (world @ mesh.vertices[i].co for i in triangle.vertices)
        if (b-a).cross(c-a).length_squared < 1e-12:
            continue
        used.update(triangle.vertices)
        for index in triangle.vertices:
            materials.setdefault(index, set()).add(mesh.materials[triangle.material_index].name)
        triangles += 1
    used = sorted(used)
    transform = world.to_3x3()
    shapes = []
    absent = []
    affected = {}
    for name, target in mapping.items():
        values = []
        for point, index in enumerate(used):
            delta = transform @ Vector(deltas[target][index])
            if delta.length > 1e-6:
                values.append([point, *delta])
        if not values:
            absent.append(name)
            continue
        shapes.append(dict(name=name, source_shape=target, baked_value=baked[name], deltas=values))
        affected[name] = dict(
            materials=sorted({material for point, *_ in values for material in materials[used[point]]}),
            max_delta_cm=max(Vector(v[1:]).length for v in values),
            points=len(values))
    payload = dict(schema=1, body_points=[list(world @ mesh.vertices[i].co) for i in used],
                   morph_targets=shapes, expressions=template['expressions'])
    if 'performances' in template:
        payload['performances'] = template['performances']
    # Keep the source definitions intact, but flag controls absent on this mesh.
    # These candidates must not be advertised as complete facial support.
    payload['absent_controls'] = absent
    payload['diagnostic_only'] = True
    if any(digest(Path(path)) != sha for path, sha in protected.items()):
        raise ValueError('Source changed during export')
    args.output.mkdir(parents=True)
    target = args.output / 'facial-shapes.json'
    target.write_text(json.dumps(payload, separators=(',', ':'), allow_nan=False) + '\n')
    report = dict(protected=protected, sources_unchanged=True, blender=bpy.app.version_string,
                  points=len(used), triangles=triangles, hidden_vertices=len(hidden),
                  shapes=len(shapes), mapping=mapping, active_fit_keys=active,
                  absent_controls=absent,
                  affected_geometry=affected,
                  visible_face_objects=[dict(name=o.name, vertices=len(o.data.vertices),
                      shapes=len(o.data.shape_keys.key_blocks) if o.data.shape_keys else 0)
                      for o in bpy.data.objects if o.type=='MESH' and o.name in ('UG_Face','UG_Eyes')],
                  expressions_with_absent_controls=[e['name'] for e in template['expressions']
                      if any(e['weights'].get(name, 0) != 0 for name in absent)],
                  output_sha256=digest(target), installed=False,
                  scope='Interchange only; current-mesh correspondence requires native verification.')
    (args.output / 'export-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('UNHOLY_FACE_EXPORT_PASS', len(shapes), len(used), flush=True)


if __name__ == '__main__':
    main()
