"""Check isolated support geometry against V44 and measured poses. Blender 5.2.2."""
import collections
import hashlib
import json
import os
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Matrix,Quaternion,Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[4];WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import read_bones
out=Path(os.environ.get('CSS_HEEL_VERIFY_EXPORT_DIR', str(WORK/'heel-support-export-v2'))).resolve();assert out.parent==WORK
old_path=WORK/'arm-rest-correctives-export-v1/candidate.mesh.json';new_path=out/'candidate.mesh.json'
a=json.loads(old_path.read_text());b=json.loads(new_path.read_text());n=len(a['points'])
assert len(b['points'])==n+168
for k in ('bones','materials'):assert a[k]==b[k],k
for k in ('points','wedges','faces','normals','colors','influences'):assert b[k][:len(a[k])]==a[k],k
assert a['uv_channels']==b['uv_channels']
for old,new in zip(a['morph_targets'],b['morph_targets'],strict=True):
 assert old['name']==new['name'];assert [row for row in new['deltas'] if row[0]<n]==old['deltas']
parts=json.loads((out/'candidate.mesh.audit.json').read_text())['parts'];assert parts[-1]['material_slots']==[20]
assert parts[-1]['baked_fit_keys'].get('FBMBodyTone',0)==0, 'Public delta baked twice into support base'
triangles=[[b['wedges'][w][0] for w in f[:3]] for f in b['faces'][len(a['faces']):]]
edges=collections.Counter(tuple(sorted((t[i],t[(i+1)%3]))) for t in triangles for i in range(3))
assert all(v==2 for v in edges.values()), 'Support is not closed/manifold'
assert all(b['faces'][i][3]==20 for i in range(len(a['faces']),len(b['faces'])))
assert len(triangles)==328
positions=np.array(b['points'],dtype=float)
areas=[np.linalg.norm(np.cross(positions[t[1]]-positions[t[0]],positions[t[2]]-positions[t[0]]))/2 for t in triangles]
assert min(areas)>1e-5
# Include the old shoes plus new supports. Other regions are verified unchanged
# above and deliberately excluded from this bounded attachment check.
shoe_ids=set(range(53472,57672));ids=sorted(shoe_ids|set(range(n,n+168)));index={v:i for i,v in enumerate(ids)}
points=positions[ids];weights={}
for v,bone,w in b['influences']:
 if v in index:weights.setdefault(bone,[]).append((index[v],w))
weights={bone:(np.array([v for v,w in rows]),np.array([w for v,w in rows])) for bone,rows in weights.items()}
bones,bind=read_bones(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json');inverse=[m.inverted() for m in bind]
faces=[[index[b['wedges'][w][0]] for w in reversed(f[:3])] for f in b['faces'][:len(a['faces'])] if all(b['wedges'][w][0] in shoe_ids for w in f[:3])]
delta=np.zeros_like(points)
for morph in b['morph_targets']:
 for v,*change in morph['deltas']:
  if v in index:
   assert morph['name']=='FBMBodyTone';delta[index[v]]=change
samples=json.loads((WORK/'b2-body-hand-motion-v1/animation-raw.json').read_text())['samples']
poses=[('neutral',bind)]
for sample in samples:
 if 'raw_pose' not in sample:continue
 pose=[]
 for row,bone in zip(sample['raw_pose'],bones,strict=True):
  assert row['bone'].lower()==bone['name'].lower()
  x,y,z,w=row['rotation'];pose.append(Matrix.LocRotScale(Vector(row['translation']),Quaternion((w,x,y,z)),Vector(row['scale'])))
 poses.append((str(sample['frame']),pose))
collar=[index[n+s*84+72+i] for s in range(2) for i in range(12)]
rows=[]
for frame,pose in poses:
 for morph in (0,1):
  fitted=points+delta*morph;skin=np.zeros_like(fitted)
  for bone,(vertices,values) in weights.items():
   mat=np.array(pose[bone]@inverse[bone],dtype=float)
   skin[vertices]+=(fitted[vertices]@mat[:3,:3].T+mat[:3,3])*values[:,None]
  bvh=BVHTree.FromPolygons([Vector(p) for p in skin],faces,all_triangles=True)
  distances=[];signed=[]
  for vertex in collar:
   point=Vector(skin[vertex]);near,normal,face,distance=bvh.find_nearest(point)
   assert near is not None
   distances.append(distance);signed.append((point-near).dot(normal))
  rows.append(dict(frame=frame,morph=morph,max_distance_cm=max(distances),max_signed_cm=max(signed)))
report=dict(original_points_preserved=n,original_triangles_preserved=len(a['faces']),bones=379,materials=30,morphs=22,added_vertices=168,added_triangles=328,manifold=True,minimum_triangle_area_cm2=min(areas),collar_samples=rows,maximum_collar_distance_cm=max(r['max_distance_cm'] for r in rows),maximum_signed_collar_distance_cm=max(r['max_signed_cm'] for r in rows),sha256=hashlib.sha256(new_path.read_bytes()).hexdigest(),scope='Exact original payload preservation, closed support topology, and nearest-surface collar attachment through measured component poses at both body-tone endpoints. No cooked or live support acceptance.')
# Unsigned gate bounds the attachment seam independent of winding conventions.
report['passed']=report['maximum_collar_distance_cm']<.5 and report['maximum_signed_collar_distance_cm']<=.05
(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='collar_samples'}),flush=True)
assert report['passed'], 'Collar separated too far from the existing shoe'
