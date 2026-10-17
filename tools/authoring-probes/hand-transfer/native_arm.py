"""Native two-bone arm fit, with the incoming prop transform preserved."""
from build_controlrig_chain_probe import Pin, TYPES


def append_arm_support(g, start):
    TYPES['FQuat'] = '/Script/CoreUObject.Quat'
    g.types.update(ShoulderEnabled='bool', ShoulderCarryAlpha='float')
    g.member('ShoulderGrip', 'FTransform', '', True)
    g.member('ShoulderArmEnabled', 'bool', 'False', True)
    g.member('ShoulderArmValid', 'bool', 'False', True)
    g.member('ShoulderPoseValid', 'bool', 'False', True)
    g.member('ShoulderInputWristHeight', 'float', '0', True)
    g.member('ShoulderInputPropZ', 'float', '0', True)
    bones = ['clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r', 'prop_r']
    for space in ['Global', 'Local']:
        for bone in bones:
            name = 'Arm' + space + bone
            g.member(name, 'FTransform', '')
            node = g.unit('RigUnit_GetTransform', Space=space+'Space', bInitial=False,
                          Item=f'(Type=Bone,Name="{bone}")')
            start = g.set(start, name, Pin(node+'.Transform'))
    def glob(bone, field):
        return g.get('ArmGlobal'+bone, field)
    def loc(bone, field):
        return g.get('ArmLocal'+bone, field)
    start = g.set(start,'ShoulderInputWristHeight',g.math('FloatSub',A=glob('hand_r','Translation.Z'),B=glob('clavicle_r','Translation.Z')))
    start = g.set(start,'ShoulderInputPropZ',loc('prop_r','Translation.Z'))
    conditions=[]
    for name,minimum,maximum in [('ShoulderInputWristHeight',-24.,5.),('ShoulderInputPropZ',-1.,6.)]:
        conditions.append(g.math('BoolAnd',A=g.math('FloatGreater',A=g.get(name),B=minimum),B=g.math('FloatLess',A=g.get(name),B=maximum)))
    start = g.set(start,'ShoulderPoseValid',g.math('BoolAnd',A=conditions[0],B=conditions[1]))
    mul = lambda a,b: g.math('QuaternionMul', A=a, B=b)
    inv = lambda q: g.math('QuaternionInverse', Value=q)
    rot = lambda q,v: g.math('QuaternionRotateVector', Transform=q, Vector=v)
    p0,p1,p2,pp = [glob(n,'Translation') for n in bones[1:]]
    q0,q1,q2,qp = [glob(n,'Rotation') for n in bones[1:]]
    desiredq = mul(qp, inv(g.get('ShoulderGrip','Rotation')))
    offset = g.math('VectorMul', A=g.get('ShoulderGrip','Translation'), B=glob('hand_r','Scale3D'))
    desiredp = g.sub(pp, rot(desiredq,offset))
    l1,l2 = g.length(g.sub(p1,p0)),g.length(g.sub(p2,p1))
    distance = g.length(g.sub(desiredp,p0))
    valid = g.math('BoolAnd', A=g.math('FloatGreater',A=distance,B=g.math('FloatAdd',A=g.math('FloatAbs',Value=g.math('FloatSub',A=l1,B=l2)),B=.0001)),
                   B=g.math('FloatLess',A=distance,B=g.math('FloatSub',A=g.math('FloatAdd',A=l1,B=l2),B=.0001)))
    start = g.set(start,'ShoulderArmValid',valid)
    active = g.math('BoolAnd',A=g.get('ShoulderEnabled'),B=g.get('ShoulderArmEnabled'))
    active = g.math('BoolAnd',A=active,B=g.get('ShoulderArmValid'))
    active = g.math('BoolAnd',A=active,B=g.get('ShoulderPoseValid'))
    active = g.math('BoolAnd',A=active,B=g.math('FloatGreater',A=g.get('ShoulderCarryAlpha'),B=0.))
    start,bypass = g.branch(start,active)
    solve = g.unit('RigUnit_TwoBoneIKSimpleVectors', Root=p0, PoleVector=p1, Effector=desiredp,
                   BoneALength=l1, BoneBLength=l2, bEnableStretch=False)
    mid,end = Pin(solve+'.Elbow'),Pin(solve+'.Effector')
    delta0 = g.math('QuaternionFromTwoVectors', A=g.sub(p1,p0),B=g.sub(mid,p0))
    new0 = mul(delta0,q0)
    delta1 = g.math('QuaternionFromTwoVectors', A=rot(delta0,g.sub(p2,p1)),B=g.sub(end,mid))
    new1 = mul(delta1,mul(delta0,q1))
    targets = [mul(inv(glob('clavicle_r','Rotation')),new0),mul(inv(new0),new1),mul(inv(new1),desiredq)]
    alpha = g.math('FloatClamp',Value=g.get('ShoulderCarryAlpha'),Minimum=0.,Maximum=1.)
    for bone,q in zip(bones[1:4],targets):
        g.member('ArmOutput'+bone,'FQuat','(X=0,Y=0,Z=0,W=1)')
        start = g.set(start,'ArmOutput'+bone,g.math('QuaternionSlerp',A=loc(bone,'Rotation'),B=q,T=alpha))
    for bone in bones[1:4]:
        node = g.unit('RigUnit_SetTransform', Space='LocalSpace', bInitial=False,
                      bPropagateToChildren=True, Item=f'(Type=Bone,Name="{bone}")')
        g.link(start,node+'.ExecuteContext')
        g.value(node+'.Value.Translation',loc(bone,'Translation'))
        g.value(node+'.Value.Scale3D',loc(bone,'Scale3D'))
        g.value(node+'.Value.Rotation',g.get('ArmOutput'+bone))
        start = Pin(node+'.ExecuteContext')
    node = g.unit('RigUnit_SetTransform', Space='GlobalSpace', bInitial=False,
                  bPropagateToChildren=True, Item='(Type=Bone,Name="prop_r")', Value=g.get('ArmGlobalprop_r'))
    g.link(start,node+'.ExecuteContext')
    return Pin(node+'.ExecuteContext'),bypass
