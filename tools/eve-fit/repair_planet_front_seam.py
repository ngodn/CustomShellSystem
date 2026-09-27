"""Correct shallow front-seam breakthrough in the confirmed hip review region."""
import copy,hashlib,json
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
w=Path(__file__).resolve().parents[2]/'work/eve26'
p=w/'planet-fit10/planet.mesh.json';raw=p.read_bytes();m=json.loads(raw)
audit=json.loads(p.with_name('planet.mesh.audit.json').read_text())
assert hashlib.sha256(raw).hexdigest()==audit['output_sha256']
nb=audit['parts'][0]['points'];ns=audit['parts'][1]['points'];nf=audit['parts'][0]['faces']
faces=[[m['wedges'][j][0] for j in f[:3]] for f in m['faces']]
hidden=set(json.loads((w/'planet-body-mask.json').read_text())['hidden_body_faces'])
body_faces=[f for i,f in enumerate(faces[:nf]) if i not in hidden]
suit_faces=faces[nf:nf+audit['parts'][1]['faces']]
base=np.asarray(m['points']);morph=np.zeros_like(base)
for t in m['morph_targets']:
    if t['name'].startswith(('FBM','PBM')):
        for i,*v in t['deltas']:morph[i]+=v
origins=[Vector((x,100,z)) for x in np.arange(-17,17.01,.2) for z in np.arange(98,108.01,.2)]
direction=Vector((0,-1,0));offsets={};reports=[]
for maximum in [False,True]:
    points=[Vector(v) for v in base+(morph if maximum else 0)]
    body=BVHTree.FromPolygons(points,body_faces,all_triangles=True)
    suit=BVHTree.FromPolygons(points,suit_faces,all_triangles=True)
    hits=[]
    for origin in origins:
        bp,bn,bf,bd=body.ray_cast(origin,direction)
        sp,sn,sf,sd=suit.ray_cast(origin,direction)
        if bp is None or sp is None:continue
        penetration=sd-bd
        if not .005<penetration<.3:continue
        hits.append(penetration)
        for v in suit_faces[sf]:
            assert nb<=v<nb+ns
            offsets[v]=max(offsets.get(v,0),penetration+.06)
    reports.append(dict(maximum_morphs=maximum,shallow_ray_hits=len(hits),deepest_cm=max(hits,default=0)))
result=copy.deepcopy(m)
for v,delta in offsets.items():result['points'][v][1]+=delta
assert result['points'][:nb]==m['points'][:nb] and result['points'][nb+ns:]==m['points'][nb+ns:]
assert all(result[k]==v for k,v in m.items() if k!='points')
out=w/'planet-fit13';out.mkdir(exist_ok=False)
target=out/'planet.mesh.json';target.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(target.read_bytes()).hexdigest()
(out/'planet.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'offsets.json').write_text(json.dumps({'offsets':[[i,0,d,0] for i,d in offsets.items()]})+'\n')
report=dict(cases=reports,changed_vertices=len(offsets),max_offset_cm=max(offsets.values(),default=0),
    scope='Local front-ray clearance proposal, X+-17 cm Z98..108 cm, visible body faces, default and combined maximum morphs. Not full fitting or game acceptance.')
(out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
