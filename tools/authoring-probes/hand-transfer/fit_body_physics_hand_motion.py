"""Extend the two hand boxes to include measured articulated skin.

Blender 5.2.2 Python. Keeps the entire previous box volume, its orientation,
all other bodies and all joints. This covers the recorded poses, not all weapons.
"""
import copy
import hashlib
import itertools
import json
import os
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_BODY_HAND_FIT_DIR']).resolve()
assert OUT.parent == WORK.resolve() and not OUT.exists()
OUT.mkdir()
sys.path[:0] = [str(Path(__file__).parent), str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools')]
from body_physics_geometry import rotator, sdf, vector, volume, xyz
from export_seduxtress_eve import read_bones

baseline_path = WORK/'b2-body-refined-fit-v1/candidate.json'
cloud_path = WORK/'b2-body-motion-audit-v1/hand-local-clouds.json'
query_path = WORK/'b2-body-refined-fit-v1/queries-verified.json'
bind_path = WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {str(p): digest(p) for p in (baseline_path, cloud_path, query_path, bind_path)}
measured = json.loads(cloud_path.read_text())
hashes.update(measured['protected_hashes'])
assert all(digest(Path(p)) == h for p, h in hashes.items())
baseline = json.loads(baseline_path.read_text())
candidate = copy.deepcopy(baseline)
queries = json.loads(query_path.read_text())
bones, bind = read_bones(bind_path)
by_name = {b['name'].lower(): i for i, b in enumerate(bones)}
reports = []
for body in candidate['bodies']:
    name = body['BoneName'].lower()
    if name not in measured['points']:
        continue
    assert len(body['AggGeom']['BoxElems']) == 1
    assert sum(len(v) for v in body['AggGeom'].values()) == 1
    shape = body['AggGeom']['BoxElems'][0]
    old = copy.deepcopy(shape)
    rotation = np.asarray(rotator(shape['Rotation']).to_matrix(), dtype=float)
    center = xyz(shape['Center'])
    half = np.asarray([shape[k]/2 for k in 'XYZ'])
    cloud = np.asarray(measured['points'][name], dtype=float)
    local = (cloud-center)@rotation
    # Retain the complete old box, so no earlier morph coverage is lost.
    low = np.minimum(-half, local.min(axis=0)-.35)
    high = np.maximum(half, local.max(axis=0)+.35)
    shape['Center'] = vector(center+rotation@((low+high)/2))
    shape.update({k: float(v) for k, v in zip('XYZ', high-low)})
    corners = np.asarray(list(itertools.product((-1., 1.), repeat=3)))*half
    corners = corners@rotation.T+center
    assert sdf(shape, 'BoxElems', corners).max() <= .0001
    assert sdf(shape, 'BoxElems', cloud).max() < 0
    old_gap = max(0., float(sdf(old, 'BoxElems', cloud).max()))
    assert old_gap > 1, (name, old_gap)
    reports.append(dict(bone=name, points=len(cloud), before_gap_cm=old_gap,
        after_gap_cm=max(0., float(sdf(shape, 'BoxElems', cloud).max())),
        old_volume_cm3=volume(old, 'BoxElems'), new_volume_cm3=volume(shape, 'BoxElems'),
        old=old, new=copy.deepcopy(shape)))
    world = np.asarray(bind[by_name[name]], dtype=float)
    selected = set(np.argmin(local, axis=0)) | set(np.argmax(local, axis=0))
    selected.add(int(np.argmax(sdf(old, 'BoxElems', cloud))))
    for index in sorted(selected):
        point = world[:3, :3]@cloud[index]+world[:3, 3]
        for far in (False, True):
            test = point+(np.full(3, 1000.) if far else 0)
            queries.append(dict(id=f'{name}/motion/{index}/'+('miss' if far else 'hit'),
                start=(test-np.array([100., 0, 0])).tolist(), end=(test+np.array([100., 0, 0])).tolist(),
                expected=not far, probe_point=test.tolist(), probe_bone=name))
assert len(reports) == 2
for old, new in zip(baseline['bodies'], candidate['bodies'], strict=True):
    if old['BoneName'].lower() not in measured['points']:
        assert old == new
    else:
        assert {k:v for k,v in old.items() if k != 'AggGeom'} == {k:v for k,v in new.items() if k != 'AggGeom'}
assert all(baseline[k] == candidate[k] for k in baseline if k != 'bodies')
candidate['hand_motion_fit'] = dict(samples=31, source_sha256=hashes[str(cloud_path)], all_weapons_verified=False)
for filename, value in [('candidate.json', candidate), ('queries.json', queries), ('report.json',
    dict(passed=True, protected_hashes=hashes, scope=__doc__, hands=reports, original_box_volume_preserved=True,
         game_verified=False, physics_queries_verified=False))]:
    (OUT/filename).write_text(json.dumps(value, indent=2)+'\n')
assert all(digest(Path(p)) == h for p, h in hashes.items())
print(json.dumps(reports, indent=2))
