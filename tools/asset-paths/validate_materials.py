"""Compare independent material readback, allowing only declared path changes."""
import argparse
import json
from pathlib import Path
import sys

CSS = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(CSS / 'tools'))
from css_convert import digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    load = lambda p: json.loads(p.read_text())
    mapping = load(args.work / 'map.json')
    report = load(args.work / 'report.json')
    assets = load(args.assets)

    def normalized(value):
        if isinstance(value, dict):
            return {k: normalized(v) for k, v in value.items()}
        if isinstance(value, list):
            return [normalized(v) for v in value]
        if isinstance(value, str):
            for old, new in mapping.items():
                if value == old or value.startswith(old + '.') or value.startswith(old + ':'):
                    return new + value[len(old):]
        return value

    failures = []
    for row in report['materials']:
        if normalized(assets[row['old']]) != assets[row['package']]:
            failures.append(row['old'])
        if digest(Path(row['input']).with_suffix('.uexp')) != digest(Path(row['output']).with_suffix('.uexp')):
            raise ValueError('Material payload changed')
    for p, h in load(args.work / 'protected.json').items():
        if digest(Path(p)) != h:
            raise ValueError(f'Source changed: {p}')
    result = dict(passed=not failures, materials=len(report['materials']), differences=failures,
                  unchanged_payloads=True, source_hashes_unchanged=True,
                  scope='Decoded material properties and references; in-game appearance remains separate.')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    if failures:
        raise ValueError(f'{len(failures)} material readbacks differ')
    print('Material properties and references match:', len(report['materials']))


if __name__ == '__main__':
    main()
