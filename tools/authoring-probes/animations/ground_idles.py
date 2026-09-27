"""Prepare loop-continuous root-height corrections from visible-mesh measurements."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    assert not args.output.exists()
    args.output.mkdir()
    receipts = []
    for clip in range(700, 714):
        source = args.work/'e4'/f'idle{clip}-motion.json'
        measurement = args.work/f'c2-{clip}'/'report.json'
        original = json.loads(source.read_text())
        report = json.loads(measurement.read_text())
        assert report['motion_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
        frames = original['frames']
        xs = np.array([e['frame'] for e in report['errors']])
        zs = np.array([e['contact_lowest_cm'] for e in report['errors']])
        assert xs[0] == 0 and xs[-1] == len(frames)-1 and np.all(np.diff(xs) > 0)
        assert np.isfinite(zs).all() and original['fps'] == 30
        zs[0] = zs[-1] = (zs[0]+zs[-1])/2
        # Two centimetres allows the accepted Black Pearl -3 cm mesh offset.
        # Keep a constant offset when the original contact variation is already small.
        if np.ptp(zs) <= 3:
            offsets = np.full(len(frames), 2-float(np.median(zs)))
            method = 'constant'
        else:
            contact = np.interp(np.arange(len(frames)-1), xs, zs)
            smooth = sum(weight*np.roll(contact, shift) for shift, weight in
                         [(-2, 1), (-1, 2), (0, 3), (1, 2), (2, 1)])/9
            offsets = np.append(2-smooth, 2-smooth[0])
            method = 'periodic contact compensation'
        assert np.isfinite(offsets).all() and np.max(np.abs(offsets)) < 30
        assert offsets[0] == offsets[-1]
        corrected = copy.deepcopy(original)
        for frame, offset in zip(corrected['frames'], offsets, strict=True):
            snapshot = frame['pose']['Snapshot']
            i = snapshot['BoneNames'].index('root')
            snapshot['LocalTransforms'][i]['Translation']['Z'] += float(offset)
        (args.output/f'idle{clip}-motion.json').write_text(json.dumps(corrected)+'\n')
        receipts.append(dict(clip=clip, method=method, offsets_cm=offsets.tolist(),
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            measured_before_cm=[float(zs.min()), float(zs.max())],
            predicted_sample_contact_cm=(zs+offsets[xs]).tolist()))
    (args.output/'grounding.json').write_text(json.dumps(receipts, indent=2)+'\n')


if __name__ == '__main__':
    main()
