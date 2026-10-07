"""Compare independently decoded namespace candidates with their installed sources."""
import argparse
import json
from pathlib import Path


def differences(a, b, path=''):
    if type(a) is not type(b):
        return [dict(path=path, before=a, after=b)]
    if isinstance(a, dict):
        result = []
        for key in sorted(a.keys() | b.keys()):
            if key not in a or key not in b:
                result.append(dict(path=path+'/'+key, missing='before' if key not in a else 'after'))
            else:
                result.extend(differences(a[key], b[key], path+'/'+key))
        return result
    if isinstance(a, list):
        if len(a) != len(b):
            return [dict(path=path, before_length=len(a), after_length=len(b))]
        return [d for index, (x, y) in enumerate(zip(a, b))
                for d in differences(x, y, path+'/'+str(index))]
    return [] if a == b else [dict(path=path, before=a, after=b)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    plan, original, candidate = [json.loads(p.read_text()) for p in
                                 (args.plan, args.original, args.candidate)]
    mapping = sorted(((r['new_package'], r['old_package']) for r in plan['assets']),
                     key=lambda pair: len(pair[0]), reverse=True)

    def normalize(value):
        if isinstance(value, dict):
            return {key: normalize(item) for key, item in value.items()}
        if isinstance(value, list):
            return [normalize(item) for item in value]
        if isinstance(value, str):
            for new, old in mapping:
                value = value.replace(new, old)
        return value

    if set(original) != {r['old_package'] for r in plan['assets']} or set(candidate) != {r['new_package'] for r in plan['assets']}:
        raise ValueError('Decode package sets differ from the plan')
    rows, unchanged_streams = [], 0
    for entry in plan['assets']:
        a, b = original[entry['old_package']], normalize(candidate[entry['new_package']])
        diff = differences(a, b)
        streams = []
        for left, right in zip(a, b, strict=True):
            if 'OfflineAnimationDigest' in left:
                same = left['OfflineAnimationDigest'] == right['OfflineAnimationDigest']
                unchanged_streams += same
                streams.append(same)
        rows.append(dict(package=entry['old_package'], identical=not diff,
                         compressed_streams_identical=streams, differences=diff))
    args.output.write_text(json.dumps(dict(packages=len(rows), identical=sum(r['identical'] for r in rows),
        unchanged_animation_streams=unchanged_streams, results=rows,
        scope='Exact decoded fields and compressed bytes after namespace normalization. Differences require review; this is not gameplay acceptance.'), indent=2)+'\n')
    print(json.dumps(dict(packages=len(rows), identical=sum(r['identical'] for r in rows),
                          unchanged_animation_streams=unchanged_streams)))


if __name__ == '__main__':
    main()
