"""Measure a separate weighted collision proxy against the unchanged source body."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--proxy',type=Path,required=True)
p.add_argument('--native',type=Path)
p.add_argument('--output',type=Path,required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((w/'holiday.mesh.json').read_text())
source = json.loads((w/'body-collider.json').read_text())
proxy = json.loads(a.proxy.read_text())
motion = json.loads((w/'follow-sprint-base.json').read_text())
native = json.loads(a.native.read_text()) if a.native else None
names = {b['name']:i for i,b in enumerate(mesh['bones'])}
bind = []
for b in mesh['bones']:
    q = b['rotation'];m = Matrix.LocRotScale(Vector(b['translation']),Quaternion((q[3],*q[:3])),Vector(b['scale']))
    bind.append(bind[b['parent']]@m if b['parent']>=0 else m)
inverse = [m.inverted() for m in bind]

def influences(rows):
    entries = np.asarray([(v,names[n],weight) for v,row in enumerate(rows) for n,weight in row])
    return entries[:,0].astype(int),entries[:,1].astype(int),entries[:,2]
source_inf,proxy_inf = influences(source['weights']),influences(proxy['weights'])

def matrices(frame):
    s = motion['frames'][frame]['pose']['Snapshot'];e = dict(zip(s['BoneNames'],s['LocalTransforms'],strict=True));pose = []
    for b in mesh['bones']:
        t = e[b['name']]
        m = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
        pose.append(pose[b['parent']]@m if b['parent']>=0 else m)
    return np.asarray([np.asarray(m@b) for m,b in zip(pose,inverse)])

def skin(points,inf,mats):
    vi,bi,weight = inf; result = np.zeros_like(points)
    np.add.at(result,vi,(np.einsum('nij,nj->ni',mats[bi,:3,:3],points[vi])+mats[bi,:3,3])*weight[:,None])
    return result

base = np.asarray(mesh['points'])
body_ids = source['source_vertices']
roi = np.flatnonzero((np.asarray(source['positions'])[:,2]>=90)&(np.asarray(source['positions'])[:,2]<=125))
transfer_ids = np.asarray([t['source_vertices'] for t in proxy['transfer']])
bary = np.asarray([t['barycentric'] for t in proxy['transfer']])
morphs = {m['name']:m['deltas'] for m in mesh['morph_targets']}
reports = []
for label,selections in [('default',{}),('hip-waist',{'PBMHipSize':1.,'PBMWaistWidth':1.})]:
    rest = base.copy()
    for name,amount in selections.items():
        for vertex,*delta in morphs[name]:rest[vertex] += np.asarray(delta)*amount
    proxy_rest = np.asarray(proxy['positions'])+np.einsum('ni,nij->nj',bary,rest[transfer_ids]-base[transfer_ids])
    for frame in (0,4,7,16,32,48,64):
        mats = matrices(frame);body = skin(rest[body_ids],source_inf,mats);candidate = skin(proxy_rest,proxy_inf,mats)
        tree = BVHTree.FromPolygons(candidate.tolist(),proxy['indices'],all_triangles=True)
        distances = np.asarray([tree.find_nearest(Vector(body[i]))[3] for i in roi])
        report = {'case':label,'frame':frame,'body_to_proxy_roi_max_cm':float(distances.max()),'body_to_proxy_roi_p95_cm':float(np.percentile(distances,95)),
            'worst_body_vertices':[{'source_vertex':body_ids[int(roi[i])], 'rest_cm':source['positions'][int(roi[i])], 'weights':source['weights'][int(roi[i])], 'distance_cm':float(distances[i])} for i in np.argsort(distances)[-8:][::-1]]}
        if native and label=='default' and frame<len(native['frames']):
            actual = np.asarray(native['frames'][frame]['body_reference_skin_cm'])
            assert actual.shape==candidate.shape
            report['native_skin_max_cm'] = float(np.linalg.norm(actual-candidate,axis=1).max())
        reports.append(report)
        print({k:v for k,v in report.items() if k!='worst_body_vertices'},flush=True)
output = {'scope':'Body-to-proxy vertex samples in rest-z90..125 region over seven sprint poses and one combined morph case. Native comparison uses the native shape skinning API, not exported internal solver collider state. No full-surface, all-morph or game acceptance.',
    'proxy':str(a.proxy),'roi_vertices':len(roi),'cases':reports}
a.output.write_text(json.dumps(output,indent=2)+'\n')
if native:assert max(r.get('native_skin_max_cm',0) for r in reports)<.02,'Native skinning mismatch'
