"""Replace the recorded private trial with the verified body and facial candidate."""
import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha

NAME = 'CSS_LocomotionSharedAssets_P'


def require_closed():
    if processes():
        raise RuntimeError('Game is running; close it before replacing the trial')


def check_hashes(files):
    for path, expected in files.items():
        if sha(Path(path)) != expected:
            raise RuntimeError('File changed: ' + str(path))


def update(game, previous, pack, receipt, core, cine_dll, menu, delta_report):
    require_closed()
    old = json.loads((previous / 'deployment.json').read_text())
    verified = json.loads((pack / 'verification.json').read_text())
    faces = json.loads((pack / 'facial-independent-readback.json').read_text())
    deltas = json.loads(delta_report.read_text())
    expected_faces = set(verified.get('facial_meshes', []))
    if (verified.get('passed') is not True or verified.get('exact_export_payloads') != 68
            or len(expected_faces) != 6 or faces.get('passed') is not True or len(faces['results']) != 6
            or {r['target'] for r in faces['results']} != expected_faces
            or deltas.get('passed') is not True or len(deltas['results']) != 6
            or deltas.get('source_deltas_checked') is not True
            or {r['target'] for r in deltas['results']} != expected_faces):
        raise ValueError('Expected the complete verified facial package and source comparison')
    for row in deltas['results']:
        comparison = row['source_delta_comparison']
        if (comparison.get('passed') is not True or len(comparison['shapes']) != 99
                or len(comparison['expressions']) != 21):
            raise ValueError('Incomplete facial delta comparison')
    files = verified['files']
    if set(files) != {NAME + suffix for suffix in ('.pak', '.utoc', '.ucas')}:
        raise ValueError('Unexpected package files')
    mod = game / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    cine = game / 'Binaries/Win64/ue4ss/Mods/CSSX/extensions/eins0fx.cine'
    target = game / 'Content/Paks/~mods' / NAME
    if not old.get('installed') or Path(old['asset_directory']).resolve() != target.resolve():
        raise ValueError('Previous receipt does not identify this installed trial')
    selector = json.loads((mod / 'core.json').read_text())
    extension = json.loads((cine / 'extension.json').read_text())
    if (selector != {**old['previous_core'], 'file': old['core']}
            or extension != {**old['previous_cine'], 'entry': old['cine']}):
        raise ValueError('Installed selectors differ from the previous receipt')
    for filename in (old['core'], old['cine']):
        if Path(filename).name != filename:
            raise ValueError('Invalid DLL basename in previous receipt')
    if target.is_symlink() or {p.name for p in target.iterdir()} != set(old['assets']):
        raise ValueError('Unexpected installed shared asset directory')
    protected = {
        str(game / 'Binaries/Win64/ue4ss/UE4SS.dll'): EXPECTED_UE4SS,
        str(mod / 'cores' / old['core']): old['core_sha256'],
        str(cine / old['cine']): old['cine_sha256'],
        **{str(target / name): digest for name, digest in old['assets'].items()},
    }
    check_hashes(protected)
    if sha(cine / 'menu.json') != sha(previous / 'cine-menu.json'):
        raise ValueError('Installed CINE menu differs from the previous trial')
    selectors = {str(p): sha(p) for p in
                 (mod / 'core.json', cine / 'extension.json', cine / 'menu.json')}
    inputs = {str(pack / NAME / name): digest for name, digest in files.items()}
    inputs.update(verified['dependencies'])
    inputs.update(verified['facial_evidence'])
    for source in (core, cine_dll, menu, delta_report, previous / 'deployment.json',
                   pack / 'verification.json', pack / 'facial-independent-readback.json'):
        inputs[str(source)] = sha(source)
    check_hashes(inputs)
    for directory in (mod / 'state', cine.parents[1] / 'state'):
        protected.update({str(p): sha(p) for p in directory.rglob('*') if p.is_file()})
    protected[str(mod / 'dlls/main.dll')] = sha(mod / 'dlls/main.dll')
    core_hash, cine_hash = sha(core), sha(cine_dll)
    core_name, cine_name = 'css_core-facial-' + core_hash[:16] + '.dll', 'cine-facial-' + cine_hash[:16] + '.dll'
    stage = game / 'Content' / (NAME + '-facial-stage-' + core_hash[:16])
    rollback = game / 'Content' / (NAME + '-rollback-' + core_hash[:16])
    for path in (receipt, stage, rollback, mod / 'cores' / core_name, cine / cine_name):
        if path.exists():
            raise FileExistsError('Inspect existing candidate or receipt before retrying: ' + str(path))
    require_closed()
    receipt.mkdir(parents=True)
    backup = receipt / 'backup'
    for source, name in ((mod / 'core.json', 'core.json'), (cine / 'extension.json', 'cine-extension.json'),
                         (cine / 'menu.json', 'cine-menu.json')):
        copy_verified(source, backup / name)
    old_moved = new_moved = metadata_changed = False
    try:
        stage.mkdir()
        for name in files:
            copy_verified(pack / NAME / name, stage / name)
        copy_verified(core, receipt / core_name)
        copy_verified(cine_dll, receipt / cine_name)
        copy_verified(menu, receipt / 'cine-menu.json')
        copy_verified(receipt / core_name, mod / 'cores' / core_name)
        copy_verified(receipt / cine_name, cine / cine_name)
        check_hashes(inputs)
        check_hashes(protected)
        check_hashes(selectors)
        require_closed()
        target.rename(rollback)
        old_moved = True
        require_closed()
        stage.rename(target)
        new_moved = True
        require_closed()
        metadata_changed = True
        copy_verified(receipt / 'cine-menu.json', cine / 'menu.json')
        require_closed()
        atomic(cine / 'extension.json', {**extension, 'entry': cine_name})
        require_closed()
        atomic(mod / 'core.json', {**selector, 'file': core_name})
        unchanged = {p: digest for p, digest in protected.items() if not Path(p).is_relative_to(target)}
        check_hashes(unchanged)
        check_hashes({str(target / name): digest for name, digest in files.items()})
        check_hashes({str(mod / 'cores' / core_name): core_hash, str(cine / cine_name): cine_hash,
                      str(cine / 'menu.json'): sha(receipt / 'cine-menu.json')})
        require_closed()
        atomic(receipt / 'deployment.json', {
            'installed': True, 'live_verified': False, 'game_was_closed': True,
            'core': core_name, 'core_sha256': core_hash, 'previous_core': selector,
            'cine': cine_name, 'cine_sha256': cine_hash, 'previous_cine': extension,
            'assets': files, 'asset_directory': str(target), 'asset_rollback_directory': str(rollback),
            'previous_receipt': str(previous), 'protected_hashes': unchanged, 'inputs': inputs,
            'note': 'Private facial/body trial. No game launch, reload, input or recording.'})
    except Exception as error:
        if processes():
            atomic(receipt / 'install-error.json', {'error': str(error), 'old_assets_moved': old_moved,
                'new_assets_moved': new_moved, 'rollback_deferred': True,
                'note': 'Game opened during installation. Close it before restoring the backup.'})
            raise
        if metadata_changed:
            for name, destination in (('core.json', mod / 'core.json'),
                                      ('cine-extension.json', cine / 'extension.json'),
                                      ('cine-menu.json', cine / 'menu.json')):
                copy_verified(backup / name, destination)
        if new_moved:
            target.rename(stage)
        if old_moved:
            rollback.rename(target)
        if stage.exists():
            shutil.rmtree(stage)
        (mod / 'cores' / core_name).unlink(missing_ok=True)
        (cine / cine_name).unlink(missing_ok=True)
        atomic(receipt / 'install-error.json', {'error': str(error), 'rolled_back': True})
        raise
    return receipt / 'deployment.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('previous', 'pack', 'receipt', 'delta_report'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    result = update(GAME, args.previous.resolve(strict=True), args.pack.resolve(strict=True),
                    args.receipt.resolve(), ROOT / 'build/windows/css_core.dll',
                    ROOT / 'build/cine-cssx-win/cine.dll', ROOT / 'extensions/cine/menu.json',
                    args.delta_report.resolve(strict=True))
    print(result)


if __name__ == '__main__':
    main()
