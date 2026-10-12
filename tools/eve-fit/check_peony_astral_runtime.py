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


def check_uniforms(asset, required, numeric=()):
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
        numeric_names = {row['ParameterInfo']['Name'] for row in uniforms['UniformNumericParameters']
                         if row['ParameterInfo']['Association'] == 'GlobalParameter'
                         and row['ParameterInfo']['Index'] == -1}
        if not set(numeric) <= numeric_names:
            raise ValueError(f"{asset['Package']} {feature}: missing compiled controls {set(numeric) - numeric_names}")
        checked.add(feature)
    if checked != {'SM5', 'SM6'}:
        raise ValueError(f"{asset['Package']}: missing Windows feature levels")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--readback', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--test', type=Path, default=CSS / 'build/host/css_astral_adapter_tests')
    parser.add_argument('--v2', action='store_true')
    parser.add_argument('--companions-readback', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resolver_path = CSS / 'tools/diagnostics/genessa-doubles/resolve-material-state.py'
    spec = importlib.util.spec_from_file_location('material_state', resolver_path)
    resolver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(resolver)
    readbacks = [args.readback] + ([args.companions_readback] if args.companions_readback else [])
    prefix = '/Game/CSS/eins0fx/EveSB/PeonyV2/' if args.v2 else SOURCE
    unused_inputs = ({'M_PeonyV2_' + name: {'NormalTexture', 'ORMTexture'}
                      for name in ('Fabric', 'FlowFabric', 'FlowPanel', 'FlowStrap')}
                     | {'M_PeonyV2_SourceGlow': set(), 'M_PeonyV2_Stocking': {'WeaveNormal'}}
                     if args.v2 else UNLIT_UNUSED)
    exports = {e['Package']: e for path in readbacks for rows in json.loads(path.read_text()).values()
               for e in rows if e['Type'] in ('Material', 'MaterialInstanceConstant')}
    assets = resolver.load_assets(readbacks)
    materials = {p: resolver.resolve(assets, p) for p in assets if p.startswith(prefix)}
    if len(materials) != (82 if args.v2 else 34):
        raise ValueError('Unexpected Peony root/palette inventory')
    entries = [e for e in json.loads(args.manifest.read_text())['materials']
               if e.get('source_parent', '').startswith(prefix)]
    if len(entries) != (6 if args.v2 else 4):
        raise ValueError('Unexpected Peony adapter inventory')
    for entry in entries:
        required = entry.get('required_source_textures', [])
        source = exports[entry['source_parent']]
        check_uniforms(source, required)
        companion = exports[entry['material'].split('.')[0]]
        if not companion['Package'].startswith(SHARED):
            raise ValueError('Peony companion is outside the shared author namespace')
        if companion['Properties'].get('ShadingModel') != 'MSM_Unlit':
            raise ValueError('Ghost normal-input exclusion requires an unlit material')
        unused = unused_inputs[source['Name']]
        if not unused <= set(required):
            raise ValueError('Source normal texture names changed')
        numeric = set()
        if args.v2:
            numeric = {'CSS_AstralOpacity', 'CSS_AstralCorrupted', 'CSS_AstralFixedTime', 'CSS_AstralUseFixedTime'}
            if source['Name'].endswith('_Stocking'):
                numeric |= {'StockingOpacity', 'StockingTint', 'StockingTiling'}
            elif source['Name'].endswith('_SourceGlow'):
                numeric |= {'SourceGlowColor', 'SourceGlowStrength'}
            else:
                numeric |= {'BaseTint', 'FabricChannel', 'PatternTint', 'PatternTiling', 'PatternIntensity'}
                if '_Flow' in source['Name']:
                    numeric |= {'GlowColor', 'GlowStrength', 'FlowSpeed', 'FlowPhase'}
                    if not source['Name'].endswith('_FlowStrap'):
                        numeric.add('GlowFill')
        check_uniforms(companion, [name for name in required if name not in unused] + ['CSS_AstralNoise'], numeric)
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
        if numeric:
            broken = copy.deepcopy(companion)
            broken['LoadedMaterialResources'][0]['LoadedShaderMap']['Content'][
                'MaterialCompilationOutput']['UniformExpressionSet']['UniformNumericParameters'] = []
            try:
                check_uniforms(broken, [], numeric)
            except ValueError:
                pass
            else:
                raise AssertionError('Missing compiled control negative control passed')

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
    first = (next(p for p, material in sorted(materials.items())
                  if material['parameters']['texture'] and len(material['chain']) > 1)
             if args.v2 else SOURCE + 'Palettes/MI_peony_monochrome_Upper')
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
    if args.companions_readback:
        inputs.append(args.companions_readback)
    report = dict(materials=len(materials), parents=len(entries), feature_levels=['SM5', 'SM6'],
                  tests=tests, sha256={str(p.resolve()): digest(p) for p in inputs},
                  limitations='No live copy, rendering, cloth-motion or frame-time verification')
    (args.output / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Peony: {len(materials)} source materials accepted, {len(entries)} source/ghost uniform pairs checked, negative controls passed')


if __name__ == '__main__':
    main()
