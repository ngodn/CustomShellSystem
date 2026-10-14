"""Clean verified split-drive cook files for the permanent Alice cloth pair."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha
from run_eve_cloth_final import ROOT, SURFACE, read
from cook_eve_alice_candidate import filesystem_uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('alice',),default='alice')
    args = parser.parse_args()
    WORK = SURFACE/(args.variant+'-cloth-final')
    if (WORK/'cleanup-completed.json').exists():
        raise FileExistsError('Cleanup already recorded')
    proof = read(WORK/'pack/verified.json')
    if not proof['passed'] or read(WORK/'cook/exit.json')!=dict(returncode=0,changed_inputs=[]):
        raise ValueError('Verified terminal cook required')
    docker = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/tools/docker-local.sh'
    query = ['bash',str(docker),'ps','-a','--filter','name=css-eve-'+args.variant+'-cloth-final-cook','--format','{{.Names}}']
    if subprocess.check_output(query,text=True).strip():
        raise ValueError('Cook container still exists')
    retained = {}
    for package,digest in read(WORK/'reload.json')['outputs'].items():
        path = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
        if sha(path)!=digest:
            raise ValueError('Final source changed')
        retained[str(path)] = digest
    for name,digest in proof['payloads'].items():
        staged = WORK/'pack/stage'/name
        cooked = WORK/'cook/project/Saved/Cooked/Windows/CSSShared/Content'/Path(name).relative_to('MortalShell2/Content')
        if sha(staged)!=digest or sha(cooked)!=digest:
            raise ValueError('Shipping payload is not retained')
        retained[str(staged)] = digest
    inputs = read(WORK/'cook/inputs.json')
    for staged in (WORK/'cook/project/Content').rglob('*'):
        if staged.is_file():
            original = WORK/'editor/Content'/staged.relative_to(WORK/'cook/project/Content')
            if sha(staged)!=inputs[str(staged)] or sha(original)!=inputs[str(staged)]:
                raise ValueError('Staged file is not a retained duplicate')
    scratch = ROOT/'build/cook-scratch/alice-final'
    storage = read(WORK/'cook/storage.json')
    if (scratch.resolve()!=scratch or filesystem_uuid(scratch)!=filesystem_uuid('/') or
            storage['filesystem_uuid']!=filesystem_uuid(scratch)):
        raise ValueError('Scratch allocation changed')
    for name,key in (('out','scratch'),('project','project')):
        link = WORK/'cook'/name
        if storage[key]!=str(scratch/name) or not link.is_symlink() or link.resolve()!=scratch/name:
            raise ValueError('Scratch link changed')
    retained[str(WORK/'cook/storage.json')] = sha(WORK/'cook/storage.json')
    scratch_paths = [scratch/p for p in ('out/prefix','project/Content','project/Saved/Cooked')]
    paths = list(scratch_paths)
    reload = read(WORK/'reload.json')
    dependencies = {p for refs in reload['dependencies'].values() for p in refs}
    for mode in ('create','reload'):
        for index in range(len(reload['outputs'])):
            package = '/Game/CSS/AnimLab/SK_Face_FinalPathCheck'+str(index)+mode
            if package in dependencies:
                raise ValueError('Diagnostic clone is referenced')
            path = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
            if path.exists():
                paths.append(path)
    groups = []
    for path in paths:
        allowed = path in scratch_paths and path.resolve()==path or path.resolve().is_relative_to(WORK)
        if path.is_symlink() or not allowed:
            raise ValueError('Removal escapes private final project')
        files = [f for f in path.rglob('*') if f.is_file() and not f.is_symlink()] if path.is_dir() else [path]
        groups.append(dict(path=str(path),files=len(files),logical_bytes=sum(f.stat().st_size for f in files)))
    prelist = ROOT/'docs/development'/('cleanup-2026-10-14-eve-'+args.variant+'-final.md')
    df = ['df','-h','/','/run/media/eins0fx/theark']
    plan = dict(groups=groups,retained=retained,before=subprocess.check_output(df,text=True),
        prelist_sha256=sha(prelist),recipe_sha256=sha(Path(__file__)),container_absent=True)
    (WORK/'cleanup-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    for path in paths:
        shutil.rmtree(path) if path.is_dir() else path.unlink()
    changed = [p for p,h in retained.items() if sha(Path(p))!=h]
    result = dict(plan,completed=not changed,changed_retained=changed,after=subprocess.check_output(df,text=True))
    (WORK/'cleanup-completed.json').write_text(json.dumps(result,indent=2)+'\n')
    print(sum(row['logical_bytes'] for row in groups),'logical bytes removed')
    print(result['before'],result['after'])
    if changed:
        raise SystemExit(1)


if __name__=='__main__':
    main()
