"""Pin the shared-animation trial to the accepted outfit and core installation."""
import argparse
from pathlib import Path

from install_namespace_trial import (
    ROOT, GAME, EXPECTED_UE4SS, NAMES, SUFFIXES, atomic, check, check_closed,
    directory_hashes, install, mounts, package_matches, read, sha, validate_package_sets,
)


def plan(idle_carriers=False):
    work = ROOT / 'work/three-outfit-rig-physics'
    candidate = work / ('shared-cloth-native2' if idle_carriers else 'shared-cloth-native1')
    packages_receipt = work / 'genessa-cloth-assembly1/installed.json'
    core_receipt = work / 'genessa-form-first-stray1/installed.json'
    accepted = work / 'genessa-form-first-stray1/user-acceptance.json'
    evidence_paths = [packages_receipt, core_receipt, accepted,
        candidate / 'verification.json', candidate / 'host-validation.json',
        candidate / 'combined-imports.json']
    package_base, core_base = read(packages_receipt), read(core_receipt)
    if not package_base.get('installed') or not core_base.get('installed'):
        raise ValueError('Expected the accepted installed baseline')
    verified, host, imports = (read(p) for p in evidence_paths[3:])
    if not all(r.get('passed') is True for r in (verified, host, imports)):
        raise ValueError('Shared candidate verification did not pass')
    if (verified['packages'], verified['native_entries'], verified['added_cloth_packages']) != (269 if idle_carriers else 217, 63, 2):
        raise ValueError('Unexpected shared animation/cloth inventory')
    if len(imports['packages']) != (855 if idle_carriers else 803) or imports['failures']:
        raise ValueError('Combined dependency verification is incomplete')
    expected = {'native-combined': '416 animation library checks passed',
                'native-alone': '416 animation library checks passed',
                'outfit-regression': '1380 animation library checks passed'}
    if idle_carriers:
        expected = {k: v.replace('416', '422').replace('1380', '1386') for k, v in expected.items()}
    if {r['name']: r['output'].strip() for r in host['cases'] if r['exit_code'] == 0} != expected:
        raise ValueError('Catalog checks are incomplete')
    for case in host['cases']:
        check({case['command'][0]: host['binary_sha256']})
    check(verified['inputs'])
    check({str(candidate / 'CSS_SharedAssets_P' / name): digest
           for name, digest in verified['files'].items()})
    runtime = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    selector = read(runtime / 'core.json')
    if selector != core_base['selector'] or read(accepted)['dll_sha256'] != core_base['dll_sha256']:
        raise ValueError('Installed core differs from the accepted form repair')
    core = runtime / 'cores' / selector['file']
    ue4ss = runtime.parents[1] / 'UE4SS.dll'
    check({str(core): core_base['dll_sha256'], str(ue4ss): EXPECTED_UE4SS})
    baseline = package_base['package_files'] | package_base['retained_packages']
    core_source, core_sha, new_selector = core, core_base['dll_sha256'], selector.copy()
    if idle_carriers:
        previous = work / 'shared-cloth-native1'
        previous_receipt = read(previous/'installed.json')
        prior_plan = read(previous/'install-plan2.json')
        carrier_check = read(candidate/'carrier-verification.json')
        if (not previous_receipt.get('installed') or not carrier_check['passed'] or
                len(carrier_check['carriers']) != 52 or verified['added_idle_carriers'] != 52):
            raise ValueError('Idle carrier trial or installed baseline is incomplete')
        check(carrier_check['inputs'])
        baseline['CSS_SharedAssets_P'] = {n: r['sha256'] for n, r in prior_plan['new']['CSS_SharedAssets_P'].items()}
        correction = work/'native-idle-carrier-fix1'
        diagnosis = read(correction/'diagnosis.json')
        if diagnosis['host_suites_passed'] != 24:
            raise ValueError('Runtime regression checks are incomplete')
        core_source, core_sha = correction/'css_core.dll', diagnosis['candidate_core_sha256']
        check({str(core_source): core_sha})
        new_selector = dict(abi=selector['abi'], file='css_core-native-idles-'+core_sha[:16]+'.dll')
        evidence_paths += [previous/'installed.json', previous/'install-plan2.json',
                           candidate/'carrier-verification.json', correction/'diagnosis.json']
    if set(baseline) != set(NAMES):
        raise ValueError('Expected all four accepted packages')
    for name, hashes in baseline.items():
        package_matches(GAME / 'Content/Paks/~mods' / name, hashes)
    mounts(GAME, NAMES)
    name = 'CSS_SharedAssets_P'
    files = {name + s: dict(source=str(candidate / name / (name + s)),
             sha256=verified['files'][name + s], bytes=(candidate / name / (name + s)).stat().st_size)
             for s in SUFFIXES}
    result = dict(schema=2, game=str(GAME), old={name: baseline[name]}, new={name: files},
        retained={k: v for k, v in baseline.items() if k != name},
        previous_selector=selector, new_selector=new_selector,
        core_source=str(core_source), core_sha256=core_sha,
        protected={str(p): sha(p) for p in (ue4ss, core, runtime / 'dlls/main.dll')},
        protected_trees={str(p): directory_hashes(p) for p in (
            runtime / 'state', runtime.parent / 'CSSX/state', runtime.parent / 'CSSX/extensions/eins0fx.cine')},
        evidence={str(p): sha(p) for p in evidence_paths},
        stage_bytes=sum(row['bytes'] for row in files.values()) +
                    (2 * core_source.stat().st_size if idle_carriers else 0))
    validate_package_sets(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--transaction', type=Path)
    parser.add_argument('--idle-carriers', action='store_true', help='Install the 52 missing native idle carriers and matching parser.')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    if args.install:
        if args.transaction is None:
            parser.error('--install requires --transaction')
        check_closed()
    result = plan(args.idle_carriers)
    atomic(args.output, result)
    if args.install:
        install(result, args.transaction.resolve())
    print(args.output)


if __name__ == '__main__':
    main()
