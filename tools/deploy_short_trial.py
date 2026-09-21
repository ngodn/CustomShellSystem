"""Install the verified short-path trial while the game is closed, then launch normally."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

CSS = Path(__file__).resolve().parents[1]
OUT = CSS / 'work/paths/live1'
sys.path.insert(0, str(CSS / 'tools'))
from css import GAME, atomic, copy_verified, processes, sha, EXPECTED_UE4SS
from css_package import verify

load = lambda p: json.loads(p.read_text())
assert not OUT.exists() and not processes(), 'Require a closed game and unused evidence directory'
checked = load(CSS / 'work/paths/final1/verified.json')
assert checked['passed']
candidate = Path(checked['candidate'])
verify(candidate)
prepared = load(candidate.parent / 'report.json')
assert {p.name: sha(p) for p in candidate.iterdir()} == prepared['container_hashes']
baseline = load(CSS / 'docs/development/v44-accepted-baseline.json')
runtime = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
target = GAME / 'Content/Paks/~mods/CSS_SeduXtress_eins0fx_P'
assert {p.name: sha(p) for p in target.iterdir()} == baseline['package_sha256']
selector = load(runtime / 'core.json')
assert selector == baseline['selector']
old_core = runtime / 'cores' / selector['file']
assert sha(old_core) == baseline['core_sha256']
assert not (runtime / 'cssx.json').exists() and not (runtime / 'cores/cssx_core.dll').exists()
ue4ss = GAME / 'Binaries/Win64/ue4ss/UE4SS.dll'
assert sha(ue4ss) == EXPECTED_UE4SS
assert load(CSS / 'work/paths/rig1/native-build-exit.json')['exit_code'] == 0
new_core = CSS / 'build/windows/css_core.dll'
core_name = 'css_core-short1.dll'
assert not (runtime / 'cores' / core_name).exists()
OUT.mkdir(parents=True)
shutil.copytree(target, OUT / 'backup/outfit')
shutil.copytree(runtime / 'state', OUT / 'backup/state')
copy_verified(runtime / 'core.json', OUT / 'backup/core.json')
copy_verified(old_core, OUT / 'backup/core-v44.dll')
copy_verified(new_core, OUT / 'core.dll')
state = {str(p.relative_to(runtime / 'state')): sha(p) for p in (runtime / 'state').rglob('*') if p.is_file()}
assert not processes(), 'Game started during preparation; nothing installed'
try:
    for name in prepared['container_hashes']:
        copy_verified(candidate / name, target / name)
    copy_verified(OUT / 'core.dll', runtime / 'cores' / core_name)
    atomic(runtime / 'core.json', dict(abi=selector['abi'], file=core_name))
    assert {p.name: sha(p) for p in target.iterdir()} == prepared['container_hashes']
    assert sha(runtime / 'cores' / core_name) == sha(OUT / 'core.dll')
    assert state == {str(p.relative_to(runtime / 'state')): sha(p) for p in (runtime / 'state').rglob('*') if p.is_file()}
    assert sha(ue4ss) == EXPECTED_UE4SS
except Exception:
    for name in baseline['package_sha256']:
        copy_verified(OUT / 'backup/outfit' / name, target / name)
    copy_verified(OUT / 'backup/core.json', runtime / 'core.json')
    raise
atomic(OUT / 'deployment.json', dict(package_hashes=prepared['container_hashes'],
    selector=load(runtime / 'core.json'), core_sha256=sha(OUT / 'core.dll'),
    previous_selector=selector, state_hashes=state, cssx_disabled=True,
    game_was_closed=True, live_verified=False))
with (OUT / 'launch.log').open('x') as log:
    result = subprocess.run(['steam', 'steam://rungameid/2584270'], stdout=log, stderr=subprocess.STDOUT, timeout=20)
atomic(OUT / 'launch-exit.json', dict(exit_code=result.returncode))
assert result.returncode == 0
print('Short-path trial installed; normal Steam launch requested.', flush=True)
