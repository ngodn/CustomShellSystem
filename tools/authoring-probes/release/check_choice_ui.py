"""Exercise shared CSS choice scrolling and contextual footers. Python 3.14."""
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
from check_inventory_camera import checked_call,checked_get

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();out=args.output.resolve()
assert out.is_relative_to(CSS/'work') and not out.exists()
out.mkdir();c.MEDIA=out
read=lambda p:json.loads(p.read_text())
save=lambda name,data:(out/name).write_text(json.dumps(data,indent=2)+'\n')
state_path=c.MOD/'state/state.json'
before=read(state_path);status=c.command('status');shell=status['shell']
selected=before['selections'][shell];assert selected['outfit']!='css.original_shells'
menu=c.command('inventory_inspect');assert menu['css']['active'],'CSS must be open and focused; leave controls idle'
save('state-before.json',before);save('menu-before.json',menu)
view=lambda info:{k:info['css'][k] for k in ('yaw','zoom','pan','frame')}
original_view=view(menu)
def applied(variant):
    return wait_json(c.MOD/'runtime/status.json',lambda j:j.get('applied')=='css.original_shells/'+variant)

with (out/'requests.jsonl').open('x') as log:
    p=Probe(log);player=p.send('player');results={}
    try:
        c.row(0,2);first=c.command('inventory_inspect');save('first.json',first)
        assert first['css']['choice_list']['count']==10
        assert first['css']['choice_list']['offset']==0
        c.shot('shell-list')
        hit=next(h for h in first['hit_points'] if h['action'].get('action')=='select' and h['action'].get('variant')=='tiel')
        c.command('inventory_test_pointer',point=hit['pixel'])
        c.command('inventory_test_mouse',event='wheel',delta=-1200);time.sleep(1)
        scrolled=c.command('inventory_inspect');save('scrolled.json',scrolled)
        assert scrolled['css']['choice_list']['offset']>0,'Wheel did not scroll the choices'
        assert view(scrolled)==original_view,'Scrolling the choices moved the preview'
        c.row(0,2);rebuilt=c.command('inventory_inspect')
        assert abs(rebuilt['css']['choice_list']['offset']-scrolled['css']['choice_list']['offset'])<1,'Rebuild lost scroll position'
        c.click('select',outfit='css.original_shells',variant='solomon');applied('solomon')
        last=c.command('inventory_inspect');save('last.json',last)
        assert last['css']['choice_list']['selected']=='solomon'
        assert last['css']['choice_list']['offset']>0
        c.shot('last-choice')
        c.key('D');applied('tiel')
        wrapped=c.command('inventory_inspect');save('wrapped.json',wrapped)
        assert wrapped['css']['choice_list']['selected']=='tiel'
        assert wrapped['css']['choice_list']['offset']<last['css']['choice_list']['offset'],'Selected first choice was not revealed'
        c.shot('wrapped-choice')
        c.key('A');applied('solomon')
        assert c.command('inventory_inspect')['css']['choice_list']['offset']>0,'Selected last choice was not revealed'
        results.update(wheel_scroll=True,preview_unchanged=True,rebuild_preserved_scroll=True,
                       last_choice_click=True,keyboard_wrap_and_reveal=True)
    finally:
        if p.send('player')==player:
            c.command('select',outfit=selected['outfit'],variant=selected['variant'])
            wait_json(c.MOD/'runtime/status.json',lambda j:j.get('applied')==selected['outfit']+'/'+selected['variant'])
            c.motion(**original_view,seconds=.2)
    # Inspect another consumer without wearing a different outfit.
    for row in range(3,menu['css']['rows']):
        c.row(0,row);variant_menu=c.command('inventory_inspect')
        choice=variant_menu['css'].get('choice_list')
        if choice and choice['key']!='css.original_shells' and choice['count']>1:
            save('variant-list.json',variant_menu);c.shot('variant-list')
            results['shared_variant_list']=choice['key'];break
    assert results.get('shared_variant_list'),'No installed multi-variant outfit for the shared renderer check'
    for section,name in [(1,'customize-footer'),(2,'locomotion-footer')]:
        c.row(section,0);c.shot(name)
    save('interaction-result.json',results)
    # Exercise actual menu teardown/recreation, without gameplay movement.
    # Keyboard key-up is supported after CSS closes. The mouse test hook requires
    # an active CSS page and can reject button-up after the close transition.
    c.key('Escape');time.sleep(.8)
    assert not c.command('inventory_inspect')['css']['active']
    handler=checked_get(p,player['controller'],'User Interface Handler Component')
    checked_call(p,handler,'HandleGameMenu',SubTabIndex=0,AllowClose=False);time.sleep(1)
    c.command('inventory_select',index=1);time.sleep(1)
    c.row(0,2);reopened=c.command('inventory_inspect');save('reopened.json',reopened)
    assert reopened['css']['active'] and reopened['css']['choice_list']['count']==10
    assert p.send('player')==player
    after=read(state_path);save('state-after.json',after)
    assert after==before,'UI check changed the saved state'
    c.shot('reopened')
    results.update(menu_reentry=True,state_restored=True,player_preserved=True,passed=True)
    save('result.json',results);print(json.dumps(results))
