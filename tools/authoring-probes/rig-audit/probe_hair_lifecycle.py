"""Exercise existing hair rigs in a read-only UE 5.6.1 editor process.

This measures direct RigVM execution, not gameplay cost or rendered contact.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import time

import unreal


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def key(name):
    return unreal.RigElementKey(name=name, type=unreal.RigElementType.BONE)


def transform(value):
    return [value.translation.x, value.translation.y, value.translation.z,
            value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w,
            value.scale3d.x, value.scale3d.y, value.scale3d.z]


def set_value(rig, name, value):
    if isinstance(value, bool):
        value = 'True' if value else 'False'
    if not rig.set_variable_from_string(name, str(value)):
        raise RuntimeError('Cannot set rig input: ' + name)


def evaluate(blueprint, names, case, fps):
    component = unreal.SceneComponent()
    rig = blueprint.create_control_rig()
    if not rig.rename(outer=component):
        raise RuntimeError('Cannot attach the transient rig to its owner')
    actual = re.findall(r'CSS_Hair_[A-Za-z0-9_]+', rig.get_variable_as_string('BoneNames'))
    if list(dict.fromkeys(actual)) != names:
        raise ValueError('Editor and cooked hair inventories differ')
    hierarchy = rig.get_hierarchy()
    bones = hierarchy.get_bones()
    other = [item for item in bones if str(item.name) not in names]
    samples = []
    faults = []
    for frame in range(fps * 2 + 1):
        now = frame / fps
        hierarchy.reset_pose_to_initial(unreal.RigElementType.BONE)
        head = hierarchy.get_global_transform(key('head'), True)
        angle = math.radians(20) * math.sin(math.pi * now)
        head.rotation = head.rotation * unreal.Quat(0, math.sin(angle / 2), 0, math.cos(angle / 2))
        hierarchy.set_global_transform(key('head'), head, False, True)
        owner = unreal.Transform()
        if case == 'teleport' and frame >= fps:
            owner.translation = unreal.Vector(1000, 0, 0)
        component.set_world_transform(owner, False, True)
        enabled = case != 'disabled' and not (case == 'reenable' and fps // 2 <= frame < fps)
        epoch = int(case in ('reset', 'teleport') and frame >= fps)
        delta = 0 if frame == 0 else .2 if case == 'hitch' and frame == fps else 1 / fps
        incoming = [transform(hierarchy.get_global_transform(key(name))) for name in names]
        protected = [transform(hierarchy.get_global_transform(item)) for item in other]
        set_value(rig, 'UpdateSerial', frame)
        set_value(rig, 'ResetEpoch', epoch)
        set_value(rig, 'Enabled', enabled)
        rig.set_delta_time(delta)
        start = time.perf_counter()
        if not rig.execute('Forwards Solve'):
            raise RuntimeError('Hair solve failed')
        elapsed = time.perf_counter() - start
        output = [transform(hierarchy.get_global_transform(key(name))) for name in names]
        steps = int(rig.get_variable_as_string('TotalSteps'))
        resets = int(rig.get_variable_as_string('ResetCount'))
        substeps = int(rig.get_variable_as_string('Substeps'))
        if not all(math.isfinite(v) for pose in output for v in pose):
            faults.append([frame, 'Nonfinite output'])
        if any(transform(hierarchy.get_global_transform(item)) != before
               for item, before in zip(other, protected)):
            faults.append([frame, 'Non-hair bone changed'])
        if not enabled and output != incoming:
            faults.append([frame, 'Disabled solver changed incoming pose'])
        if enabled and not 0 <= substeps <= 8:
            faults.append([frame, 'Unexpected substep count'])
        if case == 'repeat':
            if not rig.execute('Forwards Solve'):
                raise RuntimeError('Repeated hair solve failed')
            repeated = [transform(hierarchy.get_global_transform(key(name))) for name in names]
            if repeated != output or int(rig.get_variable_as_string('TotalSteps')) != steps:
                faults.append([frame, 'Repeated evaluation advanced or changed the result'])
        samples.append(dict(frame=frame, seconds=now, delta=delta, enabled=enabled,
                            epoch=epoch, steps=steps, resets=resets, substeps=substeps,
                            input=incoming, output=output, execution_seconds=elapsed))
    if case == 'disabled' and samples[-1]['steps'] != 0:
        faults.append(['final', 'Disabled solver advanced its simulation'])
    if case in ('reset', 'teleport', 'hitch', 'reenable'):
        if samples[fps]['resets'] <= samples[fps - 1]['resets']:
            faults.append([fps, 'Expected reset was not observed'])
    return dict(case=case, fps=fps, hair_bones=names, protected_bones=len(other),
                failures=faults, samples=samples)


def main():
    output = Path(os.environ['CSS_HAIR_LIFECYCLE_OUTPUT']).resolve(strict=True)
    inventory_path = Path(os.environ['CSS_HAIR_LIFECYCLE_INVENTORY']).resolve(strict=True)
    inventory = json.loads(inventory_path.read_text())
    content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
    rows = []
    sources = {}
    cases = [('motion', fps) for fps in (30, 60, 120)]
    cases += [(case, 60) for case in ('disabled', 'reenable', 'reset', 'teleport', 'hitch', 'repeat')]
    for package, data in inventory['rigs'].items():
        if '/CR_Hair' not in package:
            continue
        source = (content / (package.removeprefix('/Game/') + '.uasset')).resolve(strict=True)
        sources[str(source)] = digest(source)
        blueprint = unreal.load_asset(package)
        if not isinstance(blueprint, unreal.ControlRigBlueprint):
            raise ValueError('Missing hair blueprint: ' + package)
        names = list(dict.fromkeys(data['defaults']['BoneNames']))
        for case, fps in cases:
            result = evaluate(blueprint, names, case, fps)
            target = output / (str(len(rows)) + '.json')
            with target.open('x') as handle:
                json.dump(result, handle, separators=(',', ':'), allow_nan=False)
            rows.append(dict(package=package, case=case, fps=fps, samples=len(result['samples']),
                             failures=result['failures'], report=str(target), sha256=digest(target)))
            unreal.log('CSS_HAIR_LIFECYCLE ' + package + ' ' + case + ' ' + str(fps))
    if len(rows) != 27:
        raise ValueError('Expected three rigs and nine cases each')
    for path, expected in sources.items():
        if digest(Path(path)) != expected:
            raise ValueError('Source changed: ' + path)
    report = dict(passed=not any(row['failures'] for row in rows), cases=rows,
                  sources=sources, inventory_sha256=digest(inventory_path),
                  script_sha256=digest(Path(__file__)),
                  scope='Direct editor RigVM lifecycle. No rendered contact, AnimBP scheduling or game frame-time acceptance.')
    with (output / 'report.json').open('x') as handle:
        json.dump(report, handle, indent=2)
    if not report['passed']:
        raise RuntimeError('Hair lifecycle checks failed; see report.json')


main()
