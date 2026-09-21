"""Install the checked V44 trial through a normal, user-authorized game restart."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

CSS = Path(__file__).resolve().parents[1]
ROOT = CSS.parent
WORK = CSS/'work/grip-grounding-v1'
OUT = WORK/'v44-trial-live-v1'
assert not OUT.exists()
OUT.mkdir()
(OUT/'tmp').mkdir()
tempfile.tempdir = str(OUT/'tmp')
sys.path.insert(0,str(CSS/'tools'))
from css import GAME, atomic, sha, processes, copy_verified, EXPECTED_UE4SS
from css_package import verify
from css_live_snapshot import Probe
from check_inventory_camera import checked_call

load = lambda p: json.loads(p.read_text())
mod = ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
prepared = load(mod/'work/v44-trial-v1/report.json')
candidate = Path(prepared['candidate'])
assert prepared['passed'] and candidate == mod/'work/v44-trial-v1/candidate'
assert load(WORK/'v44-final-package-readback-v1/report.json')['passed']
assert all(load(WORK/'v44-core-build-v1'/(name+'.exit.json'))['exit_code']==0
           for name in ('host-build','test','windows-build'))
verify(candidate)
hashes = {p.name:sha(p) for p in candidate.iterdir()}
assert hashes == prepared['hashes']
target = GAME/'Content/Paks/~mods/CSS_SeduXtress_eins0fx_P'
runtime = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
assert {p.name:sha(p) for p in target.iterdir()} == prepared['baseline_hashes']
assert len(processes()) == 1
assert not (runtime/'cssx.json').exists() and not (runtime/'cores/cssx_core.dll').exists()
ue4ss = GAME/'Binaries/Win64/ue4ss/UE4SS.dll'
assert sha(ue4ss)==EXPECTED_UE4SS
selector_bytes = (runtime/'core.json').read_bytes()
selector = json.loads(selector_bytes)
old_core = runtime/'cores'/selector['file']
old_core_hash = sha(old_core)
new_core = CSS/'build/windows/css_core.dll'
new_core_hash = sha(new_core)
assert old_core_hash != new_core_hash
core_name = 'css_core-v44-'+new_core_hash[:16]+'.dll'
(OUT/'core').mkdir()
copy_verified(new_core,OUT/'core'/core_name)
shutil.copytree(target,OUT/'backup/outfit')
shutil.copytree(runtime/'state',OUT/'backup/state-before-quit')
(OUT/'backup/core.json').write_bytes(selector_bytes)
copy_verified(old_core,OUT/'backup'/selector['file'])
before = processes()
with (OUT/'quit.requests.jsonl').open('x') as log:
    probe = Probe(log)
    player = probe.send('player')
    pc = player['controller'];assert pc
    library = probe.send('class_default',**{'class':'KismetSystemLibrary'})
    try:
        checked_call(probe,library,'QuitGame',WorldContextObject=pc,SpecificPlayer=pc,
                     QuitPreference=0,bIgnorePlatformRestrictions=False)
    except TimeoutError:
        pass
deadline = time.monotonic()+45
while processes() and time.monotonic()<deadline:time.sleep(.5)
assert not processes(), 'Normal quit incomplete; nothing installed'
shutil.copytree(runtime/'state',OUT/'backup/state-after-quit')
state = {str(p.relative_to(runtime/'state')):sha(p) for p in (runtime/'state').rglob('*') if p.is_file()}
try:
    for name in hashes:copy_verified(candidate/name,target/name)
    copy_verified(OUT/'core'/core_name,runtime/'cores'/core_name)
    atomic(runtime/'core.json',dict(abi=selector['abi'],file=core_name))
    assert {p.name:sha(p) for p in target.iterdir()}==hashes
    assert sha(runtime/'cores'/core_name)==new_core_hash and sha(ue4ss)==EXPECTED_UE4SS
    assert state=={str(p.relative_to(runtime/'state')):sha(p) for p in (runtime/'state').rglob('*') if p.is_file()}
except Exception:
    for name in hashes:copy_verified(OUT/'backup/outfit'/name,target/name)
    copy_verified(OUT/'backup/core.json',runtime/'core.json')
    raise
atomic(OUT/'deployment.json',dict(before_pids=before,hashes=hashes,core_hash=new_core_hash,
    previous_core_hash=old_core_hash,selector=dict(abi=selector['abi'],file=core_name),
    previous_selector=selector,state_hashes=state,ue4ss_hash=EXPECTED_UE4SS,cssx_disabled=True,
    normal_restart=True,live_gameplay_verified=False))
with (OUT/'steam-launch.log').open('x') as log:
    subprocess.run(['steam','steam://rungameid/2584270'],check=True,stdout=log,stderr=subprocess.STDOUT,timeout=20)
print('Verified V44 trial installed; normal Steam launch requested.',flush=True)
