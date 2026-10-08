"""Stage the coordinated author-namespace trial or restore its recorded baseline. Python 3.14."""
import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha

NAMES = ('CSS_EveStellarBlade_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P',
         'CSS_CommanderWhite_eins0fx_P', 'CSS_SharedAssets_P')
RETIRED = ('CSS_AstralSharedAssets_P', 'CSS_LocomotionSharedAssets_P')
SUFFIXES = ('.pak', '.utoc', '.ucas')


def read(path):
    return json.loads(path.read_text())


def check(files):
    for name, digest in files.items():
        if sha(Path(name)) != digest:
            raise ValueError('File changed: ' + name)


def directory_hashes(path):
    if path.is_symlink():
        raise ValueError('Unexpected linked installation directory: ' + str(path))
    return {str(p): sha(p) for p in path.rglob('*') if p.is_file()}


def check_closed():
    if processes():
        raise RuntimeError('Game is running; close it before changing the trial')


def mounts(game, expected):
    paks = game / 'Content/Paks'
    for name in NAMES + RETIRED:
        actual = {str(p) for p in paks.rglob(name + '.*') if p.suffix in SUFFIXES}
        wanted = {str(paks / '~mods' / name / (name + s)) for s in SUFFIXES} if name in expected else set()
        if actual != wanted:
            raise ValueError('Missing or duplicate mounted container: ' + name)


def plan_upgrade(baseline, catalog, core_report, game=GAME):
    game = game.resolve(strict=True)
    old, receipt, validation, core = (read(p) for p in
        (baseline, catalog / 'receipt.json', catalog / 'validation.json', core_report))
    if not all(p.get('passed') is True for p in (receipt, validation, core)):
        raise ValueError('Candidate verification did not pass')
    if set(receipt['container_ids']) != set(NAMES) or len(set(receipt['container_ids'].values())) != len(NAMES):
        raise ValueError('Candidate container identities are incomplete or duplicated')
    if validation.get('catalog_checks') != 1380 or validation.get('dependency_scenarios') != 28:
        raise ValueError('Expected the audited catalog and dependency checks')
    check({str(ROOT / p): h for p, h in validation['inputs'].items()})
    if receipt['inputs'].get(str(baseline.resolve())) != sha(baseline):
        raise ValueError('Candidate does not bind the installed baseline')
    runtime = game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    selector = read(runtime / 'core.json')
    if selector != old['selector'] or selector.get('abi') != 1 or Path(selector['file']).name != selector['file']:
        raise ValueError('Installed core selector differs from baseline')
    previous_core = runtime / 'cores' / selector['file']
    check({str(previous_core): old['core']['sha256']})
    source_core = Path(core['core']['staged_path']).resolve(strict=True)
    check({str(source_core): core['core']['sha256']})
    check({str(ROOT / p): h for p, h in core['inputs'].items()})
    ue4ss = game / 'Binaries/Win64/ue4ss/UE4SS.dll'
    check({str(ue4ss): EXPECTED_UE4SS})
    old_files = {}
    for row in old['packages']:
        name = row['container']
        if name not in NAMES + RETIRED or name in old_files or set(row['files']) != set(SUFFIXES):
            raise ValueError('Unexpected baseline package set')
        target = game / 'Content/Paks/~mods' / name
        expected = {name + s: row['files'][s]['sha256'] for s in SUFFIXES}
        if target.is_symlink() or {p.name for p in target.iterdir()} != set(expected):
            raise ValueError('Unexpected installed package contents: ' + name)
        check({str(target / n): h for n, h in expected.items()})
        old_files[name] = expected
    if set(old_files) != set(NAMES + RETIRED):
        raise ValueError('Expected all six baseline packages')
    mounts(game, old_files)
    candidates = {}
    for name in NAMES:
        candidates[name] = {}
        for suffix in SUFFIXES:
            source = (catalog / 'pack' / (name + suffix)).resolve(strict=True)
            digest = receipt['files'].get(source.name) if suffix == '.pak' else receipt['inputs'].get(str(source))
            if not digest:
                raise ValueError('Candidate file has no recorded hash: ' + str(source))
            check({str(source): digest})
            candidates[name][name + suffix] = {'source': str(source), 'sha256': digest, 'bytes': source.stat().st_size}
    protected = {str(p): sha(p) for p in (ue4ss, previous_core, runtime / 'dlls/main.dll')}
    protected_dirs = [runtime / 'state', runtime.parent / 'CSSX/state',
                      runtime.parent / 'CSSX/extensions/eins0fx.cine']
    trees = {str(p): directory_hashes(p) for p in protected_dirs}
    new_name = 'css_core-namespace-' + core['core']['sha256'][:16] + '.dll'
    if (runtime / 'cores' / new_name).exists():
        raise FileExistsError('Candidate core already exists; inspect the prior transaction')
    evidence = {str(p.resolve()): sha(p) for p in (baseline, catalog / 'receipt.json', catalog / 'validation.json', core_report)}
    return {'schema': 1, 'game': str(game), 'old': old_files, 'new': candidates,
            'previous_selector': selector, 'new_selector': {'abi': 1, 'file': new_name},
            'core_source': str(source_core), 'core_sha256': core['core']['sha256'],
            'protected': protected, 'protected_trees': trees, 'evidence': evidence,
            'stage_bytes': sum(f['bytes'] for files in candidates.values() for f in files.values()) + source_core.stat().st_size}


def protected_unchanged(plan):
    check(plan['protected'])
    retained_unchanged(plan)
    for name, hashes in plan['protected_trees'].items():
        if directory_hashes(Path(name)) != hashes:
            raise ValueError('Protected state or CINE files changed: ' + name)


def retained_unchanged(plan):
    for name, hashes in plan.get('retained', {}).items():
        package_matches(Path(plan['game']) / 'Content/Paks/~mods' / name, hashes)


def validate_package_sets(plan):
    old, new, retained = (set(plan.get(k, {})) for k in ('old', 'new', 'retained'))
    initial = old == set(NAMES + RETIRED) and new == set(NAMES) and not retained
    followup = bool(old) and old == new and not old & retained and old | retained == set(NAMES)
    if not (initial or followup):
        raise ValueError('Unexpected replacement or retained package set')


def mounted_set(plan, phase):
    return set(plan[phase]) | set(plan.get('retained', {}))


def package_matches(directory, hashes):
    if directory.is_symlink() or {p.name for p in directory.iterdir()} != set(hashes):
        raise ValueError('Unexpected package contents: ' + str(directory))
    check({str(directory / name): digest for name, digest in hashes.items()})


def restore(transaction, closed=check_closed):
    closed()
    plan = read(transaction / 'plan.json')
    game = Path(plan['game'])
    validate_package_sets(plan)
    if transaction.parent.resolve() != (game / 'Content').resolve():
        raise ValueError('Unexpected rollback location or package set')
    runtime = game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    selector = read(runtime / 'core.json')
    if selector not in (plan['previous_selector'], plan['new_selector']):
        raise ValueError('Core selector changed since this transaction')
    check(plan['protected'])
    retained_unchanged(plan)
    for name, hashes in plan['old'].items():
        target = game / 'Content/Paks/~mods' / name
        backup = transaction / 'backup' / name
        if not backup.exists():
            package_matches(target, hashes)
            continue
        package_matches(backup, hashes)
        if target.exists():
            package_matches(target, {n: r['sha256'] for n, r in plan['new'][name].items()})
            rejected = transaction / 'candidate' / name
            if rejected.exists():
                raise FileExistsError(rejected)
            closed()
            target.rename(rejected)
        closed()
        backup.rename(target)
    closed()
    atomic(runtime / 'core.json', plan['previous_selector'])
    mounts(game, mounted_set(plan, 'old'))
    atomic(transaction / 'result.json', {'installed': False, 'restored': True, 'live_verified': False})


def install(plan, transaction, closed=check_closed, checkpoint=lambda _: None):
    closed()
    validate_package_sets(plan)
    game = Path(plan['game'])
    if transaction.parent.resolve() != (game / 'Content').resolve():
        raise ValueError('Stage and rollback must be outside Paks on the game filesystem')
    if transaction.exists():
        raise FileExistsError(transaction)
    if shutil.disk_usage(game / 'Content').free < plan['stage_bytes'] + 256 * 1024**2:
        raise ValueError('Insufficient space for independent candidate copies and reserve')
    mounts(game, mounted_set(plan, 'old'))
    protected_unchanged(plan)
    check(plan['evidence'])
    runtime = game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    if read(runtime / 'core.json') != plan['previous_selector']:
        raise ValueError('Core selector changed before staging')
    for name, hashes in plan['old'].items():
        package_matches(game / 'Content/Paks/~mods' / name, hashes)
    transaction.mkdir()
    atomic(transaction / 'plan.json', plan)
    (transaction / 'backup').mkdir()
    try:
        for name, files in plan['new'].items():
            for filename, row in files.items():
                closed()
                check({row['source']: row['sha256']})
                copy_verified(Path(row['source']), transaction / 'candidate' / name / filename)
        copy_verified(Path(plan['core_source']), transaction / 'css_core.dll')
        check({str(transaction / 'css_core.dll'): plan['core_sha256']})
        checkpoint('staged')
        protected_unchanged(plan)
        check(plan['evidence'])
        mounts(game, mounted_set(plan, 'old'))
        if read(runtime / 'core.json') != plan['previous_selector']:
            raise ValueError('Core selector changed during staging')
        for name, hashes in plan['old'].items():
            target = game / 'Content/Paks/~mods' / name
            package_matches(target, hashes)
            closed()
            target.rename(transaction / 'backup' / name)
            checkpoint('retired:' + name)
        for name, files in plan['new'].items():
            staged = transaction / 'candidate' / name
            package_matches(staged, {n: r['sha256'] for n, r in files.items()})
            closed()
            staged.rename(game / 'Content/Paks/~mods' / name)
            checkpoint('mounted:' + name)
        closed()
        core_target = runtime / 'cores' / plan['new_selector']['file']
        if core_target.exists():
            raise FileExistsError('Candidate core appeared during staging: ' + str(core_target))
        copy_verified(transaction / 'css_core.dll', core_target)
        closed()
        atomic(runtime / 'core.json', plan['new_selector'])
        checkpoint('selected')
        protected_unchanged(plan)
        mounts(game, mounted_set(plan, 'new'))
        closed()
        atomic(transaction / 'result.json', {'installed': True, 'live_verified': False,
               'core_sha256': plan['core_sha256'], 'note': 'Full-cloth and saved settings preserved. Normal restart required.'})
    except Exception as error:
        atomic(transaction / 'error.json', {'error': str(error), 'rollback_required': True})
        restore(transaction, closed)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('plan', 'install', 'restore'))
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--catalog', type=Path)
    parser.add_argument('--core-report', type=Path)
    parser.add_argument('--transaction', type=Path, required=True)
    args = parser.parse_args()
    transaction = args.transaction.resolve()
    if args.action == 'restore':
        restore(transaction)
        return
    if not all((args.baseline, args.catalog, args.core_report)):
        parser.error('plan/install require --baseline, --catalog and --core-report')
    plan = plan_upgrade(args.baseline.resolve(), args.catalog.resolve(), args.core_report.resolve())
    if args.action == 'plan':
        print(json.dumps(plan, indent=2))
    else:
        install(plan, transaction)
        print(transaction / 'result.json')


if __name__ == '__main__':
    main()
