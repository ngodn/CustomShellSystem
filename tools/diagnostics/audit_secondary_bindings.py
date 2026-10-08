"""Compare cooked secondary-motion bindings before and during shared idle playback."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def package(reference):
    if isinstance(reference, dict):
        reference = reference.get('ObjectPath', '')
    return (reference or '').split('.', 1)[0]


def cdo(exports):
    rows = [e for e in exports if e.get('Name', '').startswith('Default__')]
    if len(rows) != 1:
        raise ValueError('Expected exactly one class default object')
    return rows[0].get('Properties', {})


def fields(exports):
    rows = [e for e in exports if e['Type'].endswith('GeneratedClass')]
    if len(rows) != 1:
        raise ValueError('Expected one generated class')
    return {p['Name']: p for p in rows[0].get('ChildProperties', [])}


def field_type(field):
    result = {k: v for k, v in field.items() if k in
              ('Type', 'ElementSize', 'ArrayDim', 'Struct', 'PropertyClass',
               'FieldSize', 'ByteOffset', 'ByteMask', 'FieldMask', 'BoolSize', 'bIsNativeBool')}
    if 'Inner' in field:
        result['Inner'] = field_type(field['Inner'])
    return result


def binding_name(name):
    # UE regenerates node GUIDs when duplicating a graph. The input pin's
    # identity still has to match, including directly bound public controls.
    return re.sub(r'_[0-9A-Fa-f]{32}$', '', name) if name.startswith('__CustomProperty_') else name


def graph(data, path):
    exports = data[path]
    props, declared = cdo(exports), fields(exports)
    rigs, transforms = [], []
    for name, node in props.items():
        if name.startswith('AnimGraphNode_ControlRig'):
            rig = package(node['ControlRigClass'])
            target_fields = fields(data[rig])
            sources, targets = node.get('SourcePropertyNames', []), node.get('DestPropertyNames', [])
            if len(sources) != len(targets) or len(set(targets)) != len(targets):
                raise ValueError('Ambiguous rig property bindings: ' + path + ':' + name)
            bindings = {}
            for source, target in zip(sources, targets, strict=True):
                if source not in declared or target not in target_fields:
                    raise ValueError('Missing bound field: ' + path + ':' + source + ' -> ' + target)
                bindings[target] = {'source': source, 'source_type': field_type(declared[source]),
                                    'target_type': field_type(target_fields[target])}
                if bindings[target]['source_type'] != bindings[target]['target_type']:
                    raise ValueError('Rig input type differs: ' + path + ':' + source + ' -> ' + target)
            rigs.append({'node': name, 'rig': rig, 'bindings': bindings,
                         'pose_flags': {k: node.get(k) for k in
                           ('bExecute', 'bResetInputPoseToInitial', 'bTransferInputPose',
                            'bTransferInputCurves', 'bTransferPoseInGlobalSpace',
                            'InputBonesToTransfer', 'OutputBonesToTransfer')}})
        elif name.startswith('AnimGraphNode_ModifyBone'):
            transforms.append({k: v for k, v in node.items() if k in
                ('BoneToModify', 'TranslationMode', 'RotationMode', 'ScaleMode',
                 'TranslationSpace', 'RotationSpace', 'ScaleSpace')})
    controls = {name: field_type(f) for name, f in declared.items()
                if name.startswith('CSS') and not name.startswith('CSSPerformance')}
    defaults = {name: props[name] for name in controls if name in props}
    return {'rigs': rigs, 'bone_transforms': transforms, 'controls': controls, 'control_defaults': defaults,
            'reset_inputs': [name for name in controls if 'ResetEpoch' in name]}


def compare_adapter(source, target):
    failures = []
    if source['controls'] != target['controls']:
        failures.append('CSS control layout differs')
    if source['control_defaults'] != target['control_defaults']:
        failures.append('CSS control defaults differ')
    def signatures(value):
        return sorted((json.dumps({'rig': r['rig'], 'pose_flags': r['pose_flags'],
                     'bindings': {k: {'source': binding_name(v['source']),
                                     'source_type': v['source_type'], 'target_type': v['target_type']}
                                  for k, v in r['bindings'].items()}}, sort_keys=True)
                       for r in value['rigs']))
    if signatures(source) != signatures(target):
        failures.append('Control Rig class, pose transfer or bound parameter layout differs')
    if sorted(map(lambda x: json.dumps(x, sort_keys=True), source['bone_transforms'])) != sorted(
            map(lambda x: json.dumps(x, sort_keys=True), target['bone_transforms'])):
        failures.append('Explicit bone-transform targets or spaces differ')
    return failures


def named_bone_overlaps(graphs, rigs):
    overlaps = []
    for path, g in graphs.items():
        owners = {}
        for r in g['rigs']:
            for bone in set(rigs[r['rig']]['defaults'].get('BoneNames', [])):
                owners.setdefault(bone, []).append(r['node'])
        overlaps.extend({'graph': path, 'bone': bone, 'nodes': nodes}
                        for bone, nodes in owners.items() if len(nodes) > 1)
    return overlaps


def rig_summary(data, path):
    p = cdo(data[path])
    retained = ('BoneNames', 'Parents', 'Children', 'StepSeconds', 'Frequency', 'DampingRatio',
                'Stiffness', 'Damping', 'Limits', 'MaxTravel', 'ContactBoneNames', 'ContactBonesA',
                'ContactBonesB', 'ContactRadii', 'ContactHairMargins', 'ContactIterations',
                'AngularLimitsEnabled', 'StrandLimits', 'PointMargin', 'ContactActive')
    return {'defaults': {k: p[k] for k in retained if k in p},
            'reset_fields': [k for k in fields(data[path]) if k in ('ResetEpoch', 'UpdateSerial', 'SeenEpoch')],
            'bone_writer_coverage': 'Named BoneNames array only; compiled literal targets and propagation require separate review'}


def mesh_summary(data, path):
    exports = data[path]
    meshes = [e for e in exports if e['Type'] == 'SkeletalMesh']
    if len(meshes) != 1:
        raise ValueError('Expected one skeletal mesh: ' + path)
    mesh, cloth = meshes[0], []
    for e in exports:
        if e['Type'] != 'ClothingAssetCommon':
            continue
        props = e['Properties']
        lods = []
        for lod in props.get('LodData', []):
            physical = lod['PhysicalMeshData']
            lods.append({'vertices': len(physical['Vertices']),
                         'indices': len(physical['Indices']),
                         'physical_fields': sorted(physical)})
        cloth.append({'name': e['Name'], 'physics_asset': package(props.get('PhysicsAsset')),
                      'used_bones': props.get('UsedBoneNames', []), 'lods': lods})
    props = mesh['Properties']
    return {'post_process': package(props.get('PostProcessAnimBlueprint')),
            'skeleton': package(props.get('Skeleton')),
            'physics_asset': package(props.get('PhysicsAsset')),
            'lods': [{'vertices': lod['NumVertices'], 'sections': len(lod['Sections']),
                      'cloth_sections': sum(s.get('CorrespondClothAssetIndex', -1) >= 0 for s in lod['Sections'])}
                     for lod in mesh['LODModels']], 'cloth': cloth}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--readback', type=Path, action='append', required=True)
    parser.add_argument('--outfits', type=Path, help='Migrated outfit manifests for the nine-variant solver inventory')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs, data = {}, {}
    for path in [args.catalog, *args.readback, *([args.outfits] if args.outfits else [])]:
        with path.open('rb') as stream:
            inputs[str(path.resolve())] = hashlib.file_digest(stream, 'sha256').hexdigest()
    for path in args.readback:
        for name, exports in json.loads(path.read_text()).items():
            if name in data and data[name] != exports:
                raise ValueError('Conflicting decoded package: ' + name)
            data[name] = exports
    library = json.loads(args.catalog.read_text())['catalog']['animation_library']
    graphs, results = {}, []
    for adapter in library['adapters']:
        source, target = map(package, (adapter['source_class'], adapter['target_class']))
        for path in (source, target):
            if path not in graphs:
                graphs[path] = graph(data, path)
        failures = compare_adapter(graphs[source], graphs[target])
        results.append({'outfit': adapter['source_outfit'], 'source': source,
                        'target': target, 'passed': not failures, 'failures': failures})
    rig_paths = sorted({r['rig'] for g in graphs.values() for r in g['rigs']})
    rigs = {path: rig_summary(data, path) for path in rig_paths}
    overlaps = named_bone_overlaps(graphs, rigs)
    report = {'passed': all(r['passed'] for r in results) and not overlaps, 'inputs': inputs,
              'adapters': results, 'graphs': graphs, 'rigs': rigs, 'named_bone_overlaps': overlaps,
              'limits': 'Cooked field/rig/pose-transfer comparison only. Does not prove graph execution order, event updates, cloth contact, reset behavior or visible motion.'}
    if args.outfits:
        faces = {package(a['source_mesh']): package(a['target_mesh'])
                 for face in library['faces'] for a in face['adapters']}
        report['variants'], report['meshes'] = [], {}
        for manifest in json.loads(args.outfits.read_text()).values():
            for outfit in manifest['catalog']['outfits']:
                for variant in outfit['variants']:
                    mesh = package(variant['mesh'])
                    facial_mesh = faces.get(mesh, mesh)
                    for path in (mesh, facial_mesh):
                        if path not in report['meshes']:
                            report['meshes'][path] = mesh_summary(data, path)
                    report['variants'].append({'outfit': outfit['id'], 'variant': variant['id'],
                                               'mesh': mesh, 'facial_mesh': facial_mesh})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': report['passed'], 'adapters': len(results), 'graphs': len(graphs),
                      'rigs': len(rigs), 'named_bone_overlaps': len(overlaps),
                      'failures': [r for r in results if not r['passed']]}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
