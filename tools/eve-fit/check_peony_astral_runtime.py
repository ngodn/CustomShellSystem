"""Check Peony's cooked uniform bindings and runtime material-selection policy.

This reads cooked data and runs the portable selector. It does not prove
in-game material copying, rendering, cloth motion or summon performance.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import resource
import subprocess


CSS = Path(__file__).resolve().parents[2]
SOURCE = '/Game/CSS/eins0fx/EveSB/Peony/Materials/'
SHARED = '/Game/CSS/eins0fx/Shared/Astral/Materials/'
# The ghost parents are unlit. Their normal inputs remain authored but do not
# contribute to the compiled emissive/opacity output.
UNLIT_UNUSED = {
    'M_PeonyFabric': {'NormalTexture'},
    'M_PeonyHardware': {'NormalTexture'},
    'M_PeonyPanels': {'ArmNormalTexture', 'Lower_1NormalTexture', 'Lower_2NormalTexture'},
    'M_PeonyStockings': set(),
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check_uniforms(asset, required):
    checked = set()
    for resource_data in asset['LoadedMaterialResources']:
        shader = resource_data['LoadedShaderMap']
        feature = shader['ShaderMapId']['FeatureLevel']
        uniforms = shader['Content']['MaterialCompilationOutput']['UniformExpressionSet']
        names = {row['ParameterInfo']['Name']
                 for group in uniforms['UniformTextureParameters'] for row in group
                 if row['ParameterInfo']['Association'] == 'GlobalParameter'
                 and row['ParameterInfo']['Index'] == -1}
        if not set(required) <= names:
            raise ValueError(f"{asset['Package']} {feature}: missing compiled source textures")
        checked.add(feature)
    if checked != {'SM5', 'SM6'}:
        raise ValueError(f"{asset['Package']}: missing Windows feature levels")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readback', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--test', type=Path, default=CSS / 'build/host/css_astral_adapter_tests')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resolver_path = CSS / 'tools/diagnostics/genessa-doubles/resolve-material-state.py'
    spec = importlib.util.spec_from_file_location('material_state', resolver_path)
    resolver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(resolver)
    exports = {e['Package']: e for rows in json.loads(args.readback.read_text()).values()
               for e in rows if e['Type'] in ('Material', 'MaterialInstanceConstant')}
    assets = resolver.load_assets([args.readback])
    materials = {p: resolver.resolve(assets, p) for p in assets if p.startswith(SOURCE)}
    if len(materials) != 34:
        raise ValueError('Expected four Peony roots and thirty palette instances')
    entries = [e for e in json.loads(args.manifest.read_text())['materials']
               if e.get('source_parent', '').startswith(SOURCE)]
    if len(entries) != 4:
        raise ValueError('Expected four Peony adapter definitions')
    for entry in entries:
        required = entry.get('required_source_textures', [])
        source = exports[entry['source_parent']]
        check_uniforms(source, required)
        companion = exports[entry['material'].split('.')[0]]
        if not companion['Package'].startswith(SHARED):
            raise ValueError('Peony companion is outside the shared author namespace')
        if companion['Properties'].get('ShadingModel') != 'MSM_Unlit':
            raise ValueError('Ghost normal-input exclusion requires an unlit material')
        unused = UNLIT_UNUSED[source['Name']]
        if not unused <= set(required):
            raise ValueError('Source normal texture names changed')
        check_uniforms(companion, [name for name in required if name not in unused] + ['CSS_AstralNoise'])
        if required:
            broken = copy.deepcopy(source)
            broken['LoadedMaterialResources'][0]['LoadedShaderMap']['Content'][
                'MaterialCompilationOutput']['UniformExpressionSet']['UniformTextureParameters'] = []
            try:
                check_uniforms(broken, required)
            except ValueError:
                pass
            else:
                raise AssertionError('Missing compiled texture negative control passed')

    def run(name, document, expected):
        path = args.output / (name + '.json')
        path.write_text(json.dumps(document, indent=2) + '\n')
        command = [str(args.test.resolve()), str(path.resolve())]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (args.output / (name + '.log')).write_text(result.stdout)
        if (result.returncode == 0) != expected:
            raise AssertionError(f'{name}: unexpected selector result: {result.stdout}')
        return dict(command=command, returncode=result.returncode)

    tests = {'policy': run('policy', {'materials': materials}, True)}
    first = SOURCE + 'Palettes/MI_peony_monochrome_Upper'
    for name in ('missing', 'placeholder'):
        broken = copy.deepcopy(materials[first])
        if name == 'missing':
            broken['parameters']['texture'] = []
        else:
            for row in broken['parameters']['texture']:
                row['value'] = {'AssetPathName': SHARED + 'T_NeutralColor.T_NeutralColor'}
        tests[name] = run(name, {'materials': {first: broken}}, False)
        expected_message = ('Required source texture is absent' if name == 'missing'
                            else 'Required source texture is missing or uses a companion placeholder')
        if expected_message not in (args.output / (name + '.log')).read_text():
            raise AssertionError('Negative control failed for an unrelated reason')
    inputs = [args.readback, args.manifest, args.test, resolver_path, Path(__file__),
              CSS / 'native/src/astral_material_catalog.inl']
    report = dict(materials=len(materials), parents=len(entries), feature_levels=['SM5', 'SM6'],
                  tests=tests, sha256={str(p.resolve()): digest(p) for p in inputs},
                  limitations='No live copy, rendering, cloth-motion or frame-time verification')
    (args.output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Peony: 34 source materials accepted, four source/ghost uniform pairs checked, negative controls passed')


if __name__ == '__main__':
    main()
