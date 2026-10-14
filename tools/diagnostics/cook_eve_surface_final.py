"""Windows-cook the four permanent Skin Suit/Bikini mesh paths."""
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
BASE = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/tangent-bake').resolve()
WORK = BASE.with_name('eve-tangent-final')


def read(path):
    return json.loads(path.read_text())


def main():
    proof,plan = read(WORK/'reload.json'),read(WORK/'plan.json')
    if not proof['passed'] or list(proof['outputs'])!=list(plan['mapping'].values()):
        raise ValueError('Final-path reload does not match plan')
    if shutil.disk_usage(WORK).free<6*1024**3:
        raise ValueError('Need 6 GiB for cooking')
    out = WORK/'cook'
    out.mkdir(exist_ok=False)
    (out/'out').mkdir()
    protected = {str(p):sha(p) for p in (WORK/'reload.json',WORK/'plan.json',Path(__file__))}
    baseline = BASE/'cook3'
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
        protected[str(source)] = protected[str(target)] = digest
    shutil.copytree(baseline/'out/ddc',out/'out/ddc')
    command = [v.replace(str(baseline),str(out)) for v in read(baseline/'command.json')]
    command[command.index('--name')+1] = 'css-eve-tangent-final-cook'
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
