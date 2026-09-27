"""Apply measured Prototype suit offsets to a separate Blender garment library."""
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

import argparse
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--part',help='Exact garment object name; defaults to the selected garment preset')
parser.add_argument('--source',type=Path,default=MOD/'gemini-work/CSS_SeduXtress_Variants_Fixed.blend')
parser.add_argument('--mesh',type=Path,default=MOD/'gemini-work/exports/SK_Eve_PlanetDiving.mesh.json')
parser.add_argument('--offsets',type=Path,default=WORK/'planet-fit4/offsets.json')
parser.add_argument('--output',type=Path,default=WORK/'planet-suit-f4c.blend')
parser.add_argument('--receipt',type=Path,default=WORK/'planet-suit-f4c.json')
parser.add_argument('--garment',choices=('prototype','skin'),default='prototype')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
output = args.output
assert not output.exists()
assert not args.receipt.exists()
assert output.resolve().parent == WORK.resolve() and args.receipt.resolve().parent == WORK.resolve()
data = json.loads(args.mesh.read_text())
offsets = json.loads(args.offsets.read_text())['offsets']
parts = json.loads(args.mesh.with_suffix('.audit.json').read_text())['parts']
garment=args.part or {'prototype':'Eve Prototype Planet Diving Suit - Suit','skin':'Eve Skin Suit - Suit Complete'}[args.garment]
part_index=next(i for i,part in enumerate(parts) if part['name']==garment)
start=sum(part['points'] for part in parts[:part_index]);count=parts[part_index]['points']
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(args.source), link=False) as (source, loaded):
    loaded.objects = (['Eve Body'] if 'Eve Body' in source.objects else [])+[garment]
for item in loaded.objects:
    bpy.context.scene.collection.objects.link(item)
body = bpy.data.objects.get('Eve Body')
before = digest(body) if body else None
obj = bpy.data.objects[garment]
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
assert body is None or digest(body) == before, 'Body changed'
ignored_modifiers = [m.type for m in obj.modifiers]
for modifier in list(obj.modifiers):
    obj.modifiers.remove(modifier)
obj.data.shape_keys.animation_data_clear()
obj.animation_data_clear()
obj.parent = None
material_names = [m.name if m else '' for m in obj.data.materials]
obj.data.materials.clear()
for index, name in enumerate(material_names):
    material = bpy.data.materials.new(f'FitSlot{index}')
    material['CSS_source_material'] = name
    obj.data.materials.append(material)
for constraint in list(obj.constraints):
    obj.constraints.remove(constraint)
for key in list(obj.keys()):
    del obj[key]
obj['CSS_source_scope'] = 'Fitted garment geometry and relative shapes; attach to the production rig during assembly.'
bpy.data.libraries.write(str(output), {obj}, fake_user=True, compress=True)
args.receipt.write_text(json.dumps({
    'body_digest': before, 'body_unchanged': True, 'changed_suit_vertices': len(offsets),
    'max_offset_cm': float(np.linalg.norm(expected_delta,axis=1).max()), 'mapping_error_cm': mapping_error, 'fitted_output_error_cm': error,
    'relative_shapes_preserved': True, 'source': str(args.source), 'offsets': str(args.offsets),
    'export_ignored_modifiers_removed': ignored_modifiers, 'driver_dependencies_removed': True,
    'original_material_slots': material_names,
    'scope': 'Garment-only geometry library with frozen fit values and preserved relative shapes/weights. Export-ignored modifiers and drivers removed to avoid importing unrelated source rigs. Requires production-rig assembly, fresh reload, morph and gameplay pose checks.',
}, indent=2)+'\n')
print('PLANET_FIT_SAVED',output)
