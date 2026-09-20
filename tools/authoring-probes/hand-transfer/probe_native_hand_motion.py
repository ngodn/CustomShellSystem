"""Evaluate sampled motion through the saved complete native graph.

No graph rebuild, offline hand repair or output interpolation. Added local
rotation steps are measured relative to actual native pre-contact snapshots.
"""
import hashlib
import json
import math
import os
import re
import statistics
import sys
import time
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=Path(os.environ['CSS_NATIVE_HAND_MOTION_DIR']).resolve()
FIXTURES=Path(os.environ['CSS_NATIVE_HAND_MOTION_FIXTURES']).resolve()
assert OUT.parent==FIXTURES.parent==WORK.resolve()
OUT.mkdir(exist_ok=True);assert not (OUT/'report.json').exists()
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from probe_controlrig_chain import key,set_value,transform
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
fixture_path=FIXTURES/'fixtures.json';fixtures=load(fixture_path)
assert fixtures['sampling']=='original-input-running-transitions-v1' and len(fixtures['cases'])==759
assert all(digest(Path(p))==h for p,h in fixtures['sources'].items())
asset=load(WORK/'hand-combined-native-v1/saved-asset.json')
assert digest(Path(asset['path']))==asset['sha256']
bp=unreal.load_asset('/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1');assert bp
rig=bp.create_control_rig();assert rig.get_variable_as_string('Enabled').lower()=='false'
model=load(WORK/'hand-clearance-b2-fit-v1/model.json')
curves=load(HERE/'left-finger-correctives-v1.json')['parameters']
changed={m['bone'] for m in load(HERE/'left-finger-calibration-v1.json')['parameters']}
curve_keys={m['shape']:unreal.RigElementKey(name=m['shape'],type=unreal.RigElementType.CURVE) for m in curves}
sentinel=unreal.RigElementKey(name='CSS_Diagnostic_UnrelatedCurve',type=unreal.RigElementType.CURVE)
proximal=[model['bones'][m['indices'][0]]['name'] for m in model['models']]

def angle(a,b):
    dot=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,dot)))

def rotations(name,count):
    raw=rig.get_variable_as_string(name)
    tuples=re.findall(r'\(X=([^,]+),Y=([^,]+),Z=([^,]+),W=([^\)]+)\)',raw)
    assert len(tuples)==count,(name,raw)
    return [list(map(float,q)) for q in tuples]

rows=[];steps=[];previous={}
for i,case in enumerate(fixtures['cases']):
    path=Path(case['source']);assert digest(path)==case['source_sha256']
    snap=load(path)['pose']['Snapshot'];h=rig.get_hierarchy()
    assert [n.lower() for n in snap['BoneNames'][:379]]==[b['name'].lower() for b in model['bones']]
    h.reset_pose_to_initial(unreal.RigElementType.BONE)
    for b,t in zip(model['bones'],snap['LocalTransforms'][:379],strict=True):
        v=h.get_local_transform(key(b['name']))
        v.translation=unreal.Vector(*b['translation'])
        v.rotation=unreal.Quat(*[t['Rotation'][k] for k in 'XYZW'])
        v.scale3d=unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ'])
        h.set_local_transform(key(b['name']),v,False,True)
    before={b['name']:transform(h.get_local_transform(key(b['name']))) for b in model['bones']}
    h.set_curve_value(sentinel,.375)
    set_value(rig,'Enabled',True);set_value(rig,'InputIsV43Compatible',False)
    stamp=time.perf_counter_ns();assert rig.execute('Forwards Solve');elapsed=(time.perf_counter_ns()-stamp)/1000
    assert rig.get_variable_as_string('HandValid').lower()=='true',case['label']
    output={}
    for b in model['bones']:
        name=b['name'];actual=transform(h.get_local_transform(key(name)))
        assert all(math.isfinite(v) for values in actual.values() for v in values)
        assert actual['translation']==before[name]['translation'] and actual['scale']==before[name]['scale']
        if name not in changed:assert actual==before[name],name
        else:output[name]=actual['rotation']
    values={n:h.get_curve_value(k) for n,k in curve_keys.items()}
    assert all(math.isfinite(v) for v in values.values()) and h.get_curve_value(sentinel)==.375
    fingers=rotations('Finger_ClearanceLocals',12);thumb=rotations('Thumb_ClearanceLocals',15)
    precontact={proximal[0]:thumb[0],**{n:fingers[j*3] for j,n in enumerate(proximal[1:])}}
    row=dict(label=case['label'],enabled=True,valid=True,antipodes=False,
             output_rotations_xyzw=output,curves=values,precontact_rotations_xyzw=precontact,
             web_degrees=float(rig.get_variable_as_string('WebCorrectionDegrees')),execute_us=elapsed)
    alpha=case['alpha']
    if alpha in previous:
        prev,prev_case=previous[alpha]
        steps.append(dict(previous=prev_case['label'],current=case['label'],alpha=alpha,
            seconds=case['time_seconds']-prev_case['time_seconds'],
            maximum_added_rotation_degrees=max(angle(prev['output_rotations_xyzw'][n],output[n])-angle(prev['precontact_rotations_xyzw'][n],precontact[n]) for n in proximal)))
    previous[alpha]=(row,case);rows.append(row)
    if i%50==0:
        (OUT/'partial.json').write_text(json.dumps(rows))
        unreal.log('CSS_NATIVE_HAND_MOTION_PROGRESS '+str(i+1))
assert len(steps)==756 and digest(Path(asset['path']))==asset['sha256']
timings=[r['execute_us'] for r in rows]
web=load(HERE/'thumb-web-guard-v2.json')
passed=all(r['web_degrees']<web['maximum_correction']-.0001 for r in rows)
report=dict(scope=__doc__,passed=passed,cases=rows,steps=steps,asset=asset,
    fixture_path=str(fixture_path),fixture_sha256=digest(fixture_path),
    maximum_added_step_degrees=max(s['maximum_added_rotation_degrees'] for s in steps),
    median_execute_us=statistics.median(timings),p95_execute_us=sorted(timings)[int(len(timings)*.95)],
    timing_scope='Changing-pose editor execute with Python bridge, not live FPS.',
    acceptance_scope='Valid finite output, preserved other data and nonsaturated web guard. Skin and visual motion are separate checks; added-step metric is descriptive.')
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
assert passed,'Web guard saturates on sampled native motion'
unreal.log('CSS_NATIVE_HAND_MOTION_PASSED')
