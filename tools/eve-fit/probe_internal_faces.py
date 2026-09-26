"""Read-only cuff/garter topology and unrestricted nearest-body distances."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords,fitted_points

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
body=bpy.data.objects['Eve Body'];body.data.calc_loop_triangles()
tree=BVHTree.FromPolygons(fitted_points(body),[t.vertices[:] for t in body.data.loop_triangles],all_triangles=True)
report={}
for part in ('Arms','Legs'):
 obj=bpy.data.objects['Eve Christmas - '+part];points=coords(obj.data.vertices)
 neighbors=[set() for _ in points];faces=[[] for _ in points]
 for e in obj.data.edges:
  i,j=e.vertices;neighbors[i].add(j);neighbors[j].add(i)
 for face in obj.data.polygons:
  for i in face.vertices:faces[i].append(face.index)
 rows=[]
 for i,point in enumerate(points):
  if not faces[i]:continue
  hit,normal,_,distance=tree.find_nearest(Vector(point))
  signed=(Vector(point)-hit).dot(normal)
  if signed>=0:continue
  adjacent=sorted(neighbors[i]);lengths=np.linalg.norm(points[adjacent]-point,axis=1)
  rows.append(dict(vertex=i,position=point.tolist(),signed_mm=signed*1000,distance_mm=distance*1000,
   degree=len(adjacent),neighbors=adjacent,longest_edge_mm=float(lengths.max()*1000),
   polygons=[dict(index=f,vertices=list(obj.data.polygons[f].vertices),material=obj.data.materials[obj.data.polygons[f].material_index].name) for f in faces[i]]))
 rows.sort(key=lambda r:r['signed_mm'])
 report[part]=dict(unused_vertices=sum(not linked for linked in faces),negative_vertices=len(rows),deeper_than_8mm=sum(r['signed_mm']< -8 for r in rows),
  worst=rows[:20],highest_degree=sorted(rows,key=lambda r:r['degree'],reverse=True)[:5])
a.output.write_text(json.dumps(report,indent=2)+'\n')
print('INTERNAL_FACE_PROBE_DONE')
