"""Stage a read-only six-outfit native preset regression using retained sources.

Host Python 3.14; UE 5.6.1 runs check_eve_graph_inputs.py. No cook or game access.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

from audit_secondary_bindings import mesh_summary, package

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'work/three-outfit-rig-physics'


def read(path):
    return json.loads(path.read_text())


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    stage = args.output.resolve()
    if stage.exists():
        raise FileExistsError(stage)
    old_stage = WORK / 'peony-v2-body-inputs1'
    source = WORK / 'eve-rig-editor2/Content'
    rig_stage = WORK / 'peony-v2-body-limits1'
    rig = rig_stage / 'editor/Content/CSS/eins0fx/EveSB/CR_Body.uasset'
    if digest(rig) != read(rig_stage / 'authored.json')['sha256']:
        raise ValueError('Accepted authored rig changed')
    if read(rig_stage / 'reloaded.json')['passed'] is not True:
        raise ValueError('Missing rig reload acceptance')
    manifests = WORK / 'peony-public-upgrade2/manifests'
    eve = read(manifests / 'CSS_EveStellarBlade_eins0fx_P.json')
    shared = read(manifests / 'CSS_SharedAssets_P.json')
    decoded_path = WORK / 'eve-final-container1/decoded.json'
    decoded = read(decoded_path)
    adapters = [a for face in shared['catalog']['animation_library']['faces']
                for a in face['adapters'] if a['source_outfit'] == 'eins0fx.seduxtress']
    variants = [v for v in eve['catalog']['outfits'][0]['variants'] if v['id'] != 'peony']
    if {v['id'] for v in variants} != {'black_pearl', 'prototype', 'skin_suit',
                                    'bikini', 'casual_sweater', 'midsummer_alice'}:
        raise ValueError('Unexpected existing Eve variants')
    matrix = []
    for variant in variants:
        matches = [a for a in adapters if a['source_mesh'] == variant['mesh']]
        if len(matches) != 1:
            raise ValueError('Ambiguous facial binding')
        for kind, mesh in [('normal', variant['mesh']), ('facial', matches[0]['target_mesh'])]:
            graph = mesh_summary(decoded, package(mesh))['post_process']
            blueprint = graph + '.' + graph.rsplit('/', 1)[1]
            matrix.append(dict(variant=variant['id'], kind=kind, mesh=mesh, blueprint=blueprint))
    stage.mkdir(parents=True)
    module = stage / 'module'
    module.mkdir()
    shutil.copyfile(old_stage / 'module/CSSBodyInputs.uproject', module / 'CSSBodyInputs.uproject')
    (module / 'Binaries').symlink_to((old_stage / 'module/Binaries').resolve(), target_is_directory=True)
    content = module / 'Content'
    protected = {}
    for path in sorted(source.rglob('*')):
        if path.is_dir():
            continue
        relative = path.relative_to(source)
        target = rig if relative.as_posix() == 'CSS/eins0fx/EveSB/CR_Body.uasset' else path
        link = content / relative
        link.parent.mkdir(parents=True, exist_ok=True)
        link.symlink_to(target.resolve(strict=True))
        protected[str(target.resolve())] = digest(target)
    for row in matrix:
        for key in ('mesh', 'blueprint'):
            path = content / (package(row[key]).removeprefix('/Game/') + '.uasset')
            if not path.is_file():
                raise ValueError('Missing authoring source: ' + str(path))
    for name in ('tmp', 'cache', 'ddc', 'user', 'config-home'):
        (stage / name).mkdir()
    matrix_path = stage / 'matrix.json'
    matrix_path.write_text(json.dumps(matrix, indent=2) + '\n')
    paths = [matrix_path, decoded_path, rig_stage / 'authored.json', rig_stage / 'reloaded.json',
             manifests / 'CSS_EveStellarBlade_eins0fx_P.json', manifests / 'CSS_SharedAssets_P.json',
             Path(__file__), ROOT / 'tools/diagnostics/check_eve_graph_inputs.py',
             ROOT / 'tools/diagnostics/verify_body_graph_inputs.py',
             old_stage / 'module/Binaries/Linux/libUnrealEditor-CSSBodyInputs.so']
    for path in paths:
        protected[str(path.resolve())] = digest(path)
    (stage / 'inputs.json').write_text(json.dumps(protected, indent=2) + '\n')
    command = [arg.replace(str(old_stage.resolve()), str(stage)).replace(str(old_stage), str(stage))
               for arg in read(old_stage / 'command.json')]
    command = [arg.replace('check_body_graph_inputs.py', 'check_eve_graph_inputs.py')
               .replace('-corelimit=4', '-corelimit=2') for arg in command]
    command = ['nice', '-n', '19', *command, '-notracethreading']
    (stage / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (stage / 'engine.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    (stage / 'exit.json').write_text(json.dumps(dict(returncode=result.returncode)) + '\n')
    print(json.dumps(dict(returncode=result.returncode, meshes=len(matrix), output=str(stage))))
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
