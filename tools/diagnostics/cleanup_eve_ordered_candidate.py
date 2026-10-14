"""Clean an ordered candidate cook after source readback, retaining failed tangent evidence."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha
from run_eve_cloth_final import ROOT, SURFACE


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',choices=('knit','alice'),required=True)
    args = parser.parse_args()
    WORK = SURFACE/(args.variant+'-ordered')
    cook = WORK/'cook'
    pack = WORK/'pack'
    receipt = WORK/'candidate-cleanup-completed.json'
    if receipt.exists():
        raise FileExistsError('Cleanup already recorded')
    proof = read(pack/'verified.json')
    fallback = read(pack/'tangent-fallbacks.json')
    if not proof['passed'] or read(cook/'exit.json')!=dict(returncode=0,changed_inputs=[]):
        raise ValueError('Verified terminal cook required')
    docker = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/tools/docker-local.sh'
    query = ['bash',str(docker),'ps','-a','--filter','name=css-eve-'+args.variant+'-ordered-corner-cook','--format','{{.Names}}']
    if subprocess.check_output(query,text=True).strip():
        raise ValueError('Cook container still exists')
    retained = {}
    inputs = read(cook/'inputs.json')
    for name,digest in read(WORK/'verified.json')['editor_assets'].items():
        path = WORK/'editor'/name
        if sha(path)!=digest:
            raise ValueError('Retained editor source changed')
        retained[str(path)] = digest
    for name,digest in read(pack/'roundtrip.json')['cooked_payloads'].items():
        staged = pack/'stage'/name
        cooked = cook/'project/Saved/Cooked/Windows/CSSShared/Content'/Path(name).relative_to('MortalShell2/Content')
        if sha(staged)!=digest or sha(cooked)!=digest:
            raise ValueError('Shipping payload is not retained')
        retained[str(staged)] = digest
    for staged in (cook/'project/Content').rglob('*'):
        if staged.is_file():
            original = WORK/'editor/Content'/staged.relative_to(cook/'project/Content')
            if sha(staged)!=inputs[str(staged)] or sha(original)!=inputs[str(staged)]:
                raise ValueError('Staged file is not a retained duplicate')
    paths = [cook/p for p in ('out/prefix','project/Content','project/Saved/Cooked')]
    groups = []
    for path in paths:
        if path.is_symlink() or not path.resolve().is_relative_to(WORK):
            raise ValueError('Removal escapes private cloth project')
        files = [f for f in path.rglob('*') if f.is_file() and not f.is_symlink()] if path.is_dir() else [path]
        groups.append(dict(path=str(path),files=len(files),logical_bytes=sum(f.stat().st_size for f in files)))
    prelist = ROOT/'docs/development'/('cleanup-2026-10-14-eve-'+args.variant+'-cloth.md')
    df = ['df','-h','/','/run/media/eins0fx/theark']
    plan = dict(groups=groups,retained=retained,before=subprocess.check_output(df,text=True),
        prelist_sha256=sha(prelist),recipe_sha256=sha(Path(__file__)),container_absent=True,unit_length_gate_still_failed=not fallback['unit_lengths_within_bound'])
    (WORK/'candidate-cleanup-plan.json').write_text(json.dumps(plan,indent=2)+'\n')
    for path in paths:
        shutil.rmtree(path) if path.is_dir() else path.unlink()
    changed = [p for p,h in retained.items() if sha(Path(p))!=h]
    result = dict(plan,completed=not changed,changed_retained=changed,after=subprocess.check_output(df,text=True))
    receipt.write_text(json.dumps(result,indent=2)+'\n')
    print(sum(row['logical_bytes'] for row in groups),'logical bytes removed')
    print(result['before'],result['after'])
    if changed:
        raise SystemExit(1)


if __name__=='__main__':
    main()
