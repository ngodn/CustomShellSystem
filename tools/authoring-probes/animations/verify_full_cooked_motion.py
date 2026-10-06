"""Compare every decoded ACL key with a saved raw UE pose capture."""
import argparse
import gzip
import hashlib
import json
import math
from pathlib import Path

import numpy as np


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('decoded', type=Path)
    parser.add_argument('raw_capture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    manifest_path = args.decoded / 'absolute-tracks-full.json'
    sample_path = args.decoded / 'absolute-tracks.json'
    manifest = json.loads(manifest_path.read_text())
    sample = json.loads(sample_path.read_text())
    if (manifest['schema'] != 1 or manifest['layout'] != 'frame,track,component' or
            manifest.get('roundingPolicy') not in ('none', 'nearest') or
            manifest['componentType'] != 'float32-le' or manifest['file'] != 'absolute-tracks.bin' or
            manifest['components'] != ['qx', 'qy', 'qz', 'qw', 'tx', 'ty', 'tz', 'sx', 'sy', 'sz']):
        raise ValueError('Unsupported full-transform layout')
    names = manifest['names']
    frames = manifest['frames']
    binary = args.decoded / manifest['file']
    if (not names or len(names) != len(set(names)) or not 1 <= frames <= 100001 or
            len(names) > 4096 or frames * len(names) > 2004096 or
            binary.stat().st_size != frames * len(names) * 40 or
            binary.stat().st_size != manifest['bytes'] or sha(binary) != manifest['sha256']):
        raise ValueError('Invalid full-transform dimensions or checksum')
    inputs = {str(p.resolve()): sha(p) for p in (manifest_path, sample_path, binary, args.raw_capture)}
    values = np.memmap(binary, dtype='<f4', mode='r', shape=(frames, len(names), 10))
    if (not np.isfinite(values).all() or
            np.max(np.abs(np.linalg.norm(values[:, :, :4].astype(float), axis=2) - 1)) >= .001):
        raise ValueError('Non-finite or non-unit cooked transforms')
    if (sample['schema'] != 2 or sample['asset'] != manifest['asset'] or
            sample['logicalSamples'] != frames or sample['sampleRate'] != manifest['sampleRate'] or
            sample['trackCount'] != len(names)):
        raise ValueError('Sample and full export disagree')
    for row in sample['frames']:
        if row['BoneNames'] != names:
            raise ValueError('Sample track order changed')
        expected = np.array([[t[field][c] for field, axes in (
            ('Rotation', 'XYZW'), ('Translation', 'XYZ'), ('Scale3D', 'XYZ')) for c in axes]
            for t in row['LocalTransforms']], dtype='<f4')
        if not np.array_equal(values[row['frame']], expected):
            raise ValueError('Binary and JSON sample transforms differ')
    opener = gzip.open if args.raw_capture.suffix == '.gz' else open
    with opener(args.raw_capture, 'rt') as stream:
        raw = json.load(stream)
    if raw.get('should_retarget') is not False:
        raise ValueError('Cook comparison requires an explicitly unretargeted raw reference')
    if len(raw['frames']) != frames or abs(raw['fps'] - manifest['sampleRate']) > .001:
        raise ValueError('Raw and cooked clocks differ')
    order = raw['frames'][0]['pose']['Snapshot']['BoneNames']
    if len(order) != len(set(order)) or not set(names) <= set(order):
        raise ValueError('Raw pose lacks unique cooked track coverage')
    indices = [order.index(name) for name in names]
    maxima = dict(position_cm=0., quaternion_distance=0., scale=0.)
    worst = {}
    for frame, row in enumerate(raw['frames']):
        snapshot = row['pose']['Snapshot']
        if (not snapshot['bIsValid'] or snapshot['BoneNames'] != order or
                len(snapshot['LocalTransforms']) != len(order)):
            raise ValueError('Invalid or inconsistent raw pose')
        actual = np.array([[snapshot['LocalTransforms'][i][field][c] for field, axes in (
            ('Rotation', 'XYZW'), ('Translation', 'XYZ'), ('Scale3D', 'XYZ')) for c in axes]
            for i in indices])
        expected = values[frame].astype(float)
        if not np.isfinite(actual).all():
            raise ValueError('Non-finite raw pose')
        errors = dict(position_cm=np.linalg.norm(actual[:, 4:7] - expected[:, 4:7], axis=1),
                      quaternion_distance=np.minimum(np.linalg.norm(actual[:, :4] - expected[:, :4], axis=1),
                                                     np.linalg.norm(actual[:, :4] + expected[:, :4], axis=1)),
                      scale=np.linalg.norm(actual[:, 7:] - expected[:, 7:], axis=1))
        for key, error in errors.items():
            index = int(np.argmax(error))
            if error[index] > maxima[key]:
                maxima[key] = float(error[index])
                worst[key] = dict(bone=names[index], frame=frame)
    if any(sha(Path(p)) != checksum for p, checksum in inputs.items()):
        raise ValueError('Input changed during comparison')
    limits = dict(position_cm=.05, quaternion_distance=2 * math.sin(math.radians(.25) / 4), scale=.001)
    passed = all(maxima[key] < limit for key, limit in limits.items())
    args.output.write_text(json.dumps(dict(complete=True, passed=passed, asset=manifest['asset'],
        frames=frames, tracks=len(names), maxima=maxima, worst=worst, limits=limits, inputs=inputs,
        binary_matches_json_samples=True, rounding_policy=manifest['roundingPolicy'],
        scope='Every source-key time decoded with the recorded rounding policy against unretargeted raw UE local poses. No between-key contact, adapters, physics or gameplay acceptance.'), indent=2) + '\n')
    print(json.dumps(dict(passed=passed, frames=frames, maxima=maxima, worst=worst)), flush=True)
    if not passed:
        raise ValueError('Full cooked motion exceeds existing review limits; inspect report')


if __name__ == '__main__':
    main()
