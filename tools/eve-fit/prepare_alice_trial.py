"""Package Alice controls and garment palettes over the verified A3 containers."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools'))
from css_controls import resource_info, validate
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_package import verify

work=root/'work/eve26'
packed=work/'a3pack'
proof=json.loads((packed/'verification.json').read_text())
for name,expected in proof['containers'].items():assert digest(packed/name)==expected
for name,expected in proof['installed_eve_dependencies'].items():assert digest(packed/'containers'/name)==expected
source=work/'p13meta'/PACKAGE_ROOT/'eins0fx.seduxtress'
original=json.loads((source/'manifest.json').read_text())
manifest=copy.deepcopy(original)
outfit=manifest['catalog']['outfits'][0]
variant=next(v for v in outfit['variants'] if v['id']=='midsummer_alice')
black=next(v for v in outfit['variants'] if v['id']=='black_pearl')['customize']
body_ids={'skin','nipple_color','areola_color','labia_color','genital_color','hair_color'}
controls=[dict(id=identity,name=name,kind='toggle',role='piece',group='outfit',default=[1,0,0,1],sections=slots)
    for identity,name,slots in [('suit','Suit',[16]),('ribbon','Ribbon',[26]),('shoes','Shoes',list(range(17,23))),('hair','Hair',[23,24,25])]]
controls.extend(copy.deepcopy(c) for c in variant['customize']['controls'] if c['kind'] in ('shape','rig'))
assert {c['id'] for c in controls if c['kind']=='rig'}=={'hair_motion','chest_motion','glute_motion','thigh_motion','belly_motion'}
available={e['name'] for e in proof['assets'][0]['exports']['exports'] if e['class']=='MorphTarget'}
assert all(c['morph'] in available for c in controls if c['kind']=='shape')
body_controls=[copy.deepcopy(c) for c in black['controls'] if c['id'] in body_ids]
assert len(body_controls)==6 and all(len(c['swatches'])==12 for c in body_controls)
controls.extend(body_controls)
parts=[('suit_color','Suit color','garment','MA_Suit',[16]),
       ('ribbon_color','Ribbon color','garment','MA_Suit',[26]),
       ('shoe_color','Shoe color','leather','CS_Heels',[17,18,19,20,21]),
       ('buckle_color','Buckle color','metal','CS_Heels',[22])]
palettes=[('oasis_alice','Oasis Alice','6BADA8','EEE1BC','375E65','D8BA7D'),
          ('xion_midnight','Xion Midnight','34425F','A6B6D2','232735','B7C5D7'),
          ('eidos_rose','Eidos Rose','B77F98','EDD7CC','754B63','CCA985'),
          ('orbital_pearl','Orbital Pearl','DFE5E8','667D9C','91A3B4','CCD8E0'),
          ('wasteland_iris','Wasteland Iris','84749E','CDC7A7','4E465F','BAA46C')]
def rgba(value):return [int(value[i:i+2],16)/255 for i in (0,2,4)]+[1]
for index,(identity,name,role,_,_) in enumerate(parts):
    shades=[(p[1],p[index+2]) for p in palettes]+[(f'Custom {i+1}',v) for i,v in enumerate(['252932','D8CCB5','6F4A42','526E79','69734D','A06870'])]
    controls.append(dict(id=identity,name=name,kind='color',group='outfit',role=role,default=[1,1,1,1],
        swatches=[dict(name='Default',color=[1,1,1,1],reset=True)]+[dict(name=n,color=rgba(v)) for n,v in shades]))
recipe=dict(schema=1,controls=controls,surfaces=[],palettes=[dict(id=p[0],name=p[1],values={part[0]:rgba(p[i+2]) for i,part in enumerate(parts)}) for p in palettes])
mesh=json.loads((work/'alice-import4/alice.mesh.json').read_text())
names=mesh['materials']
aliases={'Eve Covered Feet':'Eve Legs','Eve Covered Toenails':'Eve Toenails','MI_EVE_HR_01_TypeA.001':'MI_EVE_HR_01_TypeA'}
old_names={0:'Eve Face',1:'Eve Head',2:'Eve Body',3:'Eve Legs',4:'Eve Arms',5:'Eve Genitals',7:'Eve Toenails',21:'MI_EVE_HR_01_TypeA',22:'MI_EVE_HR_01_Tail_TypeA'}
for surface in black['surfaces']:
    if not set(surface['layers'])<=body_ids:continue
    entry=copy.deepcopy(surface)
    oldslot=entry['slots'][0]
    entry['slots']=[i for i,n in enumerate(names) if aliases.get(n,n)==old_names[oldslot]]
    assert entry['slots']
    entry['id']='body_'+str(oldslot)
    recipe['surfaces'].append(entry)
variant['mesh']='/Game/CSS/EveTest/SK_ACloth3.SK_ACloth3'
variant['customize']=recipe
for idle in variant.get('animations',{}).get('idle',[]):
    if idle['id']=='eve':idle['name']='Eve Default Idle'
identity='eins0fx.evealicefit3'
for entry in (manifest,outfit):
    entry.update(id=identity,name='Eve Alice Fitting Trial',version='0.0.3',description='Private Midsummer Alice review with cloth, body motion and palettes. Requires the installed Eve package.')
outfit['variants']=[variant]
outfit.pop('templates',None)
manifest['resources']={}
out=work/'a3trial'
out.mkdir(exist_ok=False)
metadata=out/'metadata'/PACKAGE_ROOT/identity
metadata.mkdir(parents=True)
shutil.copy2(source/'thumbnail.png',metadata/'thumbnail.png')
textures=root.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/textures_staged'
def linear(v):return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)
def srgb(v):return np.where(v<=.0031308,v*12.92,1.055*np.maximum(v,0)**(1/2.4)-.055)
sheet=Image.new('RGB',(1500,1240),'#242424')
draw=ImageDraw.Draw(sheet)
atlas_proof=[]
for index,(control,_,_,atlas,slots) in enumerate(parts):
    path=textures/atlas/'T_ShellKeeper_Hair_01_BC.png'
    tex=np.asarray(Image.open(path).convert('RGBA').resize((2048,2048),Image.Resampling.LANCZOS),dtype=np.float32)/255
    light=np.max(linear(tex[:,:,:3]),axis=2)
    reference=max(float(np.percentile(light[tex[:,:,3]>.8],90)),.001)
    gray=np.clip(light/reference,0,1)
    layer=np.dstack([srgb(gray)]*3+[tex[:,:,3]])
    filename=f'dye-{control}.png'
    Image.fromarray(np.rint(np.clip(layer,0,1)*255).astype(np.uint8)).save(metadata/filename)
    manifest['resources'][filename]=resource_info(metadata/filename)
    recipe['surfaces'].append(dict(id=control,parameter='BaseColorMap  non VT',slots=slots,resolution=2048,layers={control:filename}))
    atlas_proof.append(dict(control=control,atlas=atlas,slots=slots,materials=[names[i] for i in slots],source_sha256=digest(path)))
    for column,palette in enumerate(palettes):
        rgb=srgb(gray[:,:,None]*linear(np.asarray(rgba(palette[index+2])[:3])))
        image=Image.fromarray(np.rint(np.clip(rgb,0,1)*255).astype(np.uint8))
        image.save(out/f'{palette[0]}-{control}.png')
        sheet.paste(image.resize((300,300)),(column*300,40+index*300))
        if index==0:draw.text((column*300+8,10),palette[1],fill='white')
sheet.save(out/'atlas-palettes.jpg')
for filename in validate(recipe)-set(manifest['resources']):
    assert resource_info(source/filename)==original['resources'][filename]
    shutil.copy2(source/filename,metadata/filename)
    manifest['resources'][filename]=resource_info(metadata/filename)
stem='CSS_EveAliceFit3_P';trio=out/stem;trio.mkdir()
for suffix in ('.utoc','.ucas'):
    path=trio/(stem+suffix);shutil.copy2(packed/path.name,path)
    manifest['containers'][suffix]=dict(file=path.name,bytes=path.stat().st_size,sha256=digest(path))
(metadata/'manifest.json').write_text(json.dumps(manifest,separators=(',',':'))+'\n')
with (out/'pack.log').open('w') as log:
    subprocess.run([str(DEFAULT_REPAK),'pack',str(out/'metadata'),str(trio/(stem+'.pak')),'--version','V8B'],stdout=log,stderr=subprocess.STDOUT,check=True)
assert verify(trio)==manifest
(out/'verification.json').write_text(json.dumps(dict(atlases=atlas_proof,palettes=recipe['palettes'],
    controls=[c['id'] for c in controls],dependencies=proof['installed_eve_dependencies'],files={p.name:digest(p) for p in trio.iterdir()},
    scope='Private package validation. Live colors, modularity, cloth, morphs and persistence need game review. Final delivery remains one combined Eve trio.'),indent=2)+'\n')
print(trio)
