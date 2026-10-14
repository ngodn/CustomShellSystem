"""Plan or install the verified Eve mesh package using the existing rollback transaction.

Python 3.14. Plan is read-only for the game. Install requires a closed game and
unchanged package, core and saved-state fingerprints.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME, EXPECTED_UE4SS, sha, atomic

INSTALLER = ROOT/'tools/asset-paths/install_namespace_trial.py'
spec = importlib.util.spec_from_file_location('namespace_installer', INSTALLER)
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)
EVE, SHARED = 'CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P'
CW = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P'


def read(path):
    return json.loads(path.read_text())


def require(condition, message):
    if not condition:
        raise ValueError(message)


def prepare(out, transaction):
    require(not transaction.exists(), 'Transaction already exists')
    require(transaction.parent == (GAME/'Content').resolve(), 'Rollback must be outside Paks on the game filesystem')
    evidence = [out/n for n in ('verified.json', 'plan.json', 'packaged.json', 'customization.json')]
    evidence += [INSTALLER, ROOT/'tools/css.py', Path(__file__),
                 CW/'work/walk-final-paths1/assembly/installed.json']
    verified, source_plan, package, custom = [read(p) for p in evidence[:4]]
    require(all(x['passed'] for x in (verified, package, custom)), 'Candidate checks have not passed')
    require(verified['plan_sha256'] == package['plan_sha256'] == sha(out/'plan.json'), 'Candidate receipts differ')
    require(verified['files'] == package['files'] == custom['files'], 'Candidate package identities differ')
    installer.check(source_plan['protected'])
    installer.check(custom['inputs'])
    runtime = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    accepted = read(evidence[-1])
    selector = read(runtime/'core.json')
    require(accepted['installed'] and selector == accepted['core'], 'Core differs from accepted CW walking build')
    require(selector['abi'] == 1 and Path(selector['file']).name == selector['file'], 'Invalid core selector')
    core = runtime/'cores'/selector['file']
    installer.check({str(core): accepted['core_sha256']})
    old, new, retained = {}, {}, {}
    for owner in installer.NAMES:
        target = GAME/'Content/Paks/~mods'/owner
        hashes = {owner+s: source_plan['protected'][str(target/(owner+s))] for s in installer.SUFFIXES}
        installer.package_matches(target, hashes)
        for name in hashes:
            installer.check({str(out/'pack'/owner/name): verified['files'][owner+'/'+name]})
        if owner not in (EVE, SHARED):
            require(all(verified['files'][owner+'/'+name] == h for name, h in hashes.items()), 'Retained outfit changed')
            retained[owner] = hashes
            continue
        old[owner] = hashes
        new[owner] = {name: dict(source=str(out/'pack'/owner/name),
                                sha256=verified['files'][owner+'/'+name],
                                bytes=(out/'pack'/owner/name).stat().st_size) for name in hashes}
    changed = sorted(owner+'/'+name for owner, files in new.items()
                     for name, row in files.items() if old[owner][name] != row['sha256'])
    expected = sorted([EVE+'/'+EVE+s for s in installer.SUFFIXES]+[SHARED+'/'+SHARED+'.pak'])
    require(changed == expected, 'Changes extend beyond Eve trio and shared metadata')
    protected = {str(core): accepted['core_sha256'], str(runtime/'dlls/main.dll'): sha(runtime/'dlls/main.dll'),
                 str(GAME/'Binaries/Win64/ue4ss/UE4SS.dll'): EXPECTED_UE4SS}
    trees = {str(p): installer.directory_hashes(p) for p in
             (runtime/'state', runtime.parent/'CSSX/state', runtime.parent/'CSSX/extensions/eins0fx.cine')}
    plan = dict(schema=1, game=str(GAME), old=old, new=new, retained=retained,
                previous_selector=selector, new_selector=selector, core_source=str(core),
                core_sha256=accepted['core_sha256'], protected=protected, protected_trees=trees,
                evidence={str(p.resolve()): sha(p) for p in evidence},
                stage_bytes=sum(r['bytes'] for files in new.values() for r in files.values()),
                transaction=str(transaction), changed_files=changed,
                scope='Eve mesh repairs and shared Eve container fingerprints. Preserve accepted CW/UG, core and saved settings.')
    installer.validate_package_sets(plan)
    installer.protected_unchanged(plan)
    installer.mounts(GAME, set(installer.NAMES))
    require(shutil.disk_usage(GAME/'Content').free >= plan['stage_bytes']+256*1024**2,
            'Insufficient game-drive space for staging and reserve')
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('plan', 'install'))
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--transaction', type=Path, required=True)
    args = parser.parse_args()
    out, transaction = args.candidate.resolve(strict=True), args.transaction.resolve()
    path = out/'installation-plan.json'
    if args.action == 'plan':
        require(not path.exists(), 'Installation plan exists; preserve it before preparing a new baseline')
        plan = prepare(out, transaction)
        atomic(path, plan)
        print('Four changed files planned; game files unchanged. Rollback transaction will retain both previous package trios.')
        return
    plan = read(path)
    require(plan['transaction'] == str(transaction), 'Transaction differs from the prepared plan')
    require(not (out/'deployment.json').exists(), 'Candidate already has a deployment record')
    require(plan == prepare(out, transaction), 'Installation inputs or saved state changed; preserve and refresh the plan')
    installer.install(plan, transaction)
    for owner, files in plan['new'].items():
        installer.package_matches(GAME/'Content/Paks/~mods'/owner, {p:r['sha256'] for p,r in files.items()})
    installer.protected_unchanged(plan)
    require(read(GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem/core.json') == plan['previous_selector'], 'Core selector changed')
    atomic(out/'deployment.json', dict(installed=True, live_verified=False, transaction=str(transaction),
        plan_sha256=sha(path), changed_files=plan['changed_files'], core_unchanged=True,
        saved_state_preserved=True, activation='Next game launch'))
    print('Eve surface candidate installed and hash-verified; gameplay checks remain.')


if __name__ == '__main__':
    main()
