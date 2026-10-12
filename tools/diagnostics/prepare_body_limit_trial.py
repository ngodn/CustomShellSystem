"""Restore V42 movement limits in an isolated copy of Eve's current body rig."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal


stage = Path(os.environ['CSS_LIMIT_STAGE']).resolve(strict=True)
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
if content != stage/'editor/Content':
    raise ValueError('Expected private candidate content')
package = '/Game/CSS/eins0fx/EveSB/CR_Body'
path = content/(package.removeprefix('/Game/')+'.uasset')
if path.is_symlink() or stage not in path.resolve().parents:
    raise ValueError('Candidate rig must be a private file')
bp = unreal.load_asset(package)
if not isinstance(bp, unreal.ControlRigBlueprint):
    raise ValueError('Missing Eve body rig')
names = [str(var.name) for var in bp.get_member_variables()]
wanted = {'Limits': [math.radians(v) for v in [6, 6, 10, 10, 3, 3, 5]],
          'MaxTravel': [1.5, 1.5, 1, 1, .5, .5, .5]}


def snapshot():
    rig = bp.create_control_rig()
    values = {name: rig.get_variable_as_string(name) for name in names}
    graphs = []
    for graph in bp.get_all_models():
        text = bp.get_controller(graph).export_nodes_to_text([node.get_name() for node in graph.get_nodes()])
        graphs.append(hashlib.sha256(text.encode()).hexdigest())
    return dict(values=values, graph_sha256=graphs)


def check_bounds(values):
    for name, expected in wanted.items():
        actual = [float(v) for v in values[name].strip('()').split(',')]
        if len(actual) != 7 or max(abs(a-b) for a, b in zip(actual, expected)) > 1e-6:
            raise ValueError('Candidate limits differ: '+name)


if os.environ.get('CSS_LIMIT_VERIFY') == '1':
    expected = json.loads((stage/'authored.json').read_text())
    after = snapshot()
    (stage/'reload-observation.json').write_text(json.dumps(after, indent=2)+'\n')
    if after != expected['after']:
        raise ValueError('Saved rig changed on reload')
    check_bounds(after['values'])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    bp.recompile_vm()
    if snapshot() != after:
        raise ValueError('Saved defaults changed after compilation')
    (stage/'reloaded.json').write_text(json.dumps(dict(passed=True, after=after), indent=2)+'\n')
else:
    if (stage/'authored.json').exists():
        raise FileExistsError('Candidate already authored')
    before = snapshot()
    if [float(v) for v in before['values']['MaxTravel'].strip('()').split(',')] != [11, 11, 5, 5, 2, 2, 1.5]:
        raise ValueError('Source is not the recorded wide-limit rig')
    if not unreal.CSSBodyInputsLibrary.set_body_movement_limits(bp, wanted['Limits'], wanted['MaxTravel']):
        raise ValueError('Could not update authored body limits')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    bp.recompile_vm()
    after = snapshot()
    check_bounds(after['values'])
    if before['graph_sha256'] != after['graph_sha256']:
        raise ValueError('Rig graph changed')
    changes = {name for name in names if before['values'][name] != after['values'][name]}
    if changes != set(wanted):
        raise ValueError(f'Unexpected default changes: {changes}')
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp):
        raise RuntimeError('Could not save candidate rig')
    (stage/'authored.json').write_text(json.dumps(dict(package=package, before=before, after=after,
        sha256=hashlib.sha256(path.read_bytes()).hexdigest()), indent=2)+'\n')
