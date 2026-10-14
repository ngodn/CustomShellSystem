"""Stage the verified Skin Suit/Bikini pair sources at their existing release paths."""
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
SURFACE = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014').resolve()
BASE = SURFACE/'tangent-bake'
WORK = SURFACE/'eve-tangent-final'
EDITOR_BASE = SURFACE/'blackpearl-ordered'


def read(path):
    return json.loads(path.read_text())


def main():
    checked = BASE/'pack3/verified.json'
    if not read(checked)['passed']:
        raise ValueError('Private cooked assemblies are not verified')
    if shutil.disk_usage(BASE).free<3*1024**3:
        raise ValueError('Need 3 GiB for final-path staging')
    WORK.mkdir(exist_ok=False)
    inventory_path = ROOT/'work/three-outfit-rig-physics/current-inventory-20261014/inventory.json'
    inventory = read(inventory_path)
    script = Path(__file__).with_name('finalize_eve_surface_editor.py')
    protected = {str(p):sha(p) for p in (Path(__file__),script,checked,inventory_path)}
    mapping,assets = {},{}
    for variant in ('skinsuit','bikini'):
        plan_path = SURFACE/(variant+'-ordered')/'binding-plan.json'
        plan = read(plan_path)
        protected.update(plan['protected'])
        protected[str(plan_path)] = sha(plan_path)
        pair = next(v['meshes'] for v in inventory['variants'] if v['meshes']['normal']==plan['reference_mesh'])
        for package,row in plan['sources'].items():
            if package in assets and assets[package]!=row:
                raise ValueError('Dependency source conflict')
            assets[package] = row
        for kind,attempt in (('normal',3),('facial',2)):
            proof_path = BASE/(variant+'-'+kind+'-'+str(attempt)+'-verified.json')
            proof = read(proof_path)
            if not proof['passed']:
                raise ValueError('Private editor source is not verified')
            protected[str(proof_path)] = sha(proof_path)
            mapping[proof['target']] = pair[kind]
            for package,digest in proof['outputs'].items():
                assets[package] = dict(path=str(BASE/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')),sha256=digest)
    for final in mapping.values():
        assets.pop(final,None)
    module = read(EDITOR_BASE/'inputs.json')
    for name in ('CSSAuthoring.uproject','Binaries/Linux/UnrealEditor.modules','Binaries/Linux/libUnrealEditor-CSSAuthoring.so'):
        source = EDITOR_BASE/'editor'/name
        if sha(source)!=module[str(source)]:
            raise ValueError('Pinned editor module changed')
        target = WORK/'editor'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        protected[str(source)] = protected[str(target)] = sha(source)
    for package,row in assets.items():
        source = Path(row['path']).resolve(strict=True)
        if sha(source)!=row['sha256']:
            raise ValueError('Editor source changed')
        target = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
        target.parent.mkdir(parents=True,exist_ok=True)
        target.symlink_to(source)
        protected[str(source)] = row['sha256']
    for name in ('tmp','ddc','user'):
        (WORK/name).mkdir()
    (WORK/'plan.json').write_text(json.dumps(dict(mapping=mapping,assets=assets,protected=protected),indent=2)+'\n')
    for mode in ('create','reload'):
        command = [v.replace(str(EDITOR_BASE),str(WORK)) for v in read(EDITOR_BASE/'import-command.json')[:-2]]
        position = command.index('--')
        command[position:position] = ['--setenv','CSS_SURFACE_FINAL_MODE',mode]
        command += ['-run=pythonscript','-script='+str(script)]
        (WORK/(mode+'-command.json')).write_text(json.dumps(command,indent=2)+'\n')
        with (WORK/(mode+'.log')).open('w') as log:
            result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
        changed = [p for p,h in protected.items() if sha(Path(p))!=h]
        row = dict(returncode=result.returncode,changed_inputs=changed)
        (WORK/(mode+'-exit.json')).write_text(json.dumps(row,indent=2)+'\n')
        print(mode,row,flush=True)
        if result.returncode or changed or not (WORK/(mode+'.json')).exists():
            raise SystemExit(1)


if __name__=='__main__':
    main()
