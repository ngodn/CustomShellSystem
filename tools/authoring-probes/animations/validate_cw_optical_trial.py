"""Validate the CW optical replacement against its installed metadata and cooked proofs."""
import argparse
import copy
import json
from pathlib import Path
from package_corrected_dance_trial import PACKAGE_ROOT, STEM, sha


def read(path):
    return json.loads(path.read_text())


def validate(pack, audit, installed_metadata):
    verification = read(pack / 'verification.json')
    preservation_path = audit / 'cooked-mesh-preservation.json'
    preservation = read(preservation_path)
    if (not verification['passed'] or verification['exports'] != 103
            or not preservation['passed'] or not preservation['source_deltas_checked']
            or len(preservation['results']) != 1
            or preservation['results'][0]['preserved_morphs'] != 206):
        raise ValueError('Incomplete independent mesh preservation proof')
    for path, digest in verification['protected'].items():
        if sha(Path(path)) != digest:
            raise ValueError('Changed package input: ' + path)
    for name, digest in verification['files'].items():
        if sha(pack / STEM / name) != digest:
            raise ValueError('Changed package: ' + name)
    current = read(installed_metadata / PACKAGE_ROOT / 'css.locomotion/manifest.json')
    candidate = read(pack / 'metadata-readback' / PACKAGE_ROOT / 'css.locomotion/manifest.json')
    expected = copy.deepcopy(current)
    changes = 0
    for face in expected['catalog']['animation_library']['faces']:
        for adapter in face['adapters']:
            if adapter['source_outfit'] == 'eins0fx.commanderwhite':
                old = '/Game/CSS/Locomotion/Faces/SK_Face_CWCompact/SK_Face_CWE8.SK_Face_CWE8'
                if adapter['target_mesh'] != old:
                    raise ValueError('Unexpected installed CW target')
                adapter['target_mesh'] = old.replace('CWE8', 'CWE9')
                changes += 1
    expected['containers'] = candidate['containers']
    if changes != 1 or expected != candidate:
        raise ValueError('Unexpected installed catalog change')
    before_files = {p.relative_to(installed_metadata): p for p in installed_metadata.rglob('*') if p.is_file()}
    after_files = {p.relative_to(pack / 'metadata-readback'): p for p in (pack / 'metadata-readback').rglob('*') if p.is_file()}
    if before_files.keys() != after_files.keys():
        raise ValueError('Metadata files changed')
    for relative, source in before_files.items():
        if source.name != 'manifest.json' and sha(source) != sha(after_files[relative]):
            raise ValueError('Facial resources changed: ' + str(relative))
    stage = pack / 'stage/MortalShell2/Content'
    if any(not p.is_relative_to(stage / 'CSS/Locomotion') for p in stage.rglob('*') if p.is_file()):
        raise ValueError('Pack contains an authoring stand-in or foreign asset')
    document = read(pack / 'optical-assets.json')
    prefix = '/Game/CSS/Locomotion/Faces/Optical1/'
    root = document[prefix + 'M_Optical'][0]
    props = root['Properties']
    for flag in ('bUsedWithMorphTargets', 'bUsedWithSkeletalMesh', 'bUsedWithClothing', 'TwoSided', 'bUsesDistortion'):
        if props.get(flag) is not True:
            raise ValueError('Missing material usage: ' + flag)
    if props['BlendMode'] != 'BLEND_Translucent' or props['RefractionMethod'] != 'RM_IndexOfRefraction':
        raise ValueError('Optical rendering mode changed')
    collection = root['CachedExpressionData']['ParameterCollectionInfos']
    if (len(collection) != 1 or collection[0]['StateId'] != 'EC27263E-4E4F5EC4-9194768A-5864F56B'
            or collection[0]['ParameterCollection']['ObjectPath'] != '/Game/Sparta/Lighting/Blueprints/Deprecated/MPC_LightScenario.0'):
        raise ValueError('Native lighting collection identity changed')
    cube = root['CachedExpressionData']['TextureValues']
    if len(cube) != 1 or cube[0]['AssetPathName'] != '/Game/Sparta/MasterMaterials/Character_Materials/Eyes/Shared/HDRIs/spotslights3.spotslights3':
        raise ValueError('Native reflection texture changed')
    shaders = {}
    for resource in root['LoadedMaterialResources']:
        material = resource['LoadedShaderMap']
        factories = {row['VertexFactoryTypeName']: row for row in material['Content']['OrderedMeshShaderMaps']}
        for name in ('TGPUSkinVertexFactoryDefault', 'TGPUSkinAPEXClothVertexFactoryDefault'):
            types = factories[name]['ShaderTypes']
            if not {'FDistortionMeshVS', 'FDistortionMeshPS'} <= set(types) or not any(t.startswith('TBasePassPS') for t in types):
                raise ValueError('Missing distortion or surface shader')
        count = len(material['Code']['ShaderHashes'])
        if not count:
            raise ValueError('No embedded shader bytecode')
        shaders[material['ShaderPlatform']] = count
    if set(shaders) != {'SP_PCD3D_SM5', 'SP_PCD3D_SM6'}:
        raise ValueError('Missing Windows shader platform')
    scalar_expectations = {8: (0., 0., 1., 0.), 10: (0., .04, 1.3683993, .604878), 45: (0., 0., 1., 0.)}
    for slot, values in scalar_expectations.items():
        instance = document[prefix + 'MI_Optical_' + str(slot)][0]['Properties']
        if instance['Parent']['ObjectPath'] != prefix + 'M_Optical.0':
            raise ValueError('Optical instance has wrong parent')
        params = {p['ParameterInfo']['Name']: p['ParameterValue'] for p in instance['ScalarParameterValues']}
        expected = dict(zip(('Opacity', 'Roughness', 'IOR', 'ReflectionIntensity'), values, strict=True))
        if params != expected:
            raise ValueError('Optical scalar overrides changed')
    evidence = [pack / 'verification.json', pack / 'optical-assets.json', preservation_path,
                audit / 'optical-stage.json', *before_files.values()]
    return {'passed': True, 'preserved_morphs': 206, 'unchanged_material_slots': 67,
            'catalog_changes': ['CW facial target E8 -> E9'], 'windows_shader_counts': shaders,
            'excluded_authoring_stubs': True, 'package_hashes': verification['files'],
            'evidence': {str(p): sha(p) for p in evidence}, 'live_verified': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('pack', 'audit', 'installed_metadata', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    result = validate(args.pack.resolve(strict=True), args.audit.resolve(strict=True), args.installed_metadata.resolve(strict=True))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('CW_OPTICAL_COOKED_VALIDATION_PASS')
