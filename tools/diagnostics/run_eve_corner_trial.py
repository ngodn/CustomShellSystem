"""Import and reload one private ordered Eve candidate using Peony's pinned module.

Python 3.14. Only the small project descriptor and compiled editor module are
copied. Installed game files and accepted authoring assets stay read-only.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
EVE = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if not work.is_relative_to((ROOT / 'work').resolve()):
        raise ValueError('Use a private work directory')
    candidate = work / 'candidate.mesh.json'
    receipt = json.loads(candidate.with_suffix('.receipt.json').read_text())
    if not receipt['passed'] or sha(candidate) != receipt['output_sha256']:
        raise ValueError('Candidate receipt does not match')
    previous = EVE / 'work/peony-character1'
    saved = json.loads((previous / 'corners-import1.exit.json').read_text())
    if saved['returncode']:
        raise ValueError('Peony module was not validated')
    editor = work / 'editor'
    if (work / 'import-command.json').exists() or any((editor / 'Content').rglob('*.uasset')):
        raise FileExistsError('An import has already been attempted here')
    editor.mkdir(exist_ok=True)
    protected = {str(candidate): sha(candidate), receipt['source']: receipt['source_sha256'],
                 str(Path(__file__)): sha(Path(__file__))}
    for name, digest in saved['module'].items():
        source = previous / 'editor' / name
        if sha(source) != digest:
            raise ValueError('Validated module changed: '+str(source))
        destination = editor / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            if sha(destination) != digest:
                raise ValueError('Partial staging differs: '+str(destination))
        else:
            shutil.copy2(source, destination)
        protected[str(source)] = digest
        protected[str(destination)] = digest
    (editor / 'Content').mkdir(exist_ok=True)
    for name in ('tmp', 'ddc', 'user'):
        (work / name).mkdir(exist_ok=True)
    command = [value.replace(str(previous), str(work)) for value in saved['command']]
    command[-1] = '-Input='+str(candidate)
    executable = command[command.index('--')+1]
    protected[executable] = sha(Path(executable))
    (work / 'inputs.json').write_text(json.dumps(protected, indent=2)+'\n')
    results = []
    for mode in ('import', 'reload'):
        if mode == 'reload':
            command = command[:-2] + ['-run=CSSInspectMesh', '-Mesh='+receipt['mesh_package'],
                                     '-Output='+str(work / 'reloaded.json')]
        (work / (mode+'-command.json')).write_text(json.dumps(command, indent=2)+'\n')
        with (work / (mode+'.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        changed = [p for p, digest in protected.items() if sha(Path(p)) != digest]
        row = dict(mode=mode, returncode=result.returncode, changed_inputs=changed)
        (work / (mode+'-exit.json')).write_text(json.dumps(row, indent=2)+'\n')
        results.append(row)
        print(row, flush=True)
        if result.returncode or changed:
            raise SystemExit(1)
    reloaded = json.loads((work / 'reloaded.json').read_text())
    source = json.loads(candidate.read_text())
    if (reloaded['bones'] != len(source['bones']) or reloaded['material_slots'] != source['materials'] or
            len(reloaded['lods']) != 1 or reloaded['lods'][0]['triangles'] != len(source['faces']) or
            {m['name'] for m in reloaded['morph_targets']} != {m['name'] for m in source['morph_targets']}):
        raise ValueError('Reload changed required mesh identities')
    (work / 'verified.json').write_text(json.dumps(dict(passed=True, results=results,
        editor_assets={str(p.relative_to(editor)): sha(p) for p in (editor/'Content').rglob('*.uasset')},
        lod=reloaded['lods'][0], bones=reloaded['bones'], morphs=len(reloaded['morph_targets']),
        scope='Private geometry import and fresh reload only. No material binding, cooked preservation or gameplay acceptance.'), indent=2)+'\n')


if __name__ == '__main__':
    main()
