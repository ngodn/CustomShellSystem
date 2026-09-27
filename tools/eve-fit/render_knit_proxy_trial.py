"""View native Knitwear cloth particles against the matching posed body, not the final render mesh."""
import argparse
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--trial', type=int, choices=[2,3], required=True)
parser.add_argument('--case', choices=['sprint','no-body'], default='sprint')
parser.add_argument('--frame', type=int, default=56)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
work = Path(__file__).resolve().parents[2]/'work/eve26'
out = work/f'knit-cloth{args.trial}'/f'{args.case}-view{args.frame}'
assert not out.exists()
motion = json.loads((work/f'knit-cloth{args.trial}/{args.case}.json').read_text())
proxy = json.loads((work/f'knit-cloth{args.trial}/proxy.json').read_text())['slots']['Collar-1']
mesh = json.loads((work/'knit-w2/knit.mesh.json').read_text())
audit = json.loads((work/'knit-w2/knit.mesh.audit.json').read_text())
frame = motion['frames'][args.frame]
snapshot = frame['pose']['Snapshot']
entries = dict(zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True))
bind, pose = [], []
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']),Quaternion((q[3],*q[:3])),Vector(bone['scale']))
    bind.append(bind[bone['parent']] @ local if bone['parent']>=0 else local)
    t = entries[bone['name']]
    local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
    pose.append(pose[bone['parent']] @ local if bone['parent']>=0 else local)
matrices = np.asarray([np.asarray(a @ b.inverted()) for a,b in zip(pose,bind,strict=True)])
count = audit['parts'][0]['points']
points = np.asarray(mesh['points'])[:count]
rows = np.asarray([row for row in mesh['influences'] if row[0]<count])
vi,bi,wt = rows[:,0].astype(int),rows[:,1].astype(int),rows[:,2]
body = np.zeros_like(points)
np.add.at(body,vi,(np.einsum('nij,nj->ni',matrices[bi,:3,:3],points[vi])+matrices[bi,:3,3])*wt[:,None])
body_faces = [[mesh['wedges'][i][0] for i in face[:3]] for face in mesh['faces'][:audit['parts'][0]['faces']]]
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
def add(name, vertices, faces, color):
    vertices = np.asarray(vertices)/100
    vertices[:,1] *= -1
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices.tolist(),[],[list(reversed(face)) for face in faces])
    material = bpy.data.materials.new(name)
    material.diffuse_color = color
    data.materials.append(material)
    obj = bpy.data.objects.new(name,data)
    bpy.context.collection.objects.link(obj)
    for polygon in data.polygons: polygon.use_smooth = True
add('Body',body,body_faces,(.5,.5,.5,1))
add('Native cloth proxy',frame['positions_cm'],np.asarray(proxy['indices']).reshape(-1,3).tolist(),(.04,.35,.7,1))
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.light = 'STUDIO'
scene.display.shading.show_cavity = True
scene.render.resolution_x = 720
scene.render.resolution_y = 960
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
camera = bpy.data.objects.new('Review camera',bpy.data.cameras.new('Review camera'))
bpy.context.collection.objects.link(camera)
scene.camera = camera
camera.data.type = 'ORTHO'
camera.data.ortho_scale = 1.05
out.mkdir()
for name, location in [('front',(0,-3,1.15)),('back',(0,3,1.15)),('side',(3,0,1.15))]:
    camera.location = location
    camera.rotation_euler = (Vector((0,0,1.15))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath = str(out/f'{name}.png')
    bpy.ops.render.render(write_still=True)
(out/'scope.txt').write_text('Native proxy particles against the same default-morph posed body. No render mapping, textures, body masks or game acceptance.\n')
