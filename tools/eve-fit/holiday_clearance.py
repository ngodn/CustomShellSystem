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
p.add_argument('--outer',action='store_true',help='Correct outward-facing fabric; exclude fur roots and inner walls')
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
 obj.data.calc_loop_triangles()
 selected=[t for t in obj.data.loop_triangles if not a.outer or 'Fur' not in obj.data.materials[t.material_index].name]
 faces=np.asarray([t.vertices[:] for t in selected])
 history=[];budget_reached=False
 for iteration in range(12):
  correction=np.zeros_like(cloth);counts=np.zeros(len(cloth),dtype=np.float32)
  failures=0;penetrations=0;minimum=1
  for ids in faces:
   triangle=cloth[ids]
   outward=Vector(np.cross(triangle[1]-triangle[0],triangle[2]-triangle[0])).normalized()
   for bary in samples:
    point=Vector(bary @ triangle)
    hit,normal,_,distance=tree.find_nearest(point,.008)
    if hit is None:continue
    if a.outer and outward.dot(normal)<.15:continue
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
  step=correction[active]/counts[active,None]
  lengths=np.linalg.norm(step,axis=1)
  step*=np.minimum(1,.002/np.maximum(lengths,1e-12))[:,None]
  proposed=cloth.copy();proposed[active]+=step
  budget=.012 if a.outer else .008
  if np.linalg.norm(proposed-original,axis=1).max()>budget:
   budget_reached=True
   print(part,'Stopped at local correction budget; unresolved samples remain',flush=True)
   break
  cloth=proposed
 offset=cloth-original
 snapshots={key.name:coords(key.data) for key in obj.data.shape_keys.key_blocks}
 basis_name=obj.data.shape_keys.key_blocks[0].name
 basis_error=float(np.max(np.abs(snapshots[basis_name]-original)))
 assert basis_error<1e-6,(part,'Input Basis differs from mesh',basis_error)
 obj.data.vertices.foreach_set('co',cloth.ravel())
 for key in obj.data.shape_keys.key_blocks:
  changed=cloth if key.name==basis_name else cloth+(snapshots[key.name]-snapshots[basis_name])
  key.data.foreach_set('co',changed.ravel())
 obj.data.update()
 assert np.array_equal(coords(obj.data.vertices),cloth)
 assert np.array_equal(coords(obj.data.shape_keys.key_blocks[0].data),cloth)
 report[part]=dict(history=history,max_move_mm=float(np.linalg.norm(offset,axis=1).max()*1000),budget_reached=budget_reached,sampled_clearance_passed=history[-1]['samples_below_clearance']==0)
assert digest(body)==before
bpy.ops.wm.save_as_mainfile(filepath=str(a.output))
a.output.with_suffix('.json').write_text(json.dumps(dict(body_unchanged=True,body_sha256=before,parts=report,outer_fabric_only=a.outer,scope='Local 8 mm search-radius sample clearance only. Not proof of all intersections or morph/motion acceptance.'),indent=2)+'\n')
print('CLEARANCE_CANDIDATE_SAVED')
