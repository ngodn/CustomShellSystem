"""Refresh corrected suit points and corner normals from the verified Blender source."""
import hashlib,json,sys
from pathlib import Path
import bpy
from mathutils import Vector
root=Path(__file__).resolve().parents[3];w=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import TO_UE,fitted_mesh
source=w/'planet-suit-f7.blend';before=hashlib.sha256(source.read_bytes()).hexdigest()
p=w/'planet-fit7/planet.mesh.json';data=json.loads(p.read_text());audit=json.loads(p.with_name('planet.mesh.audit.json').read_text())
assert hashlib.sha256(p.read_bytes()).hexdigest()==audit['output_sha256']
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source),link=False) as (src,dst):dst.objects=['Eve Prototype Planet Diving Suit - Suit']
obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj)
mesh,_,_=fitted_mesh(obj);mesh.calc_loop_triangles();matrix=TO_UE@obj.matrix_world;normal_matrix=matrix.to_3x3().inverted().transposed()
start=audit['parts'][0]['points'];face_start=audit['parts'][0]['faces'];part=audit['parts'][1]
assert len(mesh.vertices)==part['points'] and len(mesh.loop_triangles)==part['faces']
error=max((matrix@v.co-Vector(data['points'][start+v.index])).length for v in mesh.vertices);assert error<.0005
old_faces=data['faces'][face_start:face_start+part['faces']]
old_wedges={wi for face in old_faces for wi in face[:3]};lo=min(old_wedges);hi=max(old_wedges)+1
assert old_wedges==set(range(lo,hi))
assert not old_wedges.intersection(wi for index,face in enumerate(data['faces']) if not face_start<=index<face_start+part['faces'] for wi in face[:3])
wedges=[];normals=[];colors=[];faces=[];corner_map={};color=mesh.color_attributes.active_color
for triangle in mesh.loop_triangles:
 loops=list(triangle.loops)
 if matrix.to_3x3().determinant()>0:loops.reverse()
 face=[]
 for loop_index in loops:
  if loop_index not in corner_map:
   loop=mesh.loops[loop_index];wedge=[start+loop.vertex_index]
   for channel in range(data['uv_channels']):
    uv=mesh.uv_layers[min(channel,len(mesh.uv_layers)-1)].data[loop_index].uv
    wedge.extend((uv.x,1-uv.y))
   normal=(normal_matrix@mesh.corner_normals[loop_index].vector).normalized();assert normal.length>.99
   rgba=color.data[loop_index if color.domain=='CORNER' else loop.vertex_index].color if color else (1,1,1,1)
   corner_map[loop_index]=lo+len(wedges);wedges.append(wedge);normals.append(list(normal));colors.append([round(max(0,min(1,v))*255) for v in rgba])
  face.append(corner_map[loop_index])
 material=mesh.materials[triangle.material_index]['CSS_source_material']
 faces.append(face+[data['materials'].index(material)])
assert len(wedges)==hi-lo
from collections import Counter
def signatures(ws,cs):return Counter((w[0],*[round(x,7) for x in w[1:]],*c) for w,c in zip(ws,cs,strict=True))
assert signatures(wedges,colors)==signatures(data['wedges'][lo:hi],data['colors'][lo:hi]), 'UV/color corner data changed'
changed_triangles=sum([data['wedges'][i][0] for i in old[:3]] != [wedges[i-lo][0] for i in new[:3]] for old,new in zip(old_faces,faces,strict=True))
data['wedges'][lo:hi]=wedges;data['normals'][lo:hi]=normals;data['colors'][lo:hi]=colors
data['faces'][face_start:face_start+part['faces']]=faces
for v in mesh.vertices:data['points'][start+v.index]=list(matrix@v.co)
out=w/'planet-export';out.mkdir(exist_ok=False)
p=out/'planet.mesh.json';p.write_text(json.dumps(data,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(p.read_bytes()).hexdigest();audit['parts'][1]['max_influences']=8
(out/'planet.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
report=dict(source_sha256=before,source_unchanged=True,suit_point_error_cm=error,uv_and_color_corners_preserved=True,
            refreshed_corners=len(wedges),changed_triangle_records=changed_triangles,other_parts_unchanged=True,
            scope='Private mesh assembly with fresh suit normals. Production skeleton binding, material instances and physics remain pending.')
(out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
