"""Check cooked idle carriers against their sequences and retained shared assets."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def read(path):
    return json.loads(path.read_text())


def check_carrier(package, sequence, decoded):
    blend, = [e for e in decoded[package] if e['Type'] == 'BlendSpace']
    clip, = [e for e in decoded[sequence] if e['Type'] == 'AnimSequence']
    properties, animation = blend['Properties'], clip['Properties']
    human = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton'
    if any(p['Skeleton']['ObjectPath'].split('.')[0] != human for p in (properties, animation)):
        raise ValueError('Native carrier skeleton differs')
    duration = animation['SequenceLength']
    if not math.isfinite(duration) or duration <= 0 or abs(properties['AnimLength']-duration) > 1e-5:
        raise ValueError('Native carrier duration differs')
    if animation.get('AdditiveAnimType', 'AAT_None') not in ('AAT_None', 'EAdditiveAnimationType::AAT_None', 0):
        raise ValueError('Native idle is additive')
    if animation.get('bEnableRootMotion', False) or animation.get('Notifies', []):
        raise ValueError('Native idle contains root motion or gameplay notifies')
    samples = properties['SampleData']
    positions = {(-180., 0., 0.), (180., 0., 0.), (-180., 800., 0.), (180., 800., 0.)}
    actual = set()
    for sample in samples:
        if sample['Animation']['ObjectPath'].split('.')[0] != sequence or sample['RateScale'] != 1.:
            raise ValueError('Native carrier changes clip or rate')
        actual.add(tuple(sample['SampleValue'][axis] for axis in ('X', 'Y', 'Z')))
    if len(samples) != 4 or actual != positions or len(properties['BlendSpaceData']['Triangles']) != 2:
        raise ValueError('Native carrier interpolation changed')
    return {'package': package, 'sequence': sequence, 'duration': duration, 'samples': 4}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('authored', type=Path)
    args = parser.parse_args()
    old, new = (read(p/'verification.json') for p in (args.baseline, args.candidate))
    if not old['passed'] or not new['passed']:
        raise ValueError('Unverified shared pack')
    if any(new['payloads'].get(p) != digest for p, digest in old['payloads'].items()):
        raise ValueError('Existing shared payload changed')
    decoded = read(args.candidate/'carrier-decoded.json')
    authored = read(args.authored)
    rows = [check_carrier(row['package'], row['sequence'], decoded) for row in authored['carriers']]
    if len(rows) != 52 or new['packages']-old['packages'] != len(rows):
        raise ValueError('Incomplete added carrier inventory')
    original = read(args.baseline/'shared-inline.json')
    candidate = read(args.candidate/'shared-inline.json')
    candidate['containers'] = original['containers']
    ids = {row['id'] for row in authored['carriers']}
    edits = 0
    for entry in candidate['catalog']['animation_library']['entries']:
        if entry['id'] in ids:
            native, = [a for a in entry['assets'] if '/NativeHuman/' in a['asset']]
            row, = [r for r in authored['carriers'] if r['id'] == entry['id']]
            if native.pop('idle_blend_space').split('.')[0] != row['package']:
                raise ValueError('Catalog carrier differs')
            edits += 1
    if edits != 52 or candidate != original:
        raise ValueError('Catalog changed beyond the added idle carriers')
    inputs = [args.baseline/'verification.json', args.candidate/'verification.json',
              args.candidate/'carrier-decoded.json', args.authored]
    result = dict(passed=True, carriers=rows, retained_payloads=len(old['payloads']),
        inputs={str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
        scope='Cooked carrier properties and unchanged existing payloads. Gameplay acceptance remains pending.')
    with (args.candidate/'carrier-verification.json').open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print('52 cooked carriers verified; existing payloads and catalog fields preserved')


if __name__ == '__main__':
    main()
