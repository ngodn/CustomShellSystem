"""Derive a neck-twist correction without changing the head or body trajectory."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from reparent_source_legs import multiply, rotate, transforms

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('bind', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
source = args.source.resolve(strict=True)
bind_path = args.bind.resolve(strict=True)
out = args.output.resolve()
if out.exists():
    raise FileExistsError(out)
inputs = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, bind_path)}
data = json.loads(source.read_text())
bones = json.loads(bind_path.read_text())
names = [b['name'] for b in bones]
if len(set(names)) != len(names) or not 2 <= len(data['frames']) <= 10000 or not 0 < data['fps'] <= 240:
    raise ValueError('Invalid skeleton or sample clock')
indices = [names.index(n) for n in ('spine_05', 'neck_01', 'neck_02', 'head')]
chest, lower, upper, head = indices
if [bones[i]['parent'] for i in indices[1:]] != indices[:-1]:
    raise ValueError('Expected the audited two-bone neck chain')
reference = transforms(bones)


def inverse(q):
    return q * np.array([-1, -1, -1, 1])


def normalized(q):
    return q / np.linalg.norm(q, axis=-1, keepdims=True)


def slerp(a, b, alpha):
    dot = np.sum(a * b, axis=-1, keepdims=True)
    b = np.where(dot < 0, -b, b)
    angle = np.arccos(np.clip(abs(dot), 0, 1))
    divisor = np.maximum(np.sin(angle), 1e-12)
    result = (np.sin((1 - alpha) * angle) * a + np.sin(alpha * angle) * b) / divisor
    return normalized(np.where(angle < 1e-5, (1-alpha)*a + alpha*b, result))


def align(a, b):
    a, b = normalized(a), normalized(b)
    dot = np.sum(a*b, axis=-1, keepdims=True)
    if not np.isfinite(dot).all() or np.min(dot) <= -.999:
        raise ValueError('Antiparallel or invalid neck direction needs a separate solution')
    return normalized(np.concatenate((np.cross(a, b), 1+dot), axis=-1))


tracks = []
for index, bone in enumerate(bones):
    keys = []
    for frame in data['frames']:
        snap = frame['pose']['Snapshot']
        if snap['BoneNames'][:len(names)] != names:
            raise ValueError('Snapshot and mesh bone order differ')
        t = snap['LocalTransforms'][index]
        keys.append({field: [t[label][c] for c in axes] for field, label, axes in
            [('translation', 'Translation', 'XYZ'), ('rotation', 'Rotation', 'XYZW'), ('scale', 'Scale3D', 'XYZ')]})
    tracks.append(dict(name=bone['name'], keys=keys))
world = transforms(bones, tracks, len(data['frames']))
if not all(np.max(abs(world[i][2]-1)) < 1e-8 for i in indices):
    raise ValueError('Scaled neck chains require a separate correction')
replacement = {i: v[1].copy() for i, v in enumerate(world)}
chest_delta = multiply(world[chest][1], inverse(reference[chest][1]))
head_delta = multiply(world[head][1], inverse(reference[head][1]))
lengths = [np.linalg.norm(bones[i]['translation']) for i in (upper, head)]
for index, child, alpha in ((lower, upper, 0.0), (upper, head, lengths[0]/sum(lengths))):
    desired = multiply(slerp(chest_delta, head_delta, alpha), reference[index][1])
    direction = world[child][0]-world[index][0]
    correction = align(rotate(desired, np.asarray(bones[child]['translation'])), direction)
    replacement[index] = normalized(multiply(correction, desired))
fixed = copy.deepcopy(data)
for index in (lower, upper, head):
    parent = bones[index]['parent']
    local = normalized(multiply(inverse(replacement[parent]), replacement[index]))
    for frame, q in zip(fixed['frames'], local, strict=True):
        frame['pose']['Snapshot']['LocalTransforms'][index]['Rotation'] = dict(zip('XYZW', q.tolist()))
new_tracks = copy.deepcopy(tracks)
for index in (lower, upper, head):
    for f, frame in enumerate(fixed['frames']):
        new_tracks[index]['keys'][f]['rotation'] = list(frame['pose']['Snapshot']['LocalTransforms'][index]['Rotation'].values())
after = transforms(bones, new_tracks, len(data['frames']))
affected = {lower, upper}
tie = names.index('CSS_Acc_Tie_01')
for i, bone in enumerate(bones):
    if i == tie or bone['parent'] in affected and i not in (upper, head):
        affected.add(i)
unchanged = set(range(len(bones))) - affected
position_error = max(float(np.max(np.linalg.norm(after[i][0]-world[i][0],axis=1))) for i in unchanged)
rotation_error = max(float(np.max(np.minimum(np.linalg.norm(after[i][1]-world[i][1],axis=1), np.linalg.norm(after[i][1]+world[i][1],axis=1)))) for i in unchanged)
if position_error >= 1e-6 or rotation_error >= 1e-6:
    raise ValueError(f'Head/body pose changed: {position_error}, {rotation_error}')
if any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != h for p, h in inputs.items()):
    raise ValueError('Source changed during derivation')
out.mkdir()
(out/'motion.json').write_text(json.dumps(fixed))
def peak(q):
    return float(np.degrees(2*np.arccos(np.clip(abs(np.sum(q[1:]*q[:-1],axis=1)),0,1))).max())
report = dict(inputs=inputs,
    frames=len(fixed['frames']), changed_local_rotations=[names[i] for i in (lower,upper,head)],
    changed_world_rotations=[names[i] for i in sorted(affected)],
    preserved_position_error_cm=position_error,preserved_quaternion_distance=rotation_error,
    neck_peaks_degrees={names[i]:dict(before=peak(world[i][1]),after=peak(after[i][1])) for i in (lower,upper)},
    scope='Offline neck-twist prototype. Preserves head and body world pose, all translations/scales; neck-attached tie follows the corrected neck. Not cooked or installed.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k != 'inputs'}))
