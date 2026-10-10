"""Install the verified private Peony/core trial with external rollback copies."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import time

CSS = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(CSS / 'tools'))
from css import GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha

NAMES = ('CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P')


def closed():
    if processes():
        raise RuntimeError('Close Mortal Shell II before replacing mounted assets or the core')


def check(files):
    for path, expected in files.items():
        if sha(Path(path)) != expected:
            raise ValueError('Changed file: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('trial', 'backup', 'core'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--install', action='store_true')
    args = parser.parse_args()
    trial, core = args.trial.resolve(strict=True), args.core.resolve(strict=True)
    proof = json.loads((trial / 'assembly.json').read_text())
    if not proof['passed'] or proof['installed']:
        raise ValueError('Expected a verified, uninstalled trial')
    if (trial / 'deployment.json').exists():
        raise FileExistsError(trial / 'deployment.json')
    check({str(trial / 'pack' / p): digest for p, digest in proof['files'].items()})
    if len(proof['asset_owners']) != 642:
        raise ValueError('Unexpected Eve/shared asset inventory')
    for name in NAMES:
        if {p.name for p in (trial / 'pack' / name).iterdir()} != {name + s for s in ('.pak', '.utoc', '.ucas')}:
            raise ValueError('Unexpected candidate package contents')
    runtime_inputs = json.loads((CSS / 'work/three-outfit-rig-physics/peony-astral-runtime1/source-identities.json').read_text())
    check({str(CSS / name): digest for name, digest in runtime_inputs.items() if name.startswith('native/')})
    # Pins the core whose successful build is recorded with the material audit.
    expected_core = '6d751cbdab2cab4c9bfd9ea4420c3cbca1706fbf9c44a66a27f8b2e8299a3dd0'
    if sha(core) != expected_core:
        raise ValueError('Core differs from the checked Peony build')
    ue4ss = GAME / 'Binaries/Win64/ue4ss'
    mod = ue4ss / 'Mods/CustomShellSystem'
    selector_path = mod / 'core.json'
    selector = json.loads(selector_path.read_text())
    if Path(selector['file']).name != selector['file']:
        raise ValueError('Invalid installed core filename')
    contract = json.loads((mod / 'loader-contract.json').read_text())
    if contract['abi'] != 2:
        raise ValueError('Expected the installed ABI 2 loader')
    loader_hash = contract['dll_sha256']
    if sha(mod / 'dlls/main.dll') != loader_hash:
        # This deployment predates the current core. Its loader was retained
        # through the accepted native-idle and CW trials, but the contract file
        # still names an older build. Keep that verified loader unchanged.
        earlier = json.loads((CSS / 'work/three-outfit-rig-physics/shared-cloth-native2/installed.json').read_text())
        if not earlier['installed']:
            raise ValueError('No verified installation record for the retained loader')
        loader_hash = earlier['files'][str(mod / 'dlls/main.dll')]
        check({str(CSS / p): h for p, h in contract['sources'].items()})
    unchanged = {str(ue4ss / 'UE4SS.dll'): EXPECTED_UE4SS,
                 str(mod / 'dlls/main.dll'): loader_hash,
                 str(mod / 'loader-contract.json'): sha(mod / 'loader-contract.json'),
                 str(selector_path): sha(selector_path),
                 str(mod / 'cores' / selector['file']): sha(mod / 'cores' / selector['file'])}
    targets = GAME / 'Content/Paks/~mods'
    for name in NAMES:
        directory = targets / name
        if directory.is_symlink() or {p.name for p in directory.iterdir()} != {name + s for s in ('.pak', '.utoc', '.ucas')}:
            raise ValueError('Unexpected installed package contents')
        for path in directory.iterdir():
            unchanged[str(path)] = proof['inputs'][str(path.resolve())]
    check(unchanged)
    closed()
    if not args.install:
        print('Peony trial, old packages, core and loader identities verified; not installed')
        return
    backup = args.backup.resolve()
    external = Path('/run/media/eins0fx/theark')
    if not os.path.ismount(external) or not backup.is_relative_to(external / 'CSS'):
        raise ValueError('The requested external CSS backup drive must be mounted')
    backup.mkdir(parents=True, exist_ok=False)
    for name in NAMES:
        for path in (targets / name).iterdir():
            copy_verified(path, backup / name / path.name)
    copy_verified(selector_path, backup / 'core.json')
    copy_verified(mod / 'cores' / selector['file'], backup / selector['file'])
    atomic(backup / 'receipt.json', dict(previous_core=selector, installed_files=unchanged,
        trial=str(trial), files={str(p.relative_to(backup)): sha(p) for p in backup.rglob('*') if p.is_file()}))
    transaction = GAME / 'Content' / ('CSS-Peony-swap-' + str(time.time_ns()))
    transaction.mkdir()
    for name in NAMES:
        shutil.copytree(trial / 'pack' / name, transaction / 'new' / name)
        check({str(transaction / 'new' / name / (name + s)): proof['files'][name + '/' + name + s]
               for s in ('.pak', '.utoc', '.ucas')})
    (transaction / 'old').mkdir()
    core_name = 'css_core-peony-' + expected_core[:16] + '.dll'
    core_target = mod / 'cores' / core_name
    if core_target.exists() and sha(core_target) != expected_core:
        raise ValueError('Core staging filename is occupied')
    copy_verified(core, core_target)
    retired, published = [], []
    try:
        closed()
        check(unchanged)
        for name in NAMES:
            closed()
            (targets / name).rename(transaction / 'old' / name)
            retired.append(name)
            (transaction / 'new' / name).rename(targets / name)
            published.append(name)
        atomic(selector_path, {**selector, 'file': core_name})
        installed = {str(targets / name / (name + s)): proof['files'][name + '/' + name + s]
                     for name in NAMES for s in ('.pak', '.utoc', '.ucas')}
        installed[str(core_target)] = expected_core
        check(installed)
        if json.loads(selector_path.read_text()) != {**selector, 'file': core_name}:
            raise ValueError('Core selection did not persist')
        atomic(trial / 'deployment.json', dict(installed=True, assets=installed, core=core_name,
            previous_core=selector, backup=str(backup), pending=proof['pending'],
            activation='Next normal launch; no input or camera automation',
            core_selector_sha256=sha(selector_path)))
    except BaseException:
        atomic(selector_path, selector)
        for name in reversed(published):
            (targets / name).rename(transaction / 'new' / name)
        for name in reversed(retired):
            (transaction / 'old' / name).rename(targets / name)
        raise
    shutil.rmtree(transaction)
    print('Peony trial installed and hashes verified. Previous packages/core: ' + str(backup))


if __name__ == '__main__':
    main()
