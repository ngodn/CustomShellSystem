"""Cook Alice's verified cloth pair with Wine and DDC scratch on the root drive."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha
from build_eve_cloth_transfer import ROOT, WORK
from cook_eve_alice_candidate import filesystem_uuid


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('alice',),default='alice')
    parser.add_argument('--normal-attempt',type=int,required=True)
    parser.add_argument('--facial-attempt',type=int,required=True)
    args = parser.parse_args()
    scratch = ROOT/'build/cook-scratch/alice-cloth/out'
    parent = scratch.parent.parent
    parent.mkdir(parents=True,exist_ok=True)
    if parent.resolve()!=parent or filesystem_uuid(parent)!=filesystem_uuid('/'):
        raise ValueError('Scratch must be on the root drive')
    if scratch.parent.exists():
        raise FileExistsError('Scratch already allocated')
    free = dict(work=shutil.disk_usage(WORK).free,scratch=shutil.disk_usage(parent).free)
    if free['work']<3*1024**3 or free['scratch']<8*1024**3:
        raise ValueError('Need 3 GiB on work and 8 GiB on root')
    out = WORK/(args.variant+'-cook')
    out.mkdir(exist_ok=False)
    helper = Path(__file__).with_name('cook_eve_alice_candidate.py')
    assets,outputs,protected = {},{}, {str(p):sha(p) for p in (Path(__file__),helper)}
    for variant in (args.variant,):
        for kind in ('normal','facial'):
            attempt = args.normal_attempt if kind=='normal' else args.facial_attempt
            if attempt<1:
                raise ValueError('Attempt must be positive')
            label = variant+'-'+kind+('-'+str(attempt) if attempt>1 else '')
            proof_path = WORK/(label+'-verified.json')
            proof = read(proof_path)
            if not proof['passed']:
                raise ValueError('Unverified editor bake')
            inputs_path = WORK/(label+'-inputs.json')
            inputs = read(inputs_path)
            for package,row in inputs['assets'].items():
                if package==proof['source']:
                    continue
                if package in assets and assets[package]!=row['sha256']:
                    raise ValueError('Conflicting dependency sources')
                assets[package] = row['sha256']
            assets.update(proof['outputs'])
            outputs.update(proof['outputs'])
            for p in (proof_path,inputs_path):
                protected[str(p)] = sha(p)
    baseline = WORK.parent/'eve-tangent-final/cook'
    if read(baseline/'exit.json')!=dict(returncode=0,changed_inputs=[]):
        raise ValueError('Baseline cook failed')
    project = out/'project'
    scratch.mkdir(parents=True)
    (out/'out').symlink_to(scratch,target_is_directory=True)
    scratch_project = scratch.parent/'project'
    scratch_project.mkdir()
    project.symlink_to(scratch_project,target_is_directory=True)
    for name in ('CSSShared.uproject','Config/DefaultEngine.ini','Config/DefaultGame.ini'):
        source,target = baseline/'project'/name,project/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        protected[str(source)] = protected[str(target)] = sha(source)
    for package,digest in assets.items():
        relative = Path('Content')/(package.removeprefix('/Game/')+'.uasset')
        source,target = WORK/'editor'/relative,project/relative
        if sha(source)!=digest:
            raise ValueError('Editor source changed')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        if sha(target)!=digest:
            raise ValueError('Cook staging changed source')
        protected[str(source)] = protected[str(target)] = digest
    shutil.copytree(baseline/'out/ddc',out/'out/ddc')
    command = [s.replace(str(baseline),str(out)) for s in read(baseline/'command.json')]
    command[command.index('--name')+1] = 'css-eve-'+args.variant+'-cloth-cook'
    mount = 'type=bind,src='+str(out/'out')+',dst=/out'
    command[command.index(mount)] = 'type=bind,src='+str(scratch)+',dst=/out'
    project_mount = 'type=bind,src='+str(project)+',dst=/project'
    command[command.index(project_mount)] = 'type=bind,src='+str(scratch_project)+',dst=/project'
    engine = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/work/ue56/Engine'
    build = read(engine/'Build/Build.version')
    if tuple(build[k] for k in ('MajorVersion','MinorVersion','PatchVersion','Changelist'))!=(5,6,1,44394996):
        raise ValueError('Windows engine version changed')
    for p in (baseline/'command.json',baseline/'exit.json',engine/'Build/Build.version',
              engine/'Binaries/Win64/UnrealEditor-Cmd.exe',ROOT/'tools/shared-assets/cook-shared-inner.sh'):
        protected[str(p)] = sha(p)
    (out/'storage.json').write_text(json.dumps(dict(scratch=str(scratch),project=str(scratch_project),work_link=str(out/'out'),
        filesystem_uuid=filesystem_uuid(scratch),free_before_bytes=free),indent=2)+'\n')
    (out/'inputs.json').write_text(json.dumps(protected,indent=2)+'\n')
    (out/'shipping-allowlist.json').write_text(json.dumps(list(outputs),indent=2)+'\n')
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    with (out/'console.log').open('w') as log:
        result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    changed = [p for p,h in protected.items() if sha(Path(p))!=h]
    (out/'exit.json').write_text(json.dumps(dict(returncode=result.returncode,changed_inputs=changed),indent=2)+'\n')
    print(dict(returncode=result.returncode,changed_inputs=changed),flush=True)
    if result.returncode or changed:
        raise SystemExit(1)


if __name__=='__main__':
    main()
