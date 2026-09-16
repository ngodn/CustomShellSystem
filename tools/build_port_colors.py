#!/usr/bin/env python3
"""Author dye recipes for the reviewed September 2026 CSS ports. Python 3.14.

These rules are specific to the inspected source atlases. The general converter
accepts explicit author recipes; it does not infer skin masks for arbitrary mods.
"""
from pathlib import Path
import argparse
import json
import hashlib
import numpy as np
from PIL import Image
from build_color_recipes import layer, smooth, control, rgba
from css_controls import validate


def build(mod:Path):
    variants=json.loads((mod/'authoring/variants.json').read_text())
    key=mod.name.split('_')[1]
    ids={'CurvyAndCutePROXIMA':'calcalmon.curvyandcuteproxima','LongHairProxima':'ducttus.longhairproxima',
         'MileenaOverTiel':'getjinxyed.mileenaovertiel','BHProxima':'erase.bhproxima'}
    outfit=ids[key]
    for index,variant in enumerate(variants,1):
        source=mod/'work'/f'source-textures-{index:02}'
        if key=='BHProxima':source=mod/'work/source-textures-01'
        fallback=mod/'work/export-source-01'
        out=mod/'authoring'/('customize-'+variant['id']);out.mkdir(exist_ok=True)
        size=2048;surfaces=[];parts=set()
        def texture(name):
            found=list(source.rglob(name+'.png')) or list(fallback.rglob(name+'.png'))
            if len(found)!=1:raise ValueError(f'Expected one source texture {name}: {found}')
            return np.array(Image.open(found[0]).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS),dtype=np.float32)/255
        def surface(id,slots,name,masks):
            tex=texture(name);files={}
            for part,(mask,gain) in masks(tex).items():
                filename=f'dye-{key.lower()}-{variant["id"]}-{id}-{part}.png'
                layer(out/filename,tex[...,:3],mask*tex[...,3],gain,optimize=False)
                # Identical masks shared by several source variants are embedded
                # once. Different atlases receive different resource names.
                shared='dye-'+hashlib.sha256((out/filename).read_bytes()).hexdigest()[:32]+'.png'
                (out/filename).replace(out/shared)
                parts.add(part);files[part]=shared
            surfaces.append(dict(id=id,slots=slots,parameter='BaseColorMap  non VT',resolution=size,layers=files))
        if key=='MileenaOverTiel':
            def mileena(tex):
                r,g,b=tex[...,:3].transpose(2,0,1)
                cloth=smooth(r-g,.08,.2)*smooth(b-g,.04,.13)
                gold=smooth(r-b,.22,.38)*smooth(g,.35,.52)
                # Skin is tan, with green above the dark hair/lip range.
                skin=smooth(g,.24,.40)*(1-cloth)*(1-gold)
                return {'clothing':(cloth,2),'metal':(gold,1.2),'skin':(skin,1.1)}
            surface('body',[21],'JinxyDiffuse',mileena)
        else:
            for slot,num in [(0,'1004'),(1,'1001'),(2,'1002'),(3,'1003')]:
                def armor(tex,num=num):
                    packed=texture('T_LadyKnight_'+num+'_BRM')
                    r,g,b=tex[...,:3].transpose(2,0,1)
                    metal=smooth(packed[...,2],.18,.6)
                    skin=np.zeros(tex.shape[:2],dtype=np.float32)
                    if key=='CurvyAndCutePROXIMA' and num in ('1002','1003'):
                        skin=(1-metal)*smooth(b-r,.015,.055)*smooth(tex[...,:3].max(2),.32,.5)
                    elif key=='LongHairProxima' and num in ('1001','1002'):
                        skin=(1-metal)*smooth(r-b,.035,.075)*smooth(g,.23,.38)
                    primary='clothing' if num=='1003' else 'armor'
                    masks={primary:((1-metal)*(1-skin),2.3),'metal':(metal*(1-skin),1.3)}
                    if key in ('CurvyAndCutePROXIMA','LongHairProxima') and num in ('1001','1002','1003'):
                        masks['skin']=(skin,1.2)
                    return masks
                surface('body-'+num,[slot],'T_LadyKnight_'+num+'_BC',armor)
            if key=='LongHairProxima':
                surface('hair',[8],'T_ShellKeeper_Hair_01_BC',lambda t:{'hair':(np.ones(t.shape[:2]),1.2)})
            if key=='BHProxima':
                surface('cape',[6],'T_Smert_HoodRobe_01_BC',lambda t:{'cape':(np.ones(t.shape[:2]),6)})
        labels={'armor':'Armor','clothing':'Clothing','metal':'Metal','skin':'Skin','hair':'Hair','cape':'Cape'}
        controls=[control(p,labels[p]) for p in labels if p in parts]
        palettes=[]
        for id,name,primary,metal in [('crimson','Crimson','9A263F','D0AC70'),('midnight','Midnight','354775','B3BDCE'),
                                     ('ash','Ash','B3AD9F','D0C7B9'),('verdigris','Verdigris','286458','B59A66')]:
            values={p:rgba(metal if p=='metal' else primary) for p in parts if p not in ('skin','hair')}
            palettes.append(dict(id=id,name=name,values=values))
        colors=dict(schema=1,controls=controls,surfaces=surfaces,palettes=palettes)
        validate(colors)
        recipe=out/'customize.json';recipe.write_text(json.dumps({'id':outfit,'customize':colors},indent=2)+'\n')
        variant['customize']=str(recipe.relative_to(mod/'authoring'))
        print(key,variant['id'],len(parts),'color controls',flush=True)
    (mod/'authoring/variants.json').write_text(json.dumps(variants,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('mods',nargs='+',type=Path)
    for mod in parser.parse_args().mods:build(mod)
