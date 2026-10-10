"""Check cooked Peony cloth offsets and Windows skin/cloth material shaders."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import shutil

import numpy as np


CSS = Path(__file__).resolve().parents[2]
EVE = CSS.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
SHAPES = dict(Glute='PBMGlutesSize', Hip='PBMHipSize', Thigh='PBMThighsTone',
              Waist='PBMWaistWidth', Tone='FBMBodyTone')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check_mesh(mesh, structure, groups):
    lod, = mesh['LODModels']
    digest, = mesh['OfflineMeshDigest']['lods']
    source_lod, = structure['render_lods']
    count = digest['vertices']
    if count != source_lod['vertices']:
        raise ValueError('Cook changed vertex count')
    cloth = np.zeros(count, dtype=bool)
    expected = {name: np.zeros((count, 3)) for name in SHAPES.values()}
    offset = 0
    found = set()
    if len(lod['Sections']) != len(source_lod['sections']):
        raise ValueError('Cook changed section count')
    for source, section in zip(source_lod['sections'], lod['Sections'], strict=True):
        end = offset + int(source['vertices'])
        if section['BaseVertexIndex'] != offset or section['NumVertices'] != end - offset:
            raise ValueError('Cook changed section vertex ordering')
        if source['cloth_asset_index'] >= 0:
            group, = [g for g in groups if g['cloth_asset'] == source['cloth_asset_index']]
            if group['slot'] in found or len(group['vertices']) != end - offset:
                raise ValueError('Ambiguous cloth section mapping')
            found.add(group['slot'])
            cloth[offset:end] = True
            for shape in group['morphs']:
                if shape['name'] not in expected:
                    continue
                for delta in shape['deltas']:
                    index = int(delta[0])
                    if not 0 <= index < end - offset:
                        raise ValueError('Morph index outside section')
                    expected[shape['name']][offset + index] = delta[1:4]
        offset = end
    if offset != count or len(found) != 2 or not cloth.any() or cloth.all():
        raise ValueError('Incomplete cloth mapping')
    names = {f'CSS_Cloth{short}{axis}' for short in SHAPES for axis in 'XYZ'}
    buffers = lod['VertexAttributeBuffers']
    if set(buffers) != names or set(digest['attribute_buffers']) != names:
        raise ValueError('Missing or additional cloth buffers')
    values = {}
    for name, buffer in buffers.items():
        if (buffer['ComponentCount'], buffer['ComponentStride'], buffer['PixelFormat'],
            buffer['ValueData']['Stride']) != (1, 4, 'PF_R32_FLOAT', 4):
            raise ValueError('Wrong buffer format')
        data = base64.b64decode(buffer['ValueData']['Data'], validate=True)
        if len(data) != count * 4 or hashlib.sha256(data).hexdigest() != digest['attribute_buffers'][name]['sha256']:
            raise ValueError('Buffer length or digest mismatch')
        array = np.frombuffer(data, '<f4')
        if not np.isfinite(array).all() or np.any(array[~cloth] != 0):
            raise ValueError('Nonfinite data or correction outside cloth')
        values[name] = array
    residuals = {}
    for short, name in SHAPES.items():
        actual = np.stack([values[f'CSS_Cloth{short}{axis}'] for axis in 'XYZ'], axis=1)
        residual = float(np.linalg.norm(actual - expected[name], axis=1).max())
        nonzero = np.any(expected[name] != 0, axis=1)
        if not np.allclose(actual[nonzero], expected[name][nonzero], rtol=0, atol=1e-6) or residual > .015001:
            raise ValueError('Cooked cloth offsets differ from native morphs: ' + name)
        residuals[name] = residual
    half = digest['half_edge']
    if half['vertices'] != count or half['edges'] != digest['indices']:
        raise ValueError('Incomplete half-edge buffers')
    for key, name in (('DefaultMeshDeformer', 'DG_ClothMorphBody'), ('TargetMeshDeformers', 'MDC_ClothMorphBody')):
        if mesh['Properties'][key]['ObjectPath'].rsplit('.', 1)[0] != '/Game/CSS/eins0fx/Shared/Deformers/' + name:
            raise ValueError('Cloth graph binding changed')
    return dict(vertices=count, buffers=len(buffers), cloth_vertices=int(cloth.sum()), residuals_cm=residuals)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', choices=('peony-windows3', 'peony-windows4'), required=True)
    args = parser.parse_args()
    stage = EVE / 'work' / args.stage / 'readback1'
    output = stage / 'validation.json'
    if output.exists():
        raise FileExistsError(output)
    inputs = {}

    def load(path):
        inputs[str(path)] = sha(path)
        return json.loads(path.read_text())

    receipt = load(stage / 'receipt.json')
    if not receipt['passed']:
        raise ValueError('Package verification failed')
    for name, expected in receipt['files'].items():
        if sha(Path(name)) != expected:
            raise ValueError('Package evidence changed: ' + name)
    decoded = load(stage / 'decoded.json')
    meshes = load(stage / 'meshes.json')
    records, digests = [], []
    negative_checks = 0
    for name, package, morph_count in (
        ('SK_Peony', '/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony', 22),
        ('SK_Peony_Face', '/Game/CSS/eins0fx/EveSB/Faces/Peony/SK_Facial1', 137),
    ):
        mesh, = [e for e in meshes[package] if e['Type'] == 'SkeletalMesh']
        native = EVE / 'work/peony-clothgpu1'
        structure = load(native / (name + '-structure.json'))
        groups = load(native / (name + '-cloth-morphs.json'))['groups']
        row = check_mesh(mesh, structure, groups)
        digest = mesh['OfflineMeshDigest']
        if len(digest['morphs']) != morph_count:
            raise ValueError('Morph count changed')
        row.update(package=package, morphs=morph_count)
        records.append(row)
        digests.append(digest)
        broken = copy.deepcopy(mesh)
        broken['LODModels'][0]['VertexAttributeBuffers'].pop('CSS_ClothGluteX')
        try:
            check_mesh(broken, structure, groups)
        except ValueError as error:
            if str(error) != 'Missing or additional cloth buffers':
                raise
            negative_checks += 1
        else:
            raise ValueError('Missing-buffer negative control passed')
    if digests[0]['lods'] != digests[1]['lods'] or digests[0]['reference_skeleton_sha256'] != digests[1]['reference_skeleton_sha256']:
        raise ValueError('Facial mesh changes base render data or skeleton')
    if any(digests[1]['morphs'].get(k) != v for k, v in digests[0]['morphs'].items()):
        raise ValueError('Facial mesh changes body morph deltas')
    materials = []
    for prefix in ('/Game/CSS/eins0fx/EveSB/Peony/Materials/', '/Game/CSS/eins0fx/Shared/Astral/Materials/'):
        names = ['M_Peony' + name for name in ('Fabric', 'Hardware', 'Panels', 'Stockings')]
        if '/Shared/' in prefix:
            names += ['MI_' + name + '_SingleSided' for name in names]
        for name in names:
            material, = decoded[prefix + name]
            platforms = {}
            for resource in material['LoadedMaterialResources']:
                shader = resource['LoadedShaderMap']
                platform = shader['ShaderPlatform']
                hashes = shader.get('Code', {}).get('ShaderHashes')
                if platform in platforms or not hashes:
                    raise ValueError('Missing or duplicate Windows shader map')
                factories = {r['VertexFactoryTypeName']: r for r in shader['Content']['OrderedMeshShaderMaps']}
                for factory in ('TGPUSkinVertexFactoryDefault', 'TGPUSkinAPEXClothVertexFactoryDefault'):
                    if 'TBasePassPSFNoLightMapPolicy' not in factories[factory]['ShaderTypes']:
                        raise ValueError('Missing skin/cloth base-pass shader')
                platforms[platform] = len(hashes)
            if set(platforms) != {'SP_PCD3D_SM5', 'SP_PCD3D_SM6'}:
                raise ValueError('Missing Windows shader target')
            materials.append(dict(package=prefix + name, shaders=platforms))
    recipe = stage / Path(__file__).name
    if recipe.exists():
        raise FileExistsError(recipe)
    shutil.copy2(__file__, recipe)
    inputs[str(recipe)] = sha(recipe)
    output.write_text(json.dumps(dict(passed=True, meshes=records, materials=materials,
        negative_checks=negative_checks, facial_base_render_equal=True, body_morph_deltas_equal=True,
        inputs=inputs, scope='Cooked cloth-offset values, normal/facial base equality, body morph equality, '
            'Windows SM5/SM6 skin/cloth shaders. Not final fit, runtime adapters, gameplay or performance acceptance.'), indent=2) + '\n')
    print(json.dumps(dict(passed=True, meshes=len(records), shader_materials=len(materials),
        negative_checks=negative_checks, max_residual_cm=max(v for r in records for v in r['residuals_cm'].values()))))


if __name__ == '__main__':
    main()
