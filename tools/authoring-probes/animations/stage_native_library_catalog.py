"""Add verified native alternatives while retaining all existing animation IDs."""
import argparse
import copy
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('authoring', type=Path)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    base, manifest = args.authoring.resolve(strict=True), args.manifest.resolve(strict=True)
    output = args.output.resolve()
    if output.exists():
        raise FileExistsError(output)
    inputs = {}

    def evidence(relative):
        path = base / relative
        inputs[str(path)] = sha(path)
        return read(path)

    for run in ('pilot3', 'batch1', 'carriers4', 'carrier-readback1', 'precision1', 'precision-verify1'):
        if int((base/run/'exit-code.txt').read_text()) != 0:
            raise ValueError('Authoring or verification failed: ' + run)
    plan = evidence('plan.json')
    created = evidence('pilot3/result.json')['clips'] + evidence('batch1/result.json')['clips']
    checks = evidence('verify2/results.json')
    if len(created) != 59 or len(checks) != 59:
        raise ValueError('Incomplete native inventory')
    failed = [row['id'] for row in checks if not row['compressed_passed']]
    if failed != ['css.eve.dance.cinderella']:
        raise ValueError('Unexpected compression failures')
    precision = evidence('precision1/result.json')
    precise_check = evidence('precision-verify1/result.json')
    if not precise_check['passed'] or not precision['original_candidate_unchanged']:
        raise ValueError('Precision repair did not pass')
    replacements = {row['id']: row for row in precision['clips']}
    if set(replacements) != set(failed):
        raise ValueError('Precision replacement scope changed')
    selected = {row['id']: replacements.get(row['id'], row) for row in created}
    verified = {row['id']: row for row in checks if row['compressed_passed']}
    verified.update({row['id']: row for row in precise_check['clips']})
    if len(selected) != 59 or set(selected) != set(verified):
        raise ValueError('Native candidates and checks differ')
    if not evidence('carrier-readback1/result.json')['passed']:
        raise ValueError('Carrier readback failed')
    carriers = {row['id']: row for row in evidence('carriers4/result.json')['carriers']}
    if len(carriers) != 7:
        raise ValueError('Incomplete native idle carriers')
    content = Path(plan['content'])
    for path, expected in plan['protected'].items():
        if sha(Path(path)) != expected:
            raise ValueError('Protected input changed: ' + path)
        inputs[path] = expected
    for row in [*created, *replacements.values(), *carriers.values()]:
        path = content / (row['package'].removeprefix('/Game/') + '.uasset')
        if sha(path) != row['sha256']:
            raise ValueError('Candidate changed: ' + row['package'])
        inputs[str(path)] = row['sha256']
    inputs[str(manifest)] = sha(manifest)
    original = read(manifest)['catalog']['animation_library']
    catalog = copy.deepcopy(original)
    skeleton = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton'
    preserved = []
    for entry in catalog['entries']:
        if entry['id'] not in selected:
            preserved.append(entry['id'])
            continue
        row = selected[entry['id']]
        check = verified[entry['id']]
        if (not check['compressed_passed'] or check['frames'] != row['keys'] or
                check['bones'] != 1208 or any(a['skeleton'] == skeleton for a in entry['assets'])):
            raise ValueError('Candidate already present or verification mismatched')
        package = row['package']
        alternative = dict(asset=package+'.'+package.rsplit('/', 1)[1], skeleton=skeleton)
        if entry.get('slot') == 'idle':
            carrier = carriers[entry['id']]['package']
            alternative['idle_blend_space'] = carrier+'.'+carrier.rsplit('/', 1)[1]
        entry['assets'].append(alternative)
    if len(catalog['entries']) != 63 or len(preserved) != 4:
        raise ValueError('Existing library identity inventory changed')
    normalized = copy.deepcopy(catalog)
    for entry in normalized['entries']:
        if entry['id'] in selected:
            entry['assets'].pop()
    if normalized != original:
        raise ValueError('Existing library fields changed')
    packages = {row['package'] for row in [*selected.values(), *carriers.values()]}
    packages.update((plan['compression'], precision['compression']['package']))
    if len(packages) != 68:
        raise ValueError('Unexpected cook package inventory')
    output.mkdir(parents=True)
    (output/'animation-library.json').write_text(json.dumps(catalog, indent=2)+'\n')
    (output/'packages.txt').write_text('\n'.join(sorted(packages))+'\n')
    (output/'verification.json').write_text(json.dumps(dict(passed=True, added_native=59,
        preserved_existing=preserved, carriers=7, cook_packages=68, inputs=inputs,
        scope='Editor-verified native catalog candidate. Cooking, packaging and live acceptance remain.'), indent=2)+'\n')


if __name__ == '__main__':
    main()
