"""Verify preview lighting across tab exit and menu recreation. Python 3.14.

Requires CSS open with idle controls. Changes only preview lighting and menu
navigation, then leaves CSS open with its original section and row selected.
"""
import argparse
import json
import math
from pathlib import Path
import time

from css import ROOT, atomic, processes
import css_capture as capture
from css_live_snapshot import Probe
from check_inventory_camera import checked_get, checked_call

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();out=args.output.resolve()
assert out.is_relative_to(ROOT/'work') and not out.exists() and len(processes())==1
out.mkdir();capture.MEDIA=out
entry=capture.command('inventory_inspect');assert entry['css']['active']
atomic(out/'entry.json',entry)
def command(action,**fields):
    r=capture.command(action,**fields)
    with (out/'commands.jsonl').open('a') as f:f.write(json.dumps(dict(action=action,fields=fields,reply=r))+'\n')
    return r
def settled(active):
    deadline=time.monotonic()+5
    while True:
        r=command('inventory_inspect')
        if r['css']['active']==active:return r
        assert time.monotonic()<deadline,'CSS transition did not settle'
        time.sleep(.25)
with (out/'requests.jsonl').open('x') as log:
    p=Probe(log);player=p.send('player');pc=player['controller']
    handler=checked_get(p,pc,'User Interface Handler Component')
    def light(info):
        display=p.send('find',path=info['display']['path'])
        return checked_get(p,display,'RectLight_Left')
    def sample(obj):
        return {name:checked_get(p,obj,name) for name in ('RelativeLocation','RelativeRotation','Intensity')}
    def matches(a,b):
        assert a['Intensity']==b['Intensity']
        errors={}
        for field,tolerance in (('RelativeLocation',1e-7),('RelativeRotation',1e-8)):
            errors[field]=max(abs(a[field][k]-b[field][k]) for k in a[field])
            assert math.isfinite(errors[field]) and errors[field]<=tolerance,(field,errors[field])
        return errors
    command('inventory_light_stop');original=light(entry);base=sample(original)
    atomic(out/'baseline.json',dict(light=original,values=base))
    results={}
    try:
        command('inventory_light_start');command('inventory_light_move',horizontal=55,vertical=10)
        moved=sample(original);assert moved!=base
        command('inventory_select',index=0);left=settled(False)
        assert not left['light_control']['active']
        assert p.send('valid',target=original),'Tab change destroyed the expected display light'
        results['tab_restore']=matches(sample(original),base)
        command('inventory_select',index=1);returned=settled(True)
        current=light(returned);results['tab_reentry']=matches(sample(current),base)
        command('inventory_light_start');command('inventory_light_move',horizontal=-55,vertical=-10)
        assert sample(current)!=base
        checked_call(p,handler,'HandleGameMenu',SubTabIndex=0,AllowClose=True)
        closed=settled(False);assert not closed['css']['menu_open'] and not closed['light_control']['active']
        old_valid=p.send('valid',target=current)
        results['old_light_valid_after_close']=old_valid
        if old_valid:
            owner=checked_call(p,current,'GetOwner')['ReturnValue']
            destroying=checked_call(p,current,'IsBeingDestroyed')['ReturnValue']
            owner_destroying=checked_call(p,owner,'IsActorBeingDestroyed')['ReturnValue'] if owner else None
            results['closed_light_destruction']=dict(component=destroying,owner=owner_destroying,has_owner=bool(owner))
            # Destruction can precede weak-handle invalidation. A discarded
            # component's detached transform is not a reusable preview state.
            if owner and not destroying and not owner_destroying:
                results['close_restore']=matches(sample(current),base)
        checked_call(p,handler,'HandleGameMenu',SubTabIndex=0,AllowClose=False)
        time.sleep(.4);command('inventory_select',index=1);reopened=settled(True)
        fresh=light(reopened);assert fresh!=current,'Expected a recreated preview light'
        results['new_light']=fresh;results['recreated_default']=matches(sample(fresh),base)
        assert not reopened['light_control']['active']
        assert p.send('player')==player
    finally:
        atomic(out/'results.json',results)
        status=command('inventory_inspect')
        if status['css']['active']:command('inventory_light_stop')
    command('inventory_capture_row',section=entry['css']['section'],row=entry['css']['row'],offset=0)
    time.sleep(.3);capture.shot('reopened')
    atomic(out/'verification.json',dict(passed=True,results=results,
        scope='One tab exit/reentry and one full menu recreation. Does not cover input remapping, focus changes or performance.'))
print(json.dumps(dict(output=str(out),passed=True,results=results)))
