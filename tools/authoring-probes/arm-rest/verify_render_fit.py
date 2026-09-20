"""Compare renderer fit policy with the verified V43 export, Blender 5.2.2."""
import json
import sys
from pathlib import Path
import bpy
import numpy as np

import os
OUT = Path(os.environ['CSS_ARM_REST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT=OUT.parents[3]
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import TO_UE,EXPORT_SHAPES
from separate_nextgen_footwear import coordinates

export=MOD/'work/exports-nextgen/SK_SeduXtress_HandBindV43.mesh.json'
payload=json.loads(export.read_text())
audit=json.loads(export.with_suffix('.audit.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(MOD/'work/CSS_SeduXtress_HandBindV43.blend'))
results=[];offset=0
for part in audit['parts']:
 obj=bpy.data.objects[part['name']]
 obj.hide_viewport=False;obj.hide_set(False)
 obj.modifiers.clear()
 if obj.data.shape_keys:
  obj.data.shape_keys.animation_data_clear()
  for key in obj.data.shape_keys.key_blocks:
   if key.name in EXPORT_SHAPES:key.value=0
 bpy.context.view_layer.update()
 evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh();mesh.calc_loop_triangles()
 basis=TO_UE@obj.matrix_world
 fitted=np.asarray([list(basis@v.co) for v in mesh.vertices],dtype=np.float64)
 faces=np.asarray([list(t.vertices) for t in mesh.loop_triangles],dtype=np.int64)
 vectors=fitted[faces]
 areas=np.cross(vectors[:,1]-vectors[:,0],vectors[:,2]-vectors[:,0])
 used=np.unique(faces[np.einsum('ij,ij->i',areas,areas)>=1e-12].ravel())
 expected=np.asarray(payload['points'][offset:offset+part['points']],dtype=np.float64)
 assert len(used)==part['points'],part['name']
 error=float(np.linalg.norm(fitted[used]-expected,axis=1).max())
 assert error<.0001,(part['name'],error)
 evaluated.to_mesh_clear()
 if obj.data.shape_keys:
  for key in obj.data.shape_keys.key_blocks:key.value=0
 bpy.context.view_layer.update()
 evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
 basis_points=np.asarray([list(basis@v.co) for v in mesh.vertices],dtype=np.float64)
 delta=np.linalg.norm(fitted-basis_points,axis=1)
 row=dict(name=obj.name,points=len(used),export_error_cm=error,basis_max_difference_cm=float(delta.max()))
 if obj.name=='Eve Body':
  hand_groups={g.index for g in obj.vertex_groups if g.name.startswith(('hand_','thumb_','index_','middle_','ring_','pinky_'))}
  ids=[v.index for v in obj.data.vertices if any(g.group in hand_groups and g.weight>.0001 for g in v.groups)]
  row['hand_basis_max_difference_cm']=float(delta[ids].max())
  row['hand_vertices_changed_over_0_01cm']=int(np.count_nonzero(delta[ids]>.01))
 evaluated.to_mesh_clear();results.append(row);offset+=part['points']
assert offset==len(payload['points'])
(OUT/'render-fit-validation.json').write_text(json.dumps(dict(parts=results,total_points=offset,
 scope='Neutral fitted geometry agrees with the existing verified V43 export. Earlier all-zero-shape renders use a different body and cannot establish final finger contact.'),indent=2)+'\n')
print(json.dumps(results),flush=True)
