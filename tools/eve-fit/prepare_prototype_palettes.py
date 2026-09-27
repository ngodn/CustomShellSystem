"""Author garment-only palettes over verified fitting assets. Python 3.14."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css_controls import resource_info, validate
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_package import verify

WORK = ROOT/'work/eve26'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--outfit',choices=('prototype','skin'),default='prototype')
args=parser.parse_args()
SKIN=args.outfit=='skin'
OUT = WORK/('s16colors' if SKIN else 'p14colors')
BASE = WORK/('s16motion' if SKIN else 'p14motion')
SOURCE = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/textures_staged'
STEM = 'CSS_EveSkinFit16_P' if SKIN else 'CSS_EveFit14_P'
PALETTES = [
    ('xion_ember', 'Xion Ember', '542D3A', '55B5AA', 'C58E64'),
    ('wasteland_recon', 'Wasteland Recon', '3E5147', 'D7AF66', '88918B'),
    ('great_desert', 'Great Desert', 'B5A18A', '458EA2', '82583B'),
    ('stargazer', 'Stargazer', '303A65', 'AC91DE', 'CCD6E7'),
    ('angels_descent', "Angel's Descent", 'DEE5E5', '74C4DC', 'C6AF7B'),
]
PARTS = [('suit_color', 'Suit panels', 'garment'),
         ('tech_color', 'Technical accents', 'accent'),
         ('trim_color', 'Trim and markings', 'metal')]

if SKIN:
    PALETTES = [
        ('lunar_pearl','Lunar Pearl','C9D7DF','7F99A9','EEE4D1'),
        ('rose_alloy','Rose Alloy','784858','C18D91','D8BBA3'),
        ('abyssal_blue','Abyssal Blue','283C61','617C9E','B9CBD5'),
        ('jade_circuit','Jade Circuit','32564D','79A79B','C3AD79'),
        ('crimson_eclipse','Crimson Eclipse','542431','96606A','B8ADB5'),
    ]
    PARTS=[('suit_color','Suit panels','garment'),('inset_color','Inset panels','accent'),('hardware_color','Hardware','metal')]

def rgba(value):
    return [round(int(value[i:i+2],16)/255,7) for i in (0,2,4)]+[1]

def linear(v):
    return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)

def srgb(v):
    return np.where(v<=.0031308,v*12.92,1.055*np.maximum(v,0)**(1/2.4)-.055)

def smooth(v,lo,hi):
    t=np.clip((v-lo)/(hi-lo),0,1)
    return t*t*(3-2*t)

assert not OUT.exists()
manifest=verify(BASE/STEM)
original=copy.deepcopy(manifest)
OUT.mkdir()
shutil.copytree(BASE/'metadata',OUT/'metadata')
metadata=OUT/'metadata'/PACKAGE_ROOT/manifest['id']
variant=manifest['catalog']['outfits'][0]['variants'][0]
recipe=variant['customize']
assert not recipe['surfaces'] and not recipe['palettes']
extra=['202830','576879','8EADB1','D1CCC0','705866','607353']
for index,(identity,name,role) in enumerate(PARTS):
    shades=[(p[1],p[2+index]) for p in PALETTES]+[(f'Custom {i+1}',h) for i,h in enumerate(extra)]
    recipe['controls'].append(dict(id=identity,name=name,kind='color',group='outfit',role=role,
        default=[1,1,1,1],swatches=[dict(name='Default',color=[1,1,1,1],reset=True)]+
        [dict(name=n,color=rgba(h)) for n,h in shades]))
recipe['palettes']=[dict(id=p[0],name=p[1],values={part[0]:rgba(p[i+2]) for i,part in enumerate(PARTS)}) for p in PALETTES]
previews={p[0]:[] for p in PALETTES}
proof={}
atlases=[('SS_Suit',[16])] if SKIN else [('PD_Suit',[16]),('PD_Acc',[17,26,27,28])]
for atlas,slots in atlases:
    path=SOURCE/atlas/'T_ShellKeeper_Hair_01_BC.png'
    tex=np.asarray(Image.open(path).convert('RGBA').resize((2048,2048),Image.Resampling.LANCZOS),dtype=np.float32)/255
    rgb=tex[:,:,:3];r,g,b=rgb.transpose(2,0,1)
    # These thresholds are specific to the inspected garment atlases.
    # Body material slots are never bound.
    tech=smooth(g-r,.025,.10)*(1-smooth(r-g,0,.025))
    trim=smooth(r-g,.12,.25)*smooth(g-b,.12,.25)
    cloth=(1-smooth(np.max(rgb,axis=2),.20,.31))*(1-smooth(r-g,.012,.045))*(1-tech)*(1-trim)
    if SKIN:
        hardware=smooth(np.max(rgb,axis=2),.77,.88)
        inset=smooth(r-g,.045,.09)*smooth(g-b,.025,.06)*(1-hardware)
        panels=(1-hardware)*(1-smooth(r-g,.045,.09)*smooth(g-b,.025,.06))
        masks=[panels,inset,hardware]
    else:
        masks=[cloth,tech,trim]
    assert np.max(sum(masks))<=1.00001
    detail=[];layers={}
    for (identity,_,_),mask in zip(PARTS,masks):
        light=np.max(linear(rgb),axis=2)
        reference=float(np.percentile(light[mask>.8],90))
        gray=np.clip(light/max(reference,.001),0,1)
        alpha=mask*tex[:,:,3]
        layer=np.dstack([srgb(gray)]*3+[alpha])
        filename=f'dye-{atlas.lower()}-{identity}.png'
        Image.fromarray(np.rint(np.clip(layer,0,1)*255).astype(np.uint8)).save(metadata/filename)
        layers[identity]=filename
        manifest['resources'][filename]=resource_info(metadata/filename)
        detail.append((gray,alpha))
    recipe['surfaces'].append(dict(id=atlas.lower(),parameter='BaseColorMap  non VT',slots=slots,resolution=2048,layers=layers))
    proof[atlas]=dict(source_sha256=digest(path),slots=slots,coverage=[float(m.mean()) for m in masks])
    for p in PALETTES:
        composite=linear(rgb).copy()
        for i,(gray,alpha) in enumerate(detail):
            color=linear(np.array(rgba(p[i+2])[:3]))
            composite=composite*(1-alpha[:,:,None])+gray[:,:,None]*color*alpha[:,:,None]
        image=Image.fromarray(np.rint(np.clip(srgb(composite),0,1)*255).astype(np.uint8))
        image.save(OUT/f'{p[0]}-{atlas.lower()}.png')
        previews[p[0]].append(image)
assert all(set(p['values'])=={x[0] for x in PARTS} for p in recipe['palettes'])
assert all(slot>=16 and slot not in (18,19,20) for s in recipe['surfaces'] for slot in s['slots'])
validate(recipe)
(metadata/'manifest.json').write_text(json.dumps(manifest,separators=(',',':'))+'\n')
trio=OUT/STEM;trio.mkdir()
for suffix in ('.ucas','.utoc'):
    shutil.copy2(BASE/STEM/(STEM+suffix),trio/(STEM+suffix))
    assert digest(trio/(STEM+suffix))==digest(BASE/STEM/(STEM+suffix))
with (OUT/'pack.log').open('w') as log:
    subprocess.run([str(DEFAULT_REPAK),'pack',str(OUT/'metadata'),str(trio/(STEM+'.pak')),'--version','V8B'],check=True,stdout=log,stderr=subprocess.STDOUT)
assert verify(trio)==manifest
sheet=Image.new('RGB',(1500,650),'#242424');draw=ImageDraw.Draw(sheet)
for i,p in enumerate(PALETTES):
    draw.text((i*300+10,10),p[1],fill='white')
    for j,img in enumerate(previews[p[0]]):sheet.paste(img.resize((300,300)),(i*300,40+j*300))
sheet.save(OUT/'atlas-palettes.jpg')
(OUT/'verification.json').write_text(json.dumps(dict(atlases=proof,palettes=recipe['palettes'],
    files={p.name:digest(p) for p in trio.iterdir()},assets_byte_identical=True,
    original_controls_unchanged=recipe['controls'][:len(original['catalog']['outfits'][0]['variants'][0]['customize']['controls'])]==original['catalog']['outfits'][0]['variants'][0]['customize']['controls'],
    scope='Offline garment atlas previews and packaging only. Live palette application and Original restoration pending.'),indent=2)+'\n')
print(trio)
