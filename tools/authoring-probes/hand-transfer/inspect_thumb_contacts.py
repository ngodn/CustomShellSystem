"""Locate retained intersections by triangle skin weights and joint distance."""
import json
import os
import sys
from collections import Counter
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from skin_fixture import SkinFixture, AUDIT, WORK, load, pair_set

out = Path(os.environ['CSS_THUMB_CONTACT_AUDIT_DIR']).resolve()
assert out.parent == WORK.resolve()
out.mkdir(exist_ok=True)
assert not (out / 'report.json').exists()
assert load(WORK / 'arm-rest-batch-pose-v1/report.json')['passed']
skin = SkinFixture()
neutral = pair_set(skin.evaluate(load(AUDIT / 'left-corrected-controller-sweep-v1/control-0.json'), batch_pose=True))
rows = []
for case in load(WORK / 'arm-rest-thumb-stress-v1/report.json')['samples']:
    name = case['pose']
    doc = load(AUDIT / 'left-graph-anchor-thumb-stress-v1' / name)
    result = skin.evaluate(doc, batch_pose=True)
    contacts = []
    for pair in result['pairs']:
        if next(iter(pair_set({'pairs':[pair]}))) in neutral:
            continue
        triangles = []
        for triangle in pair['triangles']:
            weights = Counter()
            for vi in triangle:
                for g in skin.body.data.vertices[vi].groups:
                    name_bone = skin.body.vertex_groups[g.group].name
                    if name_bone in skin.index:
                        weights[name_bone] += g.weight / 3
            centroid = np.mean(skin.last_positions[triangle], axis=0)
            distances = {}
            for bone in ('hand_l','thumb_01_l','thumb_02_l','thumb_03_l'):
                point = skin.body.matrix_world.inverted() @ skin.rig.matrix_world @ skin.rig.pose.bones[bone].matrix.translation
                distances[bone] = float(np.linalg.norm(centroid - np.array(point))) * 100
            triangles.append(dict(vertices=triangle, weights=weights.most_common(4), distance_to_joint_cm=distances))
        contacts.append(dict(parts=pair['parts'], triangles=triangles))
    rows.append(dict(pose=name, contacts=contacts))
    dominant = Counter('/'.join(sorted(t['weights'][0][0] for t in p['triangles'])) for p in contacts)
    print(json.dumps(dict(pose=name, dominant_bones=dict(dominant))), flush=True)
skin.assert_unchanged()
(out / 'report.json').write_text(json.dumps(dict(scope=__doc__, samples=rows), indent=2)+'\n')
