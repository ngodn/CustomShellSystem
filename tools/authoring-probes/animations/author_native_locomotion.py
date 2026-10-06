"""Retarget only the four requested Eve locomotion choices to the native rig."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / 'CustomShellSystem'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_NATIVE_LOCOMOTION_WORK']).resolve()
assert WORK.is_relative_to(REPO / 'work/cine-animation-port')
assert not (WORK / 'result.json').exists()
PACKAGE = '/Game/CSS/Locomotion/NativeHuman/Studies/NL2'
NATIVE = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton'
SOURCE = '/Game/CSS/SeduXtress/SK_BlackPearl2'
TARGET = '/Game/CSSAuthoring/DiagnosticReferences/SK_GameHuman1199_V1'
SETTINGS = '/Game/CSS/Locomotion/Studies/L5X/ACS_Motion'


def write(name, value):
    (WORK / name).write_text(json.dumps(value, indent=2) + '\n')


def disk(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


spec_path = REPO / 'work/anim14/spec.json'
spec = json.loads(spec_path.read_text())
assert [d['slot'] for d in spec['definitions']] == ['Walk', 'Jog', 'Sprint']
labels = sorted({s['clip'] for d in spec['definitions'] for s in d['samples']})
assert len(labels) == 19
sources = {label: '/Game/CSS/Eve/Anim/AN_S1_' + label for label in labels}
sources['Idle'] = '/Game/CSS/Eve/Anim/AN_I2_Idle'
protected = {str(disk(p)): sha(disk(p)) for p in [SOURCE, TARGET, NATIVE, SETTINGS,
    '/Game/CSS/Shared/SKEL_Base', '/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHuman1199_V1',
    *sources.values()]}
protected[str(spec_path)] = sha(spec_path)
write('protected.json', protected)
for label in sources:
    assert not disk(PACKAGE + '/AN_' + label).exists()
source, target, native, settings = [unreal.load_asset(p) for p in (SOURCE, TARGET, NATIVE, SETTINGS)]
assert source and target and native and settings
assert unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(target) == 9
assert unreal.CSSRetargetLibrary.inspect_skeleton(target.get_editor_property('skeleton')) == \
       unreal.CSSRetargetLibrary.inspect_skeleton(native)
assert unreal.CSSRetargetLibrary.inspect_virtual_bones(native) == \
       unreal.CSSRetargetLibrary.inspect_virtual_bones(target.get_editor_property('skeleton'))
tools = unreal.AssetToolsHelpers.get_asset_tools()
chains = json.loads((REPO / 'work/cine-animation-port/retarget-l2/chains.json').read_text())


def create(name, kind, factory):
    assert not unreal.EditorAssetLibrary.does_asset_exist('/Game/CSS/AnimLab/' + name)
    asset = tools.create_asset(name, '/Game/CSS/AnimLab', kind, factory)
    assert asset
    return asset


rigs = []
for name, mesh in [('Source', source), ('Target', target)]:
    rig = create('IK_NL2_' + name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    assert ctrl.set_skeletal_mesh(mesh)
    assert ctrl.set_retarget_root('pelvis')
    for chain, _, _, first, last in chains:
        assert str(ctrl.add_retarget_chain(chain, first, last, 'None')) == chain
    rigs.append(rig)
retarget = create('RT_NL2', unreal.IKRetargeter, unreal.IKRetargetFactory())
ctrl = unreal.IKRetargeterController.get_controller(retarget)
mode = unreal.RetargetSourceOrTarget
ctrl.set_ik_rig(mode.SOURCE, rigs[0])
ctrl.set_ik_rig(mode.TARGET, rigs[1])
ctrl.set_preview_mesh(mode.SOURCE, source)
ctrl.set_preview_mesh(mode.TARGET, target)
ctrl.add_default_ops()
for chain, *_ in chains:
    assert ctrl.set_source_chain(chain, chain)
ctrl.auto_align_all_bones(mode.TARGET)
clips, results = {}, []
for label, path in sources.items():
    original = unreal.load_asset(path)
    assert original and original.get_editor_property('skeleton') == source.get_editor_property('skeleton')
    raw = unreal.CSSAnimationLibrary.retarget_clip(source, target, original, retarget,
        '/Game/CSS/AnimLab/AN_NL2_' + label, True, True)
    assert raw, label
    candidate = unreal.CSSAnimationLibrary.bind_native_human_clip(raw, native, PACKAGE + '/AN_' + label)
    assert candidate, label
    candidate.set_editor_property('bone_compression_settings', settings)
    assert math.isclose(candidate.get_play_length(), original.get_play_length(), abs_tol=1e-6)
    # BindNativeHumanClip compares every track/key before returning across the
    # diagnostic-to-game skeleton change. CompareSequenceData requires one skeleton.
    assert not candidate.get_editor_property('retarget_source_asset')
    assert candidate.get_editor_property('skeleton') == native
    assert unreal.EditorAssetLibrary.save_loaded_asset(candidate, False)
    clips[label] = candidate
    results.append(dict(label=label, source=path, package=PACKAGE + '/AN_' + label,
        seconds=candidate.get_play_length(), exact_native_binding=True,
        sha256=sha(disk(PACKAGE + '/AN_' + label))))
    write('progress.json', dict(completed=len(results), total=len(sources), clips=results))
    print('NATIVE_LOCOMOTION', label, len(results), len(sources), flush=True)

assert all(sha(Path(path)) == digest for path, digest in protected.items())
report = dict(authored=True, clips=results, choices=['Idle', 'Walk', 'Jog', 'Sprint'],
    protected=protected, spec=spec, package=PACKAGE,
    scope='Twenty retargeted sequence candidates for four choices. BlendSpaces, full pose/contact checks, cook and runtime acceptance pending.')
write('result.json', report)
(WORK / 'sequence-cook.txt').write_text('\n'.join([row['package'] for row in results] + [SETTINGS]) + '\n')
