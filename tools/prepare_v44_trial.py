"""Assemble the verified V44 cook with V43 materials and controls, without installing."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

CSS = Path(__file__).resolve().parents[1]
ROOT = CSS.parent
WORK = CSS/'work/grip-grounding-v1'
MOD = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
OUT = MOD/'work/v44-trial-v1'
assert not OUT.exists()
OUT.mkdir()
(OUT/'tmp').mkdir()
tempfile.tempdir = str(OUT/'tmp')
os.environ['TMPDIR'] = str(OUT/'tmp')
load = lambda p: json.loads(p.read_text())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
readback = WORK/'b2-body-bound-cooked-readback-v1'
assert load(readback/'validation.json')['passed']
assert load(WORK/'b2-body-bound-cook-v2/verify-v3.exit.json')['exit_code'] == 0
protected = load(readback/'report.json')['protected_hashes']
assert all(digest(Path(p)) == h for p,h in protected.items())
sys.path.insert(0,str(CSS/'tools'))
from css_package import verify
old_trio = MOD/'work/packaging/hand-v43'
verify(old_trio)
old_hashes = {p.name:digest(p) for p in old_trio.iterdir()}
assert old_hashes == load(WORK/'hand-v43-live/deployment.json')['hashes']
old_plan_dir = MOD/'work/material-stage-hand-v43'
old_plan = load(old_plan_dir/'plan.json')
old_mesh = old_plan['mesh']
mesh = '/Game/CSSAuthoring/DiagnosticReferences/SK_B2PhysicsBound_V1'
plan = copy.deepcopy(old_plan)
plan.update(mesh=mesh, runtime_tested=False,
    source_mesh_sha256=digest(WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'))
for material in plan['materials']:
    for row in material['files'].values():
        source = (old_plan_dir/row['file']).resolve()
        assert source.is_relative_to(ROOT) and digest(source) == row['sha256']
        row['file'] = str(source)
plan_dir = OUT/'plan';plan_dir.mkdir()
packages = (old_plan_dir/'packages.txt').read_text().splitlines()
assert old_mesh in packages
packages.remove(old_mesh)
fresh_cook = WORK/'b2-body-bound-cook-v2'
new_packages = load(fresh_cook/'input.json')['packages']
packages += [p for p in new_packages if p not in packages]
(plan_dir/'plan.json').write_text(json.dumps(plan,indent=2)+'\n')
(plan_dir/'packages.txt').write_text('\n'.join(packages)+'\n')
combined = OUT/'cooked'
copied = {}
for package in packages:
    cook = fresh_cook/'cooked' if package in new_packages else MOD/'work/cooked_hand_v43'
    relative = Path('CSSAuthoring/Content')/package.removeprefix('/Game/')
    for suffix in ('.uasset','.uexp','.ubulk'):
        source = cook/(str(relative)+suffix)
        if suffix in ('.uasset','.uexp'):assert source.is_file(),source
        if not source.exists():continue
        target = combined/(str(relative)+suffix)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target);assert digest(source)==digest(target)
        copied[str(target.relative_to(combined))]=dict(source=str(source),sha256=digest(source))
metadata = combined/'CSSAuthoring/Metadata';metadata.mkdir(parents=True)
shutil.copy2(fresh_cook/'cooked/CSSAuthoring/Metadata/scriptobjects.bin',metadata/'scriptobjects.bin')
meta = OUT/'metadata'
shutil.copytree(MOD/'work/metadata-hand-v43',meta)
catalog = load(meta/'catalog.json')
for outfit in catalog['outfits']:
    for variant in outfit['variants']:
        assert variant['mesh'] == old_mesh+'.'+old_mesh.rsplit('/',1)[1]
        variant['mesh'] = mesh+'.'+mesh.rsplit('/',1)[1]
def accepted_hair(value):
    changed = 0
    if isinstance(value,dict):
        if value.get('id') == 'hair_motion' and value.get('kind') == 'rig':
            assert value['stiffness']['default']==150 and value['damping']['default']==18
            value['stiffness']['default']=200
            value['damping']['default']=24
            changed += 1
        for item in value.values():changed += accepted_hair(item)
    elif isinstance(value,list):
        for item in value:changed += accepted_hair(item)
    return changed
recipe = load(meta/'customize.json')
assert accepted_hair(catalog)==1 and accepted_hair(recipe)==1
for name,value in [('catalog.json',catalog),('customize.json',recipe)]:
    (meta/name).write_text(json.dumps(value,indent=2)+'\n')
script = MOD/'tools/package_seduxtress.py'
spec = importlib.util.spec_from_file_location('v44_packager',script)
module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
module.WORK_DIR = OUT/'packaging-work'
module.RETOC = CSS/'build/retoc-css-target/release/retoc'
candidate = OUT/'candidate'
sys.argv = [str(script),'--nextgen','--candidate-only','--candidate-output',str(candidate),
    '--cook-output',str(combined),'--material-plan',str(plan_dir/'plan.json'),'--metadata',str(meta)]
module.main()
manifest = verify(candidate)
assert all(digest(Path(p))==h for p,h in protected.items())
assert {p.name:digest(p) for p in old_trio.iterdir()}==old_hashes
report = dict(passed=True,candidate=str(candidate),hashes={p.name:digest(p) for p in candidate.iterdir()},
    baseline_hashes=old_hashes,mesh=mesh,packages=packages,copied=copied,
    protected_hashes=protected,accepted_hair_defaults=dict(stiffness=200,damping=24),
    installed=False,game_verified=False)
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('V44_TRIAL_PREPARED',candidate,flush=True)
