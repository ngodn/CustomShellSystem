"""Compare current saved Skin Suit geometry with Gemini's older export, read-only."""
import hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np

root=Path(__file__).resolve().parents[3]
mod=root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'
w=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(mod))
from export_variant_clean import fitted_mesh,TO_UE
source=mod/'CSS_SeduXtress_Variants_Fixed.blend'
before=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(source),link=False) as (_,data):
    data.objects=['Eve Skin Suit - Suit Complete']
obj=data.objects[0];bpy.context.scene.collection.objects.link(obj)
modifiers=[dict(name=m.name,type=m.type) for m in obj.modifiers]
mesh,deltas,active=fitted_mesh(obj)
mesh.calc_loop_triangles()
transform=TO_UE@obj.matrix_world
used=set()
for triangle in mesh.loop_triangles:
    a,b,c=[transform@mesh.vertices[i].co for i in triangle.vertices]
    if (b-a).cross(c-a).length_squared>=1e-12:used.update(triangle.vertices)
points=np.asarray([transform@mesh.vertices[i].co for i in sorted(used)])
old=json.loads((mod/'exports/SK_Eve_SkinSuit.mesh.json').read_text())
audit=json.loads((mod/'exports/SK_Eve_SkinSuit.mesh.audit.json').read_text())
part=audit['parts'][1];assert len(points)==part['points'],(len(points),part['points'])
expected=np.asarray(old['points'][36787:36787+len(points)])
distances=np.linalg.norm(points-expected,axis=1)
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
out=w/'skin-source-comparison.json';assert not out.exists()
out.write_text(json.dumps(dict(source_sha256=before,source_unchanged=True,points=len(points),
    saved_modifiers=modifiers,baked_keys=active,exported_shape_names=list(deltas),
    old_export_comparison_cm=dict(maximum=float(distances.max()),median=float(np.median(distances)),p95=float(np.percentile(distances,95))),
    scope='Index-wise saved garment comparison. Topology correspondence, current export, fitting and runtime still require verification.'),indent=2)+'\n')
print(out.read_text())
