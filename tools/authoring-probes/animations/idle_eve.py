"""UE 5.6.1: build the source-fitted Eve idle and validate its timed carrier."""
import hashlib,json,math,os
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE=os.environ.get('CSS_IDLE_MODE','create')
assert MODE in ('create','readback') and not (WORK/(MODE+'-result.json')).exists()
content=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
private='/Game/CSS/AnimLab/RT_I1_Idle'
public='/Game/CSS/Eve/Anim/AN_I1_Idle'
carrier_path='/Game/CSS/Eve/Anim/BS_I1_Idle'
outputs={content/(p.removeprefix('/Game/')+'.uasset') for p in (private,public,carrier_path)}
if MODE=='create':
    assert not any(p.exists() for p in outputs)
    protected={str(p):hashlib.sha256(p.read_bytes()).hexdigest()
               for p in (content/'CSS').rglob('*.uasset') if p not in outputs}
    (WORK/'protected.json').write_text(json.dumps(protected,indent=2)+'\n')
else:protected=json.loads((WORK/'protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items())
mesh=unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
blueprint=unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
bind=json.loads((ROOT/'CustomShellSystem/work/anim10/batch/target-bind.json').read_text())
names=[b['name'] for b in bind]
document=json.loads((WORK/'idle-motion.json').read_text())
changed={n+'_'+s for n in ('thigh','calf','foot') for s in ('l','r')}
options=unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW)
options.set_editor_property('optional_skeletal_mesh',mesh)
tools=unreal.AssetToolsHelpers.get_asset_tools()
if MODE=='create':
    source=unreal.load_asset('/Game/CSS/AnimLab/RT_D2_Idle')
    clip=tools.duplicate_asset('RT_I1_Idle','/Game/CSS/AnimLab',source)
    assert clip
    edit=clip.get_editor_property('controller')
    edit.open_bracket('Fit Eve idle leg trajectories',False)
    try:
        for name in sorted(changed):
            i=names.index(name)
            ts=[f['pose']['Snapshot']['LocalTransforms'][i] for f in document['frames']]
            assert edit.set_bone_track_keys(name,
                [unreal.Vector(*[t['Translation'][k] for k in 'XYZ']) for t in ts],
                [unreal.Quat(*[t['Rotation'][k] for k in 'XYZW']) for t in ts],
                [unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ']) for t in ts],False)
    finally:edit.close_bracket(False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(clip,False)
    exported=tools.duplicate_asset('AN_I1_Idle','/Game/CSS/Eve/Anim',clip)
    assert exported and unreal.EditorAssetLibrary.save_loaded_asset(exported,False)
    carrier=unreal.CSSAnimationLibrary.create_idle_carrier(mesh,exported,carrier_path)
    assert carrier and unreal.EditorAssetLibrary.save_loaded_asset(carrier,False)
else:
    clip=unreal.load_asset(private);exported=unreal.load_asset(public)
    carrier=unreal.load_asset(carrier_path)
assert clip and exported and carrier
raw=[]
for asset in (clip,exported):
    assert asset.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
    assert math.isclose(asset.get_play_length(),7.,abs_tol=.0001)
    max_position=max_rotation=0.
    for f,entry in enumerate(document['frames']):
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(asset,f/document['fps'],options)
        assert unreal.AnimPoseExtensions.is_valid(pose)
        for i,name in enumerate(names):
            t=unreal.AnimPoseExtensions.get_bone_pose(pose,name,unreal.AnimPoseSpaces.LOCAL)
            e=entry['pose']['Snapshot']['LocalTransforms'][i]
            pe=math.dist([t.translation.x,t.translation.y,t.translation.z],[e['Translation'][k] for k in 'XYZ'])
            a=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]
            b=[e['Rotation'][k] for k in 'XYZW']
            qe=min(math.dist(a,b),math.dist(a,[-x for x in b]))
            assert pe<.001 and qe<.0001,(asset.get_name(),f,name,pe,qe)
            assert math.dist([t.scale3d.x,t.scale3d.y,t.scale3d.z],[e['Scale3D'][k] for k in 'XYZ'])<.0001
            max_position=max(max_position,pe);max_rotation=max(max_rotation,qe)
    raw.append(dict(asset=asset.get_path_name(),frames=211,max_position_cm=max_position,max_quaternion_distance=max_rotation))
    (WORK/(MODE+'-raw-progress.json')).write_text(json.dumps(raw,indent=2)+'\n')
points=[unreal.Vector(-180,0,0),unreal.Vector(180,0,0),unreal.Vector(0,0,0),unreal.Vector(47,233,0)]
query=json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(carrier,points))
for point in query['queries']:
    assert all(s['animation']==exported.get_path_name() and math.isclose(s['rate'],1.,abs_tol=1e-6) for s in point['samples'])
(WORK/(MODE+'-blend.json')).write_text(json.dumps(query,indent=2)+'\n')
playback=[]
if MODE=='readback':
    for name,point in [('center',unreal.Vector(0,0,0)),('corner',unreal.Vector(180,0,0))]:
        path=WORK/(name+'-component.json')
        assert not path.exists()
        data=json.loads(unreal.CSSAnimationLibrary.evaluate_clip(mesh,clip,blueprint,2,carrier,point,True))
        assert len(data['frames'])==841 and data['advance_blend_clock']
        errors=[]
        for frame in data['frames']:
            delta=abs(frame['blend_normalized_time']-(frame['time']/7)%1)
            errors.append(min(delta,abs(1-delta)))
        assert max(errors)<.0001,(name,max(errors))
        path.write_text(json.dumps(data,separators=(',',':'))+'\n')
        playback.append(dict(case=name,frames=841,clock_error=max(errors)))
        (WORK/'playback-progress.json').write_text(json.dumps(playback,indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in protected.items())
(WORK/'cook.txt').write_text(public+'\n'+carrier_path+'\n')
(WORK/(MODE+'-result.json')).write_text(json.dumps(dict(passed=True,raw=raw,cases=playback,
    scope='Offline saved raw tracks and real-clock compressed carrier. No claim of exact secondary-pose equivalence, live weapon restoration or gameplay acceptance.'),indent=2)+'\n')
