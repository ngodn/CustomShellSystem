"""Compare saved War Aegis garment geometry without modifying either blend."""
import hashlib
import json
import sys
from pathlib import Path
import bpy
import numpy as np

root = Path(__file__).resolve().parents[3]
mod = root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
work = root/'CustomShellSystem/work/eve26'
sys.path.insert(0, str(mod/'gemini-work'))
from export_variant_clean import TO_UE, fitted_mesh

output = work/'aegis-source-compare.json'
assert not output.exists()
export = mod/'gemini-work/exports/SK_Eve_WarAegis.mesh.json'
data = json.loads(export.read_text())
audit = json.loads(export.with_suffix('.audit.json').read_text())
start = audit['parts'][0]['points']
count = audit['parts'][1]['points']
expected = np.asarray(data['points'][start:start+count])
rows = []
for source in (mod/'reference/body-type-variant-EVE/eve_beta10.blend', mod/'gemini-work/CSS_SeduXtress_Variants_Fixed.blend'):
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with bpy.data.libraries.load(str(source), link=False) as (_, loaded):
        loaded.objects = ['Eve War Aegis - Suit']
    obj = loaded.objects[0]
    bpy.context.scene.collection.objects.link(obj)
    world = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = world
    obj.hide_viewport = obj.hide_render = False
    obj.hide_set(False)
    obj.animation_data_clear()
    mesh, _, active = fitted_mesh(obj)
    mesh.calc_loop_triangles()
    transform = TO_UE @ world
    used = set()
    for triangle in mesh.loop_triangles:
        a,b,c = [transform @ mesh.vertices[i].co for i in triangle.vertices]
        if (b-a).cross(c-a).length_squared >= 1e-12:
            used.update(triangle.vertices)
    used = sorted(used)
    assert len(used) == count
    points = np.asarray([transform @ mesh.vertices[i].co for i in used])
    error = np.linalg.norm(points-expected, axis=1)
    rows.append(dict(source=str(source),source_sha256=before,active_keys=active,
        max_export_difference_cm=float(error.max()),median_export_difference_cm=float(np.median(error)),
        points=count))
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
output.write_text(json.dumps(dict(export_sha256=hashlib.sha256(export.read_bytes()).hexdigest(),comparisons=rows,
    scope='Saved fit values with armature modifiers excluded; point-order comparison. Not evaluated animation or clothing contact.'),indent=2)+'\n')
print(json.dumps(rows))
