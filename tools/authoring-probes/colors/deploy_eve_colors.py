"""Install verified Eve color metadata and the swatch core with a normal restart. Python 3.14."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
from css import GAME, EXPECTED_UE4SS, atomic, copy_verified, processes, sha
from css_live_snapshot import Probe
from check_inventory_camera import checked_call
from css_package import verify

load=lambda p:json.loads(p.read_text())
hashes=lambda d:{str(p.relative_to(d)):sha(p) for p in d.rglob('*') if p.is_file()}
out=CSS/'work/colors1/live';assert not out.exists()
runtime=GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
package=GAME/'Content/Paks/~mods/CSS_SeduXtress_eins0fx_P'
mod=CSS.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
candidate=mod/'work/colors3/trio'
prior=load(CSS/'work/ground1/live/deployment.json')
selector=load(runtime/'core.json')
assert selector==prior['selector'] and sha(runtime/'cores'/selector['file'])==prior['core_sha256']
assert hashes(package)==prior['package_sha256']
assert load(mod/'work/colors3/verification.json')['passed']
assert hashes(candidate)==load(mod/'work/colors3/verification.json')['hashes']
assert verify(candidate)['catalog']['outfits'][0]['variants'][0]['ground_offset_cm']==-3
assert not (runtime/'cssx.json').exists() and not (runtime/'cores/cssx_core.dll').exists()
assert sha(GAME/'Binaries/Win64/ue4ss/UE4SS.dll')==EXPECTED_UE4SS
for name in ('tests','python','windows','shipping','native-package'):
    assert load(CSS/f'work/colors1/{name}-exit.json')['exit_code']==0
assert len(processes())==1
name='css_core-colors1.dll';assert not (runtime/'cores'/name).exists()
out.mkdir();copy_verified(CSS/'build/windows/css_core.dll',out/'core.dll')
copy_verified(runtime/'cores'/selector['file'],out/'old-core.dll')
copy_verified(runtime/'core.json',out/'old-selector.json')
shutil.copytree(package,out/'old-package');shutil.copytree(runtime/'state',out/'state-before')
before=processes()
with (out/'quit.requests.jsonl').open('x') as log:
    p=Probe(log);pc=p.send('player')['controller'];assert pc
    library=p.send('class_default',**{'class':'KismetSystemLibrary'})
    try:
        checked_call(p,library,'QuitGame',WorldContextObject=pc,SpecificPlayer=pc,
            QuitPreference=0,bIgnorePlatformRestrictions=False)
    except TimeoutError:
        pass
deadline=time.monotonic()+45
while processes() and time.monotonic()<deadline:time.sleep(.5)
assert not processes(),'Normal exit did not complete; nothing installed'
shutil.copytree(runtime/'state',out/'state-after');state=hashes(runtime/'state')
try:
    for file in candidate.iterdir():copy_verified(file,package/file.name)
    copy_verified(out/'core.dll',runtime/'cores'/name)
    atomic(runtime/'core.json',dict(abi=selector['abi'],file=name))
    assert hashes(package)==hashes(candidate) and hashes(runtime/'state')==state
    assert sha(runtime/'cores'/name)==sha(out/'core.dll')
except Exception:
    for file in (out/'old-package').iterdir():copy_verified(file,package/file.name)
    copy_verified(out/'old-selector.json',runtime/'core.json')
    raise
atomic(out/'deployment.json',dict(selector=load(runtime/'core.json'),core_sha256=sha(out/'core.dll'),
    package_sha256=hashes(package),state_sha256=state,before_pids=before,
    normal_restart=True,cssx_disabled=True,live_verified=False))
with (out/'launch.log').open('x') as log:
    r=subprocess.run(['steam','steam://rungameid/2584270'],stdout=log,stderr=subprocess.STDOUT,timeout=20)
atomic(out/'launch-exit.json',dict(exit_code=r.returncode));assert r.returncode==0
print('Color core and package installed; normal Steam launch requested.')
