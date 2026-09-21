"""Check CSS disable/re-enable restores world height without losing the saved look."""
import json
from pathlib import Path
import sys
import time

CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
from css import GAME,atomic
import css_capture as capture
from css_live_snapshot import Probe
from check_inventory_camera import checked_get

out=CSS/'work/ground1/live/restore';assert not out.exists();out.mkdir()
state_file=GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem/state/state.json'
state=json.loads(state_file.read_text());assert state['enabled'];atomic(out/'state-before.json',state)
with (out/'requests.jsonl').open('x') as log:
    p=Probe(log);player=p.send('player');mesh=checked_get(p,player['pawn'],'Mesh')
    asset=checked_get(p,mesh,'SkeletalMesh')
    assert asset['name']=='SkeletalMesh /Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2'
    assert checked_get(p,mesh,'RelativeLocation')['Z']==-99
    def wait_height(z,original):
        end=time.monotonic()+10
        while True:
            assert p.send('player')==player
            row=dict(location=checked_get(p,mesh,'RelativeLocation'),asset=checked_get(p,mesh,'SkeletalMesh'))
            if row['location']['Z']==z and (row['asset']==asset)!=original:return row
            assert time.monotonic()<end,row
            time.sleep(.2)
    results={}
    try:
        atomic(out/'disable.json',capture.command('disable'))
        results['disabled']=wait_height(-96,True)
    finally:
        atomic(out/'enable.json',capture.command('enable'))
        results['enabled']=wait_height(-99,False)
        after=json.loads(state_file.read_text());atomic(out/'state-after.json',after)
        results['saved_state_unchanged']=after==state
        atomic(out/'results.json',results)
    assert results['saved_state_unchanged']
    atomic(out/'verification.json',dict(passed=True,results=results))
print('World height restored and reapplied; saved look preserved.')
