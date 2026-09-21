"""Resolve five observed import weight ties toward the accepted V44 bytes."""
import copy
import hashlib
import json
from pathlib import Path
import struct

CSS = Path(__file__).resolve().parents[2]
before = CSS / 'work/paths/rig1'
out = CSS / 'work/paths/rig2'
assert out.is_dir() and not (out / 'source.mesh.json').exists()
load = lambda p: json.loads(p.read_text())
source_file = CSS / 'work/grip-grounding-v1/heel-support-export-v2/candidate.mesh.json'
original = load(source_file)
source = copy.deepcopy(original)
source['mesh_package'] = '/Game/CSS/SeduXtress/SK_BlackPearl2'
source['skeleton_package'] = '/Game/CSS/SeduXtress/Reference/SKEL_Import2'
points = {}
for vertex, (x, y, z) in enumerate(source['points']):
    point = struct.unpack('<3f', struct.pack('<3f', x, -y, z))
    points.setdefault(point, []).append(vertex)
influences = {(row[0], row[1]): row for row in source['influences']}
changes = []
differences = load(before / 'skin-differences.json')
assert len(differences) == 5
for difference in differences:
    assert len(difference['old']) == len(difference['new']) == 1
    a, b = dict(difference['old'][0]), dict(difference['new'][0])
    changed = [bone for bone in a if a[bone] != b[bone]]
    assert len(changed) == 2 and all(258 <= bone <= 264 for bone in changed)
    high, low = sorted(changed, key=lambda bone: -a[bone])
    assert a[high] == b[low] and a[low] == b[high]
    for vertex in points[tuple(difference['point'])]:
        upper, lower = influences[vertex, high], influences[vertex, low]
        assert abs(upper[2] - lower[2]) <= 1 / 65535
        total = upper[2] + lower[2]
        previous = [upper[2], lower[2]]
        # One 16-bit unit on either side of the mean makes the first
        # influence explicit without crossing an 8-bit truncation boundary.
        upper[2] = total / 2 + 1 / 65535
        lower[2] = total - upper[2]
        assert int(previous[0] * 65535 + .5) >> 8 == int(upper[2] * 65535 + .5) >> 8
        assert int(previous[1] * 65535 + .5) >> 8 == int(lower[2] * 65535 + .5) >> 8
        changes.append(dict(vertex=vertex, bones=[high, low], before=previous, after=[upper[2], lower[2]]))
assert len(changes) == 5
for key in original:
    if key not in ('mesh_package', 'skeleton_package', 'influences'):
        assert source[key] == original[key], key
changed_indices = {row['vertex'] for row in changes}
assert [row for row in source['influences'] if row[0] not in changed_indices] == [row for row in original['influences'] if row[0] not in changed_indices]
(out / 'source.mesh.json').write_text(json.dumps(source, separators=(',', ':')) + '\n')
(out / 'ties.json').write_text(json.dumps(dict(source=str(source_file),
    source_sha256=hashlib.sha256(source_file.read_bytes()).hexdigest(), changes=changes,
    scope='Import-only tie ordering. Original Blender/export untouched; require exact cooked V44 skin weights before acceptance.'), indent=2) + '\n')
print('Prepared five import tie corrections')
