"""Remove only prelisted tangent-bake intermediates after corrected readback passes."""
import json
from pathlib import Path
import shutil
import subprocess

from build_eve_tangent_bake import ROOT, WORK
from check_import_cooked_mesh import sha


def read(path):
    return json.loads(path.read_text())


def main():
    receipt = WORK/'cleanup-completed.json'
    if receipt.exists():
        raise FileExistsError(receipt)
    if not read(WORK/'pack3/verified.json')['passed']:
        raise ValueError('Corrected packaged validation must pass first')
    prelist = ROOT/'docs/development/cleanup-2026-10-14-eve-tangent-bake.md'
    if not prelist.is_file():
        raise ValueError('Missing prewritten removal list')
    docker = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/tools/docker-local.sh'
    query = ['bash',str(docker),'ps','-a','--filter','name=css-eve-tangent-bake-cook','--format','{{.Names}}']
    if subprocess.check_output(query,text=True).strip():
        raise ValueError('Cook container still exists')
    groups,retained = [],{}
    for cook_name,pack_name in (('cook','pack'),('cook3','pack3')):
        cook,pack = WORK/cook_name,WORK/pack_name
        if read(cook/'exit.json')!=dict(returncode=0,changed_inputs=[]):
            raise ValueError('Cook is not terminal and successful')
        inputs = read(cook/'inputs.json')
        for f in (cook/'project/Content').rglob('*'):
            if not f.is_file():
                continue
            source = WORK/'editor/Content'/f.relative_to(cook/'project/Content')
            expected = inputs[str(f)]
            if sha(f)!=expected or sha(source)!=expected:
                raise ValueError('Cook input is not a retained source duplicate')
        roundtrip = read(pack/'roundtrip.json')
        if not roundtrip['passed']:
            raise ValueError('Missing package roundtrip')
        for name,digest in roundtrip['payloads'].items():
            payload = pack/'stage'/name
            cooked = cook/'project/Saved/Cooked/Windows/CSSShared/Content'/Path(name).relative_to('MortalShell2/Content')
            if sha(payload)!=digest or sha(cooked)!=digest:
                raise ValueError('Cooked mesh payload is not retained')
            retained[str(payload)] = digest
        for suffix in ('out/prefix','project/Content','project/Saved/Cooked'):
            path = cook/suffix
            if path.exists():
                files = [p for p in path.rglob('*') if p.is_file() and not p.is_symlink()]
                groups.append(dict(path=str(path),files=len(files),logical_bytes=sum(p.stat().st_size for p in files)))
    for variant in ('skinsuit','bikini'):
        old = read(WORK/(variant+'-normal-2-verified.json'))
        new = read(WORK/(variant+'-normal-3-verified.json'))
        if not new['passed']:
            raise ValueError('Replacement source is not verified')
        for package,digest in new['outputs'].items():
            f = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
            if sha(f)!=digest:
                raise ValueError('Replacement source changed')
            retained[str(f)] = digest
        for package,digest in old['outputs'].items():
            f = WORK/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
            if sha(f)!=digest or f.is_symlink():
                raise ValueError('Unexpected rejected source')
            groups.append(dict(path=str(f),files=1,logical_bytes=f.stat().st_size,sha256=digest))
    for row in groups:
        p = Path(row['path'])
        if p.is_symlink() or not p.resolve().is_relative_to(WORK):
            raise ValueError('Removal escapes private trial')
    df = ['df','-h','/','/run/media/eins0fx/theark']
    before = subprocess.check_output(df,text=True)
    plan = dict(groups=groups,retained_payloads=retained,prelist_sha256=sha(prelist),before=before,
        recipe_sha256=sha(Path(__file__)),container_absent=True)
    (WORK/'cleanup-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    for row in groups:
        p = Path(row['path'])
        shutil.rmtree(p) if p.is_dir() else p.unlink()
    changed = [p for p,h in retained.items() if sha(Path(p))!=h]
    result = dict(plan,completed=not changed,changed_retained=changed,after=subprocess.check_output(df,text=True))
    receipt.write_text(json.dumps(result,indent=2)+'\n')
    print(sum(row['logical_bytes'] for row in groups),'logical bytes removed')
    print(result['before'],result['after'])
    if changed:
        raise SystemExit(1)


if __name__=='__main__':
    main()
