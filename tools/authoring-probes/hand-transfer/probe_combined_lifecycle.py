"""Warm-instance disable, invalid-input and recovery checks on the saved graph."""
import hashlib
import json
import math
import os
import sys
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=Path(os.environ['CSS_COMBINED_LIFECYCLE_DIR']).resolve()
assert OUT.parent==WORK.resolve()
OUT.mkdir(exist_ok=True);assert not (OUT/'report.json').exists()
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from probe_controlrig_chain import key,set_value,transform
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
native=WORK/'hand-combined-native-v1'
report=load(native/'report.json');assert report['passed'] and load(native/'exit.json')['exit_code']==0
fixture_path=Path(report['fixture_path']);assert digest(fixture_path)==report['fixture_sha256']
fixtures={c['label']:c for c in load(fixture_path)['cases']}
measured=max((r for r in report['cases'] if r['enabled'] and r['valid'] and not r['antipodes']),
             key=lambda r:sum(abs(v) for v in r['curves'].values()))
assert sum(abs(v) for v in measured['curves'].values())>1
active_case=fixtures[measured['label']]
singular=next(c for c in fixtures.values() if c['singular'])
model=load(WORK/'hand-clearance-b2-fit-v1/model.json')
curves=load(HERE/'left-finger-correctives-v1.json')['parameters']
asset=load(native/'saved-asset.json');assert digest(Path(asset['path']))==asset['sha256']
bp=unreal.load_asset(report['package']);assert bp
rig=bp.create_control_rig();assert rig.get_variable_as_string('Enabled').lower()=='false'
sentinel=unreal.RigElementKey(name='CSS_Diagnostic_UnrelatedCurve',type=unreal.RigElementType.CURVE)
curve_keys={m['shape']:unreal.RigElementKey(name=m['shape'],type=unreal.RigElementType.CURVE) for m in curves}

def angle(a,b):
    dot=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,dot)))

def evaluate(label,case=active_case,enabled=True,invalid_scale=False,instance=rig,seed_private=False):
    path=Path(case['source']);assert digest(path)==case['source_sha256']
    snap=load(path)['pose']['Snapshot'];h=instance.get_hierarchy()
    h.reset_pose_to_initial(unreal.RigElementType.BONE)
    for b,t in zip(model['bones'],snap['LocalTransforms'][:379],strict=True):
        value=h.get_local_transform(key(b['name']))
        value.translation=unreal.Vector(*b['translation'])
        value.scale3d=unreal.Vector(*([2,1,1] if invalid_scale and b['name']=='hand_l' else [t['Scale3D'][k] for k in 'XYZ']))
        value.rotation=unreal.Quat(*[t['Rotation'][k] for k in 'XYZW'])
        h.set_local_transform(key(b['name']),value,False,True)
    before={b['name']:transform(h.get_local_transform(key(b['name']))) for b in model['bones']}
    h.set_curve_value(sentinel,.375)
    if seed_private:
        for k in curve_keys.values():h.set_curve_value(k,.625)
    set_value(instance,'Enabled',enabled)
    set_value(instance,'InputIsV43Compatible',case['v43_compatible'])
    assert instance.execute('Forwards Solve')
    valid=instance.get_variable_as_string('HandValid').lower()=='true'
    assert valid==(enabled and not invalid_scale and not case['singular'])
    errors=[]
    for b in model['bones']:
        name=b['name'];after=transform(h.get_local_transform(key(name)))
        assert after['translation']==before[name]['translation'] and after['scale']==before[name]['scale']
        expected=measured['output_rotations_xyzw'][name] if valid and name in measured['output_rotations_xyzw'] else before[name]['rotation']
        errors.append(angle(after['rotation'],expected))
    values={n:h.get_curve_value(k) for n,k in curve_keys.items()}
    curve_error=max(abs(v-(measured['curves'][n] if valid else 0)) for n,v in values.items())
    assert h.get_curve_value(sentinel)==.375
    if not valid:assert float(instance.get_variable_as_string('WebCorrectionDegrees'))==0
    return dict(label=label,valid=valid,max_rotation_error_degrees=max(errors),max_curve_error=curve_error)

rows=[evaluate('fresh-disabled-with-stale-private-curves',enabled=False,seed_private=True),
      evaluate('active'),evaluate('warm-disabled',enabled=False),evaluate('reenabled'),
      evaluate('warm-singular',case=singular),evaluate('after-singular'),
      evaluate('warm-unsupported-scale',invalid_scale=True),evaluate('after-scale'),
      evaluate('repeated-fresh-input')]
fresh=bp.create_control_rig();assert fresh.get_variable_as_string('Enabled').lower()=='false'
rows += [evaluate('replacement-disabled',enabled=False,instance=fresh,seed_private=True),
         evaluate('replacement-enabled',instance=fresh)]
assert digest(Path(asset['path']))==asset['sha256']
passed=all(r['max_rotation_error_degrees']<.001 and r['max_curve_error']<1e-5 for r in rows)
(OUT/'report.json').write_text(json.dumps(dict(scope=__doc__,passed=passed,cases=rows,
    active_pose=active_case['label'],native_report_sha256=digest(native/'report.json'),asset=asset),indent=2)+'\n')
assert passed,rows
unreal.log('CSS_COMBINED_LIFECYCLE_PASSED')
