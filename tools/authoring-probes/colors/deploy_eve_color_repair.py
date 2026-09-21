"""Install the verified colors4 asset repair without changing the core. Python 3.14."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

CSS = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(CSS / 'tools'))
from css import GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha
from css_live_snapshot import Probe
from check_inventory_camera import checked_call
from css_package import verify

load = lambda p: json.loads(p.read_text())
hashes = lambda d: {str(p.relative_to(d)): sha(p) for p in d.rglob('*') if p.is_file()}
out = CSS / 'work/colors1/repair-live'
assert not out.exists()
runtime = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
package = GAME / 'Content/Paks/~mods/CSS_SeduXtress_eins0fx_P'
mod = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
candidate = mod / 'work/colors4/trio'
prior = load(CSS / 'work/colors1/live/deployment.json')
selector = load(runtime / 'core.json')
assert selector == prior['selector']
assert sha(runtime / 'cores' / selector['file']) == prior['core_sha256']
assert hashes(package) == prior['package_sha256']
verification = load(mod / 'work/colors4/verification.json')
assert verification['checks']['passed'] and hashes(candidate) == verification['hashes']
assert verify(candidate)['catalog']['outfits'][0]['variants'][0]['ground_offset_cm'] == -3
assert not (runtime / 'cssx.json').exists() and not (runtime / 'cores/cssx_core.dll').exists()
assert sha(GAME / 'Binaries/Win64/ue4ss/UE4SS.dll') == EXPECTED_UE4SS
assert len(processes()) == 1
out.mkdir()
shutil.copytree(package, out / 'old-package')
shutil.copytree(runtime / 'state', out / 'state-before')
before = processes()
with (out / 'quit.jsonl').open('x') as log:
    p = Probe(log)
    pc = p.send('player')['controller']
    assert pc
    library = p.send('class_default', **{'class': 'KismetSystemLibrary'})
    try:
        checked_call(p, library, 'QuitGame', WorldContextObject=pc, SpecificPlayer=pc,
                     QuitPreference=0, bIgnorePlatformRestrictions=False)
    except TimeoutError:
        pass
deadline = time.monotonic() + 45
while processes() and time.monotonic() < deadline:
    time.sleep(.5)
assert not processes(), 'Normal exit did not finish; package was not changed'
shutil.copytree(runtime / 'state', out / 'state-after')
state = hashes(runtime / 'state')
try:
    for file in candidate.iterdir():
        copy_verified(file, package / file.name)
    assert hashes(package) == hashes(candidate) and hashes(runtime / 'state') == state
except Exception:
    for file in (out / 'old-package').iterdir():
        copy_verified(file, package / file.name)
    raise
assert load(runtime / 'core.json') == selector
atomic(out / 'deployment.json', dict(selector=selector, core_sha256=prior['core_sha256'],
    package_sha256=hashes(package), state_sha256=state, before_pids=before,
    normal_restart=True, cssx_disabled=True, live_verified=False))
with (out / 'launch.log').open('x') as log:
    result = subprocess.run(['steam', 'steam://rungameid/2584270'], stdout=log, stderr=subprocess.STDOUT, timeout=20)
atomic(out / 'launch-exit.json', dict(exit_code=result.returncode))
assert result.returncode == 0
print('Asset repair installed; normal Steam launch requested.')
