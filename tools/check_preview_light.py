"""Check actual preview light/camera transforms and retain Steam stills. Python 3.14.

Requires an open CSS preview with idle controls. Resets preview lighting before
and after the check. Never moves the player or sends gameplay input.
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
info=capture.command('inventory_inspect');assert info['css']['active'] and info.get('camera_child')
atomic(out/'entry.json',info)
failures=[];states={}
def command(action,**fields):
    reply=capture.command(action,**fields)
    with (out/'commands.jsonl').open('a') as log:
        log.write(json.dumps(dict(action=action,fields=fields,reply=reply))+'\n')
    return reply
with (out/'requests.jsonl').open('x') as log:
    p=Probe(log);player=p.send('player');pc=player['controller']
    manager=checked_get(p,pc,'PlayerCameraManager')
    display=p.send('find',path=info['display']['path'])
    lights={name:checked_get(p,display,name) for name in
            ('RectLight_Left','RectLight_AxeLight','SpotLight_Fill','SpotLight_Rim')}
    character=checked_get(p,display,'CharacterChildActor')
    def call(target,fn,**kwargs):return checked_call(p,target,fn,**kwargs)['ReturnValue']
    def state(label):
        value={'camera':{'location':call(manager,'GetCameraLocation'),
                         'rotation':call(manager,'GetCameraRotation'),'fov':call(manager,'GetFOVAngle')},
               'pivot':call(character,'K2_GetComponentLocation'),'lights':{}}
        for name,obj in lights.items():
            value['lights'][name]={'relative_location':checked_get(p,obj,'RelativeLocation'),
                'relative_rotation':checked_get(p,obj,'RelativeRotation'),
                'world':call(obj,'K2_GetComponentLocation'),'forward':call(obj,'GetForwardVector'),
                'intensity':checked_get(p,obj,'Intensity')}
        assert p.send('player')==player
        states[label]=value;atomic(out/(label+'.json'),value);return value
    try:
        command('inventory_light_stop');time.sleep(.3)
        base=state('baseline');capture.shot('baseline')
        command('inventory_light_start')
        for name,h,v in [('right',60,0),('left',-120,15)]:
            command('inventory_light_move',horizontal=h,vertical=v)
            command('inventory_test_motion',movement=[20,.2,10,5])
            time.sleep(.3)
            current=state(name);capture.shot(name)
            if current['camera']!=base['camera']:failures.append(name+': camera changed while lighting controls were active')
            for key in lights:
                if key!='RectLight_Left' and current['lights'][key]!=base['lights'][key]:
                    failures.append(name+': secondary light changed: '+key)
            old=base['lights']['RectLight_Left'];new=current['lights']['RectLight_Left']
            if math.dist([old['world'][k] for k in 'XYZ'],[new['world'][k] for k in 'XYZ'])<1:failures.append(name+': primary light did not move')
            def radius(sample):
                return math.dist([sample['lights']['RectLight_Left']['world'][k] for k in 'XYZ'],[sample['pivot'][k] for k in 'XYZ'])
            if abs(radius(current)-radius(base))>.001:failures.append(name+': orbit radius changed')
            if new['intensity']!=old['intensity']:failures.append(name+': intensity changed')
    finally:
        command('inventory_light_stop')
    restored=state('restored')
    # UE derives the relative transform through world-space setters. Double
    # subtraction and quaternion conversion can change its last few bits.
    # Keep exact equality for untouched state; bound only the restored light.
    original_light=base['lights']['RectLight_Left'];restored_light=restored['lights']['RectLight_Left']
    errors={}
    for field,tolerance in (('relative_location',1e-7),('relative_rotation',1e-8),('world',1e-7),('forward',1e-12)):
        errors[field]=max(abs(restored_light[field][key]-value) for key,value in original_light[field].items())
        if not math.isfinite(errors[field]) or errors[field]>tolerance:failures.append('Reset differs beyond numeric precision: '+field)
    if restored_light['intensity']!=original_light['intensity']:failures.append('Reset changed intensity')
    untouched=dict(restored,lights=dict(restored['lights'],RectLight_Left=original_light))
    if untouched!=base:failures.append('Reset changed the camera, pivot or secondary lights')
    atomic(out/'verification.json',dict(failures=failures,passed=not failures,
        restore_error=errors,scope='Two diagnostic orbit positions, actual camera readback and reset. Physical controls, focus loss and menu recreation need separate evidence.'))
print(json.dumps(dict(output=str(out),failures=failures)))
raise SystemExit(bool(failures))
