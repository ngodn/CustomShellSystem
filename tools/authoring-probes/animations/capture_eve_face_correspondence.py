"""Read Eve's shape basis and fitted vertex order for an isolated head transfer.

Run in Blender 5.2.2 with --background --factory-startup --disable-autoexec.
The original assembly is opened without scripts and is never saved.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'))
from export_seduxtress_eve import fitted_mesh, TO_UE


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def coordinates(data):
    values = np.empty(len(data)*3, dtype=np.float32)
    data.foreach_get('co', values)
    return values.reshape(-1, 3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--head-audit', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists() or bpy.context.preferences.filepaths.use_scripts_auto_execute:
        raise ValueError('Use a new output directory and disable embedded scripts')
    protected = {str(path.resolve()): digest(path) for path in (args.source, args.head_audit)}
    audit = json.loads(args.head_audit.read_text())
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()), use_scripts=False)
    body = bpy.data.objects['Eve Body']
    mesh, keys = body.data, body.data.shape_keys.key_blocks
    folded = {key.name.casefold(): key for key in keys}
    if len(folded) != len(keys):
        raise ValueError('Ambiguous shape names')
    names = [m['name'] for m in audit['morphs'] if m['name'].casefold() in folded]
    selected = [folded[name.casefold()] for name in names]
    if not names or any(key.vertex_group for key in selected):
        raise ValueError('Missing or vertex-group-masked correspondence controls')
    points = coordinates(keys[0].data)
    deltas = np.array([coordinates(key.data)-coordinates(key.relative_key.data) for key in selected])
    mesh.calc_loop_triangles()
    triangles = np.array([tuple(face.vertices) for face in mesh.loop_triangles], dtype=np.int32)
    loops = np.array([tuple(face.loops) for face in mesh.loop_triangles], dtype=np.int32)
    uvs = np.array([tuple(uv.uv) for uv in mesh.uv_layers.active.data], dtype=np.float32)
    materials = np.array([face.material_index for face in mesh.loop_triangles], dtype=np.int32)
    fitted, _, _ = fitted_mesh(body)
    fitted.calc_loop_triangles()
    world = TO_UE @ body.matrix_world
    retained = set()
    for face in fitted.loop_triangles:
        a, b, c = (world @ fitted.vertices[index].co for index in face.vertices)
        if (b-a).cross(c-a).length_squared >= 1e-12:
            retained.update(face.vertices)
    used = np.array(sorted(retained), dtype=np.int32)
    fitted_points = np.array([tuple(world @ fitted.vertices[index].co) for index in used])
    if any(digest(Path(path)) != value for path, value in protected.items()):
        raise ValueError('An input changed during capture')
    args.output.mkdir(parents=True)
    output = args.output/'body-samples.npz'
    np.savez_compressed(output, points=points, deltas=deltas, names=np.array(names),
        triangles=triangles, triangle_uv=uvs[loops], materials=materials,
        used_points=used, fitted_points=fitted_points, matrix_world=np.array(body.matrix_world))
    report = dict(sources=protected, sources_unchanged=True, blender=bpy.app.version_string,
        vertices=len(points), retained_vertices=len(used), controls=len(names),
        materials=[material.name for material in mesh.materials], output_sha256=digest(output))
    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'CORRESPONDENCE_CAPTURE: {len(points)} vertices, {len(names)} controls')


if __name__ == '__main__':
    main()
