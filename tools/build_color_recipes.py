#!/usr/bin/env python3
"""Build reviewed dye masks from the three proof-of-concept model exports.

Python 3.14, Pillow 12 and NumPy 2. These recipes are specific to these UV atlases.
General converters must use author-provided masks, never guess their skin regions.
The grayscale layers preserve surface detail; alpha selects the dyeable pixels.
"""
from pathlib import Path
import argparse
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT=Path(__file__).resolve().parents[1]

def rgba(hexcode):
    return [int(hexcode[i:i+2],16)/255 for i in (0,2,4)]+[1]

def smooth(x,lo,hi):
    t=np.clip((x-lo)/(hi-lo),0,1)
    return t*t*(3-2*t)

def load(root,name,size):
    files=list(root.rglob(name+'.png'))
    if len(files)!=1: raise ValueError(f'Expected exactly one {name} texture')
    return np.array(Image.open(files[0]).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS),dtype=np.float32)/255

def layer(path,rgb,mask,gain=1,*,optimize=True):
    # Desaturate in linear space, then encode sRGB for the engine's texture sampler.
    linear=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
    light=np.clip(np.max(linear,axis=2)*gain,0,1)
    gray=np.where(light<=.0031308,12.92*light,1.055*light**(1/2.4)-.055)
    out=np.concatenate((np.repeat(gray[...,None],3,axis=2),np.clip(mask,0,1)[...,None]),axis=2)
    Image.fromarray(np.rint(out*255).astype(np.uint8)).save(path,optimize=optimize)

def control(id,name,color='FFFFFF',**extra):
    return dict(id=id,name=name,default=rgba(color),**extra)

def build(out:Path,size:int):
    out.mkdir(parents=True,exist_ok=True)
    roots={key:ROOT/'work'/folder for key,folder in [('genessa','genessa-portrait-model'),('knightlady','knightlady-portrait-model'),('hit2','hit2-portrait-model')]}
    palettes=[dict(id='crimson',name='Crimson regalia',values={}),dict(id='midnight',name='Midnight regalia',values={})]
    for key,root in roots.items():
        directory=out/key;directory.mkdir(exist_ok=True)
        surfaces=[]; controls=[]
        def surface(id,slots,bc,brm,masks):
            tex=load(root,bc,size);packed=load(root,brm,size) if brm else None
            masks=masks(tex,packed)
            files={}
            for part,(mask,gain) in masks.items():
                filename=f'dye-{key}-{id}-{part}.png';layer(directory/filename,tex[...,:3],mask*tex[...,3],gain);files[part]=filename
            surfaces.append(dict(id=id,slots=slots,parameter='BaseColorMap  non VT',resolution=size,layers=files))
        if key=='genessa':
            controls=[control('clothing','Clothing','242B32'),control('metal','Metal','C5B98C'),control('skin','Skin','A9BACB'),control('face','Face','D4CDC0')]
            def top(tex,packed):
                face=Image.new('L',(size,size));d=ImageDraw.Draw(face);d.ellipse((size*.49,size*.155,size*.653,size*.3),fill=255)
                face=np.array(face.filter(ImageFilter.GaussianBlur(size/2048)),dtype=np.float32)/255
                metal=smooth(packed[...,2],.2,.65)*(1-face)
                return {'clothing':((1-metal)*(1-face),9),'metal':(metal,1.3),'face':(face,1.2)}
            def body(tex,packed):
                metal=smooth(packed[...,2],.2,.65)
                skin=smooth(tex[...,2]-tex[...,0],.025,.08)*smooth(tex[...,:3].max(2),.24,.45)*(1-metal)
                return {'clothing':((1-metal)*(1-skin),12),'metal':(metal,1.3),'skin':(skin,1.2)}
            surface('top',[0,1,2],'Genessa_top_BC','Genessa_top_BRM',top)
            surface('body',[3,7],'Genessa_body_BC','Genessa_body_BRM',body)
            # Smoke retains its authored opacity, animation and noise texture.
            controls+=[control('eye-glow','Eyes','CCE5FF',bindings=[dict(slot=5,parameter='Color'),dict(slot=10,parameter='Color')]),
                       control('eye-intensity','Eye glow',type='scalar',max=5,step=.05,bindings=[dict(slot=5,parameter='Intensity'),dict(slot=10,parameter='Intensity')])]
            controls[-1]['default']=[1.5,0,0,1]
        elif key=='knightlady':
            controls=[control('armor','Armor','73777E'),control('metal','Metal highlights','BEC5C7'),control('cloth','Waist cloth','25222B')]
            for slot,num in [(0,'1004'),(1,'1001'),(2,'1002'),(3,'1003')]:
                def armor(tex,packed):
                    highlight=smooth(tex[...,:3].max(2),.45,.68)
                    primary='cloth' if num=='1003' else 'armor'
                    return {primary:(1-highlight,2.3),'metal':(highlight,1.4)}
                surface('armor-'+num,[slot],'T_LadyKnight_'+num+'_BC','T_LadyKnight_'+num+'_BRM',armor)
        else:
            controls=[control('clothing','Clothing','6A6B7B'),control('metal','Metal','CDD2D4'),control('ribbons','Ribbons','650F1C'),control('gems','Gems','2687DD'),control('skin','Skin','E4D3C9'),control('face','Face','E4D3C9'),control('hair','Hair','C3C4CE')]
            def body(tex,packed):
                rgb=tex[...,:3];red,green,blue=rgb.transpose(2,0,1)
                ribbons=smooth(red-green,.025,.08)*smooth(red-blue,.025,.08)
                gems=smooth(blue-red,.15,.35)*smooth(blue-green,.06,.2)
                metal=smooth(packed[...,2],.22,.7)*(1-gems)*(1-ribbons)
                skin=smooth(red-blue,.025,.06)*smooth(rgb.max(2),.65,.8)*(1-metal)*(1-ribbons)
                return {'clothing':((1-metal)*(1-ribbons)*(1-gems)*(1-skin),2.5),'metal':(metal,1.25),'ribbons':(ribbons,5),'gems':(gems,1.5),'skin':(skin,1.1)}
            surface('body',[0],'Scythe_Della_Body_D','Scythe_Della_Body_BRM',body)
            def face(tex,packed):
                # Preserve lips, eyes and eyebrows when changing complexion.
                r,g,b=tex[...,:3].transpose(2,0,1)
                mask=smooth(g,.28,.5)*(1-smooth(r-g,.22,.35))
                return {'face':(mask,1.15)}
            surface('face',[1],'Scythe_Della_Face_D','Scythe_Della_Face_BRM',face)
            surface('hair',[2],'Scyther_Hair_001_D','Scyther_Hair_001_BRM',lambda t,p:{'hair':(np.ones(t.shape[:2]),1.5)})
        custom_palettes=json.loads(json.dumps(palettes))
        names={c['id'] for c in controls}
        for palette in custom_palettes:
            colors=({'clothing':'8F233C','armor':'76253D','metal':'C5A56B','cloth':'251923','ribbons':'291820','gems':'B7294C'} if palette['id']=='crimson' else
                    {'clothing':'293B72','armor':'363755','metal':'BBCBD7','cloth':'221E3D','ribbons':'49365F','gems':'688CE0'})
            palette['values']={id:rgba(color) for id,color in colors.items() if id in names}
        id={'genessa':'beaute.genessa','knightlady':'beaute.knightlady','hit2':'xtgmods.hit2_de_scyther'}[key]
        recipe=dict(id=id,customize=dict(schema=1,controls=controls,surfaces=surfaces,palettes=custom_palettes))
        (directory/(key+'.customize.json')).write_text(json.dumps(recipe,indent=2)+'\n')
        print(directory,flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'work/color-recipes')
    parser.add_argument('--resolution',type=int,choices=[1024,2048,4096],default=2048)
    args=parser.parse_args();build(args.output,args.resolution)
