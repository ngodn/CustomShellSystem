"""Prepare a native cloth trial from the aligned author's proxy and existing skin weights."""
import json
from pathlib import Path
import numpy as np

work=Path(__file__).resolve().parents[2]/'work/eve26'
out=work/'planet-tail-cloth.json'
assert not out.exists()
proxy=json.loads((work/'planet-tail-proxy.json').read_text())
mesh=json.loads((work/'planet-export/planet.mesh.json').read_text())
p=np.array(proxy['positions_cm']);triangles=np.array(proxy['triangles'])
normals=np.zeros_like(p)
for t in triangles:
    cross=np.cross(p[t[1]]-p[t[0]],p[t[2]]-p[t[0]])
    assert np.linalg.norm(cross)>1e-5
    normals[t]+=cross
normals/=np.linalg.norm(normals,axis=1)[:,None]
start=proxy['target_render_start'];points=np.array(mesh['points'][start:start+840])
nearest=np.argmin(np.linalg.norm(p[:,None,:]-points[None,:,:],axis=2),axis=1)+start
weights={int(i):[] for i in nearest}
for vertex,bone,weight in mesh['influences']:
    if vertex in weights:
        weights[vertex].append([mesh['bones'][bone]['name'],weight])
for value in weights.values():
    assert value and abs(sum(w for _,w in value)-1)<.0001
# Preserve the author's four pins. The 18 cm cap is a conservative trial limit,
# not a translation of Blender's solver settings.
map_values=[0 if i in proxy['pinned_vertices'] else min(18,max(0,(p[2,2]-v[2])*.35)) for i,v in enumerate(p)]
assert sum(v==0 for v in map_values)==4
out.write_text(json.dumps({'slots':{'PlanetTail_17':{'positions':p.tolist(),'normals':normals.tolist(),
    'indices':triangles.flatten().tolist(),'weights':[weights[int(i)] for i in nearest],
    'max_distances':map_values}},'scope':'Native trial input; render mapping, motion and collision unverified'},indent=2)+'\n')
print('Prepared authored tail proxy:',len(p),'vertices; four pins; nearest existing tail skin weights')
