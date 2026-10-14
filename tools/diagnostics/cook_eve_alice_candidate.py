"""Cook the ordered Alice candidate with disposable output on the root drive.

Python 3.14. Preserve the earlier pinned cook recipes and the work symlink.
"""
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
COLLECTIONS = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections'
WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/alice-ordered').resolve()
SCRATCH = ROOT/'build/cook-scratch/alice-ordered/out'


def read(path):
    return json.loads(path.read_text())


def filesystem_uuid(path):
    value = subprocess.check_output(['findmnt','--noheadings','--output','UUID','--target',str(path)], text=True).strip()
    if not value or '\n' in value:
        raise ValueError('Cannot identify scratch filesystem')
    return value


def main():
    proof_path = WORK/'verified.json'
    proof = read(proof_path)
    if not proof['passed'] or len(proof['editor_assets']) != 2:
        raise ValueError('Expected verified private mesh and skeleton')
    if WORK.name != 'alice-ordered' or not WORK.is_relative_to((ROOT/'work').resolve()):
        raise ValueError('Unexpected candidate path')
    scratch_parent = ROOT/'build/cook-scratch'
    scratch_parent.mkdir(parents=True, exist_ok=True)
    if scratch_parent.resolve() != scratch_parent or filesystem_uuid(scratch_parent) != filesystem_uuid('/'):
        raise ValueError('Scratch must be an ordinary directory on the root drive')
    if SCRATCH.parent.exists() or (WORK/'cook').exists():
        raise FileExistsError('This cook has already been staged')
    free = dict(work=shutil.disk_usage(WORK).free, scratch=shutil.disk_usage(scratch_parent).free)
    if free['work'] < 3*1024**3 or free['scratch'] < 8*1024**3:
        raise ValueError('Need 3 GiB on work and 8 GiB on root for the split cook')
    cache = WORK.parent/'knit-ordered/cook'
    if read(cache/'exit.json') != dict(returncode=0, changed_inputs=[]):
        raise ValueError('Expected successful Knitwear cache')
    out = WORK/'cook'
    out.mkdir()
    project = out/'project'
    project.mkdir()
    SCRATCH.mkdir(parents=True)
    (out/'out').symlink_to(SCRATCH, target_is_directory=True)
    shutil.copytree(cache/'out/ddc', SCRATCH/'ddc')
    baseline = COLLECTIONS/'CSS_CommanderWhite_eins0fx_P/work/walk-final-paths1/cook'
    protected = {str(p):sha(p) for p in (proof_path, Path(__file__), cache/'exit.json')}
    for name in ('CSSShared.uproject', 'Config/DefaultEngine.ini', 'Config/DefaultGame.ini'):
        source, target = baseline/'project'/name, project/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        protected[str(source)] = protected[str(target)] = sha(source)
    for name, digest in proof['editor_assets'].items():
        source, target = WORK/'editor'/name, project/name
        if sha(source) != digest:
            raise ValueError('Editor candidate changed')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        if sha(target) != digest:
            raise ValueError('Staging changed candidate')
        protected[str(source)] = protected[str(target)] = digest
    command = [v.replace(str(baseline), str(out)) for v in read(baseline/'command.json')]
    command[command.index('--name')+1] = 'css-eve-alice-ordered-corner-cook'
    mount = 'type=bind,src='+str(out/'out')+',dst=/out'
    command[command.index(mount)] = 'type=bind,src='+str(SCRATCH)+',dst=/out'
    engine = COLLECTIONS/'CSS_UnholyGenessa_eins0fx_P/work/ue56/Engine'
    build = read(engine/'Build/Build.version')
    if tuple(build[k] for k in ('MajorVersion','MinorVersion','PatchVersion','Changelist')) != (5,6,1,44394996):
        raise ValueError('Windows engine version changed')
    for p in (baseline/'command.json', engine/'Build/Build.version',
              engine/'Binaries/Win64/UnrealEditor-Cmd.exe', ROOT/'tools/shared-assets/cook-shared-inner.sh'):
        protected[str(p)] = sha(p)
    storage = dict(scratch=str(SCRATCH), work_link=str(out/'out'), free_before_bytes=free,
                   cache_source=str(cache), cache_receipt_sha256=sha(cache/'exit.json'),
                   docker_bind_source=str(SCRATCH), filesystem_uuid=filesystem_uuid(scratch_parent))
    for name, value in (('storage.json',storage),('command.json',command),('inputs.json',protected)):
        (out/name).write_text(json.dumps(value, indent=2)+'\n')
    with (out/'console.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    changed = [p for p,digest in protected.items() if sha(Path(p)) != digest]
    receipt = dict(returncode=result.returncode, changed_inputs=changed)
    (out/'exit.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(receipt, flush=True)
    if result.returncode or changed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
