"""Replay both Genessa facial packets and pin their retained authoring inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
AUTHOR = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P'
WORK = ROOT / 'work/three-outfit-rig-physics'


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() or not out.is_relative_to(WORK.resolve()) or out == WORK.resolve():
        raise ValueError('Use a new directory beneath the goal workspace')
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError(f'Input changed: {path}')
        protected[str(path)] = digest

    packets = AUTHOR / 'work/face-integration-input3'
    integration = AUTHOR / 'work/face-integration6'
    packet_report = read(packets / 'report.json')
    mapping_path = AUTHOR / 'work/face-mapping1/mapping.json'
    mapping = read(mapping_path)
    for table in (packet_report['inputs'], mapping['inputs']):
        for path, digest in table.items():
            protect(AUTHOR / path, digest)
    specs = read(integration / 'input.json')['meshes']
    built = read(integration / 'report.json')
    assert built['source_files_unchanged'] and read(integration / 'exit.json')['exit_code'] == 0
    assert len(specs) == len(built['meshes']) == 2
    assert {r['package'].rsplit('/', 1)[-1] for r in specs} == {'SK_EveW3', 'SK_GenessaW3'}
    for spec, row in zip(specs, built['meshes'], strict=True):
        assert all(row[k] == v for k, v in spec.items()) and row['result'].startswith('PASS:')
        source = next(r for r in mapping['meshes'] if r['mesh'] == spec['package'])
        assert source['source_file'] == spec['source_file'] and source['source_sha256'] == spec['sha256']
        protect(Path(spec['source_file']), spec['sha256'])
        protect(Path(spec['patch']), spec['patch_sha256'])
    for path, digest in read(integration / 'module-inputs.json').items():
        protect(Path(path), digest)
    for name in ('input.json', 'module-inputs.json', 'report.json', 'exit.json', 'command.json',
                 'recipe.py', 'integrate_face_candidate.py', 'audit_face_render.py'):
        protect(integration / name)
    protect(packets / 'recipe.py')
    protect(packets / 'report.json')
    protect(AUTHOR / 'tools/prepare_face_integration.py', sha(packets / 'recipe.py'))

    installed_evidence = WORK / 'genessa-current-facial-cook1'
    assert read(installed_evidence / 'verified.json')['passed']
    archived_inputs = read(installed_evidence / 'inputs.json')
    ug_inputs = {p: h for p, h in archived_inputs.items()
                 if '/~mods/CSS_UnholyGenessa_eins0fx_P/' in p}
    assert len(ug_inputs) == 3
    for path, digest in ug_inputs.items():
        protect(Path(path), digest)
    for path, digest in archived_inputs.items():
        if str(integration) + '/' in path:
            protect(Path(path), digest)
    protect(installed_evidence / 'inputs.json')
    protect(installed_evidence / 'verified.json')
    verifier = AUTHOR / 'tools/verify_face_cook.py'
    protect(verifier, archived_inputs[str(verifier)])

    command = [sys.executable, str(packets / 'recipe.py'), '--mapping', 'work/face-mapping1/mapping.json',
               '--patch', 'work/face-patch1/head-patch.json', '--audit', 'work/face-mesh-audit1',
               '--output', str(out / 'replay')]
    (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (out / 'replay.log').open('x') as log:
        result = subprocess.run(command, cwd=AUTHOR, stdout=log, stderr=subprocess.STDOUT)
    (out / 'replay-exit.json').write_text(json.dumps({'exit_code': result.returncode}) + '\n')
    result.check_returncode()
    regenerated = read(out / 'replay/report.json')
    assert regenerated == packet_report
    for row in regenerated['outputs']:
        name = row['mesh'] + '.json'
        assert sha(out / 'replay' / name) == sha(packets / name) == row['sha256']
        assert row['expression_controls'] == 115 and row['lip_duplicates'] == 7

    comparison = [sys.executable, str(verifier), '--stage', str(integration),
                  '--decoded', str(installed_evidence), '--report', str(out / 'cooked-comparison.json')]
    (out / 'comparison-command.json').write_text(json.dumps(comparison, indent=2) + '\n')
    with (out / 'comparison.log').open('x') as log:
        result = subprocess.run(comparison, cwd=AUTHOR, stdout=log, stderr=subprocess.STDOUT)
    (out / 'comparison-exit.json').write_text(json.dumps({'exit_code': result.returncode}) + '\n')
    result.check_returncode()
    for path, digest in protected.items():
        assert sha(Path(path)) == digest, path
    report = dict(passed=True, inputs=protected, packets=regenerated['outputs'], installed_trio=ug_inputs,
                  comparison=sha(out / 'cooked-comparison.json'),
                  scope='Exact facial packet replay, retained W3/editor-module identities and current installed '
                        'facial geometry/morph comparison. No new UE cook, Blender source replay, '
                        'full namespace/cloth assembly proof, runtime or performance acceptance.')
    (out / 'verified.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(passed=True, protected_inputs=len(protected), packets=len(report['packets']))))


if __name__ == '__main__':
    main()
