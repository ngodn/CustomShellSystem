"""Compare a complete native hand graph with its separately verified stages.

All stages receive fresh B2-local poses. Sequential native evaluation is the
composition oracle; independent numerical/skin evidence exists per stage.
This does not replace complete-output skin, transition or live acceptance.
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
OUT = Path(os.environ['CSS_COMBINED_HAND_DIR']).resolve()
FIXTURES = Path(os.environ['CSS_COMBINED_HAND_FIXTURES']).resolve()
assert OUT.parent == FIXTURES.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT/'report.json').exists()
MOD = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE),str(MOD/'tools')]
from build_controlrig_chain_probe import Graph, Pin
from probe_controlrig_chain import key, set_value, transform
from native_hand import append_hand

load = lambda p: json.loads(p.read_text())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
calibration = load(HERE/'left-finger-calibration-v1.json')['parameters']
curves = load(HERE/'left-finger-correctives-v1.json')['parameters']
articulation = load(WORK/'hand-articulation-fixtures-v1/fixtures.json')['parameters']
clearance = load(WORK/'hand-clearance-b2-fit-v1/model.json')
web = load(HERE/'thumb-web-guard-v2.json')
fixture_path = FIXTURES/'fixtures.json'
fixtures = load(fixture_path)
assert all(digest(Path(p)) == h for p,h in fixtures['sources'].items())
cases = fixtures['cases']; assert len(cases) == 454
PACKAGE = '/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCombinedV1'
assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE)
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
protected = [content/'Shared/Skeletons/SKEL_CSS_Base.uasset',content/'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes = {str(p):digest(p) for p in protected}
mesh = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B_V1'); assert mesh
bp = unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(PACKAGE)
bp.set_auto_vm_recompile(False)
controller = bp.get_hierarchy_controller()
names = [str(k.name) for k in controller.import_bones_from_skeletal_mesh(mesh,'None')]
assert [n.lower() for n in names] == [b['name'].lower() for b in clearance['bones']]
bp.set_preview_mesh(mesh,False)
for m in curves: controller.add_curve(m['shape'],0,False,False)
sentinel = controller.add_curve('CSS_Diagnostic_UnrelatedCurve',.375,False,False)
g = Graph(bp); g.member('Enabled','bool','False',True)
append_hand(g,controller,Pin(g.unit('RigUnit_BeginExecution')+'.ExecuteContext'),
            calibration,articulation,clearance,web,curves)
bp.recompile_vm(); unreal.BlueprintEditorLibrary.compile_blueprint(bp)
rig = bp.create_control_rig(); assert rig.get_variable_as_string('Enabled').lower() == 'false'
bone_keys = {n:key(n) for n in names}
curve_keys = {m['shape']:unreal.RigElementKey(name=m['shape'],type=unreal.RigElementType.CURVE) for m in curves}
changed = {m['bone'] for m in calibration}
reference_specs = [
    ('calibration','CR_CSS_LeftHandCalibrationV1','ValidInput'),
    ('articulation','CR_CSS_LeftHandArticulationV1','ArticulationValid'),
    ('fingers','CR_CSS_LeftFingerClearanceV2_I1_E1p0','ClearanceValid'),
    ('thumb','CR_CSS_LeftThumbTipClearanceV2_I12_E0p05','ClearanceValid'),
    ('web','CR_CSS_LeftHandWebCorrectivesV3',None),
]
references = []
reference_hashes = {}
for stage,name,valid in reference_specs:
    asset = unreal.load_asset('/Game/CSSAuthoring/TransientProbes/'+name); assert asset
    references.append((stage,asset.create_control_rig(),valid))
    path = content/('TransientProbes/'+name+'.uasset')
    reference_hashes[str(path)] = digest(path)

def angle(a,b):
    dot = abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
    return math.degrees(2*math.acos(min(1,dot)))

def initial(case, antipodes=False, invalid_scale=False):
    path = Path(case['source']); assert digest(path) == case['source_sha256']
    snap = load(path)['pose']['Snapshot']
    assert [n.lower() for n in snap['BoneNames'][:379]] == [n.lower() for n in names]
    result = {}
    for name,b,t in zip(names,clearance['bones'],snap['LocalTransforms'][:379],strict=True):
        q = [t['Rotation'][k] for k in 'XYZW']
        if antipodes:q = [-x for x in q]
        scale = [t['Scale3D'][k] for k in 'XYZ']
        if invalid_scale and name.lower() == 'hand_l':scale = [2,1,1]
        value = unreal.Transform()
        value.translation = unreal.Vector(*b['translation'])
        value.rotation = unreal.Quat(*q)
        value.scale3d = unreal.Vector(*scale)
        result[name] = value
    return result

def seed(instance, pose, compatible):
    h = instance.get_hierarchy(); h.reset_pose_to_initial(unreal.RigElementType.BONE)
    for n,t in pose.items():h.set_local_transform(bone_keys[n],t,False,True)
    h.set_curve_value(sentinel,.375)
    set_value(instance,'Enabled',True)
    return h

def serial_reference(pose, compatible):
    current = pose
    times = {}
    for stage,instance,valid in references:
        h = seed(instance,current,compatible)
        if stage == 'calibration':set_value(instance,'InputIsV43Compatible',compatible)
        stamp = time.perf_counter_ns(); assert instance.execute('Forwards Solve')
        times[stage] = (time.perf_counter_ns()-stamp)/1000
        if valid and instance.get_variable_as_string(valid).lower() != 'true':
            return pose,{n:0 for n in curve_keys},False,times
        current = {n:h.get_local_transform(k) for n,k in bone_keys.items()}
    return current,{n:h.get_curve_value(k) for n,k in curve_keys.items()},True,times

def evaluate(case, enabled=True, antipodes=False, instance=rig, suffix='', invalid_scale=False):
    pose = initial(case,antipodes,invalid_scale)
    expected,expected_curves,expected_valid,stage_times = serial_reference(pose,case['v43_compatible']) if enabled else (pose,{n:0 for n in curve_keys},False,{})
    h = seed(instance,pose,case['v43_compatible'])
    before = {n:transform(h.get_local_transform(k)) for n,k in bone_keys.items()}
    set_value(instance,'Enabled',enabled)
    set_value(instance,'InputIsV43Compatible',case['v43_compatible'])
    stamp = time.perf_counter_ns(); assert instance.execute('Forwards Solve')
    elapsed = (time.perf_counter_ns()-stamp)/1000
    valid = instance.get_variable_as_string('HandValid').lower() == 'true'
    assert valid == expected_valid,(case['label'],valid,expected_valid)
    errors = {}; rotations = {}
    for name,k in bone_keys.items():
        actual = transform(h.get_local_transform(k)); wanted = transform(expected[name])
        assert all(math.isfinite(x) for values in actual.values() for x in values)
        assert actual['translation'] == before[name]['translation'] and actual['scale'] == before[name]['scale'],name
        if name not in changed:assert actual == before[name],('other bone',name)
        else:
            errors[name] = angle(actual['rotation'],wanted['rotation'])
            rotations[name] = actual['rotation']
    values = {n:h.get_curve_value(k) for n,k in curve_keys.items()}
    error = max(abs(v-expected_curves[n]) for n,v in values.items())
    assert h.get_curve_value(sentinel) == .375
    return dict(label=case['label']+suffix,enabled=enabled,antipodes=antipodes,valid=valid,
                max_rotation_error_degrees=max(errors.values()),bone_errors=errors,
                max_curve_error=error,curves=values,output_rotations_xyzw=rotations,
                execute_us=elapsed,stage_execute_us=stage_times)

rows = []
for i,case in enumerate(cases):
    rows.append(evaluate(case))
    rows.append(evaluate(case,antipodes=True,suffix='-antipodes'))
    if i%25 == 0:
        (OUT/'partial.json').write_text(json.dumps(rows))
        unreal.log('CSS_COMBINED_HAND_PROGRESS '+str(i))
case = cases[0]
rows += [evaluate(case,False,suffix='-disabled'),evaluate(case,suffix='-reenabled'),evaluate(case,suffix='-repeat')]
fresh = bp.create_control_rig(); assert fresh.get_variable_as_string('Enabled').lower() == 'false'
rows += [evaluate(case,False,instance=fresh,suffix='-fresh-disabled'),evaluate(case,instance=fresh,suffix='-fresh-enabled')]
rows.append(evaluate(case,invalid_scale=True,suffix='-unsupported-scale'))
maximum = max(r['max_rotation_error_degrees'] for r in rows)
curve_error = max(r['max_curve_error'] for r in rows)
times = [r['execute_us'] for r in rows if r['enabled'] and r['valid']]
report = dict(scope=__doc__,package=PACKAGE,native_nodes=g.serial,cases=rows,
              fixture_path=str(fixture_path),fixture_sha256=digest(fixture_path),
              protected_hashes=hashes,reference_asset_hashes=reference_hashes,
              max_rotation_error_degrees=maximum,max_curve_error=curve_error,
              median_execute_us=statistics.median(times),p95_execute_us=sorted(times)[int(len(times)*.95)],
              timing_scope='Editor changing-pose execute with Python bridge; excludes setup; not live FPS.',
              passed=maximum<.001 and curve_error<1e-5)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(digest(Path(p)) == h for p,h in (hashes|reference_hashes).items())
assert report['passed'],(maximum,curve_error)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
unreal.log('CSS_COMBINED_HAND_PASSED')
