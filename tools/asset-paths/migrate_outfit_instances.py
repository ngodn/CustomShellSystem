"""Preserve cooked outfit material instances while moving their package references."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--container', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / 'work/three-outfit-rig-physics'):
        raise ValueError('Use a new private migration directory')
    baseline = json.loads(args.baseline.read_text())
    plan = json.loads(args.plan.read_text())
    sources = [p for p in baseline['packages'] if p['container'] == args.container]
    if len(sources) != 1:
        raise ValueError('Expected one pinned source container')
    sources = sources[0]['files']
    for row in sources.values():
        if sha(Path(row['path'])) != row['sha256']:
            raise ValueError('Baseline container changed: ' + row['path'])
    selected = [a for a in plan['assets'] if a['target_container'] == args.container
                and not a.get('deduplicated_to')]
    mapping = {a['old_package']: a['new_package'] for a in selected}
    if len(mapping) != len(selected) or len({p.casefold() for p in mapping.values()}) != len(mapping):
        raise ValueError('Outfit migration paths overlap')
    for old, new in mapping.items():
        if (not old.startswith('/Game/') or not new.startswith('/Game/CSS/') or
                any(part in ('', '.', '..') for p in (old, new) for part in p[1:].split('/'))):
            raise ValueError('Invalid outfit package path')
    # Inspect export classes after extraction, rather than trusting M_/MI_ names.
    packages = {a['old_package'] for a in selected}
    if not packages:
        raise ValueError('No outfit packages in migration plan')
    output.mkdir(parents=True)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    mount = output / 'containers'
    converter.base_containers(GAME, mount)
    for row in sources.values():
        path = Path(row['path'])
        (mount / path.name).symlink_to(path)
    # Some outfit-owned adapters currently live in the shared library. Only
    # extract the packages this pinned outfit actually supplies.
    supplied = {e['package'] for p in baseline['packages'] if p['container'] == args.container
                for e in p['exports']}
    filters = [arg for package in sorted(packages & supplied)
               for arg in ('-f', package.removeprefix('/Game/') + '.uasset')]
    converter.run(converter.retoc, 'to-legacy', mount, output / 'original',
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', *filters)
    jobs, hashes, skipped = [], {}, []
    for path in sorted((output / 'original').rglob('*.uasset')):
        info = asset_info(path.read_bytes())
        if info['package'] not in mapping:
            raise ValueError('Unexpected extracted package: ' + info['package'])
        if len(info['exports']) != 1 or info['exports'][0]['class'] != 'MaterialInstanceConstant':
            skipped.append(dict(package=info['package'], classes=[e['class'] for e in info['exports']]))
            continue
        target = output / 'stage/MortalShell2/Content' / (mapping[info['package']].removeprefix('/Game/') + '.uasset')
        jobs.append(dict(input=str(path), output=str(target)))
        hashes[str(path)] = sha(path)
        hashes[str(path.with_suffix('.uexp'))] = sha(path.with_suffix('.uexp'))
    if not jobs:
        raise ValueError('No cooked material instances found')
    request = output / 'request.json'
    request.write_text(json.dumps(dict(mapping=mapping, jobs=jobs,
        report=str(output / 'relocation.json')), indent=2) + '\n')
    writer = ROOT / 'build/retoc-css-target/release/examples/rewrite_material'
    with (output / 'relocation.log').open('x') as log:
        result = subprocess.run([writer, request], stdout=log, stderr=subprocess.STDOUT)
    (output / 'relocation-exit.json').write_text(json.dumps(dict(exit_code=result.returncode)) + '\n')
    result.check_returncode()
    report = json.loads((output / 'relocation.json').read_text())
    if not report['passed'] or len(report['materials']) != len(jobs):
        raise ValueError('Incomplete relocation')
    for job in jobs:
        original, candidate = Path(job['input']), Path(job['output'])
        if sha(original.with_suffix('.uexp')) != sha(candidate.with_suffix('.uexp')):
            raise ValueError('Material payload changed')
    if any(sha(Path(p)) != digest for p, digest in hashes.items()):
        raise ValueError('Extracted source changed')
    if any(sha(Path(r['path'])) != r['sha256'] for r in sources.values()):
        raise ValueError('Installed source changed')
    (output / 'verification.json').write_text(json.dumps(dict(passed=True,
        material_instances=len(jobs), source_files=hashes, source_containers=sources,
        evidence={str(args.baseline.resolve()): sha(args.baseline),
                  str(args.plan.resolve()): sha(args.plan), str(writer): sha(writer)},
        payloads={str(p.relative_to(output / 'stage')): sha(p)
                  for p in (output / 'stage').rglob('*') if p.is_file()},
        other_assets=skipped, installed=False,
        scope='Cooked instance header relocation with unchanged export bytes and reversible headers. '
              'Final container readback and migrated dependencies remain.'), indent=2) + '\n')
    print(f'Preserved {len(jobs)} cooked material instances; {len(skipped)} other assets remain')


if __name__ == '__main__':
    main()
