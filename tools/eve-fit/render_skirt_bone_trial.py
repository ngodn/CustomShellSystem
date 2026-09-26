"""Inspect shared garment deformation with synthetic bone bends and combined body morphs."""
import json
import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
data = json.loads((WORK / 'holiday-bones.mesh.json').read_text())
audit = json.loads((WORK / 'holiday.mesh.audit.json').read_text())
out = WORK / 'skirt-bone-views'
out.mkdir(exist_ok=False)
local, bind, pose = [], [], []
for bone in data['bones']:
    q = bone['rotation']
    matrix = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    local.append(matrix)
    parent = bone['parent']
    bind.append(bind[parent] @ matrix if parent >= 0 else matrix)
    current = pose[parent] @ matrix if parent >= 0 else matrix.copy()
    if bone['name'].startswith('CSS_Cloth_Skirt_') and int(bone['name'][-2:]) <= 3:
        side = bone['name'].split('_')[-2]
        axis, sign = {'F': ('X', -1), 'B': ('X', 1), 'L': ('Y', -1), 'R': ('Y', 1)}[side]
        pivot = current.translation.copy()
        current = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(sign*4), 4, axis) @ Matrix.Translation(-pivot) @ current
    pose.append(current)
transforms = [np.asarray(p @ b.inverted()) for p, b in zip(pose, bind)]
base = np.asarray(data['points'], dtype=np.float64)
weights = {}
for vertex, bone, weight in data['influences']:
    weights.setdefault(vertex, []).append((bone, weight))
morphs = {m['name']: m['deltas'] for m in data['morph_targets']}
palette = [(1, .5, .04, 1), (.1, .4, .85, 1), (.9, .08, .1, 1), (.1, .8, .3, 1), (.7, .12, .85, 1)]
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
materials = []
for i, name in enumerate(data['materials']):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = palette[(i-16) % 5] if 16 <= i <= 20 else (.32, .32, .32, 1)
    materials.append(mat)
objects, start = [], 0
for part in audit['parts']:
    faces = data['faces'][start:start+part['faces']]
    start += part['faces']
    points = sorted({data['wedges'][w][0] for f in faces for w in f[:3]})
    lookup = {v: i for i, v in enumerate(points)}
    mesh = bpy.data.meshes.new(part['name'])
    mesh.from_pydata([(base[i, 0]/100, -base[i, 1]/100, base[i, 2]/100) for i in points], [],
                     [[lookup[data['wedges'][w][0]] for w in reversed(f[:3])] for f in faces])
    for mat in materials:
        mesh.materials.append(mat)
    for polygon, face in zip(mesh.polygons, faces):
        polygon.material_index = face[3]
        polygon.use_smooth = True
    obj = bpy.data.objects.new(part['name'], mesh)
    bpy.context.collection.objects.link(obj)
    objects.append((obj, points))
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_cavity = True
scene.render.resolution_x = 800
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
cam = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
bpy.context.collection.objects.link(cam)
scene.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = .70
rows = []
for label, selections in [('default', {}), ('hip-waist', {'PBMHipSize': 1., 'PBMWaistWidth': 1.})]:
    rest = base.copy()
    for name, amount in selections.items():
        for index, *delta in morphs[name]:
            rest[index] += np.asarray(delta)*amount
    deformed = np.zeros_like(rest)
    for i, row in weights.items():
        point = np.append(rest[i], 1.)
        deformed[i] = sum((weight*(transforms[bone] @ point)[:3] for bone, weight in row), np.zeros(3))
    assert np.isfinite(deformed).all()
    for obj, points in objects:
        xyz = deformed[points].copy()/100
        xyz[:, 1] *= -1
        obj.data.vertices.foreach_set('co', xyz.ravel())
        obj.data.update()
    rows.append({'case': label, 'morphs': selections, 'max_displacement_cm': float(np.linalg.norm(deformed-rest, axis=1).max())})
    for view, direction in [('front', (0, -3, 0)), ('side', (3, 0, 0))]:
        target = Vector((0, 0, 1.12))
        cam.location = target+Vector(direction)
        cam.rotation_euler = (target-cam.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(out/f'{label}-{view}.png')
        bpy.ops.render.render(write_still=True)
(out/'report.json').write_text(json.dumps({'rows': rows, 'scope': 'Synthetic 4-degree outward bend per joint on the first three bones of each skirt chain. No simulation, collisions or gameplay playback.'}, indent=2)+'\n')
