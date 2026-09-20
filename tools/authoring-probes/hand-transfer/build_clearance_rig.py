"""Build and measure isolated native four-finger directional clearance.

Calibrated/articulated inputs only. Thumb stages and full production integration
remain separate. A strict numerical mismatch prevents saving the candidate.
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
OUT=Path(os.environ['CSS_NATIVE_CLEARANCE_DIR']).resolve()
assert OUT.parent==WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT/'report.json').exists()
MOD=ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE),str(MOD/'tools')]
from build_controlrig_chain_probe import Graph,Pin
from probe_controlrig_chain import key,set_value,transform
from native_clearance import append_finger_clearance

load=lambda p:json.loads(p.read_text())
model_path=WORK/'hand-clearance-b2-fit-v1/model.json'
model=load(model_path)
fixture_dir=Path(os.environ.get('CSS_NATIVE_CLEARANCE_FIXTURES',str(WORK/'hand-native-clearance-fixtures-one-v3'))).resolve()
assert fixture_dir.parent==WORK.resolve()
fixture_path=fixture_dir/'fixtures.json'
fixtures=load(fixture_path)
assert fixtures['model_sha256']==hashlib.sha256(model_path.read_bytes()).hexdigest()
iterations=fixtures.get('finger_iteration_limit',model['iteration_limit'])
assert 1<=iterations<=model['iteration_limit']
gradient_step=fixtures.get('gradient_step_degrees',model['gradient_step_degrees'])
model=dict(model,iteration_limit=iterations,gradient_step_degrees=gradient_step)
cases=fixtures['cases'];assert len(cases)==464
epsilon_tag=str(gradient_step).replace('.', 'p')
PACKAGE=f'/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftFingerClearanceV1_I{iterations}_E{epsilon_tag}'
assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE)
content=ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
protected=[content/'Shared/Skeletons/SKEL_CSS_Base.uasset',content/'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
mesh=unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B_V1');assert mesh
bp=unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(PACKAGE)
bp.set_auto_vm_recompile(False)
controller=bp.get_hierarchy_controller()
imported=controller.import_bones_from_skeletal_mesh(mesh,'None');assert len(imported)==379
names=[str(k.name) for k in imported]
assert [n.lower() for n in names]==[b['name'].lower() for b in model['bones']]
bp.set_preview_mesh(mesh,False)
sentinel=controller.add_curve('CSS_Diagnostic_UnrelatedCurve',.375,False,False)
g=Graph(bp)
g.member('Enabled','bool','False',True)
begin=Pin(g.unit('RigUnit_BeginExecution')+'.ExecuteContext')
active,bypass=g.branch(begin,g.get('Enabled'))
active=append_finger_clearance(g,active,model)
changed=[]
for fi,m in enumerate(model['models'][1:]):
    name=model['bones'][m['indices'][0]]['name'];changed.append(name)
    node=g.unit('RigUnit_SetTransform',Space='LocalSpace',bInitial=False,bPropagateToChildren=True)
    g.link(active,node+'.ExecuteContext')
    g.value(node+'.Item.Type','Bone');g.value(node+'.Item.Name',name)
    g.value(node+'.Value',g.at('ClearanceOutput',fi))
    active=Pin(node+'.ExecuteContext')
g.set(bypass,'ClearanceValid',False)
bp.recompile_vm();unreal.BlueprintEditorLibrary.compile_blueprint(bp)
rig=bp.create_control_rig();assert rig.get_variable_as_string('Enabled').lower()=='false'

def angle(a,b):
    dot=abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,dot)))

def evaluate(case, enabled=True, antipodes=False, instance=rig, suffix='', invalid_scale=False):
    path=Path(case['source']);assert hashlib.sha256(path.read_bytes()).hexdigest()==case['source_sha256']
    doc=load(path)['pose']['Snapshot'];h=instance.get_hierarchy()
    h.reset_pose_to_initial(unreal.RigElementType.BONE)
    for name,b,t in zip(names,model['bones'],doc['LocalTransforms'][:379],strict=True):
        x=h.get_local_transform(key(name))
        # Seed B2 translations and captured scales explicitly, including the
        # attachment locals absent from the diagnostic mesh's initial pose.
        x.translation=unreal.Vector(*b['translation'])
        x.scale3d=unreal.Vector(*[t['Scale3D'][k] for k in 'XYZ'])
        if invalid_scale and name.lower()=='hand_l':x.scale3d=unreal.Vector(2,1,1)
        rotation=[t['Rotation'][k] for k in 'XYZW']
        if antipodes:rotation=[-q for q in rotation]
        x.rotation=unreal.Quat(*rotation)
        h.set_local_transform(key(name),x,False,True)
    before={name:transform(h.get_local_transform(key(name))) for name in names}
    h.set_curve_value(sentinel,.375)
    set_value(instance,'Enabled',enabled)
    start=time.perf_counter_ns();assert instance.execute('Forwards Solve');elapsed=(time.perf_counter_ns()-start)/1000
    valid=instance.get_variable_as_string('ClearanceValid').lower()=='true'
    assert valid==(enabled and not invalid_scale),(case['label'],valid)
    raw=instance.get_variable_as_string('ClearanceOutput')
    tuples=re.findall(r'\(X=([^,]+),Y=([^,]+),Z=([^,]+),W=([^\)]+)\)',raw)
    if enabled:
        assert len(tuples)==4,raw
    computed={n:list(map(float,q)) for n,q in zip(changed,tuples)}
    math_errors={n:angle(computed[n],case['expected_xyzw'][n]) for n in changed} if enabled and valid else {}
    applied_errors={};rotations={}
    for name in names:
        actual=transform(h.get_local_transform(key(name)))
        assert all(math.isfinite(v) for values in actual.values() for v in values)
        assert actual['translation']==before[name]['translation'] and actual['scale']==before[name]['scale'],name
        if name not in changed or not enabled or not valid:assert actual==before[name],name
        else:
            rotations[name]=actual['rotation']
            wanted=computed[name];incoming=before[name]['rotation']
            if min(max(abs(a-b) for a,b in zip(incoming,wanted)),max(abs(a+b) for a,b in zip(incoming,wanted)))<=.0001:wanted=incoming
            applied_errors[name]=angle(actual['rotation'],wanted)
    assert h.get_curve_value(sentinel)==.375
    return dict(label=case['label']+suffix,enabled=enabled,antipodes=antipodes,valid=valid,
                max_math_error_degrees=max(math_errors.values(),default=0),math_errors=math_errors,
                max_applied_error_degrees=max(applied_errors.values(),default=0),output_rotations_xyzw=rotations,
                angles=instance.get_variable_as_string('ClearanceAngles'),iterations=int(instance.get_variable_as_string('ClearanceIterations')),
                execute_us=elapsed)

results=[]
for i,case in enumerate(cases):
    results.append(evaluate(case))
    results.append(evaluate(case,antipodes=True,suffix='-antipodes'))
    if i%50==0:
        unreal.log('CSS_CLEARANCE_PROGRESS '+str(i))
        (OUT/'partial.json').write_text(json.dumps(results))
case=cases[0]
results.extend([evaluate(case,False,suffix='-disabled'),evaluate(case,True,suffix='-reenabled'),evaluate(case,True,suffix='-repeat')])
fresh=bp.create_control_rig();assert fresh.get_variable_as_string('Enabled').lower()=='false'
results.extend([evaluate(case,False,instance=fresh,suffix='-fresh-disabled'),evaluate(case,True,instance=fresh,suffix='-fresh-enabled')])
results.append(evaluate(case,invalid_scale=True,suffix='-unsupported-scale'))
max_math=max(r['max_math_error_degrees'] for r in results)
max_applied=max(r['max_applied_error_degrees'] for r in results)
times=[r['execute_us'] for r in results if r['enabled']]
report=dict(scope=__doc__,package=PACKAGE,native_nodes=g.serial,cases=results,
            fixture_sha256=hashlib.sha256(fixture_path.read_bytes()).hexdigest(),fixture_path=str(fixture_path),
            finger_iteration_limit=iterations,gradient_step_degrees=gradient_step,protected_hashes=hashes,
            max_math_error_degrees=max_math,max_applied_error_degrees=max_applied,
            timing_scope='Editor execute with Python bridge across changing fixtures, not live FPS or a paired benchmark.',
            median_execute_us=statistics.median(times),p95_execute_us=sorted(times)[int(len(times)*.95)],
            passed=max_math<.001 and max_applied<.001)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
assert report['passed'],(max_math,max_applied)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
unreal.log('CSS_NATIVE_FINGER_CLEARANCE_PASSED')
