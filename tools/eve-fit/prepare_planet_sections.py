"""Partition Prototype visibility without deleting body or accessory geometry."""
import copy
import hashlib
import json
from collections import Counter
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=root/'work/eve26'
source=work/'planet-fit4/planet.mesh.json'
data=json.loads(source.read_text())
audit=json.loads((work/'planet-fit4/planet.mesh.audit.json').read_text())
assert hashlib.sha256(source.read_bytes()).hexdigest()==audit['output_sha256']
mask=json.loads((work/'planet-body-mask.json').read_text())
assert mask['topology_equal'] and mask['mapping_error_cm']<.0005
result=copy.deepcopy(data)
aliases={}
def duplicate(old,name):
    assert name not in result['materials']
    result['materials'].append(name)
    index=len(result['materials'])-1
    aliases[str(index)]={'name':name,'source_slot':old,'source_material':data['materials'][old]}
    return index
covered={}
for face_index in mask['hidden_body_faces']:
    assert 0<=face_index<audit['parts'][0]['faces']
    old=data['faces'][face_index][3]
    if old not in covered:covered[old]=duplicate(old,'PlanetCovered_'+str(old))
    result['faces'][face_index][3]=covered[old]
controls=[]
face_offset=0
for part in audit['parts']:
    begin,end=face_offset,face_offset+part['faces']
    face_offset=end
    suffix=part['name'].removeprefix('Eve Prototype Planet Diving Suit - ')
    if suffix not in ('Tail','Ribbon Left','Ribbon Right'):continue
    section_map={}
    for i in range(begin,end):
        old=data['faces'][i][3]
        if old not in section_map:
            section_map[old]=duplicate(old,'Planet'+suffix.replace(' ','')+'_'+str(old))
        result['faces'][i][3]=section_map[old]
    controls.append(dict(id=suffix.lower().replace(' ','_'),name=suffix,kind='toggle',role='piece',default=[1,0,0,1],sections=list(section_map.values())))
suit=audit['parts'][1]
suit_begin=audit['parts'][0]['faces']
suit_slots=sorted({result['faces'][i][3] for i in range(suit_begin,suit_begin+suit['faces'])})
controls.insert(0,dict(id='suit',name='Suit',kind='toggle',role='piece',default=[1,0,0,1],sections=suit_slots,occludes_sections=list(covered.values())))
assert len(result['materials'])<=128
assert all(a[:3]+a[4:]==b[:3]+b[4:] for a,b in zip(data['faces'],result['faces']))
assert all(result[k]==v for k,v in data.items() if k not in ('faces','materials'))
result['mesh_package']='/Game/CSS/EveTest/SK_PlanetFit'
result['skeleton_package']='/Game/CSS/EveTest/SKEL_PlanetFit'
out=work/'planet-sections'
out.mkdir(exist_ok=False)
p=out/'planet.mesh.json';p.write_text(json.dumps(result,separators=(',',':'))+'\n')
audit['output_sha256']=hashlib.sha256(p.read_bytes()).hexdigest()
audit['stage']='Private visibility partition, not production rig binding or game acceptance'
audit['mesh_package']=result['mesh_package'];audit['skeleton_package']=result['skeleton_package']
offset=0
for part in audit['parts']:
    part['material_slots']=sorted({f[3] for f in result['faces'][offset:offset+part['faces']]})
    offset+=part['faces']
(out/'planet.mesh.audit.json').write_text(json.dumps(audit,indent=2)+'\n')
(out/'controls.json').write_text(json.dumps(controls,indent=2)+'\n')
(out/'material-aliases.json').write_text(json.dumps(aliases,indent=2)+'\n')
report=dict(covered_faces=len(mask['hidden_body_faces']),covered_slots=list(covered.values()),
            materials=len(result['materials']),geometry_preserved=True,weights_and_morphs_preserved=True,
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            scope='Private reversible section partition. Original material instances and all color bindings must extend to their duplicate slots. Production 386-bone binding, tail physics and runtime persistence remain pending.')
(out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
