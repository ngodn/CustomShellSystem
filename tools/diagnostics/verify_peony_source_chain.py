"""Connect retained Peony V2 authoring and cooks to the installed Eve. Python 3.14."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUTHORING = Path('/mnt/eins0fxE/CSS-Development')
EVE = 'CSS_EveStellarBlade_eins0fx_P'
PREFIX = 'MortalShell2/Content/CSS/eins0fx/EveSB/PeonyV2/'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--installed', type=Path, required=True,
                        help='Installed CSS_EveStellarBlade_eins0fx_P directory')
    parser.add_argument('--shared-candidate', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Use a new output file')
    inputs = {}
    historical_game_dependencies = []

    def pin(path, expected=None):
        path = Path(path).resolve(strict=True)
        key = str(path)
        if key not in inputs:
            inputs[key] = sha(path)
        require(expected is None or inputs[key] == expected, 'Hash differs: ' + key)
        return inputs[key]

    def load(path, expected=None):
        pin(path, expected)
        return json.loads(Path(path).read_text())

    def receipt(base, name):
        record = load(base / name)
        for relative, digest in record['evidence'].items():
            path = Path(relative)
            if (base.name == 'peony-v2-stockings1'
                    and path.parent == Path('cooked-readback1/containers')
                    and (path.name.startswith('pakchunk') or path.stem == 'global')):
                # These links follow game updates; they are not authored outfit sources.
                historical_game_dependencies.append(dict(
                    path=str(base / path), historical_sha256=digest,
                    current_target=str((base / path).resolve(strict=True))))
                continue
            pin(base / relative, digest)
        for name, value in record['recipes'].items():
            digest = value['sha256'] if isinstance(value, dict) else value
            frozen = 'recipes-material1' if base.name == 'peony-v2-stockings1' else 'recipes'
            pin(base / frozen / name, digest)
        return record

    pin(__file__)
    plan = load(args.candidate / 'plan.json')
    proof = load(args.candidate / 'verified.json')
    require(proof['passed'] and proof['plan_sha256'] == pin(args.candidate / 'plan.json'),
            'Combined candidate has no matching verification')
    installed = {}
    for suffix in ('.pak', '.utoc', '.ucas'):
        name = EVE + suffix
        installed[name] = pin(args.installed / name, proof['files'][EVE + '/' + name])

    character = AUTHORING / 'peony-v2-character2'
    receipts = receipt(character, 'stocking-integration-receipt.json')
    load(receipts['stocking_material_receipt'], receipts['stocking_material_receipt_sha256'])
    receipt(AUTHORING / 'peony-v2-stockings1', 'stocking-material1-receipt.json')
    receipt(AUTHORING / 'peony-v2-options1', 'completion-receipt.json')
    assembly = load(character / 'assembly.json')
    for path, digest in assembly['inputs'].items():
        pin(path, digest)
    pin(character / 'peony-character.mesh.json', assembly['output_sha256'])
    pin(character / 'recipes/append_peony_stockings.py', assembly['script_sha256'])
    for name in ('assembly-verified.json', 'bindings-reloaded.json',
                 'face-reloaded.json', 'cooked-character1/verified.json',
                 'cooked-body-deformation-verified.json',
                 'cooked-face-deformation-verified.json'):
        require(load(character / name)['passed'], 'Failed Peony evidence: ' + name)

    stages = []
    covered = {}
    for name in ('peony-v2-character2', 'peony-v2-options1'):
        base = AUTHORING / name
        authored = load(base / 'authored.json')
        cook = load(base / 'windows-cook1/inputs.json')
        require(load(base / 'windows-cook1/exit.json') ==
                {'returncode': 0, 'sources_unchanged': True}, 'Cook failed: ' + name)
        for path, digest in cook['files'].items():
            pin(path, digest)
        for package, digest in authored['packages'].items():
            source = base / 'editor/Content' / (package.removeprefix('/Game/') + '.uasset')
            require(cook['files'].get(str(source)) == digest,
                    'Authored asset differs from cook input: ' + package)
        readback = load(base / 'cooked-readback1/verified.json')
        require(readback['passed'], 'Cook readback failed: ' + name)
        payloads = {k: v for k, v in readback['payloads'].items() if k.startswith(PREFIX)}
        require(not covered.keys() & payloads.keys(), 'Duplicate Peony payload owner')
        for relative, digest in payloads.items():
            retained = plan['payloads'][relative]
            require(retained['sha256'] == digest, 'Candidate changed Peony: ' + relative)
            pin(retained['path'], digest)
        covered.update(payloads)
        stages.append(dict(name=name, editor_inputs=len(cook['files']),
                           authored_packages=len(authored['packages']),
                           peony_payloads=len(payloads), cook_command=cook['command']))
    require(set(covered) == {k for k in plan['payloads'] if k.startswith(PREFIX)},
            'Peony payload coverage incomplete')
    require(len(covered) == 294, 'Unexpected accepted Peony payload count')
    ghosts = None
    if args.shared_candidate:
        base = AUTHORING / 'peony-v2-astral1'
        receipt(base, 'completion-receipt.json')
        authored = load(base / 'authored.json')
        cook = load(base / 'windows-cook2/inputs.json')
        require(load(base / 'windows-cook2/exit.json') ==
                {'returncode': 0, 'sources_unchanged': True}, 'Ghost cook failed')
        require(load(base / 'reloaded.json')['passed'], 'Ghost editor reload failed')
        for path, digest in cook['files'].items():
            pin(path, digest)
        for package, digest in authored['packages'].items():
            source = base / 'editor/Content' / (package.removeprefix('/Game/') + '.uasset')
            require(cook['files'].get(str(source)) == digest,
                    'Ghost authoring/cook mismatch: ' + package)
        shared = load(args.shared_candidate / 'verified.json')
        require(shared['passed'], 'Shared candidate failed')
        for name, digest in shared['files'].items():
            pin(args.installed.parent / 'CSS_SharedAssets_P' / name, digest)
        original = load(ROOT / 'work/three-outfit-rig-physics/peony-v2-package3/assembly.json')
        original_files = original['files']
        # Later pre-boss shared changes were metadata-only; the original cooked
        # containers must be exactly those used by the boss integration baseline.
        for suffix in ('.utoc', '.ucas'):
            name = 'CSS_SharedAssets_P' + suffix
            original_matches = [digest for path, digest in original_files.items()
                                if Path(path).name == name]
            require(len(original_matches) == 1, 'Missing original shared identity')
            baseline_digest = proof['files']['CSS_SharedAssets_P/' + name]
            require(original_matches[0] == baseline_digest, 'Pre-boss shared payload changed')
            require(any(Path(path).name == name and digest == baseline_digest
                        for path, digest in shared['inputs'].items()),
                    'Boss integration did not use the accepted shared baseline')
        readback = load(base / 'cooked-readback2/verified.json')
        require(readback['passed'], 'Ghost cooked readback failed')
        exports = {k: v for k, v in readback['payloads'].items()
                   if '/Shared/Astral/Materials/' in k and 'PeonyV2' in k and k.endswith('.uexp')}
        require(len(exports) == 12, 'Expected six ghost parents and six culling instances')
        for path, digest in exports.items():
            require(original['payloads']['CSS_SharedAssets_P'][path] == digest
                    and shared['preserved_payloads'][path] == digest,
                    'Ghost export changed in package lineage: ' + path)
            source = base / 'windows-cook2/project/Saved/Cooked/Windows/CSSShared/Content'
            source /= path.removeprefix('MortalShell2/Content/')
            pin(source, digest)
        ghosts = dict(editor_packages=len(authored['packages']),
                      editor_inputs=len(cook['files']), exports=exports,
                      original_shared_container_identity_preserved=True,
                      current_shared_files=shared['files'],
                      header_scope='IoStore reconstructs headers. Existing boss-integration '
                                   'proof preserves all 283 baseline packages; this check '
                                   'links that exact baseline to the accepted Peony assembly.')
    for path, digest in inputs.items():
        require(sha(path) == digest, 'Input changed during audit: ' + path)
    report = dict(passed=True, installed=installed, stages=stages,
                  payloads=covered, inputs=inputs,
                  historical_game_dependencies_not_revalidated=historical_game_dependencies,
                  shared_ghost_sources=ghosts,
                  scope='Retained Peony assembly inputs, frozen recipes, editor sources and successful '
                        'cooks linked to all 294 PeonyV2 payloads in the installed Eve package. '
                        'Optional shared ghost lineage is recorded separately. '
                        'No fresh cook, original archive replay or new live acceptance.')
    with args.output.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(passed=True, retained_files=len(inputs), peony_payloads=len(covered))))


if __name__ == '__main__':
    main()
