"""Keep unweighted attachment transforms unchanged in the isolated rest candidate."""
import copy
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix

import os
OUT = Path(os.environ['CSS_ARM_REST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT=OUT.parents[3]
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import read_bones,TO_UE
from separate_nextgen_footwear import geometry_digest
source=MOD/'work/CSS_SeduXtress_ArmRestV44B.blend'
output=MOD/'work/CSS_SeduXtress_ArmRestV44B2.blend'
bind_path=output.with_suffix('.bindpose.json')
assert not output.exists() and not bind_path.exists()
source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
old,_=read_bones(MOD/'work/CSS_SeduXtress_HandBindV43.bindpose.json')
bones,world=read_bones(source.with_suffix('.bindpose.json'))
canonical,_=read_bones(MOD/'authoring/reference/SKEL_CSS_Base.refskel.json')
names={'Socket_Prop_InHand_Hook_R','Socket_Prop_InHand_Lowerarm_L_01','Socket_Prop_InHand_Hook_L','Socket_Prop_Upperarm_L_01','Socket_ShoulderBash'}
indices={i for i,b in enumerate(bones) if b['name'] in names}
assert len(indices)==5 and not any(b['parent'] in indices for b in bones)
bpy.ops.wm.open_mainfile(filepath=str(source))
rig=bpy.data.objects['SKEL_CSS_Base']
parts=[o for o in bpy.data.objects if o.type=='MESH']
before={o.name:geometry_digest(o) for o in parts}
for obj in parts:
 groups={g.index for g in obj.vertex_groups if g.name in names}
 assert not any(g.group in groups and g.weight>0 for v in obj.data.vertices for g in v.groups),obj.name
result=copy.deepcopy(bones)
for i in indices:
 result[i]=copy.deepcopy(old[i])
 assert old[i]['name']==canonical[i]['name']
 for key in ('translation','rotation','scale'):
  error=math.dist(old[i][key],canonical[i][key])
  if key=='rotation':error=min(error,math.dist(old[i][key],[-v for v in canonical[i][key]]))
  assert error<.001,(old[i]['name'],key,error)
for actual,reference_bone in zip(result,canonical,strict=True):
 error=min(math.dist(actual['rotation'],reference_bone['rotation']),math.dist(actual['rotation'],[-v for v in reference_bone['rotation']]))
 assert error<1e-6,(actual['name'],error)
 actual['rotation']=copy.deepcopy(reference_bone['rotation'])
bind_path.write_text(json.dumps(result,indent=2)+'\n')
saved,new_world=read_bones(bind_path)
basis=TO_UE@rig.matrix_world
rest={b.name:b.matrix_local.copy() for b in rig.data.bones}
lengths={b.name:b.length for b in rig.data.bones}
rig.hide_viewport=False;rig.hide_set(False)
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode='EDIT')
for i in indices:
 b=rig.data.edit_bones[bones[i]['name']];b.use_connect=False
 b.matrix=basis.inverted()@new_world[i]@world[i].inverted()@basis@rest[b.name]
 b.length=lengths[b.name]
bpy.ops.object.mode_set(mode='OBJECT')
for pb in rig.pose.bones:pb.matrix_basis=Matrix.Identity(4)
rig['CSS_bind_pose']=str(bind_path.relative_to(MOD))
if 'CSS_bind_frame_policy' in rig:del rig['CSS_bind_frame_policy']
rig['CSS_arm_rest_alignment']='V44B2 diagnostic: V44B pose alignment with original unweighted attachment locals preserved'
bpy.context.view_layer.update()
assert before=={o.name:geometry_digest(o) for o in parts}
assert all({k:v for k,v in saved[i].items() if k!='rotation'}=={k:v for k,v in bones[i].items() if k!='rotation'} for i in range(len(bones)) if i not in indices)
assert all(a['rotation']==b['rotation'] for a,b in zip(saved,canonical,strict=True))
head_error=max((basis@rig.data.bones[b['name']].head_local-w.translation).length for b,w in zip(saved,new_world,strict=True))
assert head_error<.001
assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
bpy.ops.wm.save_as_mainfile(filepath=str(output),check_existing=False)
(OUT/'attachments.json').write_text(json.dumps(dict(output=str(output),bind=str(bind_path),source_sha256=source_hash,
 restored_names=sorted(names),unweighted_leaf_bones=True,canonical_local_match=True,other_bind_translations_scales_and_hierarchy_unchanged=True,
 canonical_rotations_exact=True,obsolete_hand_frame_policy_removed=True,
 all_mesh_and_morph_coordinates_unchanged=True,geometry_hashes=before,max_bind_head_error_cm=head_error,
 scope='Attachment-preserving derivative of V44B. No runtime or all-weapon validation.'),indent=2)+'\n')
print(json.dumps(dict(output=str(output),restored=sorted(names),head_error_cm=head_error)),flush=True)
