"""Isolate thumb twist, distal hyperextension and corrective effects on B2.

These counterfactual poses are diagnostics, not a production rotation limit.
"""
import copy
import json
import math
import os
import sys
import time
from pathlib import Path

from mathutils import Quaternion

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, load, pair_set

OUT = Path(os.environ['CSS_THUMB_ABLATION_AUDIT_DIR']).resolve()
assert OUT.parent == WORK.resolve()
OUT.mkdir(exist_ok=True)
assert not (OUT / 'report.json').exists()
skin = SkinFixture()
stage = os.environ.get('CSS_THUMB_ABLATION_STAGE', 'initial')
assert stage in ('initial', 'base-axis')
batch = stage == 'base-axis'
if batch:
    assert load(WORK / 'arm-rest-batch-pose-v1/report.json')['passed']
neutral_pairs = pair_set(skin.evaluate(load(AUDIT / 'left-corrected-controller-sweep-v1/control-0.json')))
assert len(neutral_pairs) == 4
cases = [r for r in load(WORK / 'arm-rest-thumb-stress-v1/report.json')['samples'] if r['new_pairs']]
models = {m['bone']: m for m in skin.curves if m['bone'].startswith('thumb')}
rows = []
started = time.monotonic()
for case in cases:
    name = case['pose']
    original = load(AUDIT / 'left-graph-anchor-thumb-stress-v1' / name)
    variants = ('base_twist_zero', 'base_flex_half', 'base_up_zero', 'base_twist_minus10') if batch else (
        'base_twist_half', 'distal_extension_zero', 'both', 'base_correctives_off')
    for variant in variants:
        doc = copy.deepcopy(original)
        changes = {}
        for bone, m in models.items():
            transform = doc['pose']['Snapshot']['LocalTransforms'][m['index']]
            incoming = Quaternion([transform['Rotation'][k] for k in 'WXYZ']).normalized()
            source = (Quaternion(m['left_wxyz']) @ incoming @ Quaternion(m['right_wxyz'])).normalized()
            angles = source.to_euler(m['euler_order'])
            changed = False
            if bone == 'thumb_01_l' and variant in ('base_twist_half', 'both'):
                angles.y *= .5
                changed = True
            if bone == 'thumb_01_l' and variant in variants and batch:
                if variant == 'base_twist_zero': angles.y = 0
                if variant == 'base_flex_half': angles.x *= .5
                if variant == 'base_up_zero': angles.z = 0
                if variant == 'base_twist_minus10': angles.y = max(angles.y, math.radians(-10))
                changed = True
            if bone != 'thumb_01_l' and variant in ('distal_extension_zero', 'both') and angles.x < 0:
                angles.x = 0
                changed = True
            if changed:
                q = (Quaternion(m['left_wxyz']).inverted() @ angles.to_quaternion() @ Quaternion(m['right_wxyz']).inverted()).normalized()
                transform['Rotation'] = dict(zip('XYZW', (q.x, q.y, q.z, q.w)))
                dot = max(-1.0, min(1.0, abs(q.dot(incoming))))
                changes[bone] = math.degrees(2 * math.acos(dot))
        values = skin.shapes(doc)
        if variant == 'base_correctives_off':
            values['pJCMThumb1Bend_50_L'] = 0
            values['pJCMThumb1Up_20_L'] = 0
        before = original['pose']['Snapshot']['LocalTransforms']
        after = doc['pose']['Snapshot']['LocalTransforms']
        changed_indices = {models[bone]['index'] for bone in changes}
        assert all(a == b for i, (a, b) in enumerate(zip(before, after, strict=True)) if i not in changed_indices)
        assert all(a[k] == b[k] for a, b in zip(before, after, strict=True) for k in ('Translation', 'Scale3D'))
        result = skin.evaluate(doc, values, batch_pose=batch)
        extra = pair_set(result) - neutral_pairs
        row = dict(pose=name, variant=variant, new_pairs=len(extra), regions=result['regions'], rotation_changes_degrees=changes)
        prefix = variant + '-' + name
        (OUT / prefix).write_text(json.dumps(doc, indent=2) + '\n')
        (OUT / ('shapes-' + prefix)).write_text(json.dumps(dict(values=values), indent=2) + '\n')
        (OUT / ('contacts-' + prefix)).write_text(json.dumps(result, indent=2) + '\n')
        rows.append(row)
        print(json.dumps(row), flush=True)
skin.assert_unchanged()
(OUT / 'report.json').write_text(json.dumps(dict(scope=__doc__, samples=rows,
    blend_sha256=skin.source_hash, seconds=time.monotonic()-started), indent=2) + '\n')
