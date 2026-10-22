"""Compare the four cooked Genessa cloth meshes with the installed namespace baseline."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path

import numpy as np

from check_cooked_cloth_basis import SHAPES


def differences(a, b, path=''):
    if type(a) is not type(b):
        return [path + ': type changed']
    if isinstance(a, dict):
        result = [path + '/' + k + ': added or removed' for k in a.keys() ^ b.keys()]
        for k in a.keys() & b.keys():
            result.extend(differences(a[k], b[k], path + '/' + k))
        return result
    if isinstance(a, list):
        if len(a) != len(b):
            return [path + ': length changed']
        return [d for i, (x, y) in enumerate(zip(a, b)) for d in differences(x, y, path + '/' + str(i))]
    return [] if a == b else [path + ': value changed']


def normalize_reader_names(exports):
    # Tagged properties retain editor FName spelling; mapped properties use the
    # game's spelling and qualify these enum members. Keep their values intact.
    enum_types = dict(TerminationCriterion='SkeletalMeshTerminationCriterion',
                      ReductionMethod='SkeletalMeshOptimizationType')
    enum_types.update(dict.fromkeys(('SilhouetteImportance', 'TextureImportance',
                                    'ShadingImportance', 'SkinningImportance'),
                                   'SkeletalMeshOptimizationImportance'))
    for export in exports:
        props = export.get('Properties', {})
        if export['Type'] == 'ClothingAssetCommon' and 'LODData' in props:
            assert 'LodData' not in props
            props['LodData'] = props.pop('LODData')
        if export['Type'] == 'SkeletalMesh':
            for lod in props['LODInfo']:
                reduction = lod['ReductionSettings']
                for field, enum in enum_types.items():
                    reduction[field] = reduction[field].removeprefix(enum + '::')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expanded', action='store_true', help='Upgrade the existing five-channel cloth basis to ten channels.')
    args = parser.parse_args()
    before = json.loads(args.baseline.read_text())
    after = json.loads(args.candidate.read_text())
    assert before.keys() == after.keys() and len(before) == 4
    expected = {f'CSS_Cloth{s}{axis}' for s in SHAPES for axis in 'XYZ'}
    original_names = set(expected)
    if args.expanded:
        expected |= {f'CSS_Cloth{s}{axis}' for s in ('GluteWidth', 'GluteLift', 'HipBone', 'ThighSize', 'CalfSize')
                     for axis in 'XYZ'}
    records, errors = [], {}
    for package in before:
        original, candidate = copy.deepcopy(before[package]), copy.deepcopy(after[package])
        if args.expanded:
            normalize_reader_names(original)
            normalize_reader_names(candidate)
        old, = [r for r in original if r['Type'] == 'SkeletalMesh']
        new, = [r for r in candidate if r['Type'] == 'SkeletalMesh']
        old_lod, = old['LODModels']
        lod, = new['LODModels']
        old_digest, = old['OfflineMeshDigest']['lods']
        digest, = new['OfflineMeshDigest']['lods']
        buffers = lod['VertexAttributeBuffers']
        if args.expanded:
            assert set(old_lod['VertexAttributeBuffers']) == original_names
            assert {k: buffers[k] for k in original_names} == old_lod['VertexAttributeBuffers']
            assert digest['half_edge'] == old_digest['half_edge']
        else:
            assert not old_lod['VertexAttributeBuffers']
        assert set(buffers) == set(digest['attribute_buffers']) == expected
        values = {}
        for name, buffer in buffers.items():
            assert buffer['ComponentCount'] == 1 and buffer['ComponentStride'] == 4
            assert buffer['PixelFormat'] == 'PF_R32_FLOAT' and buffer['ValueData']['Stride'] == 4
            data = base64.b64decode(buffer['ValueData']['Data'], validate=True)
            assert len(data) == digest['vertices'] * 4
            assert hashlib.sha256(data).hexdigest() == digest['attribute_buffers'][name]['sha256']
            array = np.frombuffer(data, dtype='<f4')
            assert np.isfinite(array).all()
            values[name] = dict(minimum=float(array.min()), maximum=float(array.max()))
        assert digest['half_edge']['vertices'] == digest['vertices']
        assert digest['half_edge']['edges'] == digest['indices']
        props, old_props = new['Properties'], old['Properties']
        for field, cls, name in [('DefaultMeshDeformer', 'OptimusDeformer', 'DG_ClothMorphBody'),
                                 ('TargetMeshDeformers', 'MeshDeformerCollection', 'MDC_ClothMorphBody')]:
            if args.expanded:
                reference = props[field]
                assert old_props[field]['ObjectName'] == f"{cls}'{name}'"
                assert old_props[field]['ObjectPath'].rsplit('.', 1)[0] == '/Game/CSS/eins0fx/Shared/Deformers/' + name
                props[field] = copy.deepcopy(old_props[field])
                name += 'V2'
            else:
                assert field not in old_props
                reference = props.pop(field)
            assert reference['ObjectName'] == f"{cls}'{name}'"
            assert reference['ObjectPath'].rsplit('.', 1)[0] == '/Game/CSS/eins0fx/Shared/Deformers/' + name
        info, = props['LODInfo']
        if args.expanded:
            assert {v['Name'] for v in old_props['LODInfo'][0]['VertexAttributes']} == original_names
        else:
            assert not old_props['LODInfo'][0]['VertexAttributes']
        attributes = info['VertexAttributes']
        assert len(attributes) == len(expected) and {v['Name'] for v in attributes} == expected
        assert all(v['EnabledForRender']['Value'] and
                   v['DataType'] == 'ESkeletalMeshVertexAttributeDataType::Float' for v in attributes)
        info['VertexAttributes'] = copy.deepcopy(old_props['LODInfo'][0]['VertexAttributes'])
        lod['VertexAttributeBuffers'] = copy.deepcopy(old_lod['VertexAttributeBuffers'])
        for key in ('attributes_sha256', 'attribute_buffers', 'half_edge'):
            digest[key] = copy.deepcopy(old_digest[key])
        for obj in (old, new):
            pages = obj['NaniteResources']['StreamablePages']
            assert pages['ElementCount'] == pages['SizeOnDisk'] == 0
            pages['OffsetInFile'] = 0
        changed = differences(original, candidate)
        if changed:
            errors[package] = changed
        records.append(dict(package=package, exports=len(original), vertices=digest['vertices'],
                            morphs=len(new['OfflineMeshDigest']['morphs']), attributes=values))
    report = dict(passed=not errors, meshes=records, differences=errors,
                  inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.baseline, args.candidate)},
                  scope='Cooked baseline preservation and finite typed correction buffers; no live rendering acceptance.')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    assert not errors, json.dumps(errors, indent=2)
    print('Four cooked meshes preserve the baseline outside the authored deformer additions')


if __name__ == '__main__':
    main()
