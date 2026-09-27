"""Compare native foot rotations with the offline skeletal correction."""
import json
import math
from pathlib import Path
from mathutils import Matrix, Quaternion, Vector

work = Path(__file__).resolve().parents[2] / 'work/eve26'
mesh = json.loads((work / 'bikini-anklew1/bikini.mesh.json').read_text())
receipt = json.loads((work / 'bikini-heelpose1/receipt.json').read_text())
bind = []
names = {}
for bone in mesh['bones']:
    q = bone['rotation']
    local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
    parent = bone['parent']
    bind.append(bind[parent] @ local if parent >= 0 else local)
    names[bone['name']] = bind[-1]
corrections = {}
for row in receipt['shoes']:
    rotation = Matrix.Rotation(math.radians(row['angle_degrees']), 3, Vector(row['axis']))
    basis = names[row['foot_bone']].to_quaternion().to_matrix()
    corrections[row['foot_bone']] = (basis.inverted() @ rotation @ basis).to_quaternion()
report = {}
for clip in ('walk', 'jog', 'sprint'):
    base = json.loads((work / f'feet-{clip}-base.json').read_text())
    candidate = json.loads((work / f'feet-{clip}-candidate.json').read_text())
    worst = 0.0
    for a, b in zip(base['frames'], candidate['frames'], strict=True):
        pa, pb = a['pose']['Snapshot'], b['pose']['Snapshot']
        for name, correction in corrections.items():
            i = pa['BoneNames'].index(name)
            assert pb['BoneNames'][i] == name
            qa = Quaternion([pa['LocalTransforms'][i]['Rotation'][c] for c in 'WXYZ'])
            qb = Quaternion([pb['LocalTransforms'][i]['Rotation'][c] for c in 'WXYZ'])
            expected = (qa @ correction).normalized()
            actual = qb.normalized()
            error = min(max(abs(a-b) for a,b in zip(expected,actual)),
                max(abs(a+b) for a,b in zip(expected,actual)))
            worst = max(worst, error)
    assert worst < .00001, (clip, worst)
    report[clip] = dict(frames=len(candidate['frames']), maximum_quaternion_component_error=worst)
output = work / 'feet-rotation-verification.json'
assert not output.exists()
output.write_text(json.dumps(report, indent=2)+'\n')
print(report, flush=True)
