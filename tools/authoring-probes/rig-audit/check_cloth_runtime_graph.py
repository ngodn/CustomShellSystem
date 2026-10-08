"""Verify the private runtime graph's cooked bindings against the rendered study."""
import argparse
import hashlib
import json
from pathlib import Path

from check_cooked_cloth_basis import SHAPES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'cook-log', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    paths = (args.baseline, args.candidate, args.cook_log)
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    prefix = '/Game/CSS/eins0fx/Shared/Deformers/'
    before = json.loads(args.baseline.read_text())[prefix + 'DG_ClothMorphStudy4']
    after = json.loads(args.candidate.read_text())[prefix + 'DG_ClothMorphStudy5']
    rename = {'DG_ClothMorphStudy4': 'DG_ClothMorphStudy5'}
    rename.update({f'CSS_Cloth{s}Weight': f'CSS_Morph_{m}' for s, m in SHAPES.items()})
    expected = set(rename.values()) - {'DG_ClothMorphStudy5'}
    variables = [r['Properties'] for r in after if r['Type'] == 'OptimusVariableDescription']
    assert len(variables) == 5 and {v['VariableName'] for v in variables} == expected
    assert all(v['DataType']['TypeName'] == 'DoubleProperty' for v in variables)
    graph, = [r['Properties'] for r in after if r['Type'] == 'OptimusDeformer']
    assert graph['Status'] == 'EOptimusDeformerStatus::Compiled'
    values = graph['ValueMap']
    assert len(values) == 5 and {v['Key']['Name'] for v in values} == expected
    for v in values:
        assert v['Key']['Type'] == 'EOptimusValueType::Variable'
        assert v['Value']['DataType']['TypeName'] == 'DoubleProperty'
        assert v['Value']['ValueUsage'] == 'EOptimusValueUsage::GPU'
        assert v['Value']['ShaderValue'] == {'ShaderValue': [0, 0, 0, 0], 'ArrayList': []}
    for kind, count in (('OptimusKernelSource', 2), ('OptimusGraphDataInterface', 1)):
        a = [r['Properties'] for r in before if r['Type'] == kind]
        b = [r['Properties'] for r in after if r['Type'] == kind]
        assert len(a) == len(b) == count
        text = json.dumps(a)
        for old, new in rename.items():
            text = text.replace(old, new)
        assert sorted(json.dumps(v, sort_keys=True) for v in json.loads(text)) == sorted(
            json.dumps(v, sort_keys=True) for v in b), kind
    names = [r['Properties']['AttributeName'] for r in after
             if r['Type'] == 'OptimusSkinnedMeshVertexAttributeDataInterface']
    assert len(names) == 30
    assert set(names) == {f'CSS_Cloth{s}{a}' for s in SHAPES for a in 'XYZ'}
    log = args.cook_log.read_text()
    for kernel in ('LinearBlendSkin_Morph_Cloth_PositionOnly_OptimusNode_ComputeKernel_1',
                   'ComputeNormalsTangents_OptimusNode_CustomComputeKernel_0'):
        for platform in ('PCD3D_SM5', 'PCD3D_SM6'):
            assert f'DG_ClothMorphStudy5_{kernel}: {platform} shader compilation success!' in log
    report = dict(passed=True, variables=sorted(expected), zero_defaults=True,
                  equivalent_kernel_sources=True, vertex_attributes=15, inputs=hashes,
                  scope='Cooked graph bindings and compilation. Runtime GPU execution remains pending.')
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
