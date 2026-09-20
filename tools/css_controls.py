"""Validate and embed an author's control recipes in CSS.Package v1."""
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

def vector(value,low=0,high=32):
    if not isinstance(value,list) or len(value)!=4: raise ValueError('Color requires RGBA components')
    for x in value: number(x,low,high)
    number(value[3],0,1)

def parameter(value):
    if not isinstance(value,str) or not 1<=len(value)<=128 or any(not 32<=ord(c)<=126 for c in value): raise ValueError('Invalid material parameter')

def bounded_array(value,limit):
    if not isinstance(value,list) or len(value)>limit: raise ValueError('Invalid color array size')

# docs/control-convention.md. A role outside this table is allowed, because a part
# can exist that none of these describe, but lint_convention reports it so the
# choice is deliberate rather than a typo.
ROLES={'garment':('outfit',False),'accent':('outfit',False),'leather':('outfit',False),
       'metal':('outfit',True),'gem':('outfit',True),'glow':('outfit',False),
       'skin':('body',True),'face':('body',False),'hair':('body',False),
       'eyes':('body',False),'eye-glow':('body',False),'body-hair':('body',False),
       'nipple':('body',True),'areola':('body',True),'labia':('body',True),'vestibule':('body',True),
       # 1.0: roles for the kinds that are not colours. Hue locking does not apply to a
       # single number, so they are all False and the menu ignores it.
       'gloss':('outfit',False),'roughness':('outfit',False),'opacity':('outfit',False),
       'piece':('outfit',False),'skin-gloss':('body',False),'pattern':('outfit',False),
       'figure':('body',False),'motion':('body',False)}

# Every kind but a colour is edited as one number, so the menu draws a slider for it
# and a saved look stores a single value.
KINDS=('color','intensity','scalar','toggle','choice','spring','shape','glow','opacity','dynamics','rig')

BONE=re.compile(r'[A-Za-z0-9_]{1,64}\Z')
BODY_REGIONS=('brust001','brust002','butt001','butt002','thigh_twist_02_l','thigh_twist_02_r','belly')

def body_rig(control):
    return kind_of(control)=='rig' and control.get('solver','positional_hair')=='angular_body'
# Same rule CSSImportMesh applies. It refuses any name the engine would have had to
# rename at build time, so a name that passes here is one the runtime can address.
MORPH=re.compile(r'[A-Za-z0-9_]{1,64}\Z')

def spring_range(value,what,ceiling):
    """An author's {"min","max","default"} for one of a spring's two sliders."""
    if not isinstance(value,dict) or set(value)!={'min','max','default'}:
        raise ValueError(f'A spring {what} needs min, max and default')
    low,high,start=value['min'],value['max'],value['default']
    for x in (low,high,start): number(x,0,ceiling)
    if not 0<low<high<=ceiling or not low<=start<=high: raise ValueError(f'Invalid spring {what} range')
    return low,high,start

def spring_tuning(frequency,damping_ratio):
    """The two numbers the engine wants, from the two a person can reason about.

    FAnimNode_SpringBone integrates a = K*error - D*velocity at a fixed 1/120 s with
    no mass term, so the system is x'' + D x' + K x = 0. Must match spring_tuning()
    in native/src/controls.cpp.
    """
    w=2*math.pi*frequency
    return w*w,2*damping_ratio*w

def dynamics_ranges(control):
    ranges=[]
    for key,low,high in [('angular_spring',0,1000),('damping',.7,1),('gravity',-5,5)]:
        value=control.get(key)
        if not isinstance(value,dict) or set(value)!={'min','max','default'}:
            raise ValueError('A dynamics range needs min, max and default')
        for x in value.values(): number(x,low,high)
        if not value['min']<value['max'] or not value['min']<=value['default']<=value['max']:
            raise ValueError('Dynamics range outside solver limits')
        ranges.append(value)
    return ranges

def rig_ranges(control):
    ranges=[]
    if control.get('solver','positional_hair') not in ('positional_hair','angular_body'):
        raise ValueError('Unknown rig solver')
    fields=([('frequency',.5,6),('damping_ratio',.1,2),('motion_amount',0,1)] if body_rig(control)
            else [('stiffness',1,1000),('damping',0,120),('gravity',-5,5)])
    for key,low,high in fields:
        value=control.get(key)
        if not isinstance(value,dict) or set(value)!={'min','max','default'}:
            raise ValueError('A rig range needs min, max and default')
        for x in value.values(): number(x,low,high)
        if not value['min']<value['max'] or not value['min']<=value['default']<=value['max']:
            raise ValueError('Rig range outside solver limits')
        ranges.append(value)
    return ranges

def spring_defaults(stiffness,damping):
    """The other direction: read an animation blueprint's numbers as a slider default.

    A spring control's default has to be what the blueprint already ships, or the menu
    opens on a value the body is not actually at. Point this at the node and copy what
    it prints.
    """
    w=math.sqrt(stiffness)
    return w/(2*math.pi),(damping/(2*w) if w else 0)

# Same rule as valid_asset() in native/src/data.cpp: a full object path, so the dot
# comes after the last slash. /Game/CSS/x/T_Foo.T_Foo, not /Game/CSS/x/T_Foo.
ASSET=re.compile(r'/Game/[A-Za-z0-9_/.]{1,1010}\Z')

def asset(value):
    if not isinstance(value,str) or not ASSET.fullmatch(value) or '..' in value: raise ValueError('Invalid asset path')
    tail=value.rsplit('/',1)[-1]
    if '.' not in tail or tail.endswith('.'): raise ValueError('An asset path needs its object name')

def kind_of(control) -> str:
    kind=control.get('kind')
    if kind is not None: return kind
    # `type` predates the split between a strength and an ordinary material scalar.
    return 'intensity' if control.get('type')=='scalar' else 'color'

def scalar(control) -> bool:
    return kind_of(control)!='color'

def validate(recipe:dict) -> set[str]:
    if not isinstance(recipe,dict) or recipe.get('schema')!=1: raise ValueError('Unsupported customize schema')
    controls=recipe.get('controls');bounded_array(controls,32)
    by_id={};used=set();files=set();dynamics_roots=set();rig_owner=False;body_owners=set()
    for c in controls:
        identifier(c['id'])
        if c['id'] in by_id or not isinstance(c['name'],str) or not 1<=len(c['name'])<=96: raise ValueError('Invalid color control name')
        by_id[c['id']]=c
        if c.get('type','color') not in ('color','scalar','intensity'): raise ValueError('Invalid color control type')
        if 'kind' in c and c['kind'] not in KINDS: raise ValueError('Invalid color control kind')
        if 'kind' in c and 'type' in c and scalar(c)!=(c['type'] in ('scalar','intensity')): raise ValueError('Color control kind contradicts its type')
        if kind_of(c)=='glow':
            number(c.get('pulse_hz',0),0,10)
            if not isinstance(c.get('combat_reactive',False),bool):
                raise ValueError('combat_reactive must be true or false')
        if kind_of(c)=='toggle':
            sections=c.get('sections');bounded_array(sections,128)
            if not sections: raise ValueError('A toggle control needs the sections it hides')
            for index in sections:
                if type(index) is not int: raise ValueError('Material section must be an integer')
                number(index,0,127)
            used.add(c['id'])
        elif 'sections' in c: raise ValueError('Only a toggle control hides material sections')
        if 'occludes_sections' in c:
            if kind_of(c)!='toggle': raise ValueError('Only a toggle control occludes material sections')
            covered=c['occludes_sections'];bounded_array(covered,128)
            if not covered: raise ValueError('Occluded sections must not be empty')
            for index in covered:
                if type(index) is not int: raise ValueError('Occluded section must be an integer')
                number(index,0,127)
            if len(set(covered))!=len(covered) or set(covered).intersection(c['sections']):
                raise ValueError("A toggle's occluded sections must be distinct from its own sections")
        for key in ('max_displacement','translate','rotate','error_reset','planar_constraint',
                    'world_damping','limit_angle','collision_radius','gravity_scale'):
            if key in c and kind_of(c)!='spring':
                raise ValueError(f'Only a spring control accepts {key}')
        for key in ('angular_spring','damping','gravity'):
            if key in c and (kind_of(c) not in ('dynamics','rig') or (key=='angular_spring' and kind_of(c)=='rig')):
                raise ValueError('Only dynamics controls accept solver ranges')
        if kind_of(c)=='rig':
            if body_rig(c):
                regions=c.get('regions');bounded_array(regions,7)
                if not regions or any(r not in BODY_REGIONS for r in regions) or len(set(regions))!=len(regions):
                    raise ValueError('Expected distinct canonical body regions')
                if body_owners.intersection(regions): raise ValueError('Body regions must have one control owner')
                body_owners.update(regions)
                if any(k in c for k in ('stiffness','damping','gravity')):
                    raise ValueError('Body rig uses frequency, damping ratio and motion amount')
            else:
                if rig_owner: raise ValueError('The post-process rig needs one control owner')
                rig_owner=True
                if any(k in c for k in ('frequency','damping_ratio','motion_amount','regions')):
                    raise ValueError('Hair rig does not accept body settings')
            if any(k in c for k in ('default','min','max','step','nodes','bindings','angular_spring')):
                raise ValueError('Rig controls use their own ranges and no material bindings or node names')
            if not isinstance(c.get('enabled',True),bool): raise ValueError('Rig enabled default must be boolean')
            used.add(c['id'])
        elif any(k in c for k in ('stiffness','enabled','solver','regions','motion_amount')):
            raise ValueError('Only rig controls accept stiffness and enabled')
        if kind_of(c) in ('spring','dynamics'):
            nodes=c.get('nodes');bounded_array(nodes,32)
            if not nodes: raise ValueError('A spring control needs between one and thirty-two bones')
            if len(set(nodes))!=len(nodes): raise ValueError('A spring control names the same bone twice')
            for name in nodes:
                if not isinstance(name,str) or not BONE.fullmatch(name): raise ValueError('Invalid spring bone name')
            if 'default' in c: raise ValueError('A spring control takes its default from its frequency and damping ratio')
            if any(k in c for k in ('min','max','step')): raise ValueError('A spring control takes its limits from its frequency and damping ratio')
            if c.get('bindings'): raise ValueError('A spring control writes no material parameter')
            if kind_of(c)=='dynamics':
                if any(key in c for key in ('frequency','damping_ratio','bindings')):
                    raise ValueError('Dynamics does not accept spring ranges or material bindings')
                if dynamics_roots.intersection(nodes): raise ValueError('Dynamics chain roots must have one control owner')
                dynamics_roots.update(nodes)
            used.add(c['id'])
        elif not body_rig(c) and any(k in c for k in ('nodes','frequency','damping_ratio')):
            raise ValueError('Only a spring control tunes skeleton nodes')
        if kind_of(c)=='shape':
            morph=c.get('morph')
            if not isinstance(morph,str) or not MORPH.fullmatch(morph): raise ValueError('Invalid morph target name')
            if c.get('bindings'): raise ValueError('A shape control writes no material parameter')
            used.add(c['id'])
        elif 'morph' in c: raise ValueError('Only a shape control drives a morph target')
        if kind_of(c)=='choice':
            options=c.get('options');bounded_array(options,16)
            if len(options)<2: raise ValueError('A choice control needs between two and sixteen options')
            for option in options:
                if not isinstance(option.get('name'),str) or not 1<=len(option['name'])<=96: raise ValueError('Invalid choice option name')
                asset(option['texture'])
            if not c.get('bindings'): raise ValueError('A choice control needs a texture parameter to write into')
        elif 'options' in c: raise ValueError('Only a choice control lists texture options')
        # The convention's three fields. All optional: CSS infers them from the id
        # for packages that predate it, so an old recipe still loads.
        if 'role' in c: identifier(c['role'])
        if 'role' in c and len(c['role'])>32: raise ValueError('Color control role is too long')
        if 'group' in c and c['group'] not in ('outfit','body'): raise ValueError('Invalid color control group')
        if 'hue_locked' in c and not isinstance(c['hue_locked'],bool): raise ValueError('hue_locked must be true or false')
        if kind_of(c)=='rig':
            rig_ranges(c)
            continue
        if kind_of(c)=='dynamics':
            dynamics_ranges(c)
            continue
        if kind_of(c)=='spring':
            # 8 Hz is far past anything a body part does and the slowest useful wobble is
            # well above a tenth of a hertz, so a typo lands outside rather than shipping.
            frequency=spring_range(c.get('frequency'),'frequency',8)
            damping=spring_range(c.get('damping_ratio'),'damping ratio',2)
            # Above 1/FixedTimeStep the engine scales damping down instead of using what it
            # was given, so a range whose corner crosses that line stops meaning what the
            # slider says. Refuse it here rather than silently disagreeing in game.
            if spring_tuning(frequency[1],damping[1])[1]>100:
                raise ValueError('Spring range is too stiff and damped for the engine to integrate as written')
            # Optional travel clamp (MaxDisplacement, cm) on channel 2, and optional axis
            # filters and reset threshold. All are absent on a plain two-channel spring.
            if 'max_displacement' in c: spring_range(c['max_displacement'],'travel',16)
            for axis in ('translate','rotate'):
                if axis in c:
                    a=c[axis]
                    if not isinstance(a,list) or len(a)!=3 or not all(isinstance(x,bool) for x in a):
                        raise ValueError(f"A spring's {axis} needs three true or false values")
            if 'error_reset' in c:
                er=c['error_reset']
                if isinstance(er,bool) or not isinstance(er,(int,float)) or not 0<er<=4096:
                    raise ValueError("A spring's error_reset is out of range")
            if 'planar_constraint' in c and c['planar_constraint'] not in ('none','x','y','z'):
                raise ValueError('Invalid planar constraint axis')
            for key,low,high in (('world_damping',0,1),('limit_angle',0,180),
                                 ('collision_radius',0,100),('gravity_scale',-5,5)):
                if key in c: number(c[key],low,high)
            continue
        vector(c['default'])
        if kind_of(c)=='toggle':
            minimum,maximum,step=0,1,1
            if c['default'][0] not in (0,1): raise ValueError('A toggle is on or off')
        elif kind_of(c)=='choice':
            # The value is which option, so the range is the list and nothing else.
            minimum,maximum,step=0,len(c['options'])-1,1
            if c['default'][0]!=int(c['default'][0]) or not 0<=c['default'][0]<=maximum:
                raise ValueError('A choice default must name one of its options')
        else:
            minimum=c.get('min',0);maximum=c.get('max',32 if kind_of(c)=='glow' else 1);step=c.get('step',.01)
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
    surfaces=recipe.get('surfaces',[]);bounded_array(surfaces,16)
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
    palettes=recipe.get('palettes',[]);bounded_array(palettes,64);ids=set()
    for p in palettes:
        identifier(p['id'])
        if p['id']=='original' or p['id'] in ids or not isinstance(p['name'],str) or not 1<=len(p['name'])<=96:raise ValueError('Invalid palette identity')
        ids.add(p['id'])
        if not isinstance(p['values'],dict):raise ValueError('Invalid palette values')
        for id,v in p['values'].items():
            if id not in by_id:raise ValueError('Unknown palette part')
            c=by_id[id]
            if kind_of(c)=='rig':
                vector(v,-5,1000)
                for index,r in enumerate(rig_ranges(c)): number(v[index],r['min'],r['max'])
                if v[3] not in (0,1): raise ValueError('Rig enabled value must be zero or one')
                continue
            if kind_of(c)=='dynamics':
                vector(v,-5,1000)
                for index,r in enumerate(dynamics_ranges(c)):number(v[index],r['min'],r['max'])
                if v[3]!=1:raise ValueError('Dynamics reserved channel must be one')
                continue
            vector(v)
            if kind_of(c)=='spring':
                # Two channels, two ranges. A spring carries no `default`, so its protected
                # opacity is the one CSS gives every control that does not state one.
                number(v[0],c['frequency']['min'],c['frequency']['max'])
                number(v[1],c['damping_ratio']['min'],c['damping_ratio']['max'])
                if 'max_displacement' in c:
                    number(v[2],c['max_displacement']['min'],c['max_displacement']['max'])
                if v[3]!=1:raise ValueError('Palette changes protected opacity')
                continue
            maximum = len(c['options'])-1 if kind_of(c)=='choice' else c.get('max',32 if kind_of(c)=='glow' else 1)
            if kind_of(c)=='toggle': maximum=1
            minimum = 0 if kind_of(c) in ('choice','toggle') else c.get('min',0)
            for x in v[:1 if scalar(c) else 3]:number(x,minimum,maximum)
            if kind_of(c) in ('choice','toggle') and v[0]!=int(v[0]):
                raise ValueError('A toggle or choice needs a whole-number palette value')
            if v[3]!=c['default'][3]:raise ValueError('Palette changes protected opacity')
    for c in controls:
        if kind_of(c) in ('spring','dynamics') and body_owners.intersection(c.get('nodes',[])):
            raise ValueError('Body region cannot also be driven by a spring or AnimDynamics control')
    return files

def resource_info(path:Path) -> dict:
    from css_convert import digest
    data=path.read_bytes()
    if not RESOURCE.fullmatch(path.name) or not 33<=len(data)<=32*1024*1024 or data[:8]!=b'\x89PNG\r\n\x1a\n' or data[12:16]!=b'IHDR':raise ValueError('Invalid dye PNG')
    w,h=struct.unpack_from('>II',data,16)
    if w!=h or w not in (1024,2048,4096) or data[24]!=8 or data[25] not in (4,6):raise ValueError('Dye layers require square 8-bit PNG with alpha, 1024 to 4096 pixels')
    return dict(bytes=len(data),width=w,height=h,sha256=digest(path))

def block(owner:dict) -> dict|None:
    """The customize block, under either name.

    1.0 renamed it from "colors" to "customize", because it carries switches, textures
    and springs as well as colour. The runtime reads either; a recipe or manifest written
    before the rename keeps working.
    """
    if 'customize' in owner:
        if 'colors' in owner: raise ValueError('Name the controls once: customize, or the older colors, not both')
        return owner['customize']
    return owner.get('colors')

def embed(recipe:Path,manifest:dict,metadata:Path,variant:dict|None=None):
    j=json.loads(recipe.read_text())
    if j['id']!=manifest['id']:raise ValueError('Control recipe belongs to another outfit')
    controls=block(j)
    if controls is None: raise ValueError('Recipe has no customize block')
    files=validate(controls)
    (variant if variant is not None else manifest['catalog']['outfits'][0])['customize']=controls
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
    controls=block(manifest['catalog']['outfits'][0])
    files=validate(controls) if controls else set()
    for variant in manifest['catalog']['outfits'][0].get('variants',[]):
        inner=block(variant)
        if inner is not None:files.update(validate(inner))
    resources=manifest.get('resources',{})
    if set(resources)!=files:raise ValueError('Dye resource manifest does not match recipe')
    total=0
    for name,info in resources.items():
        if resource_info(metadata/name)!=info:raise ValueError('Dye resource checksum or dimensions differ')
        total+=info['bytes']
    if total>256*1024*1024:raise ValueError('Dye resources exceed 256 MiB')

def shape_names(mesh_json:dict) -> set[str]:
    """The morph targets an authored mesh actually carries, from its CSSImportMesh input."""
    return {m['name'] for m in mesh_json.get('morph_targets',[])}

def check_shapes(recipe:dict,available:set[str]):
    """Every shape control must name a morph the mesh really has.

    At runtime a control CSS cannot apply takes the whole outfit off, which is the right
    call for a half-applied look but a miserable way to find a typo. This is the check
    that stops it shipping, and it needs no editor: the mesh JSON that CSSImportMesh
    consumes lists the shapes, and the recipe names them.
    """
    missing=sorted(c['morph'] for c in recipe.get('controls',[])
                   if kind_of(c)=='shape' and c.get('morph') not in available)
    if missing:
        raise ValueError('Shape controls name morph targets the mesh does not have: '+', '.join(missing))

def lint_convention(recipe:dict) -> list[str]:
    """Check a recipe against docs/control-convention.md. Authoring only.

    validate() decides whether a recipe is safe to load, and stays permissive so
    that every package published before the convention keeps working. This is the
    other half: it decides whether a recipe is a *good* one, and it is meant to be
    called from a package's own builder, where breaking the build is the point.
    """
    problems=[]
    controls=recipe.get('controls',[])
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
    palettes=recipe.get('palettes',[])
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
