"""Stage a small final-path project and run create/reload with the pinned editor."""
import json
import argparse
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
BASE = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/blackpearl-ordered').resolve()
WORK = BASE.with_name('blackpearl-final')


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-saved',action='store_true')
    args = parser.parse_args()
    script = Path(__file__).with_name('finalize_blackpearl_surface_editor.py')
    if args.resume_saved:
        if read(WORK/'create-exit.json')['returncode']==0 or (WORK/'create.json').exists():
            raise ValueError('Expected a failed create with saved files')
        plan_path = WORK/'plan.json'
        plan = read(plan_path)
        for p,digest in plan['protected'].items():
            if p not in (str(script),str(Path(__file__))) and sha(Path(p))!=digest:
                raise ValueError('Protected input changed during recovery')
        shutil.copy2(plan_path,WORK/'initial-plan.json')
        protected = plan['protected']
        for p in (script,Path(__file__)):
            protected[str(p)] = sha(p)
        for package in plan['mapping'].values():
            p=WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
            protected[str(p)] = sha(p)
        plan['recovery_reason']='Create exited 255 after saving both meshes: AssetRegistry.get_dependencies returned None. Explicitly rescan saved files and verify them without rebuilding.'
        plan_path.write_text(json.dumps(plan,indent=2)+'\n')
        run_editor(script,protected,('recover','reload'))
        return
    if not read(BASE/'binding-pack/assembly-verified.json')['passed']:
        raise ValueError('Private assembly is unverified')
    if shutil.disk_usage(BASE).free < 3*1024**3:
        raise ValueError('Need 3 GiB for final-path staging')
    WORK.mkdir(exist_ok=False)
    plan = read(BASE/'binding-plan.json')
    proof = read(BASE/'bindings-reload.json')
    inventory = read(ROOT/'work/three-outfit-rig-physics/current-inventory-20261014/inventory.json')
    pair = next(v['meshes'] for v in inventory['variants'] if v['meshes']['normal']==plan['reference_mesh'])
    mapping = {plan['normal_target']:pair['normal'],plan['facial_target']:pair['facial']}
    assets = {p:r for p,r in plan['sources'].items() if p not in mapping.values()}
    for p,digest in proof['outputs'].items():
        assets[p] = dict(path=str(BASE/'editor/Content'/(p.removeprefix('/Game/')+'.uasset')),sha256=digest)
    protected = dict(plan['protected'])
    for p in (Path(__file__),script,BASE/'binding-pack/assembly-verified.json'):
        protected[str(p)] = sha(p)
    module = read(BASE/'inputs.json')
    for name in ('CSSAuthoring.uproject','Binaries/Linux/UnrealEditor.modules','Binaries/Linux/libUnrealEditor-CSSAuthoring.so'):
        source = BASE/'editor'/name
        if sha(source)!=module[str(source)]:
            raise ValueError('Pinned module changed')
        target = WORK/'editor'/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        protected[str(source)] = protected[str(target)] = sha(source)
    for p,row in assets.items():
        source = Path(row['path'])
        if sha(source)!=row['sha256']:
            raise ValueError('Editor source changed')
        target = WORK/'editor/Content'/(p.removeprefix('/Game/')+'.uasset')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
        protected[str(source)] = row['sha256']
    for name in ('tmp','ddc','user'):
        (WORK/name).mkdir()
    (WORK/'plan.json').write_text(json.dumps(dict(mapping=mapping,assets=assets,protected=protected),indent=2)+'\n')
    run_editor(script,protected,('create','reload'))


def run_editor(script,protected,modes):
    for mode in modes:
        if (WORK/(mode+'-command.json')).exists():
            raise FileExistsError('An editor invocation already exists for '+mode)
        command = [v.replace(str(BASE),str(WORK)) for v in read(BASE/'import-command.json')[:-2]]
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


if __name__ == '__main__':
    main()
