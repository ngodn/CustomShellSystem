"""Read surface channels using the established six-Eve cooked provenance inputs.

Python 3.14. Reuses the existing reader without rebuilding it or copying packages.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--summary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / 'work').resolve()):
        raise ValueError('Readback must be under CSS/work')
    output.mkdir(exist_ok=False)
    summary = json.loads(args.summary.read_text())
    rows = []
    pinned = {str(args.summary.resolve()): sha(args.summary), str(Path(__file__)): sha(__file__)}
    for row in summary['rows']:
        comparison = ROOT / row['comparison']
        if sha(comparison) != row['sha256']:
            raise ValueError('Comparison changed: ' + str(comparison))
        folder = comparison.parent
        source_inputs = json.loads((folder / 'inputs.json').read_text())
        # The CW catalog changed since this inventory; these checks concern Eve
        # geometry and the shared skeleton, whose installed inputs must still match.
        protected = {p: h for p, h in source_inputs.items() if
                     '/CSS_EveStellarBlade_eins0fx_P/' in p or
                     ('/CSS_SharedAssets_P/' in p and p.endswith(('.utoc', '.ucas')))}
        if len(protected) != 5:
            raise ValueError('Expected Eve and shared binary identities')
        pinned.update(protected)
        command_path = folder / 'skin-command.json'
        command = json.loads(command_path.read_text())['command']
        pinned[str(command_path)] = sha(command_path)
        for index in (0, 1, 3):
            pinned[command[index]] = sha(command[index])
        rows.append((row, command))
    for path, digest in pinned.items():
        if sha(path) != digest:
            raise ValueError('Input changed: ' + path)
    (output / 'inputs.json').write_text(json.dumps(pinned, indent=2) + '\n')
    results = []
    for row, original in rows:
        folder = output / row['variant']
        folder.mkdir()
        command = [*original[:-1], str(folder / 'readback')]
        overrides = dict(CSS_SKIN_AUDIT='1', CSS_SKIN_SURFACE='1')
        (folder / 'command.json').write_text(json.dumps(dict(command=command, environment=overrides), indent=2) + '\n')
        with (folder / 'reader.log').open('w') as log:
            proc = subprocess.run(command, env={**os.environ, **overrides}, stdout=log, stderr=subprocess.STDOUT)
        result = dict(variant=row['variant'], returncode=proc.returncode)
        (folder / 'exit.json').write_text(json.dumps(result) + '\n')
        results.append(result)
        print(result, flush=True)
        if proc.returncode:
            break
    changed = [p for p, digest in pinned.items() if sha(p) != digest]
    passed = len(results) == 6 and all(r['returncode'] == 0 for r in results) and not changed
    (output / 'summary.json').write_text(json.dumps(dict(passed=passed, results=results, changed_inputs=changed), indent=2) + '\n')
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
