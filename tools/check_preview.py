#!/usr/bin/env python3
"""Live CSS preview lifecycle regression. Run with the player safely in the world."""
import json
from pathlib import Path
import time
from css import GAME, ROOT, atomic, wait_json


def main():
    mod=GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    def request(action,**values):
        rid=str(time.time_ns())
        atomic(mod/'request.json',{'id':rid,'action':action,**values})
        file=mod/'runtime'/('inspection.json' if action=='inspect' else 'status.json')
        key='id' if action=='inspect' else 'last_request'
        return wait_json(file,lambda j:j.get(key)==rid)
    original=request('status')
    baseline=request('inspect')
    results=[]
    try:
        for outfit,variant in [('beaute.genessa','regular'),('beaute.genessa','corrupted'),('beaute.knightlady','default')]:
            request('select',outfit=outfit,variant=variant)
            request('open')
            first=request('inspect')
            time.sleep(1)
            second=request('inspect')
            assert first['paused'] and second['paused'], 'World was not paused'
            assert first['world_time']==second['world_time'], 'World time advanced in the wardrobe'
            assert second['preview_cloth_tick_when_paused'], 'Preview cloth is frozen, reproducing the rigid costume defect'
            assert second['gameplay_cloth_tick_when_paused']==baseline['gameplay_cloth_tick_when_paused'], 'Gameplay cloth setting changed'
            assert second['preview_bones_moving'], 'Preview idle bones did not animate'
            assert second['gameplay_animation']==baseline['gameplay_animation'], 'Gameplay animation instance changed'
            results.append({'outfit':outfit,'variant':variant,'first':first,'second':second})
        request('close')
        closed=request('inspect')
        assert not closed['preview_active'] and not closed['camera_active'] and not closed['input_owned'], 'Preview resources leaked'
        assert not closed['player_hidden'] and not closed['paused'], 'Player visibility or world pause was not restored'
    finally:
        # Always release preview ownership and restore the prior selection.
        request('close')
        selected=original.get('applied','').split('/',1)
        if len(selected)==2: request('select',outfit=selected[0],variant=selected[1])
        else: request('restore')
        if not original.get('enabled'): request('disable')
    result={'checks':'passed','variants':results,'closed':closed,'original_status':original}
    output=ROOT/'work/cloth-preview-lifecycle.json';output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Animated cloth, frozen world, unchanged gameplay animation and cleanup passed for three variants: {output}')


if __name__=='__main__': main()
