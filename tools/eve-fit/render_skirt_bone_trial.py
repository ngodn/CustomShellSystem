"""Inspect shared garment deformation with synthetic bone bends and combined body morphs."""
import json
import argparse
import sys
import math
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh', type=Path, default=WORK/'holiday-bones.mesh.json')
parser.add_argument('--output', type=Path, default=WORK/'skirt-bone-views')
parser.add_argument('--motion', type=Path, help='Use a measured UE local-pose snapshot instead of synthetic bends')
parser.add_argument('--frame', type=int, default=0)
parser.add_argument('--upstream', action='store_true', help='Render the recorded pose before secondary dynamics')
parser.add_argument('--views', nargs='+', choices=('front','side','rear'), default=['front','side'])
parser.add_argument('--lower-dress', action='store_true', help='Center the camera on the posed lower dress')
parser.add_argument('--surface-motion', type=Path, help='Apply an offline fabric surface only to its matching pose and morph case')
parser.add_argument('--transfer-trim', action='store_true', help='Diagnostic barycentric displacement transfer to other dress sections')
parser.add_argument('--rotate-trim', action='store_true', help='Rotate attachment offsets with the fabric triangle, without scaling the offsets')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
data = json.loads(args.mesh.read_text())
motion_data = json.loads(args.motion.read_text()) if args.motion else None
record = motion_data['frames'][args.frame] if motion_data else None
surface_motion = json.loads(args.surface_motion.read_text()) if args.surface_motion else None
assert not args.transfer_trim or surface_motion
assert not args.rotate_trim or args.transfer_trim
if surface_motion:
    assert args.motion and not args.upstream
    assert Path(surface_motion['source_motion']).resolve() == args.motion.resolve()
    surface_frame = surface_motion['frames'][args.frame]
    assert surface_frame['frame'] == args.frame
assert not args.upstream or record, '--upstream requires --motion'
snapshot = (record['upstream'] if args.upstream else record['pose']['Snapshot']) if record else None
measured = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True)) if snapshot else None
if measured:
    assert snapshot['bIsValid'] and len(measured) == len(snapshot['BoneNames'])
    assert all(b['name'] in measured for b in data['bones'])
audit = json.loads((WORK / 'holiday.mesh.audit.json').read_text())
out = args.output
out.mkdir(exist_ok=False)
local, bind, pose = [], [], []
for bone in data['bones']:
    q = bone['rotation']
    matrix = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    local.append(matrix)
    parent = bone['parent']
    bind.append(bind[parent] @ matrix if parent >= 0 else matrix)
    if measured:
        entry = measured[bone['name']]
        pose_local = Matrix.LocRotScale(Vector([entry['Translation'][k] for k in 'XYZ']),
            Quaternion([entry['Rotation'][k] for k in 'WXYZ']), Vector([entry['Scale3D'][k] for k in 'XYZ']))
    else:
        pose_local = matrix
    current = pose[parent] @ pose_local if parent >= 0 else pose_local.copy()
    if not measured and bone['name'].startswith('CSS_Cloth_Skirt_') and int(bone['name'][-2:]) <= 3:
        side = bone['name'].split('_')[-2]
        axis, sign = {'F': ('X', -1), 'B': ('X', 1), 'L': ('Y', -1), 'R': ('Y', 1)}[side]
        pivot = current.translation.copy()
        current = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(sign*4), 4, axis) @ Matrix.Translation(-pivot) @ current
    pose.append(current)
transforms = [np.asarray(p @ b.inverted()) for p, b in zip(pose, bind)]
base = np.asarray(data['points'], dtype=np.float64)
main_slot=data['materials'].index('MI_CH_P_EVE_Christmas_01_01.001')
lower_vertices=sorted({data['wedges'][w][0] for f in data['faces'] if f[3]==main_slot
                       for w in f[:3] if data['points'][data['wedges'][w][0]][2]<120})
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
    if surface_motion and label != surface_motion['morph_case']:
        continue
    selections = {**(record.get('morphs', {}) if record else {}), **selections}
    rest = base.copy()
    for name, amount in selections.items():
        assert name in morphs, name
        for index, *delta in morphs[name]:
            rest[index] += np.asarray(delta)*amount
    deformed = np.zeros_like(rest)
    for i, row in weights.items():
        point = np.append(rest[i], 1.)
        deformed[i] = sum((weight*(transforms[bone] @ point)[:3] for bone, weight in row), np.zeros(3))
    assert np.isfinite(deformed).all()
    if surface_motion:
        if args.transfer_trim:
            source_ids = surface_motion['source_vertices']
            lookup = {v:i for i,v in enumerate(source_ids)}
            triangles = [[lookup[data['wedges'][w][0]] for w in f[:3]] for f in data['faces']
                         if f[3] == main_slot and all(data['wedges'][w][0] in lookup for w in f[:3])]
            tree = BVHTree.FromPolygons(rest[source_ids].tolist(), triangles, all_triangles=True)
            displacements = np.asarray(surface_frame['positions_cm'])-deformed[source_ids]
            old_surface = deformed[source_ids].copy()
            new_surface = np.asarray(surface_frame['positions_cm'])
            rotations = {}
            dress_slots = {data['materials'].index(name) for name in (
                'MI_CH_P_EVE_Christmas_01_01.001', 'MI_CH_P_EVE_Christmas_01_Decal.001',
                'MI_EVE_HR_Christmas_01_Fur.001', 'MI_EVE_HR_15_Emissive1.001',
                'MI_CH_P_EVE_Christmas_01_03.001')}
            attached = {data['wedges'][w][0] for f in data['faces'] if f[3] in dress_slots for w in f[:3]}-set(source_ids)
            assert all(v >= audit['parts'][0]['points'] for v in attached)
            max_distance = 0.
            for v in sorted(attached):
                point, _, face, distance = tree.find_nearest(Vector(rest[v]))
                tri = triangles[face]
                origin, b, c = rest[np.asarray(source_ids)[tri]]
                basis = np.column_stack((b-origin, c-origin))
                uv = np.linalg.lstsq(basis, np.asarray(point)-origin, rcond=None)[0]
                bary = np.asarray([1-uv.sum(), *uv])
                assert bary.min() > -1e-4 and bary.max() < 1.0001
                if args.rotate_trim:
                    if face not in rotations:
                        bases = []
                        for positions in (old_surface, new_surface):
                            first = positions[tri[1]]-positions[tri[0]]
                            second = positions[tri[2]]-positions[tri[0]]
                            normal = np.cross(first, second)
                            assert np.linalg.norm(normal) > 1e-9
                            bases.append(np.column_stack((first, second, normal/np.linalg.norm(normal))))
                        u, _, vt = np.linalg.svd(bases[1]@np.linalg.inv(bases[0]))
                        fix = np.diag((1., 1., np.linalg.det(u@vt)))
                        rotations[face] = u@fix@vt
                    offset = deformed[v]-bary@old_surface[tri]
                    deformed[v] = bary@new_surface[tri]+rotations[face]@offset
                else:
                    deformed[v] += bary@displacements[tri]
                max_distance = max(max_distance, distance)
            rows.append({'attachment_vertices':len(attached), 'max_reference_distance_cm':max_distance,
                         'method': 'Diagnostic triangle rotation and displacement transfer' if args.rotate_trim else 'Diagnostic displacement transfer only',
                         'independent_attachment_collision': False})
        deformed[surface_motion['source_vertices']] = np.asarray(surface_frame['positions_cm'])
    for obj, points in objects:
        xyz = deformed[points].copy()/100
        xyz[:, 1] *= -1
        obj.data.vertices.foreach_set('co', xyz.ravel())
        obj.data.update()
    rows.append({'case': label, 'morphs': selections, 'max_displacement_cm': float(np.linalg.norm(deformed-rest, axis=1).max())})
    directions={'front':(0,-3,0),'side':(3,0,0),'rear':(0,3,0)}
    for view in args.views:
        direction=directions[view]
        target = Vector((0, 0, 1.12))
        if args.lower_dress:
            center=np.mean(deformed[lower_vertices],axis=0)/100
            center[1]*=-1
            target=Vector(center)
        cam.location = target+Vector(direction)
        cam.rotation_euler = (target-cam.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(out/f'{label}-{view}.png')
        bpy.ops.render.render(write_still=True)
scope = ('Recorded pose applied to exported weights; source_scope identifies measured versus generated transforms. Combined morph view reuses that pose; it does not rerun collision for changed body shapes.'
         if record else 'Synthetic 4-degree outward bend per joint on the first three bones of each skirt chain. No simulation, collisions or gameplay playback.')
(out/'report.json').write_text(json.dumps({'rows': rows, 'motion': str(args.motion) if record else None,
    'frame': args.frame if record else None, 'upstream': args.upstream,
    'views':args.views,'lower_dress':args.lower_dress,
    'surface_motion': str(args.surface_motion) if surface_motion else None,
    'surface_scope': surface_motion['scope'] if surface_motion else None,
    'transfer_trim': args.transfer_trim,
    'rotate_trim': args.rotate_trim,
    'source_scope': motion_data.get('scope', 'Measured editor evaluation') if motion_data else None,
    'scope': scope}, indent=2)+'\n')
