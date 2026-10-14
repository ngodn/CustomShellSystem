"""Run saved Blender-part comparisons from an explicit source plan."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--blender', type=Path, required=True)
    args = parser.parse_args()
    plan = args.plan.resolve(strict=True)
    rows = json.loads(plan.read_text())
    checker = Path(__file__).with_name('check_eve_source_part.py').resolve()
    inputs = {str(path): sha(path) for path in (plan, checker, args.blender.resolve(strict=True))}
    labels = [row['label'] for row in rows]
    if len(set(labels)) != len(rows) or any(Path(label).name != label for label in labels):
        raise ValueError('Plan labels must be distinct filenames')
    output = plan.parent / 'results'
    output.mkdir(exist_ok=False)
    results = []
    for row in rows:
        report = output / (row['label'] + '.json')
        command = [str(args.blender), '-b', '--factory-startup', '--python-exit-code', '1',
                   '--python', str(checker), '--', '--source', row['source'],
                   '--mesh', row['mesh'], '--part', row['part'], '--output', str(report)]
        (output / (row['label'] + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        with (output / (row['label'] + '.log')).open('w') as log:
            process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        result = dict(label=row['label'], outfit=row['outfit'], part=row['part'],
                      returncode=process.returncode, report=str(report), passed=False)
        if report.exists():
            data = json.loads(report.read_text())
            result.update(passed=process.returncode == 0 and data['passed'], report_sha256=sha(report))
        results.append(result)
        (output / 'progress.json').write_text(json.dumps(results, indent=2) + '\n')
        print(row['label'], 'PASS' if result['passed'] else 'FAIL', flush=True)
    unchanged = all(sha(Path(path)) == digest for path, digest in inputs.items())
    summary = dict(passed=all(row['passed'] for row in results) and unchanged,
                   inputs=inputs, inputs_unchanged=unchanged, rows=results,
                   scope='Saved Blender parts versus retained imports. No source edits or game changes.')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    if not summary['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
