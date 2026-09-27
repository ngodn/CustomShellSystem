"""Save and fresh-load verify Prototype suit weights without changing its geometry."""
import argparse,json,hashlib,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
root=Path(__file__).resolve().parents[2];work=root/'work/eve26'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--garment',choices=('prototype','skin','bikini'),default='prototype')
p.add_argument('--part',help='Exact garment object name for multipart outfits')
p.add_argument('--source',type=Path,default=work/'planet-suit-f5.blend')
p.add_argument('--candidate',type=Path,default=work/'planet-weight-repair')
p.add_argument('--output',type=Path,default=work/'planet-suit-f6.blend')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
assert a.garment!='bikini' or a.part
name=a.part or {'prototype':'Eve Prototype Planet Diving Suit - Suit','skin':'Eve Skin Suit - Suit Complete'}[a.garment]
stem={'prototype':'planet','skin':'skin','bikini':'bikini'}[a.garment]
sys.path.insert(0,str(root.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import TO_UE
output=a.output;assert not output.exists() and not output.with_suffix('.json').exists()
raw=(a.candidate/f'{stem}.mesh.json').read_bytes();data=json.loads(raw)
audit=json.loads((a.candidate/f'{stem}.mesh.audit.json').read_text())
assert hashlib.sha256(raw).hexdigest()==audit['output_sha256']
part_index=next(i for i,part in enumerate(audit['parts']) if part['name']==name)
start=sum(part['points'] for part in audit['parts'][:part_index]);count=audit['parts'][part_index]['points']
def load(path):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 with bpy.data.libraries.load(str(path),link=False) as (src,dst):
  assert list(src.objects)==[name];dst.objects=[name]
 obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj);return obj
def geometry_digest(obj):
 h=hashlib.sha256()
 for key in obj.data.shape_keys.key_blocks:
  points=np.empty(len(obj.data.vertices)*3,dtype=np.float32);key.data.foreach_get('co',points)
  h.update(key.name.encode());h.update(points.tobytes());h.update(str((key.value,key.relative_key.name)).encode())
 return h.hexdigest()
obj=load(a.source);before=geometry_digest(obj)
obj.data.calc_loop_triangles();transform=TO_UE@obj.matrix_world
used=set()
for tri in obj.data.loop_triangles:
 x,y,z=[transform@obj.data.vertices[i].co for i in tri.vertices]
 if (y-x).cross(z-x).length_squared>=1e-12:used.update(tri.vertices)
used=sorted(used);assert len(used)==count
error=max((transform@obj.data.vertices[i].co-Vector(data['points'][start+j])).length for j,i in enumerate(used))
assert error<.0005,error
weights=[(used[v-start],data['bones'][b]['name'],w) for v,b,w in data['influences'] if start<=v<start+count]
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
            source=str(a.source),candidate_sha256=hashlib.sha256(raw).hexdigest(),
            scope='Candidate garment weights persisted and fresh-load verified; geometry and shape keys unchanged. Not full-motion or game acceptance.')
output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
