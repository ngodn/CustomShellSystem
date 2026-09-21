"""Assemble the verified short-path Black Pearl candidate without installing."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

CSS = Path(__file__).resolve().parents[2]
ROOT = CSS.parent
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
WORK = CSS / 'work/paths'
OUT = MOD / 'work/short1'
load = lambda p: json.loads(p.read_text())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert not OUT.exists()
for file in ('mat1/validation2.json', 'rig1/verified.json', 'rig1/cooked-validation.json',
             'rig1/motion.json', 'rig2/bound.json', 'rig2/mesh-validation.json'):
    assert load(WORK / file)['passed'], file
for file in ('rig1/motion2-exit.json', 'rig2/bind-exit.json', 'rig2/cook-exit.json',
             'rig2/mesh-check-exit.json'):
    assert load(WORK / file)['exit_code'] == 0, file
protected = load(WORK / 'rig1/verified.json')['protected_hashes']
assert all(digest(Path(p)) == h for p, h in protected.items())
mapping = load(WORK / 'rig1/map.json')
mesh = '/Game/CSS/SeduXtress/SK_BlackPearl2'
old = MOD / 'work/v44-trial-v1'
old_plan = load(old / 'plan/plan.json')
mapping[old_plan['mesh']] = mesh


def mapped(value):
    if isinstance(value, dict):
        return {k: mapped(v) for k, v in value.items()}
    if isinstance(value, list):
        return [mapped(v) for v in value]
    if isinstance(value, str):
        for before, after in sorted(mapping.items(), key=lambda p: -len(p[0])):
            value = value.replace(before + '.' + before.rsplit('/', 1)[1], after + '.' + after.rsplit('/', 1)[1])
            value = value.replace(before, after)
    return value


plan = mapped(copy.deepcopy(old_plan))
plan.update(mesh=mesh, runtime_tested=False, stage=str(OUT),
            source_mesh_sha256=digest(WORK / 'rig2/source.mesh.json'))
for material in plan['materials']:
    for suffix, record in material['files'].items():
        path = WORK / 'mat1/legacy/MortalShell2/Content' / (material['package'].removeprefix('/Game/') + suffix)
        assert path.is_file()
        record.update(file=str(path), sha256=digest(path))
rig = (WORK / 'rig1/cook.txt').read_text().splitlines()
rig = [mesh if p == '/Game/CSS/SeduXtress/SK_BlackPearl' else p for p in rig]
textures = list(load(WORK / 'mat1/textures.json').values())
packages = rig + textures
assert len(packages) == len(set(packages)) == 97
OUT.mkdir()
(OUT / 'plan').mkdir()
(OUT / 'plan/plan.json').write_text(json.dumps(plan, indent=2) + '\n')
(OUT / 'plan/packages.txt').write_text('\n'.join(packages) + '\n')
copied = {}
for package in packages:
    cook = WORK / ('rig2/cooked' if package == mesh else 'rig1/cooked' if package in rig else 'mat1/cooked')
    relative = Path('CSSAuthoring/Content') / package.removeprefix('/Game/')
    for suffix in ('.uasset', '.uexp', '.ubulk'):
        source = cook / (str(relative) + suffix)
        if suffix in ('.uasset', '.uexp'):
            assert source.is_file(), source
        if source.exists():
            target = OUT / 'cooked' / (str(relative) + suffix)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            assert digest(target) == digest(source)
            copied[str(target.relative_to(OUT))] = digest(target)
metadata = OUT / 'cooked/CSSAuthoring/Metadata'
metadata.mkdir(parents=True)
shutil.copy2(WORK / 'rig2/cooked/CSSAuthoring/Metadata/scriptobjects.bin', metadata / 'scriptobjects.bin')
meta = OUT / 'metadata'
meta.mkdir()
for file in (old / 'metadata').iterdir():
    if file.name == 'build-audit.json':
        # Keep old historical package names out of the new package's audit.
        shutil.copy2(file, OUT / 'prior-audit.json')
    elif file.suffix == '.json':
        (meta / file.name).write_text(json.dumps(mapped(load(file)), indent=2) + '\n')
    else:
        shutil.copy2(file, meta / file.name)
audit = dict(mesh=mesh, skeleton='/Game/CSS/Shared/SKEL_Base',
    physics='/Game/CSS/SeduXtress/PA_Body', animation='/Game/CSS/SeduXtress/ABP_Secondary',
    source_sha256=plan['source_mesh_sha256'],
    evidence={str(WORK / p): digest(WORK / p) for p in ('rig1/cooked-validation.json', 'rig1/motion.json', 'rig2/mesh-validation.json')},
    scope='Verified offline candidate; live footwear, grounding and combined runtime checks remain.')
(meta / 'build-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
(OUT / 'tmp').mkdir()
tempfile.tempdir = str(OUT / 'tmp')
os.environ['TMPDIR'] = str(OUT / 'tmp')
script = MOD / 'tools/package_seduxtress.py'
spec = importlib.util.spec_from_file_location('short_packager', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.WORK_DIR = OUT / 'pack'
module.RETOC = CSS / 'build/retoc-css-target/release/retoc'
sys.argv = [str(script), '--nextgen', '--candidate-only', '--windows-root', 'C:/CSS',
            '--candidate-output', str(OUT / 'trio'), '--cook-output', str(OUT / 'cooked'),
            '--material-plan', str(OUT / 'plan/plan.json'), '--metadata', str(meta)]
module.main()
assert all(digest(Path(p)) == h for p, h in protected.items())
(OUT / 'report.json').write_text(json.dumps(dict(passed=True, packages=127, installed=False,
    candidate=str(OUT / 'trio'), copied=copied,
    container_hashes={p.name: digest(p) for p in (OUT / 'trio').iterdir()}), indent=2) + '\n')
print('SHORT_PACKAGE_PREPARED', OUT, flush=True)
