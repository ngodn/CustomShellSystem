"""Verify known outfit animation copies against independently decoded originals."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--decoded', type=Path, required=True)
    parser.add_argument('--mount-receipt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inventory, decoded, receipt = [json.loads(p.read_text()) for p in
                                   (args.inventory, args.decoded, args.mount_receipt)]
    if not receipt['passed']:
        raise ValueError('Dependency mount receipt did not pass')
    for path, checksum in receipt['dependencies'].items():
        if sha(Path(path)) != checksum:
            raise ValueError('Mounted baseline dependency changed: '+path)
    aliases = {r['package']: r['source_package'] for r in inventory['assets'] if r['cooked_alias_recipe']}
    if not aliases or set(decoded) != set(aliases) | set(aliases.values()):
        raise ValueError('Decoded packages do not match the alias inventory')
    replacements = sorted(aliases.items(), key=lambda pair: len(pair[0]), reverse=True)

    def normalize(value):
        if isinstance(value, dict):
            return {key: normalize(item) for key, item in value.items()}
        if isinstance(value, list):
            return [normalize(item) for item in value]
        if isinstance(value, str):
            for old, new in replacements:
                value = value.replace(old, new)
        return value

    rows = []
    for alias, source in aliases.items():
        old, canonical = decoded[alias], decoded[source]
        if len(old) != 1 or old[0]['Type'] not in ('AnimSequence', 'BlendSpace'):
            raise ValueError('Unsupported alias export: '+alias)
        if old[0]['Type'] == 'AnimSequence' and 'OfflineAnimationDigest' not in old[0]:
            raise ValueError('Missing compressed-stream digest: '+alias)
        if normalize(old) != canonical:
            raise ValueError('Alias data differs: '+alias)
        rows.append(dict(alias=alias, source=source, object=canonical[0]['Name'],
                         kind=canonical[0]['Type'], identical=True))
    args.output.write_text(json.dumps(dict(passed=True, aliases=rows,
        dependencies=receipt['dependencies'],
        evidence={str(p.resolve()): sha(p) for p in (args.inventory, args.decoded, args.mount_receipt)},
        scope='Exact decoded fields, compressed bone/curve bytes and mapped blend samples after replacing only the documented package names.'), indent=2)+'\n')
    print('Verified animation aliases:', len(rows))


if __name__ == '__main__':
    main()
