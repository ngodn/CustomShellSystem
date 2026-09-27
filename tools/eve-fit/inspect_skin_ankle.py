"""Measure original Skin Suit foot alignment before authoring a separate lining."""
import hashlib,json,sys
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
root=Path(__file__).resolve().parents[3]
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
w=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(mod/'gemini-work'))
from export_variant_clean import fitted_mesh,TO_UE
source=mod/'reference/body-type-variant-EVE/eve_beta10.blend'
digest=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source),link=False) as (_,dst):
    dst.objects=['Eve Skin Suit - Suit Complete']
obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj)
mesh,_,_=fitted_mesh(obj);mesh.calc_loop_triangles()
points=[TO_UE@obj.matrix_world@v.co for v in mesh.vertices]
slots={m.name:i for i,m in enumerate(mesh.materials)}
suit=slots['MI_EVE_Costume_Temp_Inner_Suit']
skin=slots['MI_EVE_Costume_Temp_Inner_Skin01']
used=sorted({i for t in mesh.loop_triangles if t.material_index==suit and
             (points[t.vertices[1]]-points[t.vertices[0]]).cross(points[t.vertices[2]]-points[t.vertices[0]]).length_squared>=1e-12 for i in t.vertices})
data=json.loads((w/'skin-fit5/skin.mesh.json').read_text())
assert len(used)==18633
errors=[(points[i]-Vector(data['points'][36787+j])).length for j,i in enumerate(used) if points[i].z<22]
triangles=[list(t.vertices) for t in mesh.loop_triangles if t.material_index==skin and
           max(points[i].z for i in t.vertices)<40]
indices=sorted({i for t in triangles for i in t});remap={v:i for i,v in enumerate(indices)}
mapped={i:Vector(data['points'][36787+j]) for j,i in enumerate(used)}
suit_triangles=[list(t.vertices) for t in mesh.loop_triangles if t.material_index==suit and all(i in mapped for i in t.vertices)]
tree=BVHTree.FromPolygons(points,suit_triangles,all_triangles=True)
aligned=[]
for i in indices:
    hit,_,face,_=tree.find_nearest(points[i])
    a,b,c=suit_triangles[face]
    weights=barycentric_transform(hit,points[a],points[b],points[c],Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
    delta=sum(((mapped[j]-points[j])*weight for j,weight in zip((a,b,c),weights)),Vector())
    aligned.append(list(points[i]+delta))
out=w/'skin-ankle-aligned.json';assert not out.exists()
assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
report=dict(source_sha256=digest,source_unchanged=True,foot_suit_vertices=len(errors),
            foot_suit_max_error_cm=max(errors),points_cm=aligned,
            alignment='Interpolated current-minus-original suit displacement at nearest original suit triangle',
            triangles=[[remap[i] for i in t] for t in triangles],
            scope='Original skin surface under 40 cm. Foot suit alignment is measured, not assumed; no production body changes.')
out.write_text(json.dumps(report,separators=(',',':'))+'\n')
print({k:v for k,v in report.items() if k not in ('points_cm','triangles')},flush=True)
