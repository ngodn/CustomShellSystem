"""Bound skin-weight gradients across connected fabric and coincident seam vertices."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils.kdtree import KDTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords,digest

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);assert not a.output.exists()
body=bpy.data.objects['Eve Body'];before=digest(body);report={}
for part in ('Dress','Arms','Panties'):
 obj=bpy.data.objects['Eve Christmas - '+part];points=coords(obj.data.vertices)
 snapshots={key.name:coords(key.data) for key in obj.data.shape_keys.key_blocks}
 weights=np.zeros((len(points),len(obj.vertex_groups)),dtype=np.float64)
 for vertex in obj.data.vertices:
  for group in vertex.groups:weights[vertex.index,group.group]=group.weight
 assert np.allclose(weights.sum(axis=1),1,atol=1e-5)
 original=weights.copy();pairs=set();used=set()
 for face in obj.data.polygons:
  ids=list(face.vertices);used.update(ids)
  pairs.update(tuple(sorted((i,j))) for i,j in zip(ids,ids[1:]+ids[:1]))
 tree=KDTree(len(used))
 for i in used:tree.insert(points[i],i)
 tree.balance();seams=0
 for i in used:
  for _,j,distance in tree.find_range(points[i],.0001):
   if j>i and (i,j) not in pairs:pairs.add((i,j));seams+=1
 edges=np.asarray(sorted(pairs));i,j=edges.T
 length=np.linalg.norm(points[i]-points[j],axis=1)
 limit=np.maximum(length,.0001)*15
 initial=float(np.max(np.linalg.norm(weights[i]-weights[j],axis=1)/np.maximum(length,.0001)))
 for iteration in range(500):
  difference=weights[i]-weights[j];amount=np.linalg.norm(difference,axis=1)
  excess=amount-limit;active=excess>1e-5
  if not active.any():break
  correction=difference[active]*(excess[active]/np.maximum(amount[active],1e-12))[:,None]*.5
  move=np.zeros_like(weights);degree=np.zeros(len(points))
  np.add.at(move,i[active],-correction);np.add.at(move,j[active],correction)
  np.add.at(degree,i[active],1);np.add.at(degree,j[active],1)
  selected=degree>0;weights[selected]+=move[selected]/degree[selected,None]
 assert weights.min()>-1e-8
 weights=np.maximum(weights,0);weights/=weights.sum(axis=1)[:,None]
 for vertex in obj.data.vertices:
  for group in obj.vertex_groups:group.remove([vertex.index])
  for index,value in enumerate(weights[vertex.index]):
   if value>0:obj.vertex_groups[index].add([vertex.index],float(value),'REPLACE')
 assert np.array_equal(coords(obj.data.vertices),points)
 for key in obj.data.shape_keys.key_blocks:assert np.array_equal(coords(key.data),snapshots[key.name])
 report[part]=dict(iterations=iteration+1,seam_links=seams,initial_max_gradient=initial,
  final_max_gradient=float(np.max(np.linalg.norm(weights[i]-weights[j],axis=1)/np.maximum(length,.0001))),
  unresolved_edges=int(np.sum(np.linalg.norm(weights[i]-weights[j],axis=1)-limit>1e-5)),
  max_weight_change=float(np.abs(weights-original).max()),geometry_and_morphs_unchanged=True)
assert digest(body)==before
bpy.ops.wm.save_as_mainfile(filepath=str(a.output))
a.output.with_suffix('.json').write_text(json.dumps(dict(parts=report,body_unchanged=True,scope='Weight continuity candidate only. Requires replay and cloth validation.'),indent=2)+'\n')
print('WEIGHT_CONTINUITY_SAVED')
