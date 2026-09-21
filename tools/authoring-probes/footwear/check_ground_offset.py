"""Record a bounded world mesh-offset trial, then restore it. Python 3.14.

Coordinate movement with the player before running. No movement input, capsule
changes or persistent outfit settings. Requires the installed short1 mesh.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
from css import atomic, processes
import css_capture as capture
from css_live_snapshot import Probe
from check_inventory_camera import checked_get,checked_call

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--window-id',required=True)
args=parser.parse_args();out=args.output.resolve()
assert out.is_relative_to(CSS/'work') and not out.exists() and len(processes())==1
out.mkdir();capture.MEDIA=out
assert not capture.command('inventory_inspect')['css']['menu_open']
with (out/'requests.jsonl').open('x') as log:
    p=Probe(log);player=p.send('player');mesh=checked_get(p,player['pawn'],'Mesh')
    asset=checked_get(p,mesh,'SkeletalMesh')
    assert asset['name']=='SkeletalMesh /Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2'
    before=checked_get(p,mesh,'RelativeLocation')
    assert before==dict(X=0,Y=0,Z=-96),before
    target=dict(before,Z=before['Z']-3)
    # Save the exact rollback before the first write.
    baseline=dict(player=player,mesh=mesh,asset=asset,before=before,target=target)
    atomic(out/'baseline.json',baseline)
    capture.shot('before')
    report=dict(lowering_cm=3,restored=False)
    try:
        checked_call(p,mesh,'K2_SetRelativeLocation',NewLocation=target,bSweep=False,bTeleport=True)
        time.sleep(.5)
        assert checked_get(p,mesh,'RelativeLocation')==target
        capture.shot('lowered')
        result=subprocess.run([sys.executable,str(CSS/'tools/css_window_clip.py'),
            '--window-id',args.window_id,'--world','--seconds','20',
            '--output',str(out/'world.mp4')],check=False)
        report['record_exit']=result.returncode
        assert result.returncode==0
        report['after_movement']=checked_get(p,mesh,'RelativeLocation')
        assert report['after_movement']==target,'Game replaced the trial offset'
    finally:
        # Avoid changing a replacement pawn or a transform another owner wrote.
        if p.send('player')==player and p.send('valid',target=mesh):
            current=checked_get(p,mesh,'RelativeLocation')
            if current==target and checked_get(p,mesh,'SkeletalMesh')==asset:
                checked_call(p,mesh,'K2_SetRelativeLocation',NewLocation=before,bSweep=False,bTeleport=True)
                report['restored']=checked_get(p,mesh,'RelativeLocation')==before
            elif current==before:
                report['restored']=True
            else:
                report['cleanup_skipped']='Mesh or transform changed ownership; inspect before writing.'
        else:
            report['cleanup_skipped']='Player/component changed during trial.'
        atomic(out/'result.json',report)
    assert report['restored'],report
    capture.shot('restored')
print(json.dumps(report))
