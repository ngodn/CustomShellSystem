"""Save and fresh-load verify the repaired six Prototype garment morphs."""
import argparse,json,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Vector
root=Path(__file__).resolve().parents[3];work=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(root/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work'))
from export_variant_clean import TO_UE,fitted_mesh,EXPORT_SHAPES
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--garment',choices=('prototype','skin'),default='prototype')
parser.add_argument('--source',type=Path,default=work/'planet-suit-f4c.blend')
parser.add_argument('--output',type=Path,default=work/'planet-suit-f5.blend')
parser.add_argument('--mesh',type=Path,default=work/'planet-morph-repair/planet.mesh.json')
parser.add_argument('--receipt',type=Path,default=work/'planet-suit-f5.json')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
name={'prototype':'Eve Prototype Planet Diving Suit - Suit','skin':'Eve Skin Suit - Suit Complete'}[args.garment]
source=args.source;output=args.output
assert not output.exists()
data=json.loads(args.mesh.read_text())
audit=json.loads(args.mesh.with_suffix('.audit.json').read_text())
start=audit['parts'][0]['points'];count=audit['parts'][1]['points']
def load(path):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 with bpy.data.libraries.load(str(path),link=False) as (src,dst):
  assert list(src.objects)==[name]
  dst.objects=[name]
 obj=dst.objects[0];bpy.context.scene.collection.objects.link(obj)
 return obj
obj=load(source);matrix=(TO_UE @ obj.matrix_world).to_3x3();inverse=matrix.inverted()
obj.data.calc_loop_triangles()
used=set()
for triangle in obj.data.loop_triangles:
 a,b,c=[TO_UE@obj.matrix_world@obj.data.vertices[i].co for i in triangle.vertices]
 if (b-a).cross(c-a).length_squared>=1e-12:used.update(triangle.vertices)
used=sorted(used);assert len(used)==count
raw_count=len(obj.data.vertices)
def coords(key):
 result=np.empty(raw_count*3,dtype=np.float32);key.data.foreach_get('co',result)
 return result.reshape(-1,3)
keys=obj.data.shape_keys.key_blocks
original_values={k.name:k.value for k in keys}
protected={k.name:coords(k).copy() for k in keys if k.name not in EXPORT_SHAPES}
targets={}
for target in data['morph_targets']:
 if target['name'] not in EXPORT_SHAPES:continue
 delta=np.zeros((count,3),dtype=np.float64)
 for i,*value in target['deltas']:
  if start<=i<start+count:delta[i-start]=value
 targets[target['name']]=delta
 key=keys[target['name']]
 assert key.value==0 and not key.vertex_group
 local=np.asarray([inverse @ Vector(v) for v in delta])
 replacement=coords(key.relative_key).copy()
 replacement[used]+=local
 key.data.foreach_set('co',replacement.ravel())
for key_name,expected in protected.items():assert np.array_equal(coords(keys[key_name]),expected)
assert {k.name:k.value for k in keys}==original_values
bpy.data.libraries.write(str(output),{obj},fake_user=True,compress=True)
obj=load(output)
mesh,deltas,_=fitted_mesh(obj)
points=np.asarray([TO_UE @ obj.matrix_world @ mesh.vertices[i].co for i in used])
base_error=float(np.linalg.norm(points-np.asarray(data['points'][start:start+count]),axis=1).max())
assert base_error<.0005
errors={}
for key_name,expected in targets.items():
 actual=np.asarray([matrix @ Vector(deltas[key_name][i]) for i in used])
 errors[key_name]=float(np.linalg.norm(actual-expected,axis=1).max())
 assert errors[key_name]<.0005,(key_name,errors[key_name])
receipt=dict(source=str(source),output=str(output),base_error_cm=base_error,morph_errors_cm=errors,
             protected_fit_keys_unchanged=True,default_values_unchanged=True,
             scope='Saved garment morph and base geometry verification; not a motion, material or game acceptance.')
assert not args.receipt.exists()
args.receipt.write_text(json.dumps(receipt,indent=2)+'\n');print(receipt)
