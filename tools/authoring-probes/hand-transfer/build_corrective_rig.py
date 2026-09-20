"""Build and execute private finger curves using shipped UE5.6.1 RigVM units.

The graph consumes calibrated finger locals. It neither corrects the pose nor
binds a production mesh, and starts disabled. No game files are changed.
"""
import hashlib
import json
import math
import os
import sys
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_CORRECTIVE_RIG_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'report.json').exists()
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
AUDIT = MOD / 'work/nextgen-audit'
sys.path.insert(0, str(MOD / 'tools'))
from build_controlrig_chain_probe import Graph, Pin
from probe_controlrig_chain import key, set_value, transform

PACKAGE = '/Game/CSSAuthoring/TransientProbes/CR_CSS_LeftFingerCorrectivesV1'
assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE)
models = json.loads(Path(__file__).with_name('left-finger-correctives-v1.json').read_text())['parameters']
assert len(models) == 16 and all(m['euler_order'] == 'YZX' for m in models)
content = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content/CSSAuthoring'
protected = [content / 'Shared/Skeletons/SKEL_CSS_Base.uasset', content / 'CSS_SeduXtress_eins0fx_P/SK_SeduXtress_HandBindV43.uasset']
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
mesh = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B_V1')
assert mesh
blueprint = unreal.ControlRigBlueprintFactory.create_new_control_rig_asset(PACKAGE)
blueprint.set_auto_vm_recompile(False)
controller = blueprint.get_hierarchy_controller()
imported = controller.import_bones_from_skeletal_mesh(mesh, 'None')
assert len(imported) == 379
blueprint.set_preview_mesh(mesh, False)
graph = Graph(blueprint)
graph.member('Enabled', 'bool', 'False', True)
execution = Pin(graph.unit('RigUnit_BeginExecution') + '.ExecuteContext')

def quaternion(wxyz):
    w, x, y, z = wxyz
    return f'(X={x:.12g},Y={y:.12g},Z={z:.12g},W={w:.12g})'

for model in models:
    controller.add_curve(model['shape'], 0.0, False, False)
    incoming = graph.unit('RigUnit_GetTransform', Space='LocalSpace', bInitial=False)
    graph.value(incoming + '.Item.Type', 'Bone')
    graph.value(incoming + '.Item.Name', model['bone'])
    q = graph.math('QuaternionMul', A=quaternion(model['left_wxyz']), B=Pin(incoming + '.Transform.Rotation'))
    q = graph.math('QuaternionUnit', Value=graph.math('QuaternionMul', A=q, B=quaternion(model['right_wxyz'])))
    euler = graph.unit('RigVMFunction_MathQuaternionToEuler', Value=q, RotationOrder='YZX')
    radians = graph.math('FloatMul', A=Pin(euler + '.Result.' + model['axis']), B=math.pi / 180)
    desired = graph.math('FloatClamp', Value=graph.math('FloatMul', A=radians, B=model['coefficient']), Minimum=0, Maximum=1)
    weight = graph.math('FloatSub', A=desired, B=model['baked_value'])
    weight = graph.math('FloatSelectBool', Condition=graph.get('Enabled'), IfTrue=weight, IfFalse=0)
    node = graph.unit('RigUnit_SetCurveValue', Curve=model['shape'], Value=weight)
    graph.link(execution, node + '.ExecuteContext')
    execution = Pin(node + '.ExecuteContext')

blueprint.recompile_vm()
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
rig = blueprint.create_control_rig()
hierarchy = rig.get_hierarchy()
curve_keys = {m['shape']: unreal.RigElementKey(name=m['shape'], type=unreal.RigElementType.CURVE) for m in models}
bone_names = [str(k.name) for k in imported]
assert len(bone_names) == 379

def evaluate(label, doc, expected, enabled, target_rig=rig):
    h = target_rig.get_hierarchy()
    snapshot = doc['pose']['Snapshot']
    names = {n.lower(): i for i, n in enumerate(snapshot['BoneNames'])}
    h.reset_pose_to_initial(unreal.RigElementType.BONE)
    for name in bone_names:
        value = h.get_local_transform(key(name))
        pose = snapshot['LocalTransforms'][names[name.lower()]]
        value.rotation = unreal.Quat(*[pose['Rotation'][k] for k in 'XYZW'])
        h.set_local_transform(key(name), value, False, True)
    before = {name: transform(h.get_local_transform(key(name))) for name in bone_names}
    set_value(target_rig, 'Enabled', enabled)
    assert target_rig.execute('Forwards Solve'), label
    assert all(transform(h.get_local_transform(key(name))) == t for name, t in before.items()), 'Curve graph changed bones'
    values = {m['shape']: h.get_curve_value(curve_keys[m['shape']]) for m in models}
    errors = {m['shape']: abs(values[m['shape']] - (expected[m['shape']] - m['baked_value'] if enabled else 0)) for m in models}
    assert all(math.isfinite(v) for v in values.values())
    return dict(label=label, enabled=enabled, values=values, max_weight_error=max(errors.values()), errors=errors)

def load(path):
    return json.loads(path.read_text())

fixtures = []
control = AUDIT / 'left-corrected-controller-sweep-v1'
for degrees in range(0, 61, 5):
    fixtures.append((f'control-{degrees}', load(control / f'control-{degrees}.json'), load(control / f'shapes-{degrees}.json')['values']))
for case in load(AUDIT / 'left-observed-thumb-clearance-v1/manifest.json')['cases']:
    name = case['pose']
    fixtures.append((name, load(AUDIT / 'left-observed-thumb-clearance-v1' / name), load(AUDIT / 'left-observed-thumb-clearance-v1-contacts' / ('shapes-' + name))['values']))
results = [evaluate(name, doc, values, True) for name, doc, values in fixtures]
name, doc, expected = fixtures[-1]
results += [evaluate('disable-clears-private-curves', doc, expected, False), evaluate('reenable-current-pose', doc, expected, True), evaluate('repeat-current-pose', doc, expected, True)]
fresh = blueprint.create_control_rig()
assert fresh.get_variable_as_string('Enabled').lower() == 'false'
results.append(evaluate('new-instance-default-disabled', doc, expected, False, fresh))
results.append(evaluate('new-instance-enabled', doc, expected, True, fresh))
maximum = max(r['max_weight_error'] for r in results)
report = dict(scope=__doc__, package=PACKAGE, native_nodes=graph.serial, cases=results,
              max_weight_error=maximum, tolerance=1e-5, passed=maximum < 1e-5,
              bones_preserved=379, protected_hashes=hashes)
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
assert maximum < 1e-5, f'Native corrective mismatch: {maximum}'
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
unreal.log('CSS_LEFT_FINGER_NATIVE_CURVES_PASSED ' + str(maximum))
