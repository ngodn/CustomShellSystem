"""Remap accepted Eve color masks by material identity into fitted packages. Python 3.14."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools'))
from css_controls import resource_info,validate
from css_convert import DEFAULT_REPAK,PACKAGE_ROOT,digest
from css_package import verify

work=root/'work/eve26'
source=work/'p13meta'/PACKAGE_ROOT/'eins0fx.seduxtress'
old=json.loads((source/'manifest.json').read_text())
black=next(v for v in old['catalog']['outfits'][0]['variants'] if v['id']=='black_pearl')['customize']
ids={'skin','nipple_color','areola_color','labia_color','genital_color','hair_color'}
controls=[copy.deepcopy(c) for c in black['controls'] if c['id'] in ids]
assert len(controls)==6 and all(len(c['swatches'])==12 for c in controls)
# Names from the decoded V42 mesh, whose dye bindings were previously reviewed.
old_names={0:'Eve Face',1:'Eve Head',2:'Eve Body',3:'Eve Legs',4:'Eve Arms',5:'Eve Genitals',7:'Eve Toenails',21:'MI_EVE_HR_01_TypeA',22:'MI_EVE_HR_01_Tail_TypeA'}
alias={'Eve Covered Feet':'Eve Legs','Eve Covered Toenails':'Eve Toenails','SkinFootLining':'Eve Legs','SkinToenailLining':'Eve Toenails','MI_EVE_HR_01_TypeA.001':'MI_EVE_HR_01_TypeA'}
for prefix,stem,meshfile in [('p14','CSS_EveFit14_P','planet-f14-import/planet.mesh.json'),('s16','CSS_EveSkinFit16_P','skin-f16-import/skin.mesh.json'),('b1','CSS_EveBikiniFit1_P','bikini-import1/bikini.mesh.json')]:
    base=work/(prefix+'colors');out=work/(prefix+'custom');assert not out.exists()
    manifest=verify(base/stem);previous=copy.deepcopy(manifest)
    mesh=json.loads((work/meshfile).read_text());names=mesh['materials']
    resolved=[]
    for name in names:
        if name.startswith('PlanetCovered_') or name.startswith('SkinCovered_'):
            name=names[int(name.rsplit('_',1)[1])]
        resolved.append(alias.get(name,name))
    recipe=manifest['catalog']['outfits'][0]['variants'][0]['customize']
    assert not ids.intersection(c['id'] for c in recipe['controls'])
    recipe['controls'].extend(copy.deepcopy(controls))
    mappings=[]
    for surface in black['surfaces']:
        if not set(surface['layers'])<=ids:continue
        oldslot=surface['slots'][0];name=old_names[oldslot]
        slots=[i for i,n in enumerate(resolved) if n==name]
        assert slots,(prefix,name)
        item=copy.deepcopy(surface);item['id']='body_'+str(oldslot);item['slots']=slots
        recipe['surfaces'].append(item)
        mappings.append(dict(source_slot=oldslot,source_material=name,target_slots=slots,target_materials=[names[i] for i in slots],layers=item['layers']))
    required=validate(recipe)
    out.mkdir();shutil.copytree(base/'metadata',out/'metadata');metadata=out/'metadata'/PACKAGE_ROOT/manifest['id']
    added=required-set(manifest['resources'])
    for filename in added:
        assert resource_info(source/filename)==old['resources'][filename]
        shutil.copy2(source/filename,metadata/filename)
        manifest['resources'][filename]=resource_info(metadata/filename)
    assert recipe['palettes']==previous['catalog']['outfits'][0]['variants'][0]['customize']['palettes']
    (metadata/'manifest.json').write_text(json.dumps(manifest,separators=(',',':'))+'\n')
    trio=out/stem;trio.mkdir()
    for suffix in ('.utoc','.ucas'):
        shutil.copy2(base/stem/(stem+suffix),trio/(stem+suffix))
        assert digest(trio/(stem+suffix))==digest(base/stem/(stem+suffix))
    with (out/'pack.log').open('w') as log:
        subprocess.run([str(DEFAULT_REPAK),'pack',str(out/'metadata'),str(trio/(stem+'.pak')),'--version','V8B'],check=True,stdout=log,stderr=subprocess.STDOUT)
    assert verify(trio)==manifest
    (out/'verification.json').write_text(json.dumps(dict(mappings=mappings,controls=sorted(ids),mask_resources_unchanged=True,
        asset_containers_unchanged=True,clothing_palettes_unchanged=True,files={p.name:digest(p) for p in trio.iterdir()},
        scope='Named material remapping and original mask integrity verified. Live UV alignment, color output and persistence still need review.'),indent=2)+'\n')
    print(trio,flush=True)
