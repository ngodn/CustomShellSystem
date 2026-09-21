"""Install the reviewed lighting/shell-list core through a normal restart. Python 3.14."""
import json
from pathlib import Path
import shutil
import subprocess
import time

from css import ROOT, GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha
from css_live_snapshot import Probe
from check_inventory_camera import checked_call

out=ROOT/'work/ui-live1'
runtime=GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
load=lambda p:json.loads(p.read_text())
prior=load(ROOT/'work/paths/live1/deployment.json')
selector=load(runtime/'core.json')
assert selector==prior['selector'] and sha(runtime/'cores'/selector['file'])==prior['core_sha256']
assert not out.exists() and len(processes())==1
assert not (runtime/'cssx.json').exists() and not (runtime/'cores/cssx_core.dll').exists()
assert sha(GAME/'Binaries/Win64/ue4ss/UE4SS.dll')==EXPECTED_UE4SS
package=GAME/'Content/Paks/~mods/CSS_SeduXtress_eins0fx_P'
hashes=lambda directory:{str(p.relative_to(directory)):sha(p) for p in directory.rglob('*') if p.is_file()}
assert hashes(package)==prior['package_hashes']
for file in ('light-y1/host2-exit.json','light-y1/tests-exit.json',
             'favorites1/host-exit.json','favorites1/tests-exit.json','favorites1/windows-exit.json'):
    assert load(ROOT/'work'/file)['exit_code']==0,file
name='css_core-ui1.dll'
assert not (runtime/'cores'/name).exists()
out.mkdir()
copy_verified(ROOT/'build/windows/css_core.dll',out/'core.dll')
copy_verified(runtime/'cores'/selector['file'],out/'old-core.dll')
copy_verified(runtime/'core.json',out/'old-selector.json')
shutil.copytree(runtime/'state',out/'state-before')
before=processes()
with (out/'quit.requests.jsonl').open('x') as log:
    probe=Probe(log)
    player=probe.send('player');pc=player['controller'];assert pc
    library=probe.send('class_default',**{'class':'KismetSystemLibrary'})
    try:
        checked_call(probe,library,'QuitGame',WorldContextObject=pc,SpecificPlayer=pc,
                     QuitPreference=0,bIgnorePlatformRestrictions=False)
    except TimeoutError:
        pass
deadline=time.monotonic()+45
while processes() and time.monotonic()<deadline:time.sleep(.5)
assert not processes(),'Normal quit incomplete; nothing installed'
shutil.copytree(runtime/'state',out/'state-after')
state=hashes(runtime/'state')
try:
    copy_verified(out/'core.dll',runtime/'cores'/name)
    atomic(runtime/'core.json',dict(abi=selector['abi'],file=name))
    assert sha(runtime/'cores'/name)==sha(out/'core.dll')
    assert hashes(package)==prior['package_hashes'] and hashes(runtime/'state')==state
except Exception:
    copy_verified(out/'old-selector.json',runtime/'core.json')
    raise
atomic(out/'deployment.json',dict(previous_selector=selector,selector=load(runtime/'core.json'),
    core_sha256=sha(out/'core.dll'),package_sha256=hashes(package),state_sha256=state,
    before_pids=before,normal_restart=True,cssx_disabled=True,live_verified=False))
with (out/'launch.log').open('x') as log:
    result=subprocess.run(['steam','steam://rungameid/2584270'],stdout=log,stderr=subprocess.STDOUT,timeout=20)
atomic(out/'launch-exit.json',dict(exit_code=result.returncode))
assert result.returncode==0
print('UI trial installed; normal Steam launch requested.',flush=True)
