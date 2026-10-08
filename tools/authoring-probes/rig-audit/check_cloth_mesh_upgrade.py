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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    before = json.loads(args.baseline.read_text())
    after = json.loads(args.candidate.read_text())
    assert before.keys() == after.keys() and len(before) == 4
    expected = {f'CSS_Cloth{s}{axis}' for s in SHAPES for axis in 'XYZ'}
    records, errors = [], {}
    for package in before:
        original, candidate = copy.deepcopy(before[package]), copy.deepcopy(after[package])
        old, = [r for r in original if r['Type'] == 'SkeletalMesh']
        new, = [r for r in candidate if r['Type'] == 'SkeletalMesh']
        old_lod, = old['LODModels']
        lod, = new['LODModels']
        old_digest, = old['OfflineMeshDigest']['lods']
        digest, = new['OfflineMeshDigest']['lods']
        buffers = lod['VertexAttributeBuffers']
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
            assert field not in old_props
            reference = props.pop(field)
            assert reference['ObjectName'] == f"{cls}'{name}'"
            assert reference['ObjectPath'].rsplit('.', 1)[0] == '/Game/CSS/eins0fx/Shared/Deformers/' + name
        info, = props['LODInfo']
        assert not old_props['LODInfo'][0]['VertexAttributes']
        attributes = info['VertexAttributes']
        assert len(attributes) == 15 and {v['Name'] for v in attributes} == expected
        assert all(v['EnabledForRender']['Value'] and
                   v['DataType'] == 'ESkeletalMeshVertexAttributeDataType::Float' for v in attributes)
        info['VertexAttributes'] = []
        lod['VertexAttributeBuffers'] = {}
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
