"""Cook a reloaded cloth-preserving Eve pair with the established Windows recipe."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha
from build_eve_cloth_transfer import ROOT, WORK


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('planet','knit','alice'),required=True)
    parser.add_argument('--normal-attempt',type=int,required=True)
    parser.add_argument('--facial-attempt',type=int,required=True)
    args = parser.parse_args()
    if shutil.disk_usage(WORK).free<6*1024**3:
        raise ValueError('Need at least 6 GiB for the cook')
    out = WORK/(args.variant+'-cook')
    out.mkdir(exist_ok=False)
    assets,outputs,protected = {},{}, {str(Path(__file__)):sha(Path(__file__))}
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
    project = out/'project'
    (out/'out').mkdir()
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
