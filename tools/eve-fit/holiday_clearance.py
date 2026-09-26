"""Blender 5.2: bounded garment-only clearance pass on the F2 candidate."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords,digest,fitted_points

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
body=bpy.data.objects['Eve Body'];before=digest(body)
points=fitted_points(body);body.data.calc_loop_triangles()
tree=BVHTree.FromPolygons(points,[tuple(t.vertices) for t in body.data.loop_triangles],all_triangles=True)
report={}
samples=np.asarray(((1,0,0),(0,1,0),(0,0,1),(.5,.5,0),(.5,0,.5),(0,.5,.5),(1/3,1/3,1/3)))
for part in ('Dress','Arms','Legs','Panties'):
 obj=bpy.data.objects['Eve Christmas - '+part]
 assert obj.matrix_world == Matrix.Identity(4)
 original=coords(obj.data.vertices);cloth=original.copy()
 obj.data.calc_loop_triangles();faces=np.asarray([t.vertices[:] for t in obj.data.loop_triangles])
 history=[]
 for iteration in range(12):
  correction=np.zeros_like(cloth);counts=np.zeros(len(cloth),dtype=np.float32)
  failures=0;penetrations=0;minimum=1
  for ids in faces:
   triangle=cloth[ids]
   for bary in samples:
    point=Vector(bary @ triangle)
    hit,normal,_,distance=tree.find_nearest(point,.008)
    if hit is None:continue
    signed=(point-hit).dot(normal);minimum=min(minimum,signed)
    if signed>=.0006:continue
    failures+=1;penetrations+=signed<0
    delta=np.asarray(normal)*(.0012-signed)
    for vi,w in zip(ids,bary):
     if w:
      correction[vi]+=delta*w;counts[vi]+=w
  history.append(dict(samples_below_clearance=failures,penetrating_samples=int(penetrations),minimum_signed_m=minimum))
  print(part,iteration,history[-1],flush=True)
  if not failures or iteration==11:break
  active=counts>0
  cloth[active]+=correction[active]/counts[active,None]
  if np.linalg.norm(cloth-original,axis=1).max()>.008:
   raise ValueError(part+' requires more than the 8 mm local correction budget')
 offset=cloth-original
 obj.data.vertices.foreach_set('co',cloth.ravel())
 for key in obj.data.shape_keys.key_blocks:
  key.data.foreach_set('co',(coords(key.data)+offset).ravel())
 obj.data.update()
 report[part]=dict(history=history,max_move_mm=float(np.linalg.norm(offset,axis=1).max()*1000))
assert digest(body)==before
bpy.ops.wm.save_as_mainfile(filepath=str(a.output))
a.output.with_suffix('.json').write_text(json.dumps(dict(body_unchanged=True,body_sha256=before,parts=report,scope='Local 8 mm search-radius sample clearance only. Not proof of all intersections or morph/motion acceptance.'),indent=2)+'\n')
print('CLEARANCE_CANDIDATE_SAVED')
