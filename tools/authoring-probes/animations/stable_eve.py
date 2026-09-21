"""UE 5.6.1: apply source-matched leg trajectories without changing Eve's rig."""
import hashlib,json,math,os
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[4]
WORK=Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE=os.environ.get('CSS_MOVEMENT_MODE','create')
assert MODE in ('create','readback') and not (WORK/(MODE+'-result.json')).exists()
prior=ROOT/'CustomShellSystem/work/anim13'
protected=json.loads((prior/'protected.json').read_text())
content=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
for path in (content/'CSS/Eve/Anim').glob('*.uasset'):
 if not path.stem.startswith(('AN_S1_','BS_S1_')):protected[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
if MODE=='create':(WORK/'protected.json').write_text(json.dumps(protected,indent=2))
else:assert protected==json.loads((WORK/'protected.json').read_text())
mesh=unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
blueprint=unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
options=unreal.AnimPoseEvaluationOptions();options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW);options.set_editor_property('optional_skeletal_mesh',mesh)
bind=json.loads((ROOT/'CustomShellSystem/work/anim10/batch/target-bind.json').read_text());names=[b['name'] for b in bind]
phase=json.loads((ROOT/'CustomShellSystem/work/anim12/phase.json').read_text())['clips']
changed={n+'_'+s for n in ('thigh','calf','foot') for s in ('l','r')}
tools=unreal.AssetToolsHelpers.get_asset_tools();clips={};reports=[];directory='/Game/CSS/Eve/Anim'
for label in phase:
 path=directory+'/AN_S1_'+label
 document=json.loads((WORK/(label+'-motion.json')).read_text())
 if MODE=='create':
  assert not unreal.EditorAssetLibrary.does_asset_exist(path)
  source=unreal.load_asset(directory+'/AN_'+label);clip=tools.duplicate_asset('AN_S1_'+label,directory,source);assert clip
  edit=clip.get_editor_property('controller');edit.open_bracket('Match original Eve leg trajectories',False)
  try:
   for name in sorted(changed):
    i=names.index(name);ts=[f['pose']['Snapshot']['LocalTransforms'][i] for f in document['frames']]
    assert edit.set_bone_track_keys(name,[unreal.Vector(*[t['Translation'][k] for k in 'XYZ']) for t in ts],[unreal.Quat(*[t['Rotation'][k] for k in 'XYZW']) for t in ts],[unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ']) for t in ts],False)
  finally:edit.close_bracket(False)
  assert unreal.EditorAssetLibrary.save_loaded_asset(clip,False)
 else:clip=unreal.load_asset(path)
 assert clip and clip.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
 maximum_position=maximum_rotation=0.
 for f,entry in enumerate(document['frames']):
  pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,f/document['fps'],options)
  assert unreal.AnimPoseExtensions.is_valid(pose)
  for i,name in enumerate(names):
   t=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.LOCAL);e=entry['pose']['Snapshot']['LocalTransforms'][i]
   pe=math.dist([t.translation.x,t.translation.y,t.translation.z],[e['Translation'][k] for k in 'XYZ'])
   a=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w];b=[e['Rotation'][k] for k in 'XYZW'];qe=min(math.dist(a,b),math.dist(a,[-x for x in b]))
   assert pe<.001 and qe<.0001,(label,f,name,pe,qe)
   assert math.dist([t.scale3d.x,t.scale3d.y,t.scale3d.z],[e['Scale3D'][k] for k in 'XYZ'])<.0001
   maximum_position=max(maximum_position,pe);maximum_rotation=max(maximum_rotation,qe)
 reports.append(dict(clip=label,frames=len(document['frames']),max_position_cm=maximum_position,max_quaternion_distance=maximum_rotation))
 (WORK/(MODE+'-raw-progress.json')).write_text(json.dumps(reports,indent=2))
 clips[label]=clip
spec=json.loads((prior/'spec.json').read_text());definitions=spec['definitions'];nominal={d['slot']:d['nominal_forward_speed'] for d in definitions}
for d in definitions:
 slot=d['slot'];path=directory+'/BS_S1_'+slot;d['package']=path
 for s in d['samples']:s['asset']=clips[s['clip']].get_path_name()
 if MODE=='create':
  blend=unreal.CSSAnimationLibrary.create_movement_blend(mesh,[clips[s['clip']] for s in d['samples']],[unreal.Vector(s['direction'],s['speed'],0) for s in d['samples']],[s['rate'] for s in d['samples']],d['max_speed'],path)
  assert blend and unreal.EditorAssetLibrary.save_loaded_asset(blend,False)
 else:blend=unreal.load_asset(path)
 for actual,wanted in zip(blend.get_editor_property('sample_data'),d['samples'],strict=True):
  assert actual.get_editor_property('animation').get_path_name()==wanted['asset']
  assert math.isclose(actual.get_editor_property('rate_scale'),wanted['rate'],rel_tol=1e-6)
  assert actual.get_editor_property('sample_value')==unreal.Vector(wanted['direction'],wanted['speed'],0)
if MODE=='create':(WORK/'spec.json').write_text(json.dumps(spec,indent=2))
else:assert spec==json.loads((WORK/'spec.json').read_text())

cases = [('walk0','Walk',0,184),('walk22','Walk',22.5,150),('walkback','Walk',-179,150),
         ('jog0','Jog',0,540),('jogleft','Jog',-67.5,300),('jogback','Jog',180,240),
         ('sprint0','Sprint',0,800),('sprintright','Sprint',22.5,650),('sprintback','Sprint',-135,300)]
results = []
if MODE == 'readback':
    reference = unreal.load_asset('/Game/CSS/AnimLab/RT_D2_Walk')
    assert reference
    for name, slot, direction, speed in cases:
        blend = unreal.load_asset(directory+'/BS_S1_'+slot)
        output = WORK/(name+'-component.json')
        data = json.loads(output.read_text()) if output.exists() else json.loads(
            unreal.CSSAnimationLibrary.evaluate_clip(mesh, reference, blueprint, 2,
                blend, unreal.Vector(direction, speed, 0), True))
        assert len(data['frames']) == 145 and data['advance_blend_clock']
        # Triangulation can assign neighboring animations different speed-row
        # mixtures. Use the engine's grouped weighted rates for its actual clock.
        query = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(blend,
            [unreal.Vector(direction, speed, 0)]))['queries'][0]
        grouped = {}
        for sample in query['samples']:
            weight, rate = grouped.get(sample['animation'], (0., 0.))
            grouped[sample['animation']] = weight+sample['weight'], rate+sample['weight']*sample['rate']
        period = sum(weight*unreal.load_asset(path).get_play_length()/(rate/weight)
                     for path, (weight, rate) in grouped.items() if weight > 0)
        nominal_period = clips[slot].get_play_length()/(speed/nominal[slot])
        errors = []
        for frame in data['frames']:
            difference = abs(frame['blend_normalized_time']-(frame['time']/period)%1)
            errors.append(min(difference, abs(1-difference)))
        assert max(errors) < .0001, (name, max(errors))
        if not output.exists():
            output.write_text(json.dumps(data, separators=(',', ':'))+'\n')
        results.append(dict(case=name, frames=145, cycle_seconds=period,
            nominal_cycle_seconds=nominal_period, clock_error=max(errors)))
        (WORK/'playback-progress.json').write_text(json.dumps(results, indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p,h in protected.items())
packages = [c.get_path_name().split('.')[0] for c in clips.values()]+[d['package'] for d in definitions]
(WORK/'cook.txt').write_text('\n'.join(packages)+'\n')
(WORK/(MODE+'-result.json')).write_text(json.dumps(dict(passed=True, packages=packages, cases=results), indent=2)+'\n')
