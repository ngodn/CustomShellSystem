"""Replay recorded UE component poses on Holiday garments without changing the saved asset."""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix,Quaternion,Vector

ROOT=Path(__file__).resolve().parents[3]
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0,str(MOD/'tools'))
from export_seduxtress_eve import TO_UE,read_bones,EXPORT_SHAPES,LEFT_HAND_CORRECTIVES
sys.path.insert(0,str(Path(__file__).resolve().parent))
from holiday_candidate import coords

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--motion',type=Path,required=True)
p.add_argument('--bind',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--frames',type=int,nargs='+',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
assert not a.output.exists();a.output.mkdir(parents=True)
blend=Path(bpy.data.filepath);source_hash=hashlib.sha256(blend.read_bytes()).hexdigest()
rig=bpy.data.objects['SKEL_CSS_Base']
bones,bind=read_bones(MOD/rig['CSS_bind_pose'])
expected=json.loads(a.bind.read_text())
assert len(bones)==len(expected)==len(rig.data.bones)
for actual,entry in zip(bones,expected,strict=True):
 assert actual['name'].lower()==entry['name'].lower() and actual['parent']==entry['parent']
 assert math.dist(actual['translation'],entry['translation'])<.001
 assert math.dist(actual['scale'],entry['scale'])<.001
 assert min(math.dist(actual['rotation'],entry['rotation']),math.dist(actual['rotation'],[-v for v in entry['rotation']]))<.001
names=['Eve Body']+['Eve Christmas - '+part for part in ('Dress','Arms','Legs','Panties')]
objects=[bpy.data.objects[name] for name in names]
scene=bpy.data.scenes.new('Holiday pose review');bpy.context.window.scene=scene
for obj in [rig,*objects]:
 scene.collection.objects.link(obj)
 obj.driver_remove('hide_render');obj.driver_remove('hide_viewport')
 obj.hide_viewport=obj.hide_render=False;obj.hide_set(False)
rig.animation_data_clear()
for bone in rig.pose.bones:
 assert not bone.constraints
 bone.matrix_basis=Matrix.Identity(4)
for obj in objects:
 obj.animation_data_clear();obj.show_only_shape_key=False
 if obj.data.shape_keys:
  obj.data.shape_keys.animation_data_clear()
  for key in obj.data.shape_keys.key_blocks:
   if key.name in EXPORT_SHAPES:key.value=0
 for modifier in list(obj.modifiers):
  if modifier.type!='ARMATURE':obj.modifiers.remove(modifier)
  else:
   assert modifier.object==rig,(obj.name,modifier.name)
   modifier.show_viewport=modifier.show_render=True
 obj.color=(.58,.36,.22,1) if obj.name=='Eve Body' else (.12,.38,.55,1)
basis=TO_UE@rig.matrix_world;inverse=basis.inverted()
pose_bones=[rig.pose.bones[b['name']] for b in bones]
rest=[b.bone.matrix_local.copy() for b in pose_bones]
rest_inverse=[m.inverted() for m in rest];bind_inverse=[m.inverted() for m in bind]
indices={b['name']:i for i,b in enumerate(bones)}
motion=json.loads(a.motion.read_text());errors=[]
garment_rest={obj.name:coords(obj.data.vertices) for obj in objects if obj.name!='Eve Body'}
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO'
scene.display.shading.color_type='OBJECT';scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD';scene.world=bpy.data.worlds.new('Holiday background')
scene.world.color=(.12,.12,.12);scene.render.resolution_x=900;scene.render.resolution_y=1100
scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
camera=bpy.data.objects.new('Holiday camera',bpy.data.cameras.new('Holiday camera'))
scene.collection.objects.link(camera);scene.camera=camera
camera.data.type='ORTHO';camera.data.ortho_scale=2.05
for frame in a.frames:
 record=motion['frames'][frame];snapshot=record['pose']['Snapshot']
 assert snapshot['bIsValid']
 lookup={n.lower():t for n,t in zip(snapshot['BoneNames'],snapshot['LocalTransforms'],strict=True)}
 assert len(lookup)==len(snapshot['BoneNames'])
 assert all(b['name'].lower() in lookup for b in bones)
 for obj in objects:
  if not obj.data.shape_keys:continue
  for key in obj.data.shape_keys.key_blocks:
   if key.name in EXPORT_SHAPES or key.name in LEFT_HAND_CORRECTIVES:
    key.value=record.get('morphs',{}).get(key.name,0)
 world=[];wanted=[]
 for index,bone in enumerate(bones):
  t=lookup[bone['name'].lower()]
  local=Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),Quaternion([t['Rotation'][k] for k in 'WXYZ']),Vector([t['Scale3D'][k] for k in 'XYZ']))
  world.append(world[bone['parent']]@local if bone['parent']>=0 else local)
  wanted.append(inverse@world[-1]@bind_inverse[index]@basis@rest[index])
 for index,bone in enumerate(pose_bones):
  parent=indices[bone.parent.name] if bone.parent else None
  kwargs=dict(parent_matrix=wanted[parent],parent_matrix_local=rest[parent]) if parent is not None else {}
  bone.matrix_basis=bone.bone.convert_local_to_pose(wanted[index],rest[index],invert=True,**kwargs)
 bpy.context.view_layer.update()
 position_error=angle_error=0
 for index,bone in enumerate(pose_bones):
  actual=basis@bone.matrix@rest_inverse[index]@inverse@bind[index]
  position_error=max(position_error,(actual.translation-world[index].translation).length)
  angle=actual.to_quaternion().rotation_difference(world[index].to_quaternion()).angle
  angle_error=max(angle_error,min(angle,abs(2*math.pi-angle)))
 assert position_error<.001 and angle_error<.001,(frame,position_error,angle_error)
 deformation={};graph=bpy.context.evaluated_depsgraph_get()
 for obj in objects:
  if obj.name not in garment_rest:continue
  evaluated=obj.evaluated_get(graph);posed=coords(evaluated.data.vertices)
  base=garment_rest[obj.name];assert len(posed)==len(base)
  edges=np.asarray([edge.vertices[:] for edge in obj.data.edges]);i,j=edges.T
  original_lengths=np.linalg.norm(base[i]-base[j],axis=1)
  posed_lengths=np.linalg.norm(posed[i]-posed[j],axis=1)
  ratio=posed_lengths/np.maximum(original_lengths,.0001)
  selected=np.argsort(ratio)[-8:][::-1]
  deformation[obj.name]=[dict(vertices=[int(i[n]),int(j[n])],base_mm=float(original_lengths[n]*1000),posed_mm=float(posed_lengths[n]*1000),ratio=float(ratio[n])) for n in selected]
 errors.append(dict(frame=frame,position_cm=position_error,angle_rad=angle_error,deformation=deformation))
 for view,direction in [('front',(0,-4,0)),('back',(0,4,0)),('side',(4,0,0))]:
  center=Vector((0,0,.95));camera.location=center+Vector(direction)
  camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
  scene.render.filepath=str(a.output/f'{frame:03d}-{view}.png')
  bpy.ops.render.render(write_still=True)
assert hashlib.sha256(blend.read_bytes()).hexdigest()==source_hash
(a.output/'report.json').write_text(json.dumps(dict(blend=str(blend),blend_sha256=source_hash,motion=str(a.motion),bone_count=len(bones),errors=errors,
 scope='Recorded component poses on compatible authoring rig. Body remains visible. No current 386-bone cooked asset, cloth simulation or live gameplay acceptance.'),indent=2)+'\n')
print('HOLIDAY_POSE_REVIEW_DONE')
