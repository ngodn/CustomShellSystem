"""Validate and embed author color recipes in CSS.Package v1."""
from __future__ import annotations
import json
import math
from pathlib import Path
import re
import shutil
import struct

ID=re.compile(r'[A-Za-z0-9][A-Za-z0-9._-]{0,95}\Z')
RESOURCE=re.compile(r'dye-[A-Za-z0-9_-]+\.png\Z')

def identifier(value):
    if not isinstance(value,str) or not ID.fullmatch(value) or value in {'.','..'}: raise ValueError('Invalid color identifier')

def number(value,low,high):
    if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or not low<=value<=high: raise ValueError('Color value outside supported range')

def vector(value):
    if not isinstance(value,list) or len(value)!=4: raise ValueError('Color requires RGBA components')
    for x in value: number(x,0,32)
    number(value[3],0,1)

def parameter(value):
    if not isinstance(value,str) or not 1<=len(value)<=128 or any(not 32<=ord(c)<=126 for c in value): raise ValueError('Invalid material parameter')

def bounded_array(value,limit):
    if not isinstance(value,list) or len(value)>limit: raise ValueError('Invalid color array size')

# docs/color-convention.md. A role outside this table is allowed, because a part
# can exist that none of these describe, but lint_convention reports it so the
# choice is deliberate rather than a typo.
ROLES={'garment':('outfit',False),'accent':('outfit',False),'leather':('outfit',False),
       'metal':('outfit',True),'gem':('outfit',True),'glow':('outfit',False),
       'skin':('body',True),'face':('body',False),'hair':('body',False),
       'eyes':('body',False),'eye-glow':('body',False),'body-hair':('body',False),
       'nipple':('body',True),'areola':('body',True),'labia':('body',True),'vestibule':('body',True)}

def scalar(control) -> bool:
    kind=control.get('kind')
    if kind is not None: return kind=='intensity'
    return control.get('type')=='scalar'

def validate(colors:dict) -> set[str]:
    if not isinstance(colors,dict) or colors.get('schema')!=1: raise ValueError('Unsupported color schema')
    controls=colors.get('controls');bounded_array(controls,32)
    by_id={};used=set();files=set()
    for c in controls:
        identifier(c['id'])
        if c['id'] in by_id or not isinstance(c['name'],str) or not 1<=len(c['name'])<=96: raise ValueError('Invalid color control name')
        by_id[c['id']]=c
        if c.get('type','color') not in ('color','scalar'): raise ValueError('Invalid color control type')
        if 'kind' in c and c['kind'] not in ('color','intensity'): raise ValueError('Invalid color control kind')
        if 'kind' in c and 'type' in c and scalar(c)!=(c['type']=='scalar'): raise ValueError('Color control kind contradicts its type')
        # The convention's three fields. All optional: CSS infers them from the id
        # for packages that predate it, so an old recipe still loads.
        if 'role' in c: identifier(c['role'])
        if 'role' in c and len(c['role'])>32: raise ValueError('Color control role is too long')
        if 'group' in c and c['group'] not in ('outfit','body'): raise ValueError('Invalid color control group')
        if 'hue_locked' in c and not isinstance(c['hue_locked'],bool): raise ValueError('hue_locked must be true or false')
        vector(c['default']);minimum=c.get('min',0);maximum=c.get('max',1);step=c.get('step',.01)
        number(minimum,0,32);number(maximum,0,32)
        if minimum>=maximum: raise ValueError('Invalid slider limits')
        number(step,0,maximum-minimum)
        if not step: raise ValueError('Slider step cannot be zero')
        for v in c['default'][:1 if scalar(c) else 3]:number(v,minimum,maximum)
        bindings=c.get('bindings',[]);bounded_array(bindings,128)
        for b in bindings:
            if type(b['slot']) is not int: raise ValueError('Material slot must be an integer')
            number(b['slot'],0,127);parameter(b['parameter'])
            a=b.get('association','global');index=b.get('layer',-1)
            if a not in ('global','layer','blend') or type(index) is not int or (index!=-1 if a=='global' else not 0<=index<=63): raise ValueError('Invalid parameter association')
            used.add(c['id'])
    surfaces=colors.get('surfaces',[]);bounded_array(surfaces,16)
    ids=set();destinations=set()
    for s in surfaces:
        identifier(s['id']);parameter(s['parameter'])
        if s['id'] in ids:raise ValueError('Duplicate dye surface')
        ids.add(s['id'])
        if s.get('resolution',2048) not in (1024,2048,4096):raise ValueError('Invalid dye resolution')
        bounded_array(s['slots'],128)
        if not s['slots']:raise ValueError('Dye surface has no slots')
        for slot in s['slots']:
            if type(slot) is not int:raise ValueError('Material slot must be an integer')
            number(slot,0,127);destination=(slot,s['parameter'])
            if destination in destinations:raise ValueError('Overlapping dye surfaces')
            destinations.add(destination)
        if not isinstance(s['layers'],dict) or not 1<=len(s['layers'])<=16:raise ValueError('Invalid color layers')
        for id,file in s['layers'].items():
            if id not in by_id or scalar(by_id[id]) or not isinstance(file,str) or not RESOURCE.fullmatch(file):raise ValueError('Invalid dye layer')
            used.add(id);files.add(file)
    if used!=set(by_id):raise ValueError('A color control has no target')
    palettes=colors.get('palettes',[]);bounded_array(palettes,64);ids=set()
    for p in palettes:
        identifier(p['id'])
        if p['id']=='original' or p['id'] in ids or not isinstance(p['name'],str) or not 1<=len(p['name'])<=96:raise ValueError('Invalid palette identity')
        ids.add(p['id'])
        if not isinstance(p['values'],dict):raise ValueError('Invalid palette values')
        for id,v in p['values'].items():
            if id not in by_id:raise ValueError('Unknown palette part')
            vector(v);c=by_id[id]
            for x in v[:1 if scalar(c) else 3]:number(x,c.get('min',0),c.get('max',1))
            if v[3]!=c['default'][3]:raise ValueError('Palette changes protected opacity')
    return files

def resource_info(path:Path) -> dict:
    from css_convert import digest
    data=path.read_bytes()
    if not RESOURCE.fullmatch(path.name) or not 33<=len(data)<=32*1024*1024 or data[:8]!=b'\x89PNG\r\n\x1a\n' or data[12:16]!=b'IHDR':raise ValueError('Invalid dye PNG')
    w,h=struct.unpack_from('>II',data,16)
    if w!=h or w not in (1024,2048,4096) or data[24]!=8 or data[25] not in (4,6):raise ValueError('Dye layers require square 8-bit PNG with alpha, 1024 to 4096 pixels')
    return dict(bytes=len(data),width=w,height=h,sha256=digest(path))

def embed(recipe:Path,manifest:dict,metadata:Path,variant:dict|None=None):
    j=json.loads(recipe.read_text())
    if j['id']!=manifest['id']:raise ValueError('Color recipe belongs to another outfit')
    files=validate(j['colors'])
    (variant if variant is not None else manifest['catalog']['outfits'][0])['colors']=j['colors']
    resources=manifest.setdefault('resources',{})
    total=sum(info['bytes'] for info in resources.values())
    for file in sorted(files):
        source=recipe.parent/file
        info=resource_info(source)
        if file in resources:
            if resources[file]!=info:raise ValueError('Variant dye resource filename collision: '+file)
            continue
        total+=info['bytes']
        if total>256*1024*1024:raise ValueError('Dye resources exceed 256 MiB')
        resources[file]=info;shutil.copy2(source,metadata/file)

def verify_resources(manifest:dict,metadata:Path):
    colors=manifest['catalog']['outfits'][0].get('colors')
    files=validate(colors) if colors else set()
    for variant in manifest['catalog']['outfits'][0].get('variants',[]):
        if 'colors' in variant:files.update(validate(variant['colors']))
    resources=manifest.get('resources',{})
    if set(resources)!=files:raise ValueError('Dye resource manifest does not match recipe')
    total=0
    for name,info in resources.items():
        if resource_info(metadata/name)!=info:raise ValueError('Dye resource checksum or dimensions differ')
        total+=info['bytes']
    if total>256*1024*1024:raise ValueError('Dye resources exceed 256 MiB')

def lint_convention(colors:dict) -> list[str]:
    """Check a recipe against docs/color-convention.md. Authoring only.

    validate() decides whether a recipe is safe to load, and stays permissive so
    that every package published before the convention keeps working. This is the
    other half: it decides whether a recipe is a *good* one, and it is meant to be
    called from a package's own builder, where breaking the build is the point.
    """
    problems=[]
    controls=colors.get('controls',[])
    by_id={c['id']:c for c in controls}
    for c in controls:
        where=f"control {c['id']}"
        if 'role' not in c: problems.append(f'{where}: no role, so CSS has to guess it from the id')
        if 'group' not in c: problems.append(f'{where}: no group, so CSS has to guess which section it belongs in')
        role=c.get('role')
        if role is not None and role not in ROLES:
            problems.append(f'{where}: role {role!r} is not in the shared vocabulary; add it to the convention or reuse one')
        elif role is not None:
            group,locked=ROLES[role]
            if c.get('group',group)!=group:
                problems.append(f"{where}: role {role!r} belongs to the {group} group, not {c['group']!r}")
            if c.get('hue_locked',locked)!=locked:
                problems.append(f'{where}: role {role!r} is normally hue_locked={locked}; say why in the package notes if that is wrong')
        if c.get('name','')[:1].islower():
            problems.append(f'{where}: name {c.get("name")!r} should read as a label, in sentence case')
    palettes=colors.get('palettes',[])
    if len(palettes)<2:
        problems.append(f'{len(palettes)} palettes beyond Original; the convention asks for at least two')
    # A palette is an outfit look, so it has to set the whole outfit. Body parts
    # are the player's, not the look's, and a palette that leaves one alone leaves
    # it at the author's own colour, which is well defined.
    outfit={c['id'] for c in controls if not scalar(c)
            and c.get('group',ROLES.get(c.get('role',''),('outfit',))[0])=='outfit'}
    for p in palettes:
        missing=sorted(outfit-set(p.get('values',{})))
        if missing: problems.append(f"palette {p['id']}: does not set {', '.join(missing)}")
        for id in p.get('values',{}):
            if id not in by_id: problems.append(f"palette {p['id']}: sets unknown control {id}")
    return problems
