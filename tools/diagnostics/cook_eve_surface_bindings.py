"""Cook the bound Eve pair; dependencies are editor-only inputs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=('skinsuit','bikini'), required=True)
    args = parser.parse_args()
    WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014'/(args.variant+'-ordered')).resolve()
    proof_path, plan_path = WORK/'bindings-reload.json', WORK/'binding-plan.json'
    proof, plan = json.loads(proof_path.read_text()), json.loads(plan_path.read_text())
    if not proof['passed'] or len(proof['outputs']) != 2:
        raise ValueError('Expected a reloaded normal/facial pair')
    if shutil.disk_usage(WORK).free < 6*1024**3:
        raise ValueError('Need at least 6 GiB for dependency cooking')
    out = WORK/'binding-cook'
    out.mkdir(exist_ok=False)
    project = out/'project'
    project.mkdir()
    (out/'out').mkdir()
    protected = {str(p):sha(p) for p in (proof_path,plan_path,Path(__file__))}
    baseline = WORK/'cook'
    for name in ('CSSShared.uproject','Config/DefaultEngine.ini','Config/DefaultGame.ini'):
        source, target = baseline/'project'/name, project/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        protected[str(source)] = protected[str(target)] = sha(source)
    assets = {p:r['sha256'] for p,r in plan['sources'].items()}
    assets.update(proof['outputs'])
    for package,digest in assets.items():
        relative = Path('Content')/(package.removeprefix('/Game/')+'.uasset')
        source, target = WORK/'editor'/relative, project/relative
        if sha(source) != digest:
            raise ValueError('Editor input changed: '+package)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        if sha(target) != digest:
            raise ValueError('Staged asset differs')
        protected[str(source)] = protected[str(target)] = digest
    # Reuse only the small prior cache, not a second engine or full authoring tree.
    shutil.copytree(baseline/'out/ddc',out/'out/ddc')
    command = [v.replace(str(baseline),str(out)) for v in json.loads((baseline/'command.json').read_text())]
    command[command.index('--name')+1] = 'css-eve-'+args.variant+'-binding-cook'
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    (out/'inputs.json').write_text(json.dumps(protected,indent=2)+'\n')
    (out/'shipping-allowlist.json').write_text(json.dumps(list(proof['outputs']),indent=2)+'\n')
    with (out/'console.log').open('w') as log:
        proc = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    changed = [p for p,digest in protected.items() if sha(Path(p)) != digest]
    row = dict(returncode=proc.returncode,changed_inputs=changed)
    (out/'exit.json').write_text(json.dumps(row,indent=2)+'\n')
    print(row,flush=True)
    if proc.returncode or changed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
