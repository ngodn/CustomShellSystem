"""Read-only region report for Holiday garment clearance samples."""
import argparse,bpy,json,sys
from pathlib import Path
from collections import defaultdict
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import fitted_points
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--outer',action='store_true')
p.add_argument('--rays',action='store_true',help='Probe six-direction surface crossings at the worst samples')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists()
b=bpy.data.objects['Eve Body'];b.data.calc_loop_triangles();tris=list(b.data.loop_triangles)
tree=BVHTree.FromPolygons(fitted_points(b),[t.vertices[:] for t in tris],all_triangles=True)
rows={}
samples=np.asarray(((1,0,0),(0,1,0),(0,0,1),(.5,.5,0),(.5,0,.5),(0,.5,.5),(1/3,1/3,1/3)))
for part in ('Dress','Arms','Legs','Panties'):
 o=bpy.data.objects['Eve Christmas - '+part];o.data.calc_loop_triangles();groups=defaultdict(list)
 for t in o.data.loop_triangles:
  mat=o.data.materials[t.material_index].name
  if a.outer and 'Fur' in mat:continue
  triangle=np.asarray([o.data.vertices[i].co[:] for i in t.vertices])
  outward=Vector(np.cross(triangle[1]-triangle[0],triangle[2]-triangle[0])).normalized()
  for bary in samples:
   point=Vector(bary@triangle)
   hit,n,i,d=tree.find_nearest(point,.008)
   if hit is None or (a.outer and outward.dot(n)<.15):continue
   signed=(point-hit).dot(n)
   if signed>=.0006:continue
   bm=b.data.materials[tris[i].material_index].name
   groups[(mat,bm)].append([*point,signed,*t.vertices,*hit,*n])
 rows[part]=[dict(material=m,body_material=bm,count=len(v),minimum_mm=min(x[3] for x in v)*1000,bounds=[np.min(np.asarray(v)[:,:3],0).tolist(),np.max(np.asarray(v)[:,:3],0).tolist()],samples=sorted(v,key=lambda x:x[3])[:24]) for (m,bm),v in groups.items()]
if a.rays:
 for groups in rows.values():
  for group in groups:
   group['ray_evidence']=[]
   for sample in group['samples'][:8]:
    directions=[]
    for axis in range(3):
     for sign in (-1,1):
      direction=Vector((0,0,0));direction[axis]=sign
      origin=Vector(sample[:3]);crossings=0;first=None
      for step in range(64):
       hit,normal,index,distance=tree.ray_cast(origin,direction,10)
       if hit is None:break
       if first is None:first=dict(distance_mm=distance*1000,normal_dot_direction=normal.dot(direction),body_material=b.data.materials[tris[index].material_index].name)
       crossings+=1;origin=hit+direction*.00001
      directions.append(dict(axis=axis,sign=sign,crossings=crossings,truncated=crossings==64,first=first))
    group['ray_evidence'].append(dict(position=sample[:3],directions=directions))
a.output.write_text(json.dumps(rows,indent=2)+'\n')
print('CLEARANCE_REGIONS_DONE')
