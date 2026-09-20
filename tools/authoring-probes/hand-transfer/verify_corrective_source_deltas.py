"""Independently compare all B2 exported hand deltas with evaluated shape keys.

Uses Blender's evaluated key-zero/key-one meshes, not the exporter's delta
routine. Nearest neutral positions map interchange points to source vertices.
"""
import hashlib
import json
import os
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

sys.path.insert(0,str(Path(__file__).resolve().parent))
from skin_fixture import MOD, WORK, HERE, load

out = Path(os.environ['CSS_CORRECTIVE_SOURCE_AUDIT']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out/'report.json').exists()
blend = MOD/'work/CSS_SeduXtress_ArmRestV44B2.blend'
mesh_path = WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'
digest = lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {str(p):digest(p) for p in (blend,mesh_path)}
data = load(mesh_path);audit = load(mesh_path.with_suffix('.audit.json'))
assert audit['output_sha256'] == hashes[str(mesh_path)]
assert audit['parts'][0]['name'] == 'Eve Body'
count = audit['parts'][0]['points']
assert count == 36787
models = load(HERE/'left-finger-correctives-v1.json')['parameters']
assert len(models) == 16
bpy.ops.wm.open_mainfile(filepath=str(blend),use_scripts=False)
body = bpy.data.objects['Eve Body']
body.hide_set(False);body.hide_viewport=False
for mod in list(body.modifiers):body.modifiers.remove(mod)
body.data.shape_keys.animation_data_clear()
body.show_only_shape_key=False
keys = body.data.shape_keys.key_blocks
world = Matrix.Diagonal((100,-100,100,1)) @ body.matrix_world
linear = np.array(world.to_3x3(),dtype=np.float64)
translation = np.array(world.translation,dtype=np.float64)

def positions():
    bpy.context.view_layer.update()
    evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    assert len(mesh.vertices) == 36789
    coords = np.empty(len(mesh.vertices)*3,dtype=np.float32)
    mesh.vertices.foreach_get('co',coords)
    evaluated.to_mesh_clear()
    return coords.reshape((-1,3)).astype(np.float64) @ linear.T + translation

neutral = positions()
tree = KDTree(len(neutral))
for i,p in enumerate(neutral):tree.insert(Vector(p),i)
tree.balance()
mapping=[];base_errors=[]
for p in data['points'][:count]:
    _,i,distance=tree.find(Vector(p));mapping.append(i);base_errors.append(distance)
assert max(base_errors)<.0002,max(base_errors)
evaluated_deltas=[];exported=[]
shapes={m['name']:m['deltas'] for m in data['morph_targets']}
for model in models:
    key=keys[model['shape']];saved=key.value
    assert not key.mute and abs(saved-model['baked_value'])<1e-7
    key.value=0;zero=positions()
    key.value=1;one=positions()
    key.value=saved
    evaluated_deltas.append(one-zero)
    target=np.zeros((count,3),dtype=np.float64)
    for row in shapes[model['shape']]:
        assert row[0]<count,'Hand corrective unexpectedly affects another part'
        target[row[0]]=row[1:]
    exported.append(target)
evaluated_deltas=np.array(evaluated_deltas);exported=np.array(exported)
mapping=np.array(mapping)
errors=np.linalg.norm(evaluated_deltas[:,mapping,:]-exported,axis=2)
# Coincident seam vertices can share position but have distinct shape data.
# Resolve each ambiguous point against the complete 16-shape signature.
ambiguous=0
for p in np.flatnonzero(np.max(errors,axis=0)>.0002):
    candidates=[i for _,i,_ in tree.find_range(Vector(data['points'][p]),.0002)]
    if len(candidates)>1:
        scores=np.max(np.linalg.norm(evaluated_deltas[:,candidates,:]-exported[:,p:p+1,:],axis=2),axis=0)
        selected=candidates[int(np.argmin(scores))]
        mapping[p]=selected
        errors[:,p]=np.linalg.norm(evaluated_deltas[:,selected,:]-exported[:,p,:],axis=1)
        ambiguous+=1
maximum=float(np.max(errors));passed=maximum<.0002
assert all(digest(Path(p))==h for p,h in hashes.items())
report=dict(scope=__doc__,passed=passed,source_hashes=hashes,points=count,shapes=len(models),
            max_neutral_mapping_error_cm=max(base_errors),max_delta_error_cm=maximum,
            tolerance_cm=.0002,coincident_points_resolved=ambiguous,
            per_shape={m['shape']:float(np.max(errors[i])) for i,m in enumerate(models)})
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report),flush=True)
raise SystemExit(0 if passed else 2)
