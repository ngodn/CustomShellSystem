"""Review all official appearances through CSS, then restore the current look. Python 3.14."""
import argparse
import json
from pathlib import Path
import sys
import time

CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
import css_capture as c
from css import wait_json
from css_live_snapshot import Probe
from check_inventory_camera import checked_call, checked_get

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--expected',type=Path,required=True,help='Independent cooked-reference names, IDs and mesh paths')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
out=args.output.resolve()
assert out.is_relative_to(CSS/'work') and not out.exists()
out.mkdir();c.MEDIA=out
state_path=c.MOD/'state/state.json'
read=lambda p:json.loads(p.read_text())
save=lambda name,value:(out/name).write_text(json.dumps(value,indent=2)+'\n')
status=c.command('status');expected=read(args.expected)
assert status['catalog']['original_shell_choices']==expected,'Discovery differs from cooked definitions'
menu=c.command('inventory_inspect');assert menu['css']['active'],'Open CSS first and leave controls idle'
before=read(state_path);save('state-before.json',before);save('menu-before.json',menu)
shell=status['shell'];selected=before['selections'][shell]
assert selected['outfit']!='css.original_shells','Start from the custom outfit to verify restoration'
assert before['remembered_custom'][selected['outfit']]==selected['customize'],'Current customization is not remembered'

with (out/'requests.jsonl').open('x') as log:
    p=Probe(log);player=p.send('player');mesh=checked_get(p,player['pawn'],'Mesh')
    def identity():
        result={'player':p.send('player')}
        for key in ('CharacterData','AbilitySystemComponent','CapsuleComponent','CharacterMovement'):
            result[key]=checked_get(p,player['pawn'],key)
        result['anim_class']=checked_get(p,mesh,'AnimClass')
        return result
    original_identity=identity();save('identity-before.json',original_identity)
    original_location=checked_get(p,mesh,'RelativeLocation')
    results=[]
    try:
        c.row(0,2)
        view=menu['css']
        c.motion(yaw=view['yaw'],zoom=-.45,pan=view['pan'],frame=view['frame'],seconds=.2)
        c.shot('selector')
        for choice in expected:
            assert p.send('player')==player,'Player changed; stop the trial'
            # Exercise the actual menu hit binding, not just the select request handler.
            c.click('select',outfit='css.original_shells',variant=choice['id'])
            active=wait_json(c.MOD/'runtime/status.json',lambda j:j.get('applied')=='css.original_shells/'+choice['id'])
            actual=checked_call(p,mesh,'GetSkeletalMeshAsset')['ReturnValue']
            assert actual['name'].split(' ',1)[1]==choice['mesh'],choice
            assert identity()==original_identity,'Gameplay identity or animation class changed'
            current=c.command('inventory_inspect')
            display=p.send('find',path=current['display']['path'])
            actor=checked_call(p,display,'GetDisplayMenuCharacter')['ReturnValue']
            preview=checked_get(p,actor,'Mesh')
            visible=checked_call(p,preview,'GetSkeletalMeshAsset')['ReturnValue']
            assert visible==actual,'Menu and world meshes differ'
            assert not active['maintenance_error'] and not active['inventory_failed']
            c.shot(choice['id'])
            results.append(dict(choice=choice,world_mesh=actual,preview_mesh=visible,identity_preserved=True))
            save('choices.json',results)
    finally:
        if p.send('player')==player:
            c.command('select',outfit=selected['outfit'],variant=selected['variant'])
            wait_json(c.MOD/'runtime/status.json',lambda j:j.get('applied')==selected['outfit']+'/'+selected['variant'])
            view=menu['css']
            c.motion(yaw=view['yaw'],zoom=view['zoom'],pan=view['pan'],frame=view['frame'],seconds=.2)
            c.row(view['section'],view['row']);time.sleep(.5)
    after=read(state_path);save('state-after.json',after)
    assert after['selections']==before['selections'],'Selections were not restored'
    assert after['favorites']==before['favorites'] and after['presets']==before['presets'],'Favorites or profiles changed'
    for key,value in before['remembered_custom'].items():
        assert after['remembered_custom'][key]==value,'A remembered custom look changed'
    assert identity()==original_identity
    assert checked_get(p,mesh,'RelativeLocation')==original_location,'Ground offset changed after restoring the outfit'
    c.shot('restored')
    save('result.json',dict(passed=True,appearances=len(results),identity_preserved=True,selections_restored=True,
        favorites_preserved=True,profiles_preserved=True,ground_offset_restored=True,
        added_empty_official_memory=after['remembered_custom'].get('css.original_shells')))
    print('Verified all official appearance menu bindings and restored the original look.')
