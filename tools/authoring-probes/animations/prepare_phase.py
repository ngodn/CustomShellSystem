"""Python 3.14: choose whole-key phase shifts from both feet's height curves."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from review_source_tracks import rotation

ROOT = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
work = args.output.resolve()
assert work.is_relative_to(ROOT/'CustomShellSystem/work')
assert not (work/'phase.json').exists() and not (work/'spec.json').exists()
work.mkdir(exist_ok=True)
source = ROOT/'CustomShellSystem/work/anim10/batch'
prior = ROOT/'CustomShellSystem/work/anim11/spec.json'
spec = json.loads(prior.read_text())
bind_path = source/'target-bind.json'
bind = json.loads(bind_path.read_text())
names = [b['name'] for b in bind]
inputs = [bind_path, prior]
feet = {}
for label in spec['calibration']:
    path = source/(label.lower()+'-motion.json')
    inputs.append(path)
    data = json.loads(path.read_text())
    positions = []
    for frame in data['frames']:
        pose = frame['pose']['Snapshot']
        assert pose['BoneNames'][:len(bind)] == names
        world = []
        for i, t in enumerate(pose['LocalTransforms'][:len(bind)]):
            m = np.eye(4)
            m[:3, :3] = rotation([t['Rotation'][k] for k in 'XYZW'])
            m[:3, :3] *= [t['Scale3D'][k] for k in 'XYZ']
            m[:3, 3] = [t['Translation'][k] for k in 'XYZ']
            world.append(world[bind[i]['parent']] @ m if bind[i]['parent'] >= 0 else m)
        positions.append([world[names.index(n)][:3, 3] for n in ('foot_l', 'foot_r')])
    feet[label] = np.array(positions)


def normalized(positions):
    z = positions[:-1, :, 2]
    span = np.ptp(z, axis=0)
    assert np.all(span > 1)
    return (z-z.min(axis=0))/span


results = {}
for label, positions in feet.items():
    reference = 'Walk' if label.startswith('W') else 'Sprint' if label.startswith('S') else 'Jog'
    signal, anchor = normalized(positions), normalized(feet[reference])
    count = len(signal)
    target = np.array([np.interp(np.arange(count)/count,
        np.arange(len(anchor)+1)/len(anchor), np.r_[anchor[:, foot], anchor[0, foot]])
        for foot in range(2)]).T
    scores = [float(np.mean((np.roll(signal, -shift, axis=0)-target)**2)) for shift in range(count)]
    shift = int(np.argmin(scores))
    signed = shift if shift < count/2 else shift-count
    assert abs(signed) <= count/4, (label, signed)
    # Leave an already good match alone; a fitting score is not visual acceptance.
    if scores[0]-scores[shift] < .001:
        shift = signed = 0
    results[label] = dict(start_frame=shift, signed_shift=signed, cycle_keys=count,
        reference=reference, before_error=scores[0], after_error=scores[shift],
        foot_loop_gap_cm=float(np.max(np.linalg.norm(positions[-1]-positions[0], axis=1))))
for definition in spec['definitions']:
    definition['package'] = definition['package'].replace('BS_M1_', 'BS_M2_')
    for sample in definition['samples']:
        sample['revision'] = 'P1' if results.get(sample['clip'], {}).get('start_frame', 0) else 'D2'
spec['scope'] = ('Offline phase-shifted M2 candidates. M1 rates retained for controlled comparison; '
                 'cadence, game layering and moving-owner contact remain unaccepted.')
(work/'spec.json').write_text(json.dumps(spec, indent=2)+'\n')
(work/'phase.json').write_text(json.dumps(dict(schema=1, clips=results,
    inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    scope='Whole-key cyclic shifts fitted to normalized foot height, not a shoe-contact or gameplay gate.'), indent=2)+'\n')
print('Prepared', sum(bool(r['start_frame']) for r in results.values()), 'phase shifts')
