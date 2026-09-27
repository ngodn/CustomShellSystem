"""Rebuild garment morph deltas from corresponding unchanged body triangles."""
import argparse,copy,hashlib,json,sys
from pathlib import Path
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parents[2];work=root/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesh',type=Path,default=work/'planet-sections/planet.mesh.json')
parser.add_argument('--output',type=Path,default=work/'planet-morph-repair')
parser.add_argument('--garment',choices=('prototype','skin','bikini','knit'),default='prototype')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
p=args.mesh;raw=p.read_bytes();source=json.loads(raw)
audit=json.loads(p.with_suffix('.audit.json').read_text())
expected={'prototype':['Eve Prototype Planet Diving Suit - Suit'],'skin':['Eve Skin Suit - Suit Complete'],'bikini':['Eve Bikini - Top','Eve Bikini - Shorts'],'knit':['Eve Extras - Sweater']}[args.garment]
assert [part['name'] for part in audit['parts'][1:1+len(expected)]]==expected
assert hashlib.sha256(raw).hexdigest()==audit['output_sha256']
body_count=audit['parts'][0]['points'];suit_count=sum(part['points'] for part in audit['parts'][1:1+len(expected)])
points=[Vector((x,-y,z)) for x,y,z in source['points']]
faces=[[source['wedges'][w][0] for w in f[:3]] for f in source['faces'][:audit['parts'][0]['faces']]]
tree=BVHTree.FromPolygons(points[:body_count],faces,all_triangles=True)
correspondence=[]
for i in range(body_count,body_count+suit_count):
 location,normal,face,distance=tree.find_nearest(points[i]);indices=faces[face]
 a,b,c=[points[j] for j in indices];u,v,w=b-a,c-a,location-a
 denom=u.dot(u)*v.dot(v)-u.dot(v)**2
 assert denom>1e-12
 beta=(v.dot(v)*w.dot(u)-u.dot(v)*w.dot(v))/denom
 gamma=(u.dot(u)*w.dot(v)-u.dot(v)*w.dot(u))/denom
 weights=np.maximum([1-beta-gamma,beta,gamma],0);weights/=weights.sum()
 correspondence.append((indices,weights))
result=copy.deepcopy(source);report=[]
names={'FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth'}
for target in result['morph_targets']:
 if target['name'] not in names:continue
 before={d[0]:d[1:] for d in target['deltas']}
 body=np.zeros((body_count,3))
 for i,value in before.items():
  if i<body_count:body[i]=value
 replacement=[]
 for j,(indices,weights) in enumerate(correspondence,body_count):
  delta=sum((body[i]*w for i,w in zip(indices,weights)),np.zeros(3))
  if np.linalg.norm(delta)>1e-5:replacement.append([j,*delta.tolist()])
 target['deltas']=[d for d in target['deltas'] if not body_count<=d[0]<body_count+suit_count]+replacement
 report.append(dict(name=target['name'],old_max_cm=max((np.linalg.norm(v) for i,v in before.items() if body_count<=i<body_count+suit_count),default=0),new_max_cm=max((np.linalg.norm(d[1:]) for d in replacement),default=0),changed_suit_vertices=len(replacement)))
 assert [d for d in target['deltas'] if d[0]<body_count]==[d for d in source['morph_targets'][result['morph_targets'].index(target)]['deltas'] if d[0]<body_count]
assert all(result[k]==v for k,v in source.items() if k!='morph_targets')
out=args.output;out.mkdir(exist_ok=False)
stem={'prototype':'planet','skin':'skin','bikini':'bikini','knit':'knit'}[args.garment]
f=out/f'{stem}.mesh.json';f.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(f.read_bytes()).hexdigest()
(out/f'{stem}.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(raw).hexdigest(),morphs=report,body_and_base_unchanged=True,scope='Nearest-surface garment morph candidate. Source keys, accessories, motion and game integration require further validation.'),indent=2)+'\n')
print(report)
