"""Clean verified duplicate cook files for the permanent Planet cloth pair."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha
from run_eve_cloth_final import ROOT, SURFACE, read


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('planet',),required=True)
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
    paths = [WORK/p for p in ('cook/out/prefix','cook/project/Content','cook/project/Saved/Cooked')]
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
        if path.is_symlink() or not path.resolve().is_relative_to(WORK):
            raise ValueError('Removal escapes private final project')
        files = [f for f in path.rglob('*') if f.is_file() and not f.is_symlink()] if path.is_dir() else [path]
        groups.append(dict(path=str(path),files=len(files),logical_bytes=sum(f.stat().st_size for f in files)))
    prelist = ROOT/'docs/development/cleanup-2026-10-14-eve-planet-final.md'
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
