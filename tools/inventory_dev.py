#!/usr/bin/env python3
"""Local integration development driver; never packaged with CSS."""
import sys, json, time, shutil, hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from css import GAME, atomic, wait_json
MOD=GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
EVIDENCE=ROOT/'work/inventory-integration/development'
EVIDENCE.mkdir(parents=True,exist_ok=True)
action=sys.argv[1]
if action=='stage':
    backup=EVIDENCE/'original-core.json'
    if not backup.exists(): shutil.copy2(MOD/'core.json',backup)
    source=ROOT/'build/inventory-native/css_core.dll'
    digest=hashlib.sha256(source.read_bytes()).hexdigest()[:16]
    name=f'css_core-inventory-{digest}-{time.time_ns()}.dll'
    (MOD/'assets').mkdir(exist_ok=True)
    shutil.copy2(ROOT/'assets/inventory-logo-v1.png',MOD/'assets/inventory-logo-v1.png')
    shutil.copy2(source,MOD/'cores'/name)
    atomic(MOD/'core.json',{'abi':1,'file':name})
    print(wait_json(MOD/'runtime/loader.json',lambda j:j.get('core')==name))
elif action=='restore':
    original=json.loads((EVIDENCE/'original-core.json').read_text())
    atomic(MOD/'core.json',original)
    print(wait_json(MOD/'runtime/loader.json',lambda j:j.get('core')==original['file']))
else:
    rid=str(time.time_ns())
    request={'id':rid,'action':'inventory_'+action}
    if len(sys.argv)>2: request.update(json.loads(sys.argv[2]))
    atomic(MOD/'request.json',request)
    result=wait_json(MOD/'runtime/inventory.json',lambda j:j.get('id')==rid)
    path=EVIDENCE/f'{rid}-{action}.json'
    path.write_text(json.dumps(result,indent=2)+'\n')
    print(path)
    print('Main',result.get('main_page',{}).get('MainTabIndex'),'Map',result.get('map_shortcut_index'))
    for key in ['BP_HBC_Menu_Game','BP_WS_Menu_Game']:
        value=result.get('main_objects',{}).get(key,{})
        print(key,'active',value.get('active_index'),'children',[j.get('class') for j in value.get('children',[])])
