"""Blender 5.2: transfer the six CSS body controls to fitted Holiday garments."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords,digest,fitted_points
SHAPES=('FBMBodyTone','PBMBreastsSize','PBMGlutesSize','PBMHipSize','PBMThighsTone','PBMWaistWidth')
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);assert not a.output.exists()
body=bpy.data.objects['Eve Body'];before=digest(body)
assert body.matrix_world==Matrix.Identity(4)
points=fitted_points(body);body.data.calc_loop_triangles();faces=[tuple(t.vertices) for t in body.data.loop_triangles]
tree=BVHTree.FromPolygons(points,faces,all_triangles=True)
deltas={}
for name in SHAPES:
 key=body.data.shape_keys.key_blocks[name]
 delta=coords(key.data)-coords(key.relative_key.data)
 if key.vertex_group:
  gi=body.vertex_groups[key.vertex_group].index
  mask=np.asarray([next((g.weight for g in v.groups if g.group==gi),0) for v in body.data.vertices])
  delta*=mask[:,None]
 deltas[name]=delta
rows={}
for part in ('Dress','Arms','Legs','Panties'):
 obj=bpy.data.objects['Eve Christmas - '+part]
 assert obj.matrix_world==Matrix.Identity(4)
 base=coords(obj.data.vertices)
 assert np.array_equal(base,coords(obj.data.shape_keys.key_blocks[0].data))
 ids=[];factors=[]
 for v in base:
  hit,normal,ti,distance=tree.find_nearest(Vector(v))
  tri=faces[ti]
  bary=barycentric_transform(hit,*[points[i] for i in tri],Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
  ids.append(tri);factors.append(bary)
 ids=np.asarray(ids);factors=np.clip(np.asarray(factors),0,1);factors/=factors.sum(1)[:,None]
 rows[part]={}
 for name in SHAPES:
  key=obj.data.shape_keys.key_blocks.get(name) or obj.shape_key_add(name=name,from_mix=False)
  shift=(deltas[name][ids]*factors[:,:,None]).sum(1).astype(np.float32)
  key.data.foreach_set('co',(base+shift).ravel())
  key.relative_key=obj.data.shape_keys.key_blocks[0]
  key.vertex_group='';key.value=0;key.mute=False;key.slider_min=0;key.slider_max=1
  rows[part][name]={'max_displacement_mm':float(np.linalg.norm(shift,axis=1).max()*1000)}
 obj.data.update()
 assert np.array_equal(base,coords(obj.data.vertices))
assert digest(body)==before
bpy.ops.wm.save_as_mainfile(filepath=str(a.output))
a.output.with_suffix('.json').write_text(json.dumps(dict(body_unchanged=True,body_sha256=before,parts=rows,scope='Transferred body surface displacement only; requires endpoint, combined-morph and motion clearance review.'),indent=2)+'\n')
print('HOLIDAY_MORPHS_SAVED')
