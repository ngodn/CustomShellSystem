import json, math
from pathlib import Path
root=Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/npc-appearances/bosses')
plans=json.loads((root/'selected-plan.json').read_text())
out=root/'isolated-package1'
decoded=json.loads((out/'decoded.json').read_text())
mapping={p[b]:p[a] for p in plans for a,b in [('source_mesh','mesh'),('source_skeleton','skeleton')]}
def normalize(value):
    if isinstance(value,str):
        for new,old in mapping.items():value=value.replace(new,old)
        return value
    if isinstance(value,list):return [normalize(v) for v in value]
    if isinstance(value,dict):return {k:normalize(v) for k,v in value.items()}
    return value
report=[]
for plan in plans:
    source=json.loads((root/plan['source_mesh'].rsplit('/',1)[1]/'source-mesh.json').read_text())
    actual=next(v for v in decoded[plan['mesh']] if v['Type']=='SkeletalMesh')
    target=normalize(actual)
    old_ref=source['Properties']['Skeleton'];new_ref=actual['Properties']['Skeleton']
    assert old_ref['ObjectName']==new_ref['ObjectName']
    assert new_ref['ObjectPath'].rsplit('.',1)[0]==plan['skeleton']
    # Export indices differ because the new Skeleton owns fewer ancillary exports.
    target['Properties']['Skeleton']=old_ref
    assert target==source,plan['id']
    skeleton=next(v for v in decoded[plan['skeleton']] if v['Type']=='Skeleton')
    ref=skeleton['ReferenceSkeleton'];info=ref['FinalRefBoneInfo'];poses=ref['FinalRefBonePose']
    assert len(info)==len(plan['bones'])
    tree=skeleton['Properties']['BoneTree']
    assert len(tree)==len(info)
    for i,(bone,original) in enumerate(zip(info,plan['bones'])):
        assert bone['Name'].lower()==original['name'].lower() and bone['ParentIndex']==original['parent']
        name=bone['Name'].lower()
        mode='Animation' if name=='root' or name.startswith('ik_') else 'AnimationScaled' if name=='pelvis' else 'Skeleton'
        assert tree[i]['TranslationRetargetingMode'].split('::')[-1]==mode
        t=poses[i]
        for key,field,components in [('Translation','translation','XYZ'),('Rotation','rotation','XYZW'),('Scale3D','scale','XYZ')]:
            values=[t[key][k] for k in components]
            error=math.dist(values,original[field])
            if key=='Rotation':error=min(error,math.dist(values,[-v for v in original[field]]))
            assert error<.001,(plan['id'],name,key,error)
    assert '/Game/CSSAuthoring/' not in json.dumps(decoded[plan['skeleton']])
    report.append(dict(id=plan['id'],mesh_fields_preserved=True,bind_preserved=True,translation_modes_verified=True,bones=len(info)))
(out/'verified.json').write_text(json.dumps(dict(passed=True,characters=report,scope='Full decoded native mesh preservation and cooked bind/translation checks; runtime acceptance pending.'),indent=2))
print('Verified',len(report),'native mesh payloads and isolated rigs')
