"""Fresh-load the fitted Prototype garment library and verify its exported points."""
import bpy,json,sys
import numpy as np
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import fitted_mesh,TO_UE
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(work/'planet-suit-f4c.blend'),link=False) as (src,dst):
    names=list(src.objects)
    assert names == ['Eve Prototype Planet Diving Suit - Suit'], names
    dst.objects=['Eve Prototype Planet Diving Suit - Suit']
obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj)
assert not obj.modifiers
assert not obj.data.shape_keys.animation_data
keys=[k.name for k in obj.data.shape_keys.key_blocks]
mesh,_,_=fitted_mesh(obj)
points=np.asarray([TO_UE @ obj.matrix_world @ v.co for v in mesh.vertices])
expected=np.asarray(json.load(open(work/'planet-fit4/planet.mesh.json'))['points'][36787:36787+28234])
assert points.shape==expected.shape
error=float(np.linalg.norm(points-expected,axis=1).max())
assert error<.0005,error
r={'objects_in_library':names,'shape_keys':len(keys),'points':len(points),'max_reload_error_cm':error,'scope':'Fresh library load and evaluated fit only. No gameplay or production-rig verification.'}
(work/'planet-suit-f4c-reload.json').write_text(json.dumps(r,indent=2)+'\n')
print(r)
