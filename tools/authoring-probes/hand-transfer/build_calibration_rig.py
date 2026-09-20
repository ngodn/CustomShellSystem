"""Build native finger calibration followed by original corrective curves.

The contact-clearance stage is still absent. This is a diagnostic graph, not
a production mesh binding or a live-game fix. Each evaluation needs fresh input.
"""
import copy
import hashlib
import json
import math
import os
import sys
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_NATIVE_CALIBRATION_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'report.json').exists()
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(MOD / 'tools')]
from build_controlrig_chain_probe import Graph, Pin
from probe_controlrig_chain import key, set_value, transform
from native_nodes import append_calibration, append_correctives, clear_correctives

load = lambda path: json.loads(path.read_text())
models = load(HERE / 'left-finger-calibration-v1.json')['parameters']
curves = load(HERE / 'left-finger-correctives-v1.json')['parameters']
fixture_path = WORK / 'hand-native-calibration-fixtures-v2/fixtures.json'
cases = load(fixture_path)['cases']
assert len(models) == 19 and all(m['bone'].endswith('_l') for m in models)
assert len(curves) == 16
PACKAGE = '/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftHandCalibrationV1'
assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE)
content = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
protected = [content / 'Shared/Skeletons/SKEL_CSS_Base.uasset', content / 'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
mesh = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B_V1')
assert mesh
bp = unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(PACKAGE)
bp.set_auto_vm_recompile(False)
controller = bp.get_hierarchy_controller()
imported = controller.import_bones_from_skeletal_mesh(mesh, 'None')
assert len(imported) == 379
names = [str(k.name) for k in imported]
bp.set_preview_mesh(mesh, False)
for m in curves:
    controller.add_curve(m['shape'], 0, False, False)
sentinel = controller.add_curve('CSS_Diagnostic_UnrelatedCurve', .375, False, False)
g = Graph(bp)
g.member('Enabled', 'bool', 'False', True)
begin = Pin(g.unit('RigUnit_BeginExecution') + '.ExecuteContext')
active, bypass = append_calibration(g, begin, models)
append_correctives(g, controller, active, curves, True)
clear_correctives(g, bypass, curves)
bp.recompile_vm()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
rig = bp.create_control_rig()
assert rig.get_variable_as_string('Enabled').lower() == 'false'
changed = {m['bone'] for m in models}
curve_keys = {m['shape']: unreal.RigElementKey(name=m['shape'], type=unreal.RigElementType.CURVE) for m in curves}

def xyzw(wxyz):
    return wxyz[1:] + wxyz[:1]

def angular_error(a, b):
    an, bn = math.sqrt(sum(x*x for x in a)), math.sqrt(sum(x*x for x in b))
    dot = abs(sum(x*y for x, y in zip(a, b))) / (an * bn)
    return math.degrees(2 * math.acos(min(1, dot)))

def evaluate(case, enabled=True, antipodes=False, instance=rig, suffix=''):
    h = instance.get_hierarchy()
    doc = load(Path(case['source']))['pose']['Snapshot']
    lookup = {n.lower(): i for i, n in enumerate(doc['BoneNames'])}
    h.reset_pose_to_initial(unreal.RigElementType.BONE)
    for name in names:
        t = h.get_local_transform(key(name))
        values = doc['LocalTransforms'][lookup[name.lower()]]['Rotation']
        rotation = [values[k] for k in 'XYZW']
        if name.lower() in changed:
            rotation = xyzw(case['inputs_wxyz'][name.lower()])
            if antipodes:
                rotation = [-v for v in rotation]
        t.rotation = unreal.Quat(*rotation)
        h.set_local_transform(key(name), t, False, True)
    before = {name: transform(h.get_local_transform(key(name))) for name in names}
    # Seed an actual incoming curve on this instance. The authoring hierarchy's
    # curve value is not an instance default copied by create_control_rig().
    h.set_curve_value(sentinel, .375)
    assert h.get_curve_value(sentinel) == .375
    set_value(instance, 'Enabled', enabled)
    set_value(instance, 'InputIsV43Compatible', case['v43_compatible'])
    assert instance.execute('Forwards Solve'), case['label']
    valid = instance.get_variable_as_string('ValidInput').lower() == 'true'
    assert valid != case['singular'], (case['label'], valid)
    bone_errors = {}
    rotations = {}
    for name in names:
        actual = transform(h.get_local_transform(key(name)))
        if name.lower() in changed:
            rotations[name.lower()] = actual['rotation']
        assert all(math.isfinite(v) for field in actual.values() for v in field)
        assert actual['translation'] == before[name]['translation'], ('translation changed', name)
        assert actual['scale'] == before[name]['scale'], ('scale changed', name)
        if name.lower() not in changed or not enabled or not valid:
            assert actual == before[name], ('passthrough changed', name)
        else:
            bone_errors[name] = angular_error(actual['rotation'], xyzw(case['expected_wxyz'][name.lower()]))
    actual_curves = {m['shape']: h.get_curve_value(curve_keys[m['shape']]) for m in curves}
    curve_error = max(abs(v - (case['expected_curves'][n] if enabled and valid else 0)) for n, v in actual_curves.items())
    assert h.get_curve_value(sentinel) == .375, ('Unrelated curve changed', h.get_curve_value(sentinel))
    if case['label'] == 'h2-1' and enabled and not antipodes and not suffix:
        # Apply measured native local outputs to the original complete H2
        # fixture for independent skin replay. All other fixture data stays.
        exported = copy.deepcopy(load(Path(case['source'])))
        for name, rotation in rotations.items():
            exported['pose']['Snapshot']['LocalTransforms'][lookup[name]]['Rotation'] = dict(zip('XYZW', rotation))
        exported['scope'] = 'Original H2 fixture with the native graph left-finger outputs inserted. Independent skin replay input, not a live capture.'
        (OUT / 'h2-native.json').write_text(json.dumps(exported, indent=2) + '\n')
        values = {m['shape']: max(0, min(1, actual_curves[m['shape']] + m['baked_value'])) for m in curves}
        (OUT / 'h2-native-shapes.json').write_text(json.dumps(dict(values=values), indent=2) + '\n')
    return dict(label=case['label'] + suffix, enabled=enabled, antipodes=antipodes, valid=valid,
                max_rotation_error_degrees=max(bone_errors.values(), default=0),
                max_curve_error=curve_error, bone_errors=bone_errors, curves=actual_curves,
                output_rotations_xyzw=rotations)

results = []
for case in cases:
    results.append(evaluate(case))
    results.append(evaluate(case, antipodes=True, suffix='-antipodes'))
case = cases[0]
results += [evaluate(case, False, suffix='-disabled'), evaluate(case, True, suffix='-reenabled'),
            evaluate(case, True, suffix='-repeated-fresh-input')]
fresh = bp.create_control_rig()
assert fresh.get_variable_as_string('Enabled').lower() == 'false'
results += [evaluate(case, False, instance=fresh, suffix='-fresh-disabled'),
            evaluate(case, True, instance=fresh, suffix='-fresh-enabled')]
max_rotation = max(r['max_rotation_error_degrees'] for r in results)
max_curve = max(r['max_curve_error'] for r in results)
report = dict(scope=__doc__, package=PACKAGE, native_nodes=g.serial, cases=results,
              fixture_sha256=hashlib.sha256(fixture_path.read_bytes()).hexdigest(),
              max_rotation_error_degrees=max_rotation, rotation_tolerance_degrees=.001,
              max_curve_error=max_curve, curve_tolerance=1e-5,
              passed=max_rotation < .001 and max_curve < 1e-5,
              protected_hashes=hashes, unrelated_bones_and_curve_preserved=True)
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
assert report['passed'], (max_rotation, max_curve)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log('CSS_NATIVE_HAND_CALIBRATION_PASSED ' + str((max_rotation, max_curve)))
