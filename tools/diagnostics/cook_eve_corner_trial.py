"""Cook a private ordered Eve mesh with the validated Windows 5.6.1 recipe."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
COLLECTIONS = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if not work.is_relative_to((ROOT / 'work').resolve()):
        raise ValueError('Use the private CSS work directory')
    proof = json.loads((work / 'verified.json').read_text())
    if not proof['passed'] or len(proof['editor_assets']) != 2:
        raise ValueError('Expected a verified private mesh and skeleton')
    if shutil.disk_usage(work).free < 6*1024**3:
        raise ValueError('Need 6 GiB free for the cook')
    out = work / 'cook'
    out.mkdir(exist_ok=False)
    project = out / 'project'
    project.mkdir()
    (out / 'out').mkdir()
    baseline = COLLECTIONS / 'CSS_CommanderWhite_eins0fx_P/work/walk-final-paths1/cook'
    protected = {str(work / 'verified.json'): sha(work / 'verified.json'),
                 str(Path(__file__)): sha(Path(__file__))}
    for name in ('CSSShared.uproject', 'Config/DefaultEngine.ini', 'Config/DefaultGame.ini'):
        source, target = baseline / 'project' / name, project / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        protected[str(source)] = sha(source)
        protected[str(target)] = sha(target)
    for name, digest in proof['editor_assets'].items():
        source, target = work / 'editor' / name, project / name
        if sha(source) != digest:
            raise ValueError('Editor candidate changed')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        if sha(target) != digest:
            raise ValueError('Staging changed candidate')
        protected[str(source)] = protected[str(target)] = digest
    command = [v.replace(str(baseline), str(out)) for v in json.loads((baseline / 'command.json').read_text())]
    command[command.index('--name')+1] = 'css-eve-blackpearl-corner-cook'
    engine = COLLECTIONS / 'CSS_UnholyGenessa_eins0fx_P/work/ue56/Engine'
    build = json.loads((engine / 'Build/Build.version').read_text())
    if tuple(build[k] for k in ('MajorVersion', 'MinorVersion', 'PatchVersion', 'Changelist')) != (5, 6, 1, 44394996):
        raise ValueError('Windows engine version changed')
    for path in (baseline / 'command.json', engine / 'Build/Build.version',
                 engine / 'Binaries/Win64/UnrealEditor-Cmd.exe', ROOT / 'tools/shared-assets/cook-shared-inner.sh'):
        protected[str(path)] = sha(path)
    (out / 'command.json').write_text(json.dumps(command, indent=2)+'\n')
    (out / 'inputs.json').write_text(json.dumps(protected, indent=2)+'\n')
    with (out / 'console.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    changed = [p for p, digest in protected.items() if sha(Path(p)) != digest]
    (out / 'exit.json').write_text(json.dumps(dict(returncode=result.returncode, changed_inputs=changed), indent=2)+'\n')
    print(dict(returncode=result.returncode, changed_inputs=changed), flush=True)
    if result.returncode or changed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
