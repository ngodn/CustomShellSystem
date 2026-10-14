"""Transfer and reload accepted cloth onto a private tangent-repaired Eve mesh."""
import argparse
import json
from pathlib import Path
import subprocess

from build_eve_cloth_transfer import ROOT, WORK
from check_import_cooked_mesh import sha


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('planet','knit','alice'),required=True)
    parser.add_argument('--kind',choices=('normal','facial'),required=True)
    parser.add_argument('--attempt',type=int,default=1)
    args = parser.parse_args()
    if args.attempt<1:
        raise ValueError('Attempt must be positive')
    if read(WORK/'build-exit.json')['returncode']!=0:
        raise ValueError('Cloth transfer module did not build')
    base = WORK.parent/(args.variant+'-ordered')
    plan = read(base/'binding-plan.json')
    tangent = WORK.parent/'tangent-bake'
    proof_path = tangent/(args.variant+'-'+args.kind+'-1-verified.json')
    proof = read(proof_path)
    if not proof['passed'] or proof['inspection']['zero_tangents']:
        raise ValueError('Tangent source is not verified')
    label = args.variant+'-'+args.kind+('-'+str(args.attempt) if args.attempt>1 else '')
    name = args.variant.title()+('Normal' if args.kind=='normal' else 'Face')
    target = '/Game/CSSAuthoring/EveSurface20261014/SK_'+name+'Cloth'+str(args.attempt)
    reference = plan['reference_mesh' if args.kind=='normal' else 'reference_facial_mesh']
    protected = dict(plan['protected'])
    for p in (Path(__file__),proof_path,base/'binding-plan.json'):
        protected[str(p)] = sha(p)
    for name,h in read(WORK/'module.json').items():
        p = WORK/'editor'/name
        if sha(p)!=h:
            raise ValueError('Transfer module changed')
        protected[str(p)] = h
    assets = dict(plan['sources'])
    for package,digest in proof['outputs'].items():
        assets[package] = dict(path=str(tangent/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')),sha256=digest)
    for package,row in assets.items():
        source = Path(row['path']).resolve(strict=True)
        if sha(source)!=row['sha256']:
            raise ValueError('Accepted source changed')
        link = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
        if link.exists():
            if not link.is_symlink() or link.resolve()!=source:
                raise ValueError('Staging collision')
        else:
            link.parent.mkdir(parents=True,exist_ok=True)
            link.symlink_to(source)
        protected[str(source)] = row['sha256']
    inputs = WORK/(label+'-inputs.json')
    if inputs.exists():
        raise FileExistsError(inputs)
    inputs.write_text(json.dumps(dict(source=proof['target'],reference=reference,target=target,assets=assets,protected=protected),indent=2)+'\n')
    for mode in ('create','reload'):
        output = WORK/(label+'-'+mode+'.json')
        command = [s.replace(str(base),str(WORK)) for s in read(base/'import-command.json')[:-2]]
        command += ['-run=CSSClothTransfer','-Source='+(proof['target'] if mode=='create' else target),
                    '-Reference='+reference,'-Output='+str(output)]
        asset = WORK/'editor/Content'/(target.removeprefix('/Game/')+'.uasset')
        if mode=='create':
            command.append('-Target='+target)
        else:
            protected[str(asset)] = sha(asset)
        stem = label+'-'+mode
        (WORK/(stem+'-command.json')).write_text(json.dumps(command,indent=2)+'\n')
        with (WORK/(stem+'.log')).open('w') as log:
            result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
        changed = [p for p,h in protected.items() if sha(Path(p))!=h]
        (WORK/(stem+'-exit.json')).write_text(json.dumps(dict(returncode=result.returncode,changed_inputs=changed),indent=2)+'\n')
        print(stem,dict(returncode=result.returncode,changed_inputs=changed),flush=True)
        if result.returncode or changed or not output.exists() or not read(output)['passed']:
            raise SystemExit(1)
    if read(WORK/(label+'-create.json'))!=read(WORK/(label+'-reload.json')):
        raise ValueError('Cloth transfer changed on reload')
    (WORK/(label+'-verified.json')).write_text(json.dumps(dict(passed=True,source=proof['target'],reference=reference,
        outputs={target:sha(asset)},protected=protected,inspection=read(WORK/(label+'-reload.json')),
        scope='Private editor cloth transfer and reload only. Cooked geometry, mapping, cloth/config comparisons and runtime remain.'),indent=2)+'\n')


if __name__=='__main__':
    main()
