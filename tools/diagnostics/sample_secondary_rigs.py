"""Read the current post-process rig counters and bounded numeric state."""
from check_peony_live_presets import engine


FIELDS = ('Enabled', 'TotalSteps', 'ResetCount', 'SeenUpdate', 'SubSteps',
          'StepSeconds', 'Limits', 'MaxTravel', 'orientation', 'Display', 'Omega')


def sample_secondary_rigs():
    def get(target, name):
        return engine({'op': 'get', 'target': target, 'property': name})

    def post_instance(component):
        engine({'op': 'describe', 'target': component, 'function': 'GetPostProcessInstance'})
        return engine({'op': 'call', 'target': component,
                       'function': 'GetPostProcessInstance', 'args': {}})['ReturnValue']

    pawn = engine({'op': 'player'})['pawn']
    component = get(pawn, 'Mesh')
    post = post_instance(component)
    nodes = engine({'op': 'properties', 'target': post, 'inherited': True})
    rigs = []
    for node in nodes:
        if not node['name'].startswith('AnimGraphNode_ControlRig'):
            continue
        value = get(post, node['name'])
        rig = value.get('ControlRig')
        if not rig:
            continue
        properties = engine({'op': 'properties', 'target': rig, 'inherited': True})
        names = {p['name'] for p in properties}
        rigs.append(dict(node=node['name'], object=rig,
                         values={name: get(rig, name) for name in FIELDS if name in names}))
    if not rigs or not any('TotalSteps' in rig['values'] for rig in rigs):
        raise RuntimeError('No measurable secondary rig on the current post-process instance')
    if post_instance(component) != post or get(pawn, 'Mesh') != component:
        raise RuntimeError('Animation owner changed during the rig read')
    return dict(post=post, component=component, rigs=rigs)
