"""Check a private cloth-basis cook against its unmodified mesh and morph export."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path

import numpy as np


SHAPES = dict(Glute='PBMGlutesSize', Hip='PBMHipSize', Thigh='PBMThighsTone',
              Waist='PBMWaistWidth', Tone='FBMBodyTone')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readback', type=Path, required=True)
    parser.add_argument('--mesh-audit', type=Path, required=True)
    parser.add_argument('--morph-audit', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    inputs = {}

    def load(path):
        data = path.read_bytes()
        inputs[str(path)] = hashlib.sha256(data).hexdigest()
        return json.loads(data)

    cooked, audit = load(args.readback), load(args.mesh_audit)
    results = []
    names = {f'CSS_Cloth{shape}{axis}' for shape in SHAPES for axis in 'XYZ'}
    for variant in ('Eve', 'Genessa'):
        package = f'/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_{variant}W3_Face'
        original = copy.deepcopy(cooked[package])
        candidate = json.loads(json.dumps(cooked[package + '_BasisStudy2']).replace('_BasisStudy2', ''))
        base, = [e for e in original if e['Type'] == 'SkeletalMesh']
        mesh, = [e for e in candidate if e['Type'] == 'SkeletalMesh']
        source = load(args.morph_audit / f'SK_{variant}W3_Face.json')
        skirt, = [g for g in source['groups'] if g['slot'] == 'UG_Skirt']
        source_lod, = audit[package]['mesh']['render_lods']
        offset, starts = 0, []
        for section in source_lod['sections']:
            if section['cloth_asset_index'] == skirt['cloth_asset'] and section['vertices'] == len(skirt['vertices']):
                starts.append(offset)
            offset += section['vertices']
        start, = starts
        lod, = mesh['LODModels']
        digest, = mesh['OfflineMeshDigest']['lods']
        assert digest['vertices'] == offset == source_lod['vertices']
        assert len(mesh['OfflineMeshDigest']['morphs']) == 137
        buffers = lod['VertexAttributeBuffers']
        assert set(buffers) == set(digest['attribute_buffers']) == names
        for name, buffer in buffers.items():
            assert buffer['ComponentCount'] == 1 and buffer['ComponentStride'] == 4
            assert buffer['PixelFormat'] == 'PF_R32_FLOAT' and buffer['ValueData']['Stride'] == 4
            data = base64.b64decode(buffer['ValueData']['Data'], validate=True)
            assert len(data) == offset * 4
            assert hashlib.sha256(data).hexdigest() == digest['attribute_buffers'][name]['sha256']
        half_edge = digest['half_edge']
        assert half_edge['vertices'] == offset and half_edge['edges'] == digest['indices']
        attributes, = mesh['Properties']['LODInfo']
        metadata = attributes['VertexAttributes']
        assert len(metadata) == 15 and {a['Name'] for a in metadata} == names
        assert all(a['EnabledForRender']['Value'] and a['DataType'] == 'ESkeletalMeshVertexAttributeDataType::Float'
                   for a in metadata)
        threshold = attributes['BuildSettings']['MorphThresholdPosition']
        residuals = {}
        for short, morph_name in SHAPES.items():
            values = np.stack([np.frombuffer(base64.b64decode(buffers[f'CSS_Cloth{short}{axis}']['ValueData']['Data']),
                                           dtype='<f4') for axis in 'XYZ'], axis=1)
            assert np.isfinite(values).all()
            actual = values[start:start + len(skirt['vertices'])]
            expected = np.zeros(actual.shape)
            rows = np.asarray(next(m for m in skirt['morphs'] if m['name'] == morph_name)['deltas'])
            indices = rows[:, 0].astype(int)
            expected[indices] = rows[:, 1:4]
            assert np.allclose(actual[indices], expected[indices], rtol=0, atol=1e-6), morph_name
            residual = float(np.linalg.norm(actual - expected, axis=1).max())
            # The original render morph omits deltas smaller than this import threshold.
            assert residual <= threshold + 1e-6, (morph_name, residual, threshold)
            residuals[morph_name] = residual

        # Remove only intentional additions and the relocated empty Nanite bulk offset.
        assert base['LODModels'][0]['VertexAttributeBuffers'] == {}
        assert base['Properties']['LODInfo'][0]['VertexAttributes'] == []
        assert 'TargetMeshDeformers' not in base['Properties']
        collection = mesh['Properties'].pop('TargetMeshDeformers')
        assert collection == {
            'ObjectName': "MeshDeformerCollection'MDC_ClothMorphStudy4'",
            'ObjectPath': '/Game/CSS/eins0fx/Shared/Deformers/MDC_ClothMorphStudy4.0'}
        lod['VertexAttributeBuffers'] = {}
        attributes['VertexAttributes'] = []
        for key in ('attributes_sha256', 'attribute_buffers', 'half_edge'):
            digest[key] = copy.deepcopy(base['OfflineMeshDigest']['lods'][0][key])
        for obj in (base, mesh):
            bulk = obj['NaniteResources']['StreamablePages']
            assert bulk['ElementCount'] == 0 and bulk['SizeOnDisk'] == 0
            bulk['OffsetInFile'] = 0
        assert original == candidate, f'Unexpected cooked export change: {variant}'
        results.append(dict(variant=variant, exports=len(original), morphs=137, attributes=15,
                            vertices=offset, half_edge=half_edge, residual_max_cm=residuals))
    report = dict(passed=True, variants=results, inputs=inputs,
                  scope='Cooked mesh preservation, attribute values and half-edge counts. '
                        'No shader execution, gameplay, collision or performance acceptance.')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report['variants'], indent=2))


if __name__ == '__main__':
    main()
