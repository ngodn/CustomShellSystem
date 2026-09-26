"""Save and fresh-load verify Prototype suit weights without changing its geometry."""
import json,hashlib
from pathlib import Path
import bpy
import numpy as np
root=Path(__file__).resolve().parents[2];work=root/'work/eve26'
name='Eve Prototype Planet Diving Suit - Suit'
output=work/'planet-suit-f6.blend';assert not output.exists()
data=json.loads((work/'planet-weight-repair/planet.mesh.json').read_text())
audit=json.loads((work/'planet-weight-repair/planet.mesh.audit.json').read_text())
start=audit['parts'][0]['points'];count=audit['parts'][1]['points']
def load(path):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 with bpy.data.libraries.load(str(path),link=False) as (src,dst):
  assert list(src.objects)==[name];dst.objects=[name]
 obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj);return obj
def geometry_digest(obj):
 h=hashlib.sha256()
 for key in obj.data.shape_keys.key_blocks:
  points=np.empty(count*3,dtype=np.float32);key.data.foreach_get('co',points)
  h.update(key.name.encode());h.update(points.tobytes());h.update(str((key.value,key.relative_key.name)).encode())
 return h.hexdigest()
obj=load(work/'planet-suit-f5.blend');before=geometry_digest(obj)
weights=[(v-start,data['bones'][b]['name'],w) for v,b,w in data['influences'] if start<=v<start+count]
obj.vertex_groups.clear()
for bone in sorted({b for _,b,_ in weights}):obj.vertex_groups.new(name=bone)
for vertex,bone,weight in weights:obj.vertex_groups[bone].add([vertex],weight,'REPLACE')
assert geometry_digest(obj)==before
bpy.data.libraries.write(str(output),{obj},fake_user=True,compress=True)
obj=load(output);assert geometry_digest(obj)==before
actual={(v.index,obj.vertex_groups[g.group].name):g.weight for v in obj.data.vertices for g in v.groups}
expected={(v,b):w for v,b,w in weights};assert actual.keys()==expected.keys()
error=max(abs(actual[k]-v) for k,v in expected.items());assert error<1e-6
report=dict(points=count,groups=len(obj.vertex_groups),weight_rows=len(weights),max_weight_reload_error=error,
            geometry_and_shape_keys_unchanged=True,geometry_digest=before,
            scope='Garment weights persisted and fresh-load verified. Footwear and accessories retain prior weights. Not a full-motion or game acceptance.')
(work/'planet-suit-f6.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
