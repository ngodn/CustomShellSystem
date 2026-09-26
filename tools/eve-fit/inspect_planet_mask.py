"""Read Prototype's authored body mask and map its vertices to the CSS export."""
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector
root=Path(__file__).resolve().parents[3]
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
work=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(mod/'gemini-work'))
from export_variant_clean import fitted_mesh,TO_UE
mask_name='Eve Prototype Planet Diving Suit - Suit'
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(mod/'reference/body-type-variant-EVE/eve_beta10.blend'),link=False) as (_,dst):
    dst.objects=['Eve Body']
body=dst.objects[0]
bpy.context.scene.collection.objects.link(body)
polygons=[list(p.vertices) for p in body.data.polygons]
target=body.modifiers[mask_name]
metadata=dict(group=target.vertex_group,invert=target.invert_vertex_group,threshold=target.threshold)
for m in list(body.modifiers):
    if m != target:body.modifiers.remove(m)
target.show_viewport=True
target.show_render=True
attribute=body.data.attributes.new('css_source_index','INT','POINT')
attribute.data.foreach_set('value',list(range(len(body.data.vertices))))
bpy.context.view_layer.update()
evaluated=body.evaluated_get(bpy.context.evaluated_depsgraph_get())
mesh=evaluated.to_mesh()
kept={d.value for d in mesh.attributes['css_source_index'].data}
evaluated.to_mesh_clear()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(mod/'gemini-work/CSS_SeduXtress_Variants_Fixed.blend'),link=False) as (_,dst):
    dst.objects=['Eve Body']
body=dst.objects[0]
bpy.context.scene.collection.objects.link(body)
assert polygons == [list(p.vertices) for p in body.data.polygons], 'Source body topology changed'
mesh,_,_=fitted_mesh(body)
mesh.calc_loop_triangles()
transform=TO_UE @ body.matrix_world
used=set()
for t in mesh.loop_triangles:
    a,b,c=[transform @ mesh.vertices[i].co for i in t.vertices]
    if (b-a).cross(c-a).length_squared >= 1e-12:used.update(t.vertices)
used=sorted(used)
source=json.loads((mod/'gemini-work/exports/SK_Eve_PlanetDiving.mesh.json').read_text())
assert len(used)==36787
error=max((transform @ mesh.vertices[i].co-Vector(source['points'][j])).length for j,i in enumerate(used))
assert error<.0005,error
hidden=[j for j,i in enumerate(used) if i not in kept]
# Keep every face whose three vertices survive the actual Blender mask.
hidden_set=set(hidden)
faces_hidden=[f for f,record in enumerate(source['faces'][:61814]) if any(source['wedges'][w][0] in hidden_set for w in record[:3])]
report=dict(mask=metadata,topology_equal=True,mapping_error_cm=error,hidden_vertices=hidden,hidden_body_faces=faces_hidden,
            count_hidden_vertices=len(hidden),count_hidden_faces=len(faces_hidden),
            scope='Authored source mask, evaluated alone. Not a runtime visibility implementation or fitting acceptance.')
(work/'planet-body-mask.json').write_text(json.dumps(report,separators=(',',':'))+'\n')
print({k:v for k,v in report.items() if not isinstance(v,list)})
