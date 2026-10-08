"""Resolve literal hand targets from editor snapshots, not cooked VM memory."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def scalar(properties, field):
    values = properties.get(field)
    if not isinstance(values, list) or len(values) != 1:
        raise ValueError('Missing or ambiguous field: ' + field)
    return values[0]


def inspect(snapshot, asset, skeleton):
    objects = snapshot[asset]['properties']
    links = []
    functions = Counter()
    for obj in objects.values():
        props = obj['properties']
        if obj['class'].endswith('.RigVMLink'):
            links.append((scalar(props, 'SourcePinPath'), scalar(props, 'TargetPinPath')))
        elif obj['class'].endswith('.RigVMUnitNode'):
            functions[scalar(props, 'ResolvedFunctionName')] += 1

    def pin(node, field):
        local = node.split(':', 1)[1].split('.', 1)[1] + '.' + field
        # An incoming parent struct or child field can override a literal.
        incoming = [(src, dst) for src, dst in links
                    if local == dst or local.startswith(dst + '.') or dst.startswith(local + '.')]
        return {'default': scalar(objects[node + '.' + field]['properties'], 'DefaultValue'),
                'incoming': incoming}

    writers, curves = [], []
    for name, obj in objects.items():
        function = obj['properties'].get('ResolvedFunctionName', [])
        if function == ['FRigUnit_SetTransform::Execute']:
            row = {'node': name.split(':', 1)[1], 'pins': {field: pin(name, field) for field in
                   ('Item.Type', 'Item.Name', 'Space', 'bInitial', 'bPropagateToChildren', 'Weight')}}
            writers.append(row)
        elif function == ['FRigUnit_SetCurveValue::Execute']:
            curves.append({'node': name.split(':', 1)[1], 'curve': pin(name, 'Curve')})

    bones = skeleton['ReferenceSkeleton']['FinalRefBoneInfo']
    names = [b['Name'] for b in bones]
    if len(names) != len(set(names)):
        raise ValueError('Duplicate skeleton bone names')
    children = {name: [] for name in names}
    for i, bone in enumerate(bones):
        parent = bone['ParentIndex']
        if parent == -1:
            continue
        if not 0 <= parent < i:
            raise ValueError('Invalid skeleton parent index')
        children[names[parent]].append(bone['Name'])

    def descendants(name):
        return [child for immediate in children[name]
                for child in [immediate, *descendants(immediate)]]

    targets = Counter()
    affected = set()
    unresolved = []
    for row in writers:
        pins = row['pins']
        if any(p['incoming'] for p in pins.values()):
            unresolved.append(row['node'])
            continue
        if pins['Item.Type']['default'] != 'Bone':
            unresolved.append(row['node'])
            continue
        name = pins['Item.Name']['default']
        if name not in children:
            raise ValueError('Writer target absent from skeleton: ' + name)
        targets[name] += 1
        affected.add(name)
        if pins['bPropagateToChildren']['default'] == 'True':
            affected.update(descendants(name))
        elif pins['bPropagateToChildren']['default'] != 'False':
            raise ValueError('Unknown propagation literal')
    return {'evidence_kind': 'editor_source_targets_with_cooked_skeleton_hierarchy',
            'unit_functions': dict(sorted(functions.items())), 'writers': writers,
            'curve_writers': curves, 'literal_target_counts': dict(sorted(targets.items())),
            'potentially_affected_bones': sorted(affected), 'unresolved_writers': unresolved,
            'limits': ['Does not decode cooked VM literals or prove runtime branch activation.',
                       'Repeated writers can belong to mutually exclusive restoration branches.',
                       'Descendant propagation is potential influence, not measured deformation.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', required=True, type=Path)
    parser.add_argument('--asset', required=True)
    parser.add_argument('--readback', required=True, type=Path)
    parser.add_argument('--skeleton', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    source = args.snapshot.read_bytes()
    cooked = args.readback.read_bytes()
    skeletons = [e for e in json.loads(cooked)[args.skeleton] if e['Type'] == 'Skeleton']
    if len(skeletons) != 1:
        raise ValueError('Expected one Skeleton export')
    report = inspect(json.loads(source), args.asset, skeletons[0])
    report['inputs'] = [{'path': str(path), 'sha256': hashlib.sha256(data).hexdigest()}
                        for path, data in ((args.snapshot, source), (args.readback, cooked))]
    report['source_asset'] = args.asset
    report['cooked_skeleton'] = args.skeleton
    with args.output.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'writers': len(report['writers']),
                      'literal_targets': len(report['literal_target_counts']),
                      'potentially_affected_bones': len(report['potentially_affected_bones']),
                      'unresolved': len(report['unresolved_writers'])}))


if __name__ == '__main__':
    main()
