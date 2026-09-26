"""Build a separate lower-resolution cloth surface with original surface weights."""
import json
from pathlib import Path

import bpy
import numpy as np
from mathutils.bvhtree import BVHTree

w = Path(__file__).resolve().parents[2]/'work/eve26'
output = w/'panel-low-proxy.json'
assert not output.exists()
slot = 'MI_CH_P_EVE_Christmas_01_01.001'
source = json.loads((w/'skirt-proxies.json').read_text())['slots'][slot]
points = np.asarray(source['positions'])
faces = np.asarray(source['indices']).reshape((-1,3))
mesh = bpy.data.meshes.new('ClothProxy')
mesh.from_pydata(points.tolist(),[],faces.tolist())
obj = bpy.data.objects.new('ClothProxy',mesh)
bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
modifier = obj.modifiers.new('SimulationOnly','DECIMATE')
modifier.ratio = 1200/len(faces)
modifier.use_collapse_triangulate = True
bpy.ops.object.modifier_apply(modifier=modifier.name)
obj.data.calc_loop_triangles()
tree = BVHTree.FromPolygons(points.tolist(),faces.tolist(),all_triangles=True)
positions,weights,normals,transfer = [],[],[],[]
source_normals = np.asarray(source['normals'])
for vertex in obj.data.vertices:
    near,_,face,_ = tree.find_nearest(vertex.co)
    ids = faces[face]
    origin,b,c = points[ids]
    uv = np.linalg.lstsq(np.column_stack((b-origin,c-origin)),np.asarray(near)-origin,rcond=None)[0]
    bary = np.maximum([1-uv.sum(),*uv],0);bary /= bary.sum()
    assert np.linalg.norm(bary@points[ids]-np.asarray(near)) < .001
    positions.append((bary@points[ids]).tolist())
    normal = bary@source_normals[ids];normal /= np.linalg.norm(normal)
    normals.append(normal.tolist())
    influences = {}
    for i,factor in zip(ids,bary):
        for name,weight in source['weights'][i]:
            influences[name] = influences.get(name,0.)+factor*weight
    rows = sorted(((name,value) for name,value in influences.items() if value>1e-8),key=lambda row:-row[1])
    assert len(rows)<=12
    total = sum(v for _,v in rows)
    weights.append([[n,float(v/total)] for n,v in rows])
    transfer.append({'vertices':ids.tolist(),'barycentric':bary.tolist()})
indices = [i for triangle in obj.data.loop_triangles for i in triangle.vertices]
proxy = {'positions':positions,'normals':normals,'weights':weights,'indices':indices,
    'anchor_top_cm':float(points[:,2].max()),'transfer':transfer}
output.write_text(json.dumps({'scope':'Separate decimated simulation-surface trial. Original visible geometry unchanged; engine render mapping, motion and morph validation required.',
    'slots':{slot:proxy}})+'\n')
print(len(positions),'particles;',len(indices)//3,'triangles; anchor top',proxy['anchor_top_cm'])
