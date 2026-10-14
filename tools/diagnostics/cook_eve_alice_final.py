"""Windows-cook the permanent Alice pair with project/output scratch on root."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha
from cook_eve_alice_candidate import filesystem_uuid

ROOT = Path(__file__).resolve().parents[2]
BASE = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/cloth-transfer').resolve()


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('alice',),default='alice')
    args = parser.parse_args()
    WORK = BASE.with_name(args.variant+'-cloth-final')
    proof,plan = read(WORK/'reload.json'),read(WORK/'plan.json')
    if not proof['passed'] or list(proof['outputs'])!=list(plan['mapping'].values()):
        raise ValueError('Final-path reload does not match plan')
    scratch = ROOT/'build/cook-scratch/alice-final'
    scratch.parent.mkdir(parents=True,exist_ok=True)
    if scratch.parent.resolve()!=scratch.parent or filesystem_uuid(scratch.parent)!=filesystem_uuid('/'):
        raise ValueError('Scratch must be on the root drive')
    if scratch.exists():
        raise FileExistsError('Scratch already allocated')
    free = dict(work=shutil.disk_usage(WORK).free,scratch=shutil.disk_usage(scratch.parent).free)
    if free['work']<3*1024**3 or free['scratch']<8*1024**3:
        raise ValueError('Need 3 GiB on work and 8 GiB on root')
    out = WORK/'cook'
    out.mkdir(exist_ok=False)
    for name in ('project','out'):
        (scratch/name).mkdir(parents=True)
        (out/name).symlink_to(scratch/name,target_is_directory=True)
    helper = Path(__file__).with_name('cook_eve_alice_candidate.py')
    protected = {str(p):sha(p) for p in (WORK/'reload.json',WORK/'plan.json',Path(__file__),helper)}
    baseline = BASE/(args.variant+'-cook')
    if read(baseline/'exit.json')!=dict(returncode=0,changed_inputs=[]):
        raise ValueError('Private pair cook failed')
    for name in ('CSSShared.uproject','Config/DefaultEngine.ini','Config/DefaultGame.ini'):
        source,target = baseline/'project'/name,out/'project'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        protected[str(source)] = protected[str(target)] = sha(source)
    assets = {p:r['sha256'] for p,r in plan['assets'].items() if p not in plan['mapping']}
    assets.update(proof['outputs'])
    for p,digest in assets.items():
        relative = Path('Content')/(p.removeprefix('/Game/')+'.uasset')
        source,target = WORK/'editor'/relative,out/'project'/relative
        if sha(source)!=digest:
            raise ValueError('Editor input changed: '+p)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        if sha(target)!=digest:
            raise ValueError('Cook staging changed source')
        protected[str(source)] = protected[str(target)] = digest
    shutil.copytree(baseline/'out/ddc',out/'out/ddc')
    command = [v.replace(str(baseline),str(out)) for v in read(baseline/'command.json')]
    command[command.index('--name')+1] = 'css-eve-'+args.variant+'-cloth-final-cook'
    for name in ('out','project'):
        matches = [i for i,v in enumerate(command) if v.startswith('type=bind,') and v.endswith(',dst=/'+name)]
        if len(matches)!=1:
            raise ValueError('Expected one scratch mount per destination')
        command[matches[0]] = 'type=bind,src='+str(scratch/name)+',dst=/'+name
    engine = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/work/ue56/Engine'
    build = read(engine/'Build/Build.version')
    if tuple(build[k] for k in ('MajorVersion','MinorVersion','PatchVersion','Changelist'))!=(5,6,1,44394996):
        raise ValueError('Windows engine version changed')
    for p in (baseline/'command.json',baseline/'exit.json',engine/'Build/Build.version',
              engine/'Binaries/Win64/UnrealEditor-Cmd.exe',ROOT/'tools/shared-assets/cook-shared-inner.sh'):
        protected[str(p)] = sha(p)
    (out/'storage.json').write_text(json.dumps(dict(scratch=str(scratch/'out'),project=str(scratch/'project'),
        filesystem_uuid=filesystem_uuid(scratch),free_before_bytes=free),indent=2)+'\n')
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    (out/'inputs.json').write_text(json.dumps(protected,indent=2)+'\n')
    (out/'shipping-allowlist.json').write_text(json.dumps(list(proof['outputs']),indent=2)+'\n')
    with (out/'console.log').open('w') as log:
        result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    changed = [p for p,digest in protected.items() if sha(Path(p))!=digest]
    row = dict(returncode=result.returncode,changed_inputs=changed)
    (out/'exit.json').write_text(json.dumps(row,indent=2)+'\n')
    print(row,flush=True)
    if result.returncode or changed:
        raise SystemExit(1)


if __name__=='__main__':
    main()
