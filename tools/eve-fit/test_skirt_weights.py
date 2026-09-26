"""Compare proxy skinning before/after transferring leg influences to pelvis."""
import json
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
proxy = json.loads((WORK / 'skirt-surface.json').read_text())
bones = json.loads((WORK / 'holiday.mesh.json').read_text())['bones']
indices = {b['name']: i for i, b in enumerate(bones)}
leg_names = set()
bind = []
for bone in bones:
    parent = bone['parent']
    if bone['name'] in ('thigh_l', 'thigh_r') or (parent >= 0 and bones[parent]['name'] in leg_names):
        leg_names.add(bone['name'])
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    bind.append(bind[parent] @ local if parent >= 0 else local)
candidate = dict(proxy)
candidate['weights'] = []
changed = 0
for weights in proxy['weights']:
    row = dict(weights)
    transfer = sum(row.pop(name, 0) for name in leg_names)
    if transfer:
        changed += 1
        row['pelvis'] = row.get('pelvis', 0) + transfer
    candidate['weights'].append(list(row.items()))
out = WORK / 'skirt-weights'
out.mkdir(exist_ok=False)
(out / 'pelvis-proxy.json').write_text(json.dumps(candidate, separators=(',', ':')))
points = np.asarray(proxy['positions'])
homogeneous = np.column_stack((points, np.ones(len(points))))
edges = set()
triangles = np.asarray(proxy['indices']).reshape((-1, 3))
for face in triangles:
    for i in range(3):
        edges.add(tuple(sorted((int(face[i]), int(face[(i+1)%3])))))
edge = np.asarray(sorted(edges))
base = np.linalg.norm(points[edge[:,0]]-points[edge[:,1]], axis=1)
assert base.min() > 0
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('Proxy review')
mesh.from_pydata([(x/100, -y/100, z/100) for x,y,z in points], [], [list(reversed(t)) for t in triangles.tolist()])
obj = bpy.data.objects.new('Proxy review', mesh)
bpy.context.collection.objects.link(obj)
obj.color = (.12, .38, .55, 1)
for face in mesh.polygons:
    face.use_smooth = True
s = bpy.context.scene
s.render.engine = 'BLENDER_WORKBENCH'
s.display.shading.color_type = 'OBJECT'
s.display.shading.show_cavity = True
s.render.resolution_x = s.render.resolution_y = 800
s.render.resolution_percentage = 100
cam = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
bpy.context.collection.objects.link(cam)
s.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = .7
rows = []
for gait, filename in [('walk','walk22'), ('sprint','sprint0')]:
    motion = json.loads((WORK.parent / 'anim14' / (filename+'-component.json')).read_text())
    for frame in (0,10,20):
        snapshot = motion['frames'][frame]['pose']['Snapshot']
        assert snapshot['bIsValid']
        transforms = dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
        world = []
        for bone in bones:
            t = transforms[bone['name']]
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
            world.append(world[bone['parent']] @ local if bone['parent'] >= 0 else local)
        matrices = [np.asarray(w @ b.inverted()) for w,b in zip(world,bind,strict=True)]
        for name, source in [('original', proxy), ('pelvis', candidate)]:
            posed = np.zeros_like(points)
            for i, weights in enumerate(source['weights']):
                for bone, weight in weights:
                    posed[i] += (matrices[indices[bone]] @ homogeneous[i])[:3]*weight
            ratios = np.linalg.norm(posed[edge[:,0]]-posed[edge[:,1]],axis=1)/base
            rows.append({'gait':gait,'frame':frame,'weights':name,'max_edge_ratio':float(ratios.max()),'p99_edge_ratio':float(np.quantile(ratios,.99))})
            if frame != 20:
                continue
            for vertex, (x,y,z) in zip(mesh.vertices, posed, strict=True):
                vertex.co = (x/100,-y/100,z/100)
            mesh.update()
            center = Vector((float(posed[:,0].mean())/100,-float(posed[:,1].mean())/100,float(posed[:,2].mean())/100))
            for view, direction in [('front',(0,-3,0)),('side',(3,0,0))]:
                cam.location = center + Vector(direction)
                cam.rotation_euler = (center-cam.location).to_track_quat('-Z','Y').to_euler()
                s.render.filepath = str(out/f'{gait}-{name}-{view}.png')
                bpy.ops.render.render(write_still=True)
(out/'report.json').write_text(json.dumps({'changed_vertices':changed,'rows':rows,'scope':'Proxy linear skinning only; no body collision, morphs, cloth simulation or final garment validation'},indent=2)+'\n')
print(json.dumps(rows,indent=2))
