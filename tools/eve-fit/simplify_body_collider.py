"""Build a separate collision proxy and preserve source-surface transfer coordinates."""
import argparse
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--faces', type=int, default=2200)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--protect-hips', action='store_true')
p.add_argument('--lock-hips', action='store_true', help='Keep pelvis/butt-to-thigh transition vertices; may exceed the face budget')
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists() and 500 <= a.faces <= 10000
w = Path(__file__).resolve().parents[2]/'work/eve26'
source = json.loads((w/'body-collider.json').read_text())
points = np.asarray(source['positions'])
triangles = source['indices']
mesh = bpy.data.meshes.new('BodyCollision')
mesh.from_pydata(points.tolist(),[],triangles)
obj = bpy.data.objects.new('BodyCollision',mesh)
bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
modifier = obj.modifiers.new('CollisionOnly','DECIMATE')
modifier.ratio = a.faces/len(triangles)
modifier.use_collapse_triangulate = True
protected = 0
if a.protect_hips or a.lock_hips:
    group = obj.vertex_groups.new(name='CollapseAllowance')
    group.add(list(range(len(points))), 1., 'REPLACE')
    for index, row in enumerate(source['weights']):
        butt = sum(weight for bone, weight in row if bone.startswith('butt'))
        pelvis = sum(weight for bone, weight in row if bone == 'pelvis')
        thigh = sum(weight for bone, weight in row if bone.startswith('thigh'))
        if (butt + (pelvis if a.lock_hips else 0)) > .05 and thigh > .05:
            # Blender's collapse cost protects low group weights; zero prevents collapse.
            group.add([index], 0. if a.lock_hips else .02, 'REPLACE')
            protected += 1
    modifier.vertex_group = group.name
    modifier.vertex_group_factor = 1000.
bpy.ops.object.modifier_apply(modifier=modifier.name)
obj.data.calc_loop_triangles()
faces = [list(t.vertices) for t in obj.data.loop_triangles]
used = sorted({v for face in faces for v in face})
lookup = {v:i for i,v in enumerate(used)}
query_faces = [f for f in triangles if np.linalg.norm(np.cross(points[f[1]]-points[f[0]],points[f[2]]-points[f[0]])) > 1e-6]
tree = BVHTree.FromPolygons(points.tolist(),query_faces,all_triangles=True)
positions,weights,transfer = [],[],[]
max_discard = 0.
for v in used:
    nearest,_,face,distance = tree.find_nearest(obj.data.vertices[v].co)
    ids = query_faces[face]
    origin,b,c = points[ids]
    uv = np.linalg.lstsq(np.column_stack((b-origin,c-origin)),np.asarray(nearest)-origin,rcond=None)[0]
    bary = np.asarray([1-uv.sum(),*uv])
    bary = np.maximum(bary,0);bary /= bary.sum()
    projected = bary@points[ids]
    assert np.linalg.norm(projected-np.asarray(nearest)) < .001, "Unstable surface projection"
    combined = {}
    for vertex,amount in zip(ids,bary):
        for bone,weight in source['weights'][vertex]:
            combined[bone] = combined.get(bone,0)+float(amount)*weight
    rows = sorted(((bone,value) for bone,value in combined.items() if value>1e-7),key=lambda x:-x[1])
    total = sum(value for _,value in rows[:12])
    max_discard = max(max_discard,1-total)
    weights.append([[bone,value/total] for bone,value in rows[:12]])
    positions.append(projected.tolist())
    transfer.append({'source_vertices':[source['source_vertices'][i] for i in ids], 'barycentric':bary.tolist()})
report = {'scope':'Decimated collision mesh only, projected onto unchanged source body. Barycentric weights capped at native 12 influences. Pose, morph and native validation required. Not a visible-body edit.',
    'positions':positions,'indices':[[lookup[v] for v in face] for face in faces],
    'weights':weights,'transfer':transfer,'requested_faces':a.faces,'max_discarded_weight':max_discard,
    'protect_hips':a.protect_hips,'lock_hips':a.lock_hips,'protected_source_vertices':protected}
a.output.write_text(json.dumps(report)+'\n')
print(len(positions),'vertices',len(faces),'faces; maximum removed influence mass',max_discard)
