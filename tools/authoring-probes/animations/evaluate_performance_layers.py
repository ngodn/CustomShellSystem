"""Compare explicit-time adapters with each original post-process graph in a preview world."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
name = os.environ['CSS_PERFORMANCE_REPORT']
if Path(name).name != name or not name.endswith('.json'):
    raise ValueError('Expected a report filename')
report = WORK / name
if report.exists():
    raise FileExistsError(report)
bindings = json.loads((WORK.parent / 'outfit-compatibility/report.json').read_text())['results']
layers = {r['source']: r['output'] for r in json.loads((WORK / 'performance-layers.json').read_text())['results']}
selection = os.environ.get('CSS_PERFORMANCE_MESH')
if selection:
    bindings = [entry for entry in bindings if entry['mesh'] == selection]
    if len(bindings) != 1:
        raise ValueError('Unknown mesh selection')
base = '/Game/CSS/Locomotion/Eve/idle/AN_Default'
catalog = json.loads((WORK / 'catalog.json').read_text())['animation_library']
identity = os.environ.get('CSS_PERFORMANCE_CLIP', 'css.eve.dance.blackhole')
all_moving = os.environ.get('CSS_PERFORMANCE_ALL_MOVING') == '1'
performances = [entry for entry in catalog['entries'] if
                (entry['playback'] == 'once' if all_moving else entry['id'] == identity)]
if not performances or (not all_moving and len(performances) != 1):
    raise ValueError('Unknown performance selection')
if any(len(entry['assets']) != 1 for entry in performances):
    raise ValueError('Offline evaluation requires one audited asset per clip')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


protected = {str(disk(path)): digest(disk(path)) for path in
             [base] + [entry['assets'][0]['asset'].split('.')[0] for entry in performances]}
helper = Path(__file__).with_name('CSSPerformanceEvaluation.cpp')
protected[str(helper)] = digest(helper)
module = CONTENT.parent / 'Binaries/Linux/libUnrealEditor-CSSAuthoring.so'
protected[str(module)] = digest(module)
results = []
report.write_text(json.dumps(dict(complete=False, results=[])) + '\n')
for performance_entry in performances:
    performance = performance_entry['assets'][0]['asset'].split('.')[0]
    for entry in bindings:
        source = entry['post_process']
        if source == '/Game/CSS/SeduXtress/ABP_Secondary':
            source = '/Game/CSS/AnimLab/ABP_Idle'
        layer = layers[source]
        for path in (entry['mesh'], source, layer):
            protected[str(disk(path))] = digest(disk(path))
        print('PERFORMANCE_EVALUATE_START', performance_entry['id'], entry['mesh'], flush=True)
        result = unreal.CSSPerformanceLibrary.evaluate_performance_layer(
            unreal.load_asset(entry['mesh']), unreal.load_asset(source), unreal.load_asset(layer),
            unreal.load_asset(base), unreal.load_asset(performance))
        if not result:
            raise ValueError('Empty evaluation result: ' + entry['mesh'])
        result = json.loads(result)
        if (len(result['cases']) != 9 or result.get('finger_bones_checked') != 38 or
                result.get('performance', '').split('.')[0] != performance):
            raise ValueError('Incomplete evaluation cases')
        result['id'] = performance_entry['id']
        results.append(result)
        report.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
        print('PERFORMANCE_EVALUATE_END', performance_entry['id'], entry['mesh'], flush=True)
        unreal.SystemLibrary.collect_garbage()
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Evaluation changed an authoring asset')
body_passed = all(case['body_passed'] for result in results for case in result['cases'])
fingers_passed = all(case['fingers_passed'] for result in results for case in result['cases'])
passed = body_passed and fingers_passed
report.write_text(json.dumps(dict(complete=True, passed=passed, body_passed=body_passed,
                                 fingers_passed=fingers_passed, results=results,
                                 clips=[entry['id'] for entry in performances],
                                 protected=protected), indent=2) + '\n')
if not passed:
    raise ValueError('Evaluated body or finger pose differs from original graph; inspect report')
