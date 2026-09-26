import bpy,json,sys
from pathlib import Path
from collections import defaultdict
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,'/home/eins0fx/development/mods/msII/CustomShellSystem/tools/eve-fit')
from holiday_candidate import fitted_points
b=bpy.data.objects['Eve Body'];b.data.calc_loop_triangles();tris=list(b.data.loop_triangles)
tree=BVHTree.FromPolygons(fitted_points(b),[t.vertices[:] for t in tris],all_triangles=True)
rows={}
for part in ('Dress','Arms','Legs','Panties'):
 o=bpy.data.objects['Eve Christmas - '+part];o.data.calc_loop_triangles();groups=defaultdict(list)
 for t in o.data.loop_triangles:
  p=sum((o.data.vertices[i].co for i in t.vertices),Vector())/3
  hit,n,i,d=tree.find_nearest(p,.008)
  if hit is not None and (p-hit).dot(n)<0:
   mat=o.data.materials[t.material_index].name
   bm=b.data.materials[tris[i].material_index].name
   groups[(mat,bm)].append([*p,(p-hit).dot(n),*t.vertices])
 rows[part]=[dict(material=m,body_material=bm,count=len(v),minimum_mm=min(x[3] for x in v)*1000,bounds=[np.min(np.asarray(v)[:,:3],0).tolist(),np.max(np.asarray(v)[:,:3],0).tolist()],samples=v[:12]) for (m,bm),v in groups.items()]
Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26/clearance-regions.json').write_text(json.dumps(rows,indent=2))
print('CLEARANCE_REGIONS_DONE')
