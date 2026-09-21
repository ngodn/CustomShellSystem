"""Prepare a 54-bone Eve source rig and sampled clips for UE 5.6.1 authoring."""
import argparse
import hashlib
import json
import re
from pathlib import Path

import numpy as np
from review_source_tracks import sample, vector, rotation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    sources = {'Walk': 'Proto_Walk', 'Idle': 'P_Eve_Peaceful_Idle01',
               'Jog': 'Proto_Run', 'Sprint': 'Proto_Sprint'}
    parser.add_argument('--clips', nargs='+')
    parser.add_argument('--clip-map', type=Path, help='Short output labels mapped to decoded source directory names')
    parser.add_argument('--revision', default='')
    parser.add_argument('--source-yaw', type=int, choices=(0, 90), default=0)
    args = parser.parse_args()
    if args.clip_map:
        sources = json.loads(args.clip_map.read_text())
    assert isinstance(sources, dict) and 1 <= len(sources) <= 64
    assert all(re.fullmatch(r'[A-Za-z][A-Za-z0-9]{0,15}', label) and
               isinstance(name, str) and re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,80}', name)
               for label, name in sources.items())
    args.clips = args.clips or (list(sources) if args.clip_map else ['Walk', 'Idle'])
    assert 1 <= len(args.clips) <= 64 and len(set(args.clips)) == len(args.clips)
    assert all(name in sources for name in args.clips)
    assert not args.output.exists()
    assert re.fullmatch(r'[A-Za-z0-9]{0,8}', args.revision)
    assert not args.source_yaw or args.revision
    prefix = args.revision+'_' if args.revision else ''
    half = np.deg2rad(args.source_yaw)/2
    yaw = np.array([0., 0., np.sin(half), np.cos(half)])
    basis = rotation(yaw)

    def orient_root(transform):
        transform['translation'] = (basis @ transform['translation']).tolist()
        q = np.array(transform['rotation'])
        xyz = yaw[3]*q[:3] + q[3]*yaw[:3] + np.cross(yaw[:3], q[:3])
        result = np.append(xyz, yaw[3]*q[3]-np.dot(yaw[:3], q[:3]))
        transform['rotation'] = (result/np.linalg.norm(result)).tolist()
    names = {'Root', 'Bip001', 'Bip001-Pelvis', 'Bip001-Spine', 'Bip001-Spine1',
             'Bip001-Spine2', 'Bip001-Neck', 'Bip001-Head'}
    names.update(f'Bip001-{side}-{part}' for side in ('L', 'R')
                 for part in ('Clavicle', 'UpperArm', 'Forearm', 'Hand', 'Thigh', 'Calf', 'Foot', 'Toe0'))
    names.update(f'Bip001-{side}-Finger{digit}{joint}' for side in ('L', 'R')
                 for digit in range(5) for joint in ('', '1', '2'))
    assert len(names) == 54
    reference, clips, hashes = None, {}, {}
    assert len(set(args.clips)) == len(args.clips)
    for label in (sources[name] for name in args.clips):
        path = args.source/label/'source-tracks.json'
        data = json.loads(path.read_text())
        hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        assert data['interpolation'] == 'Linear' and data['rateScale'] == 1
        assert abs((data['frameCount']-1)/30-data['duration']) < 1e-6
        selected = [i for i, b in enumerate(data['bones']) if b['name'] in names]
        assert len(selected) == 54
        mapping = {old: new for new, old in enumerate(selected)}
        bones, tracks = [], []
        for index in selected:
            bone = data['bones'][index]
            parent = bone['parent']
            assert parent == -1 or parent in mapping
            ref = bone['reference']
            bones.append(dict(name=bone['name'], parent=mapping[parent] if parent >= 0 else -1,
                              translation=vector(ref['Translation']).tolist(),
                              rotation=vector(ref['Rotation'], True).tolist(),
                              scale=vector(ref['Scale3D']).tolist()))
            assert np.allclose(bones[-1]['scale'], 1, atol=.0001)
            if args.source_yaw and parent == -1:
                orient_root(bones[-1])
            if not bone['animated']:
                continue
            keys = []
            for frame in range(data['frameCount']):
                keys.append({field: sample(bone, channel, times, fallback, frame, data['frameCount']).tolist()
                             for field, channel, times, fallback in [
                                 ('translation', 'translations', 'translationTimes', 'Translation'),
                                 ('rotation', 'rotations', 'rotationTimes', 'Rotation'),
                                 ('scale', 'scales', 'scaleTimes', 'Scale3D')]})
                assert np.allclose(keys[-1]['scale'], 1, atol=.0001)
                if args.source_yaw and parent == -1:
                    orient_root(keys[-1])
            tracks.append(dict(name=bone['name'], keys=keys))
        if reference is None:
            reference = bones
        assert reference == bones
        clips[label] = dict(name=label, source=data['asset'], frames=data['frameCount'],
                            duration=data['duration'], fps=30, tracks=tracks)
    args.output.mkdir(parents=True)
    mesh = dict(schema=1, mesh_package='/Game/CSS/AnimLab/SK_'+prefix+'EveSource',
                skeleton_package='/Game/CSS/AnimLab/SKEL_'+prefix+'EveSource', bones=reference,
                points=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
                wedges=[[0, 0, 0], [1, 1, 0], [2, 0, 1]], faces=[[0, 1, 2, 0]],
                materials=['Diagnostic'], influences=[[0, 0, 1], [1, 0, 1], [2, 0, 1]])
    for name, value in [('source.mesh', mesh), *clips.items(), ('source-hashes', hashes)]:
        (args.output/(name+'.json')).write_text(json.dumps(value, indent=2)+'\n')
    (args.output/'batch.json').write_text(json.dumps(
        [[name, sources[name]] for name in args.clips], indent=2)+'\n')
    (args.output/'batch-config.json').write_text(json.dumps(dict(revision=args.revision,
        source_yaw_degrees=args.source_yaw, source_basis=basis.tolist()), indent=2)+'\n')


if __name__ == '__main__':
    main()
