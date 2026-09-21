"""Replay the captured V44 shoe pose and isolated supports. Blender 5.2.2.

No floor inference is made here. World sole points are inputs to subsequent
read-only scene traces. Mesh transforms must remain stable across those reads.
"""
import json
import os
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
OUT=Path(os.environ['CSS_FOOTWEAR_ANALYSIS']).resolve()
assert OUT.parent==WORK and not OUT.exists()
OUT.mkdir()
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import read_bones, TO_UE
source=json.loads((WORK/'arm-rest-correctives-export-v1/candidate.mesh.json').read_text())
audit=json.loads((WORK/'arm-rest-correctives-export-v1/candidate.mesh.audit.json').read_text())
live=json.loads((WORK/'footwear-followup-v1/live-floor-v2.json').read_text())
assert live['mesh_transform_before']==live['mesh_transform_after']
assert live['movement_values']['Velocity']==dict(X=0,Y=0,Z=0)
bones,bind=read_bones(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json')
indices={b['name'].lower():i for i,b in enumerate(bones)}
def transform(row):
 r=row['Rotation'];return Matrix.LocRotScale(Vector([row['Translation'][k] for k in 'XYZ']),Quaternion([r[k] for k in 'WXYZ']),Vector([row['Scale3D'][k] for k in 'XYZ']))
snapshot=live['pose']['Snapshot'];assert snapshot['bIsValid']
local={n.lower():transform(t) for n,t in zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True)}
pose=[]
for b in bones:
 t=local[b['name'].lower()];pose.append(pose[b['parent']]@t if b['parent']>=0 else t)
world=transform(live['mesh_transform_before'])
skinning=[world@p@b.inverted() for p,b in zip(pose,bind,strict=True)]
start=0
for part in audit['parts']:
 if part['name']=='Eve Black Pearl - Footwear':break
 start+=part['points']
else:raise ValueError('Shoe part missing')
ids=set(range(start,start+part['points']))
points={i:Vector(source['points'][i]) for i in ids};result={i:Vector((0,0,0)) for i in ids}
for morph in source['morph_targets']:
 for v,*delta in morph['deltas']:
  if v in ids:
   assert morph['name'] in live['morphs']
   points[v]+=Vector(delta)*live['morphs'][morph['name']]
for v,b,w in source['influences']:
 if v in ids:result[v]+=(skinning[b]@points[v])*w
bpy.ops.wm.open_mainfile(filepath=str(MOD/'work/CSS_SeduXtress_HeelSupportsV45C.blend'))
obj=bpy.data.objects['Eve Black Pearl - Heel Supports'];new=[]
for v in obj.data.vertices:
 co=v.co.copy()
 for key in obj.data.shape_keys.key_blocks[1:]:
  co+=(key.data[v.index].co-key.relative_key.data[v.index].co)*live['morphs'][key.name]
 p=TO_UE@obj.matrix_world@co;q=Vector((0,0,0))
 for g in v.groups:q+=(skinning[indices[obj.vertex_groups[g.group].name.lower()]]@p)*g.weight
 new.append(q)
rows={}
for side,sign in [('l',1),('r',-1)]:
 source_ids=[i for i in ids if source['points'][i][0]*sign>0]
 low=min(source_ids,key=lambda i:result[i].z)
 support_ids=[i for i,v in enumerate(obj.data.vertices) if v.co.x*sign>0]
 support_low=min(support_ids,key=lambda i:new[i].z)
 rows[side]=dict(shoe_low_vertex=low,shoe_low_world_cm=list(result[low]),
                 support_low_world_cm=list(new[support_low]),
                 support_vs_shoe_z_cm=new[support_low].z-result[low].z)
report=dict(rows=rows,mesh_transform=live['mesh_transform_before'],player=live['player'],
            morphs=live['morphs'],scope='Linear skin replay of one actual pose with captured public morphs. Scene floor has not been traced.')
(OUT/'pose.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
