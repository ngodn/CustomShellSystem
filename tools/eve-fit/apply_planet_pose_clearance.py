"""Apply bounded pose-clearance offsets to the fitted garment and every relative key."""
import argparse,hashlib,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
root=Path(__file__).resolve().parents[3];w=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import TO_UE,fitted_mesh
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,default=w/'planet-suit-f6.blend')
p.add_argument('--original',type=Path,default=w/'planet-weight-repair/planet.mesh.json')
p.add_argument('--candidate',type=Path,default=w/'planet-fit7')
p.add_argument('--output',type=Path,default=w/'planet-suit-f7.blend')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
name='Eve Prototype Planet Diving Suit - Suit';out=a.output;assert not out.exists()
def load(p):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 with bpy.data.libraries.load(str(p),link=False) as (src,dst):
  assert list(src.objects)==[name];dst.objects=[name]
 obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj);return obj
def coords(collection):
 a=np.empty(len(collection)*3,dtype=np.float32);collection.foreach_get('co',a);return a.reshape(-1,3)
def weights(obj):return [(v.index,obj.vertex_groups[g.group].name,g.weight) for v in obj.data.vertices for g in v.groups]
obj=load(a.source);keys=obj.data.shape_keys.key_blocks
old={k.name:coords(k.data) for k in keys};old_weights=weights(obj)
matrix=TO_UE@obj.matrix_world;inverse=matrix.to_3x3().inverted()
original=json.loads(a.original.read_text())
expected=json.loads((a.candidate/'planet.mesh.json').read_text())
start=36787;count=28234
mesh,_,_=fitted_mesh(obj.copy())
actual=np.asarray([matrix@v.co for v in mesh.vertices])
assert np.linalg.norm(actual-np.asarray(original['points'][start:start+count]),axis=1).max()<.0005
# The lightweight source has one-to-one suit point indexing, checked above.
delta=np.zeros((count,3),dtype=np.float32)
for index,*value in json.loads((a.candidate/'offsets.json').read_text())['offsets']:
 assert start<=index<start+count
 delta[index-start]=inverse@Vector(value)
for k in keys:k.data.foreach_set('co',(old[k.name]+delta).ravel())
obj.data.vertices.foreach_set('co',(old[keys[0].name]+delta).ravel());obj.data.update()
for k in keys:assert np.allclose(coords(k.data)-coords(k.relative_key.data),old[k.name]-old[k.relative_key.name],atol=3e-7)
assert weights(obj)==old_weights
bpy.data.libraries.write(str(out),{obj},fake_user=True,compress=True)
obj=load(out);assert weights(obj)==old_weights
mesh,_,_=fitted_mesh(obj);actual=np.asarray([TO_UE@obj.matrix_world@v.co for v in mesh.vertices])
error=float(np.linalg.norm(actual-np.asarray(expected['points'][start:start+count]),axis=1).max());assert error<.0005
report=dict(max_reload_error_cm=error,weights_unchanged=True,relative_morphs_preserved=True,changed_vertices=int(np.count_nonzero(np.linalg.norm(delta,axis=1))),scope='Saved bounded pose-clearance source; not full-motion or game acceptance.')
out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
