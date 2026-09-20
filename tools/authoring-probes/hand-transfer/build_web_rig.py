"""Native thumb-web guard followed by source corrective curves.

Diagnostic stage only: fixtures already contain calibration/finger/tip
clearance. No production binding, native clearance loop or live deployment.
"""
import hashlib
import json
import math
import os
import statistics
import sys
import time
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_NATIVE_WEB_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT/'report.json').exists()
MOD = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE),str(MOD/'tools')]
from build_controlrig_chain_probe import Graph, Pin
from probe_controlrig_chain import key, set_value, transform
from native_nodes import append_web_guard, append_correctives, clear_correctives

load = lambda p: json.loads(p.read_text())
curves = load(HERE/'left-finger-correctives-v1.json')['parameters']
web = load(HERE/'thumb-web-guard-v2.json')
driver = next(m for m in curves if m['bone']==web['bone'])
fixture_path = WORK/'hand-native-web-fixtures-v1/fixtures.json'
cases = load(fixture_path)['cases']
assert len(cases)==569
PACKAGE = '/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandWebCorrectivesV3'
assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE)
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
protected = [content/'Shared/Skeletons/SKEL_CSS_Base.uasset',content/'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
mesh = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B_V1')
assert mesh
bp = unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(PACKAGE)
bp.set_auto_vm_recompile(False)
controller = bp.get_hierarchy_controller()
names = [str(k.name) for k in controller.import_bones_from_skeletal_mesh(mesh,'None')]
assert len(names)==379
bp.set_preview_mesh(mesh,False)
for m in curves: controller.add_curve(m['shape'],0,False,False)
sentinel = controller.add_curve('CSS_Diagnostic_UnrelatedCurve',.375,False,False)
g = Graph(bp)
g.member('Enabled','bool','False',True)
begin = Pin(g.unit('RigUnit_BeginExecution')+'.ExecuteContext')
active,bypass = g.branch(begin,g.get('Enabled'))
active = append_web_guard(g,active,web,driver)
append_correctives(g,controller,active,curves,True)
bypass = g.set(bypass,'WebCorrectionDegrees',0)
clear_correctives(g,bypass,curves)
bp.recompile_vm()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
rig = bp.create_control_rig()
assert rig.get_variable_as_string('Enabled').lower()=='false'
curve_keys = {m['shape']:unreal.RigElementKey(name=m['shape'],type=unreal.RigElementType.CURVE) for m in curves}

def angular_error(a,b):
    na,nb=math.sqrt(sum(x*x for x in a)),math.sqrt(sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,abs(sum(x*y for x,y in zip(a,b)))/(na*nb))))

def seed(instance,case,antipodes=False):
    h=instance.get_hierarchy()
    h.reset_pose_to_initial(unreal.RigElementType.BONE)
    lookup={n.lower():i for i,n in enumerate(case['snapshot']['BoneNames'])}
    for name in names:
        t=h.get_local_transform(key(name))
        q=[case['snapshot']['LocalTransforms'][lookup[name.lower()]]['Rotation'][k] for k in 'XYZW']
        if antipodes and name.lower()==web['bone']: q=[-v for v in q]
        t.rotation=unreal.Quat(*q)
        h.set_local_transform(key(name),t,False,True)
    return h

def evaluate(case,enabled=True,antipodes=False,instance=rig,suffix=''):
    h=seed(instance,case,antipodes)
    h.set_curve_value(sentinel,.375)
    before={name:transform(h.get_local_transform(key(name))) for name in names}
    set_value(instance,'Enabled',enabled)
    assert instance.execute('Forwards Solve')
    actual_thumb=None
    for name in names:
        actual=transform(h.get_local_transform(key(name)))
        assert all(math.isfinite(v) for field in actual.values() for v in field)
        assert actual['translation']==before[name]['translation'],name
        assert actual['scale']==before[name]['scale'],name
        if not enabled or name.lower()!=web['bone'] or case['correction_degrees']==0:
            assert actual==before[name],('passthrough changed',case['label'],name)
        if name.lower()==web['bone']: actual_thumb=actual['rotation']
    expected=case['expected_rotation_xyzw'] if enabled else before[web['bone']]['rotation']
    rotation_error=angular_error(actual_thumb,expected)
    values={name:h.get_curve_value(k) for name,k in curve_keys.items()}
    curve_error=max(abs(v-(case['expected_curves'][name] if enabled else 0)) for name,v in values.items())
    correction=float(instance.get_variable_as_string('WebCorrectionDegrees'))
    correction_error=abs(correction-(case['correction_degrees'] if enabled else 0))
    assert h.get_curve_value(sentinel)==.375
    return dict(label=case['label']+suffix,enabled=enabled,antipodes=antipodes,
                rotation_error_degrees=rotation_error,curve_error=curve_error,
                correction_error_degrees=correction_error,output_rotation_xyzw=actual_thumb,curves=values)

rows=[]
started=time.monotonic()
for i,case in enumerate(cases):
    rows.append(evaluate(case))
    rows.append(evaluate(case,antipodes=True,suffix='-antipodes'))
    if i%100==0: unreal.log('CSS_NATIVE_WEB_PROGRESS '+str(i+1))
active_case=max(cases,key=lambda c:c['correction_degrees'])
rows += [evaluate(active_case,False,suffix='-disabled'),evaluate(active_case,True,suffix='-reenabled'),
         evaluate(active_case,True,suffix='-fresh-input')]
fresh=bp.create_control_rig()
assert fresh.get_variable_as_string('Enabled').lower()=='false'
rows += [evaluate(active_case,False,instance=fresh,suffix='-new-disabled'),evaluate(active_case,True,instance=fresh,suffix='-new-enabled')]
max_rotation=max(r['rotation_error_degrees'] for r in rows)
max_curve=max(r['curve_error'] for r in rows)
max_correction=max(r['correction_error_degrees'] for r in rows)
report=dict(scope=__doc__,package=PACKAGE,native_nodes=g.serial,cases=rows,
    fixture_sha256=hashlib.sha256(fixture_path.read_bytes()).hexdigest(),
    max_rotation_error_degrees=max_rotation,max_curve_error=max_curve,max_correction_error_degrees=max_correction,
    passed=max_rotation<.001 and max_curve<1e-5 and max_correction<.001,
    unchanged_bones_translations_scales_and_curve=True,protected_hashes=hashes,seconds=time.monotonic()-started)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
assert report['passed'],(max_rotation,max_curve,max_correction)

# Paired editor-only timings exclude fixture setup and include Python's execute
# bridge equally. This is not a shipping/game performance acceptance test.
baseline_bp=unreal.load_asset('/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftFingerCorrectivesV1')
assert baseline_bp
baseline=baseline_bp.create_control_rig()
timings=[]
for block,order in enumerate((('baseline','web'),('web','baseline'),('web','baseline'),('baseline','web'))):
    for label in order:
        instance=baseline if label=='baseline' else rig
        h=seed(instance,active_case)
        original=h.get_local_transform(key(web['bone']))
        set_value(instance,'Enabled',True)
        for sample in range(220):
            h.set_local_transform(key(web['bone']),original,False,True)
            stamp=time.perf_counter_ns()
            ok=instance.execute('Forwards Solve')
            elapsed=(time.perf_counter_ns()-stamp)/1000
            assert ok
            if sample>=20: timings.append(dict(block=block,label=label,microseconds=elapsed))
summary={label:dict(median_us=statistics.median(r['microseconds'] for r in timings if r['label']==label),
                    p95_us=sorted(r['microseconds'] for r in timings if r['label']==label)[759]) for label in ('baseline','web')}
(OUT/'timing.json').write_text(json.dumps(dict(scope='Isolated editor execute including equal Python bridge, one active input; not live FPS or full-hand cost.',summary=summary,samples=timings),indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
unreal.log('CSS_NATIVE_WEB_PASSED '+str((len(rows),max_rotation,max_curve,max_correction)))
