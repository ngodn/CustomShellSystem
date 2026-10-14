"""Run and reload a private tangent bake without modifying accepted sources."""
import argparse
import json
from pathlib import Path
import subprocess

from check_import_cooked_mesh import sha
from build_eve_tangent_bake import ROOT, WORK


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('skinsuit','bikini'),required=True)
    parser.add_argument('--kind',choices=('normal','facial'),default='normal')
    parser.add_argument('--attempt',type=int,default=1)
    args = parser.parse_args()
    if args.attempt<1:
        raise ValueError('Invalid attempt')
    if read(WORK/'build-exit.json')['returncode']!=0:
        raise ValueError('Authoring module did not build')
    base = WORK.parent/(args.variant+'-ordered')
    plan = read(base/'binding-plan.json')
    proof = read(base/'bindings-reload.json')
    if not proof['passed'] or not read(base/'binding-pack/assembly-verified.json')['bindings_and_base_render_preserved']:
        raise ValueError('Source assembly is not preserved')
    source = plan[args.kind+'_target']
    label = args.variant+'-'+args.kind+'-'+str(args.attempt)
    name = ('SkinSuit' if args.variant=='skinsuit' else 'Bikini')+('Normal' if args.kind=='normal' else 'Face')
    target = '/Game/CSSAuthoring/EveSurface20261014/SK_'+name+'TangentBaked'+('' if args.attempt==1 else str(args.attempt))
    protected = {str(Path(__file__)):sha(Path(__file__)),str(base/'bindings-reload.json'):sha(base/'bindings-reload.json')}
    for name,digest in read(WORK/'module.json').items():
        path = WORK/'editor'/name
        if sha(path)!=digest:
            raise ValueError('Authoring module changed')
        protected[str(path)] = digest
    assets = dict(plan['sources'])
    assets[source] = dict(path=str(base/'editor/Content'/(source.removeprefix('/Game/')+'.uasset')),sha256=proof['outputs'][source])
    for package,row in assets.items():
        original = Path(row['path']).resolve(strict=True)
        if sha(original)!=row['sha256']:
            raise ValueError('Accepted editor input changed')
        link = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
        if link.exists():
            if not link.is_symlink() or link.resolve()!=original:
                raise ValueError('Unexpected existing staging asset')
        else:
            link.parent.mkdir(parents=True,exist_ok=True)
            link.symlink_to(original)
        protected[str(original)] = row['sha256']
    inputs = WORK/(label+'-inputs.json')
    if inputs.exists():
        raise FileExistsError(inputs)
    inputs.write_text(json.dumps(dict(source=source,target=target,assets=assets,protected=protected),indent=2)+'\n')
    old_command = read(base/'import-command.json')[:-2]
    for mode in ('create','reload'):
        output = WORK/(label+'-'+mode+'.json')
        command = [s.replace(str(base),str(WORK)) for s in old_command]
        command += ['-run=CSSTangentBake','-Source='+(source if mode=='create' else target),'-Output='+str(output)]
        if mode=='create':
            command.append('-Target='+target)
        else:
            output_asset = WORK/'editor/Content'/(target.removeprefix('/Game/')+'.uasset')
            protected[str(output_asset)] = sha(output_asset)
        stem = label+'-'+mode
        (WORK/(stem+'-command.json')).write_text(json.dumps(command,indent=2)+'\n')
        with (WORK/(stem+'.log')).open('w') as log:
            result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
        changed = [p for p,h in protected.items() if sha(Path(p))!=h]
        (WORK/(stem+'-exit.json')).write_text(json.dumps(dict(returncode=result.returncode,changed_inputs=changed),indent=2)+'\n')
        print(stem,dict(returncode=result.returncode,changed_inputs=changed),flush=True)
        if result.returncode or changed or not output.exists():
            raise SystemExit(1)
    created,reloaded = read(WORK/(label+'-create.json')),read(WORK/(label+'-reload.json'))
    if created['after']!=reloaded['before'] or reloaded['before']['zero_tangents']!=0 or reloaded['before']['recompute_tangents']:
        raise ValueError('Tangent bake did not survive reload')
    (WORK/(label+'-verified.json')).write_text(json.dumps(dict(passed=True,source=source,target=target,
        outputs={target:sha(output_asset)},inspection=reloaded['before'],protected=protected,
        scope='Private editor creation and fresh reload only. Explicit tangent storage changes the recompute flag. Cooked geometry/surface/morph preservation and installed integration remain.'),indent=2)+'\n')


if __name__=='__main__':
    main()
