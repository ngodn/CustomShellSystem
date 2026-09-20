"""Moving component integration with public morphs and active body/hair rigs."""
import copy
import hashlib
import json
import math
import os
import sys
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=Path(os.environ.get('CSS_HAND_POSTPROCESS_MOTION_DIR',str(WORK/'hand-postprocess-motion-v1'))).resolve()
assert OUT.parent==WORK.resolve()
OUT.mkdir(exist_ok=True)
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from probe_controlrig_chain import key,set_value,transform
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert not (OUT/'report.json').exists()
prior=load(WORK/'hand-morph-import-regression-v1/report.json')
protected=dict(prior['protected_hashes'],**prior['assets'])
assert all(digest(Path(p))==h for p,h in protected.items())
mesh=unreal.load_asset(prior['packages'][0]);candidate=unreal.load_asset(prior['packages'][2])
bp=unreal.load_asset(prior['packages'][3]);assert mesh and candidate and bp
curves=[p['shape'] for p in load(HERE/'left-finger-correctives-v1.json')['parameters']]
changed={p['bone'] for p in load(HERE/'left-finger-calibration-v1.json')['parameters']}
bind=load(WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json');names=[b['name'] for b in bind]
body={'brust001','brust002','butt001','butt002','thigh_twist_02_l','thigh_twist_02_r','belly'}
fixtures=load(WORK/'hand-native-motion-fixtures-v1/fixtures.json')['cases']
timing=load(WORK/'running-attack-dense-hand-v2/dense/animation-poses.json')
assert all(abs(f['timeSeconds']-f['frame']/30)<.00001 for f in timing['frames'])

def angle(a,b):
    dot=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,dot)))

def excite(rotation,degrees):
    # Right-multiply a local X rotation, retaining the original source pose.
    x,y,z,w=[rotation[k] for k in 'XYZW'];s=math.sin(math.radians(degrees)/2);c=math.cos(math.radians(degrees)/2)
    q=[x*c+w*s,y*c+z*s,z*c-y*s,w*c-x*s];length=math.sqrt(sum(v*v for v in q))
    return dict(zip('XYZW',[v/length for v in q]))

results=[];sources={};motion=[]
for alpha in (0.,.5,1.):
    frames=[]
    for frame in range(112):
        case=next(c for c in fixtures if c['frame']==frame and c['alpha']==alpha and c['fraction']==0)
        path=Path(case['source']);assert digest(path)==case['source_sha256'];sources[str(path)]=case['source_sha256']
        snapshot=copy.deepcopy(load(path)['pose']['Snapshot']);time=frame/30
        for bone,degrees in [('head',20*math.sin(math.pi*time)),('pelvis',6*math.sin(2*math.pi*time))]:
            t=snapshot['LocalTransforms'][names.index(bone)];t['Rotation']=excite(t['Rotation'],degrees)
        frames.append(snapshot)
    document=dict(frames=frames)
    (OUT/f'alpha-{alpha}-input.json').write_text(json.dumps(document)+'\n')
    raw=unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(mesh,candidate,json.dumps(document),curves,False)
    assert raw
    (OUT/f'alpha-{alpha}-component.json').write_text(raw+'\n')
    measured=json.loads(raw);assert measured['moving_fixture'] and measured['fps']==30 and len(measured['samples'])==112
    rig=bp.create_control_rig();h=rig.get_hierarchy();body_peak=0.;hair_peak=0.
    for row in measured['samples']:
        h.reset_pose_to_initial(unreal.RigElementType.BONE)
        for name,value in zip(names,row['upstream'],strict=True):
            pose=h.get_local_transform(key(name));pose.translation=unreal.Vector(*value['translation'])
            pose.rotation=unreal.Quat(*value['rotation']);pose.scale3d=unreal.Vector(*value['scale'])
            h.set_local_transform(key(name),pose,False,True)
        set_value(rig,'Enabled',row['enabled']);set_value(rig,'InputIsV43Compatible',False)
        assert rig.execute('Forwards Solve')
        assert (rig.get_variable_as_string('HandValid').lower()=='true')==row['valid']
        max_angle=0.;max_position=0.;max_scale=0.
        for i,(name,input_pose,output) in enumerate(zip(names,row['upstream'],row['output'],strict=True)):
            expected=transform(h.get_local_transform(key(name))) if name in changed else input_pose
            rotation=angle(output['rotation'],expected['rotation'])
            position=max(abs(a-b) for a,b in zip(output['translation'],expected['translation']))
            scale=max(abs(a-b) for a,b in zip(output['scale'],expected['scale']))
            assert rotation<.001 and position<.0001 and scale<.00001,(alpha,row['frame'],name,rotation,position,scale)
            max_angle=max(max_angle,rotation);max_position=max(max_position,position);max_scale=max(max_scale,scale)
            source_rotation=[frames[row['frame']]['LocalTransforms'][i]['Rotation'][k] for k in 'XYZW']
            movement=angle(input_pose['rotation'],source_rotation)
            if name in body:body_peak=max(body_peak,movement)
            if name.startswith('CSS_Hair_'):hair_peak=max(hair_peak,movement)
        expected_curves={n:h.get_curve_value(unreal.RigElementKey(name=n,type=unreal.RigElementType.CURVE)) for n in curves}
        curve_error=max(abs(row['curves'][n]-v) for n,v in expected_curves.items())
        weight_error=max(abs(row['morph_weights'][n]-v) for n,v in expected_curves.items())
        assert curve_error<.00001 and weight_error<.0001,(alpha,row['frame'],curve_error,weight_error)
        assert len(row['public_curves'])==6
        public_error=max(abs(values[k]-values['upstream']) for values in row['public_curves'].values() for k in ('output','morph_weight'))
        compression_error=max(abs(values['upstream']-values['expected']) for values in row['public_curves'].values())
        assert public_error<.00001,(alpha,row['frame'],'public transfer',public_error)
        if not row['enabled']:assert all(v==0 for v in row['morph_weights'].values())
        if row['frame']>=105:assert all(v['morph_weight']==0 for v in row['public_curves'].values())
        results.append(dict(alpha=alpha,frame=row['frame'],enabled=row['enabled'],rotation_error_degrees=max_angle,
            position_error_cm=max_position,scale_error=max_scale,private_curve_error=curve_error,private_weight_error=weight_error,
            public_error=public_error,upstream_public_compression_error=compression_error))
    assert body_peak>.01 and hair_peak>.01,(alpha,body_peak,hair_peak)
    motion.append(dict(alpha=alpha,body_peak_local_rotation_degrees=body_peak,hair_peak_local_rotation_degrees=hair_peak))
    unreal.log('CSS_HAND_MOVING_COMPONENT_PASSED_ALPHA '+str(alpha))
assert len(results)==336 and all(digest(Path(p))==h for p,h in protected.items())
(OUT/'report.json').write_text(json.dumps(dict(passed=True,cases=results,motion=motion,sources=sources,protected_hashes=protected,
    scope='336 compressed moving-source component samples with synthetic head/pelvis/travel excitation, hand toggles, resets, replacement and six animated public morphs. Not full combat replay, public-shape contact acceptance, cooked/live execution or performance.'),indent=2)+'\n')
unreal.log('CSS_HAND_MOVING_COMPONENT_PASSED')
