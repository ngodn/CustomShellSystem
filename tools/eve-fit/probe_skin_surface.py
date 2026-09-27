"""Measure the saved Skin Suit Surface Deform modifier without saving the blend."""
import hashlib,json
from pathlib import Path
import bpy
import numpy as np

root=Path(__file__).resolve().parents[3]
source=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/CSS_SeduXtress_Variants_Fixed.blend'
w=root/'CustomShellSystem/work/eve26'
before=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source),link=False) as (_,data):
    data.objects=['Eve Body','Eve Skin Suit - Suit Complete']
for obj in data.objects:bpy.context.scene.collection.objects.link(obj)
body,suit=data.objects
surface=next(m for m in suit.modifiers if m.type=='SURFACE_DEFORM')
metadata=dict(name=surface.name,bound=surface.is_bound,target=surface.target.name if surface.target else None,
              viewport=surface.show_viewport,render=surface.show_render,strength=surface.strength,
              vertex_group=surface.vertex_group)
target=surface.target
if target and not target.users_collection:bpy.context.scene.collection.objects.link(target)
metadata['target_modifiers']=[dict(name=m.name,type=m.type,viewport=m.show_viewport) for m in target.modifiers] if target else []
print(json.dumps(metadata),flush=True)
for obj in {obj for obj in (body,suit,target) if obj is not None}:
    if obj.data.shape_keys:obj.data.shape_keys.animation_data_clear()
    for modifier in obj.modifiers:
        modifier.show_viewport=False
        modifier.show_render=False
def evaluate():
    bpy.context.view_layer.update()
    evaluated=suit.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh=evaluated.to_mesh()
    points=np.asarray([suit.matrix_world@v.co for v in mesh.vertices])
    evaluated.to_mesh_clear()
    return points
off=evaluate()
surface.show_viewport=True
on=evaluate()
assert on.shape==off.shape
distance=np.linalg.norm(on-off,axis=1)*100
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
out=w/'skin-surface-probe.json';assert not out.exists()
out.write_text(json.dumps(dict(source_sha256=before,source_unchanged=True,modifier=metadata,
    moved_vertices=int((distance>.001).sum()),maximum_displacement_cm=float(distance.max()),
    percentile95_cm=float(np.percentile(distance,95)),
    scope='Saved shape values, drivers disabled, all modifiers except garment Surface Deform disabled. Not full authored viewport evaluation or fitting acceptance.'),indent=2)+'\n')
print(out.read_text())
