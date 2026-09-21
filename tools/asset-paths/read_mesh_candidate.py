"""Independently decode and verify the isolated corrected heel mesh cook."""
import json
import os
from pathlib import Path
import shutil
import subprocess

CSS = Path(__file__).resolve().parents[2]
ROOT = CSS.parent
OUT = CSS / 'work/paths/rig2'
PRIOR = CSS / 'work/paths/rig1'
load = lambda p: json.loads(p.read_text())
assert load(OUT / 'cook-exit.json')['exit_code'] == 0
assert load(OUT / 'bound.json')['passed']
assert not (OUT / 'decoded').exists() and not (OUT / 'stage').exists()


def run(label, args, env=None):
    (OUT / (label + '.json')).write_text(json.dumps(args, indent=2) + '\n')
    with (OUT / (label + '.log')).open('w') as log:
        result = subprocess.run(args, env=env, stdout=log, stderr=subprocess.STDOUT)
    (OUT / (label + '-exit.json')).write_text(json.dumps(dict(exit_code=result.returncode)) + '\n')
    assert result.returncode == 0, (label, result.returncode)


stage = OUT / 'stage'
stage.mkdir()
content = OUT / 'cooked/CSSAuthoring/Content'
assert len(list(content.rglob('*.uasset'))) == 1
for file in content.rglob('*'):
    if file.is_file():
        target = stage / 'MortalShell2/Content' / file.relative_to(content)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
shutil.copy2(OUT / 'cooked/CSSAuthoring/Metadata/scriptobjects.bin', stage / 'scriptobjects.bin')
retoc = str(CSS / 'build/retoc-css-target/release/retoc')
container = OUT / 'ShortMesh_P.utoc'
run('pack', [retoc, 'to-zen', str(stage), str(container), '--version', 'UE5_6', '--no-parallel'])
run('verify', [retoc, 'verify', str(container)])
containers = OUT / 'containers'
containers.mkdir()
for file in (PRIOR / 'containers').iterdir():
    assert file.resolve().is_file(), file
    (containers / file.name).symlink_to(file.resolve())
for file in OUT.glob('ShortMesh_P.*'):
    (containers / file.name).symlink_to(file)
args = load(PRIOR / 'mesh-decode.json')
args = [v.replace(str(PRIOR / 'containers'), str(containers))
        .replace(str(PRIOR / 'decoded'), str(OUT / 'decoded'))
        .replace('/Game/CSS/SeduXtress/SK_BlackPearl', '/Game/CSS/SeduXtress/SK_BlackPearl2') for v in args]
run('mesh-decode', args, dict(os.environ, SBMESH_FORMAT='psk'))
assert all(r['Success'] for r in load(OUT / 'decoded/export-results.json'))
args = load(PRIOR / 'geometry.json')
args = [v.replace(str(PRIOR), str(OUT)) for v in args]
(OUT / 'tmp').mkdir(exist_ok=True)
run('geometry', args)
args = load(PRIOR / 'mesh-check.json')
args = [v.replace(str(PRIOR), str(OUT)) for v in args]
run('mesh-check', args, dict(os.environ, CSS_CLOSURE_WORK=str(OUT)))
print('CORRECTED_MESH_READBACK_PASSED', flush=True)
