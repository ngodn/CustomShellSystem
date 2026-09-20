"""Compose the V44 hand stages inside one shipped-node Control Rig graph."""
from build_controlrig_chain_probe import Pin
from native_nodes import (append_calibration, append_articulation,
                          append_web_guard, append_correctives, clear_correctives)
from native_clearance import append_clearance


class StageGraph:
    """Separate solver members while sharing the graph and execution chain."""
    def __init__(self, graph, prefix):
        self.graph, self.prefix = graph, prefix

    def __getattr__(self, name):
        return getattr(self.graph, name)

    def member(self, name, *args):
        return self.graph.member(self.prefix + name, *args)

    def get(self, name, *args):
        return self.graph.get(self.prefix + name, *args)

    def set(self, execution, name, value):
        return self.graph.set(execution, self.prefix + name, value)

    def at(self, name, *args):
        return self.graph.at(self.prefix + name, *args)

    def put(self, execution, name, *args):
        return self.graph.put(execution, self.prefix + name, *args)


def append_hand(g, controller, start, calibration, articulation, clearance, web, curves):
    """Fresh input only; preserve the incoming pose on a rejected stage."""
    g.member('HandInputs', 'TArray<FTransform>', '(' + ','.join('()' for _ in calibration) + ')')
    g.member('HandValid', 'bool', 'False', True)
    start = g.set(start, 'HandValid', False)
    for i, m in enumerate(calibration):
        node = g.unit('RigUnit_GetTransform', Space='LocalSpace', bInitial=False)
        g.value(node + '.Item.Type', 'Bone')
        g.value(node + '.Item.Name', m['bone'])
        start = g.put(start, 'HandInputs', i, Pin(node + '.Transform'))
    active, disabled = g.branch(start, g.get('Enabled'))
    active, invalid_calibration = append_calibration(g, active, calibration)
    active, invalid_articulation = append_articulation(g, active, articulation)
    rejected = [invalid_calibration, invalid_articulation]
    for stage, prefix, settings in (
        ('fingers', 'Finger_', dict(clearance, iteration_limit=1, gradient_step_degrees=1.)),
        ('thumb', 'Thumb_', clearance),
    ):
        scoped = StageGraph(g, prefix)
        active = append_clearance(scoped, active, settings, stage)
        active, invalid = g.branch(active, scoped.get('ClearanceValid'))
        rejected.append(invalid)
        models = clearance['models'][1:] if stage == 'fingers' else clearance['models'][:1]
        for i, m in enumerate(models):
            node = g.unit('RigUnit_SetTransform', Space='LocalSpace', bInitial=False, bPropagateToChildren=True)
            g.link(active, node + '.ExecuteContext')
            g.value(node + '.Item.Type', 'Bone')
            g.value(node + '.Item.Name', clearance['bones'][m['indices'][0]]['name'])
            g.value(node + '.Value', scoped.at('ClearanceOutput', i))
            active = Pin(node + '.ExecuteContext')
    driver = next(m for m in curves if m['bone'] == web['bone'])
    active = append_web_guard(g, active, web, driver)
    active = append_correctives(g, controller, active, curves, True)
    g.set(active, 'HandValid', True)
    # Failure after an earlier stage must restore the entire incoming hand.
    # Hierarchy comparisons retain Unreal's ordinary small-write tolerance.
    for branch in rejected:
        for i, m in enumerate(calibration):
            node = g.unit('RigUnit_SetTransform', Space='LocalSpace', bInitial=False, bPropagateToChildren=True)
            g.link(branch, node + '.ExecuteContext')
            g.value(node + '.Item.Type', 'Bone')
            g.value(node + '.Item.Name', m['bone'])
            g.value(node + '.Value', g.at('HandInputs', i))
            branch = Pin(node + '.ExecuteContext')
        branch = g.set(branch, 'WebCorrectionDegrees', 0)
        clear_correctives(g, branch, curves)
    disabled = g.set(disabled, 'WebCorrectionDegrees', 0)
    clear_correctives(g, disabled, curves)
