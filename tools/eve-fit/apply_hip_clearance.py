"""Apply measured garment offsets to every relative shape in a separate Holiday blend."""
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT/'CustomShellSystem/work/eve26'
MOD = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD/'gemini-work'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_variant_clean import TO_UE, fitted_mesh
from holiday_candidate import coords, digest

output = WORK/'holiday-f11.blend'
assert not output.exists()
data = json.loads((WORK/'holiday.mesh.json').read_text())
offsets = json.loads((WORK/'hip-offsets.json').read_text())['offsets']
parts = json.loads((WORK/'holiday.mesh.audit.json').read_text())['parts']
start = parts[0]['points']
count = parts[1]['points']
assert parts[1]['name'] == 'Eve Christmas - Dress'
bpy.ops.wm.open_mainfile(filepath=str(WORK/'holiday-f10.blend'))
body = bpy.data.objects['Eve Body']
before = digest(body)
obj = bpy.data.objects['Eve Christmas - Dress']
transform = TO_UE @ obj.matrix_world

def evaluate():
    copy = obj.copy()
    copy.data = obj.data.copy()
    bpy.context.scene.collection.objects.link(copy)
    copy.hide_viewport = copy.hide_render = False
    copy.hide_set(False)
    mesh, _, _ = fitted_mesh(copy)
    mesh.calc_loop_triangles()
    used = set()
    for tri in mesh.loop_triangles:
        a, b, c = [transform @ mesh.vertices[i].co for i in tri.vertices]
        if (b-a).cross(c-a).length_squared >= 1e-12:
            used.update(tri.vertices)
    used = sorted(used)
    points = np.asarray([transform @ mesh.vertices[i].co for i in used])
    copied_data = copy.data
    bpy.data.objects.remove(copy, do_unlink=True)
    bpy.data.meshes.remove(copied_data)
    bpy.data.meshes.remove(mesh)
    return used, points

used, exported = evaluate()
expected = np.asarray(data['points'][start:start+count])
assert len(used) == count
mapping_error = float(np.linalg.norm(exported-expected, axis=1).max())
assert mapping_error < .0005, mapping_error
delta = np.zeros((len(obj.data.vertices), 3), dtype=np.float32)
expected_delta = np.zeros_like(expected)
inverse = transform.to_3x3().inverted()
for index, *value in offsets:
    assert start <= index < start+count
    delta[used[index-start]] = inverse @ Vector(value)
    expected_delta[index-start] = value
mesh_before = coords(obj.data.vertices)
keys = obj.data.shape_keys.key_blocks
snapshots = {key.name: coords(key.data) for key in keys}
assert np.allclose(mesh_before, snapshots[keys[0].name], atol=1e-7)
for key in keys:
    key.data.foreach_set('co', (snapshots[key.name]+delta).ravel())
obj.data.vertices.foreach_set('co', (mesh_before+delta).ravel())
obj.data.update()
for key in keys:
    old_relative = snapshots[key.name]-snapshots[key.relative_key.name]
    new_relative = coords(key.data)-coords(key.relative_key.data)
    assert np.allclose(old_relative, new_relative, atol=3e-7), key.name
used_after, actual = evaluate()
assert used_after == used
error = float(np.linalg.norm(actual-(expected+expected_delta), axis=1).max())
assert error < .0005, error
assert digest(body) == before, 'Body changed'
bpy.ops.wm.save_as_mainfile(filepath=str(output))
(WORK/'holiday-f11.json').write_text(json.dumps({
    'body_digest': before, 'body_unchanged': True, 'changed_dress_vertices': len(offsets),
    'max_offset_cm': .25, 'mapping_error_cm': mapping_error, 'fitted_output_error_cm': error,
    'relative_shapes_preserved': True, 'source': 'holiday-f10.blend',
    'scope': 'Local dress clearance source candidate. Requires fresh reload/export and gameplay pose checks.',
}, indent=2)+'\n')
print('HOLIDAY_F11_HIP_SAVED')
