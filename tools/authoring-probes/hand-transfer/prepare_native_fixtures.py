"""Prepare independent saved-pose expectations for the native hand graph."""
import copy
import json
import math
import os
import sys
from pathlib import Path

from mathutils import Quaternion, Vector

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_NATIVE_HAND_FIXTURE_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'fixtures.json').exists()
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
AUDIT = MOD / 'work/nextgen-audit'
HERE = Path(__file__).resolve().parent
load = lambda path: json.loads(path.read_text())
models = load(HERE / 'left-finger-calibration-v1.json')['parameters']
curves = load(HERE / 'left-finger-correctives-v1.json')['parameters']

def pose(doc):
    return doc['pose']['Snapshot']

def q(doc, index):
    return Quaternion([pose(doc)['LocalTransforms'][index]['Rotation'][a] for a in 'WXYZ']).normalized()

def expected_curves(doc):
    result = {}
    for m in curves:
        value = (Quaternion(m['left_wxyz']) @ q(doc, m['index']) @ Quaternion(m['right_wxyz'])).normalized()
        angle = value.to_euler(m['euler_order'])['XYZ'.index(m['axis'])]
        result[m['shape']] = max(0, min(1, m['coefficient'] * angle)) - m['baked_value']
    return result

cases = []
def add(label, source, expected, compatible, input_override=None, singular=False, palm_reference=None):
    incoming, target = load(source), load(expected)
    if palm_reference:
        # The four palm joints have no closed anchor: their source animation
        # is deliberately retained. Use the independently saved graph result.
        palm = load(palm_reference)
        for m in models:
            if m['axis'] is None:
                pose(target)['LocalTransforms'][m['index']] = copy.deepcopy(pose(palm)['LocalTransforms'][m['index']])
    inputs = input_override or {m['bone']: list(q(incoming, m['index'])) for m in models}
    outputs = inputs if singular else {m['bone']: list(q(target, m['index'])) for m in models}
    values = {m['shape']: 0 for m in curves} if singular else expected_curves(target)
    cases.append(dict(label=label, source=str(source), expected_source=str(expected),
                      v43_compatible=compatible, singular=singular, inputs_wxyz=inputs,
                      expected_wxyz=outputs, expected_curves=values,
                      palm_reference=str(palm_reference) if palm_reference else None))

for sample in range(5):
    add(f'h2-{sample}', WORK / f'arm-rest-alignment-v1/candidate_game_rotations-s{sample}-compressed-ik1.json',
        WORK / f'arm-rest-hand-transfer-v1/all-h2-samples-v2/h2-s{sample}-calibrated.json', False)
for case in load(AUDIT / 'left-observed-graph-calibrated-v1/manifest.json')['cases']:
    add(case['pose'], Path(case['source']), AUDIT / 'left-observed-graph-calibrated-v1' / case['pose'], True)

# Both anchors come from independently evaluated original controls and the
# extracted source reference/idle plus its measured additive overlay.
template = WORK / 'arm-rest-alignment-v1/candidate_game_rotations-s1-compressed-ik1.json'
source = load(WORK / 'active-idle-exact-v1/animation-poses.json')
names = {n.lower(): i for i, n in enumerate(source['frames'][0]['BoneNames'])}
overlay = load(WORK / 'genessa-hand-overlay-r2/additive-deltas.json')['rows'][0]['deltas']
overlay = {n.lower(): t for n, t in overlay.items()}
raw_closed = {}
for m in models:
    a = Quaternion([overlay[m['bone']]['Rotation'][k] for k in 'WXYZ'])
    b = Quaternion([source['frames'][0]['LocalTransforms'][names[m['bone']]]['Rotation'][k] for k in 'WXYZ'])
    raw_closed[m['bone']] = list((a @ b).normalized())
controls = AUDIT / 'left-corrected-controller-sweep-v1'
add('source-reference-anchor', template, controls / 'control-0.json', False,
    {m['bone']: m['source_ref'] for m in models})
add('source-closed-anchor', template, controls / 'control-60.json', False, raw_closed,
    palm_reference=AUDIT / 'left-calibrated-live-graph-anchor-v1/captured_with_overlay.json')

# A 180-degree swing perpendicular to the twist axis has no unique twist.
inputs = {m['bone']: m['source_ref'] for m in models}
m = next(m for m in models if m['bone'] == 'thumb_01_l')
axis = Vector(m['axis'])
perpendicular = axis.cross(Vector((1, 0, 0))).normalized()
assert perpendicular.length > .99
inputs[m['bone']] = list((Quaternion(m['source_ref']) @ Quaternion(perpendicular, math.pi)).normalized())
add('singular-thumb-passthrough', template, controls / 'control-0.json', False, inputs, True)

(OUT / 'fixtures.json').write_text(json.dumps(dict(scope=__doc__, cases=cases), indent=2) + '\n')
print(json.dumps(dict(cases=len(cases), ordinary=40, anchors=2, singular=1)), flush=True)
