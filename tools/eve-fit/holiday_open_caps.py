"""Remove internal triangle fans that close Holiday cuff and garter openings."""
import argparse,json,sys
from pathlib import Path
import bpy,bmesh
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords,digest,fitted_points

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);assert not a.output.exists()
body=bpy.data.objects['Eve Body'];before=digest(body);body.data.calc_loop_triangles()
tree=BVHTree.FromPolygons(fitted_points(body),[t.vertices[:] for t in body.data.loop_triangles],all_triangles=True)
report={}
for part in ('Arms','Legs'):
 obj=bpy.data.objects['Eve Christmas - '+part];mesh=obj.data;points=coords(mesh.vertices)
 neighbors=[set() for _ in points];faces=[[] for _ in points]
 for e in mesh.edges:
  i,j=e.vertices;neighbors[i].add(j);neighbors[j].add(i)
 for face in mesh.polygons:
  for i in face.vertices:faces[i].append(face.index)
 hubs=[];remove=set()
 for i,adjacent in enumerate(neighbors):
  if len(adjacent)<20 or not faces[i]:continue
  if any(len(mesh.polygons[f].vertices)!=3 for f in faces[i]):continue
  hit,normal,_,distance=tree.find_nearest(Vector(points[i]))
  signed=(Vector(points[i])-hit).dot(normal)
  if signed>= -.008:continue
  longest=float(np.linalg.norm(points[list(adjacent)]-points[i],axis=1).max())
  if longest<.025:continue
  hubs.append(dict(vertex=i,degree=len(adjacent),depth_mm=-signed*1000,longest_edge_mm=longest*1000))
  remove.update(faces[i])
 assert hubs,(part,'No internal fan hubs detected')
 snapshots={key.name:coords(key.data) for key in mesh.shape_keys.key_blocks}
 weights=[[(g.group,g.weight) for g in v.groups] for v in mesh.vertices]
 def face_data(face):
  return (face.material_index,{layer.name:{mesh.loops[li].vertex_index:tuple(layer.data[li].uv) for li in face.loop_indices} for layer in mesh.uv_layers})
 surviving={tuple(sorted(face.vertices)):face_data(face) for face in mesh.polygons if face.index not in remove}
 count=len(mesh.polygons)
 bm=bmesh.new();bm.from_mesh(mesh);bm.faces.ensure_lookup_table()
 bmesh.ops.delete(bm,geom=[bm.faces[i] for i in sorted(remove)],context='FACES_ONLY')
 bm.to_mesh(mesh);bm.free();mesh.update()
 assert len(mesh.polygons)==count-len(remove)
 assert np.array_equal(coords(mesh.vertices),points)
 for key in mesh.shape_keys.key_blocks:assert np.array_equal(coords(key.data),snapshots[key.name]),(part,key.name)
 assert weights==[[(g.group,g.weight) for g in v.groups] for v in mesh.vertices]
 assert surviving=={tuple(sorted(face.vertices)):face_data(face) for face in mesh.polygons}
 report[part]=dict(hubs=hubs,removed_faces=len(remove),remaining_faces=len(mesh.polygons),vertices_keys_weights_uvs_preserved=True)
assert digest(body)==before
bpy.ops.wm.save_as_mainfile(filepath=str(a.output))
a.output.with_suffix('.json').write_text(json.dumps(dict(parts=report,body_unchanged=True,scope='Internal triangle fan removal candidate. Requires visual and motion review; other intersections may remain.'),indent=2)+'\n')
print('OPEN_CAP_CANDIDATE_SAVED')
