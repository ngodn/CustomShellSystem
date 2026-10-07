"""Locate and hash editable candidates without choosing between differing sources."""
import argparse
from collections import Counter
import json
from pathlib import Path

from three_outfit_baseline import identity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', type=Path, required=True)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    plan = json.loads(args.map.read_text())
    config = json.loads(args.sources.read_text())
    if args.output.exists():
        raise FileExistsError(args.output)
    roots = {name: Path(path).resolve(strict=True) for name, path in config['roots'].items()}
    aliases = config.get('cooked_aliases', {})
    pins = config.get('pinned_sources', {})
    cache, rows = {}, []
    for entry in plan['assets']:
        package = entry['old_package']
        source_package = aliases.get(package, {}).get('source_package', package)
        if not source_package.startswith('/Game/') or '..' in source_package.split('/'):
            raise ValueError('Invalid source package: ' + source_package)
        relative = source_package.removeprefix('/Game/') + '.uasset'
        candidates = []
        for name, root in roots.items():
            path = root / relative
            if not path.is_file():
                continue
            actual = path.resolve(strict=True)
            if actual not in cache:
                cache[actual] = identity(actual)
            candidates.append(dict(root=name, identity=cache[actual], lookup_path=str(path)))
        hashes = {c['identity']['sha256'] for c in candidates}
        status = 'missing' if not hashes else 'one_content_version' if len(hashes) == 1 else 'different_candidates'
        selected = candidates[0] if len(hashes) == 1 else None
        pin = pins.get(source_package)
        if pin:
            receipt = json.loads(Path(pin['receipt']).read_text())
            expected = receipt['packages'][source_package]
            matches = [c for c in candidates if c['root'] == pin['root'] and c['identity']['sha256'] == expected]
            if len(matches) != 1:
                raise ValueError('Pinned source does not match its receipt: ' + source_package)
            selected = matches[0]
            status = 'receipt_pinned'
        rows.append(dict(package=package, destination=entry['new_package'],
                         source_package=source_package, status=status, candidates=candidates,
                         selected=selected, source_receipt=identity(Path(pin['receipt'])) if pin else None,
                         cooked_alias_recipe=aliases.get(package)))
    report = dict(scope='Editable candidates and recorded alias recipes, not source-to-installed equivalence',
                  inputs=dict(asset_map=identity(args.map), source_config=identity(args.sources)),
                  counts=dict(Counter(r['status'] for r in rows)), assets=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(report['counts']))


if __name__ == '__main__':
    main()
