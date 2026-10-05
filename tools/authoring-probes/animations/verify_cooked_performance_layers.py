"""Compare independent cooked adapter readback with the installed source graphs."""
import argparse
import hashlib
import json
from pathlib import Path
import re


FAMILIES = {
    'Eve': ('SeduXtress/ABP_Secondary', 'eins0fx.seduxtress', 'CSS_EveStellarBlade_eins0fx_P'),
    'EveBikini': ('EveTest/ABP_BikiniFeet2', 'eins0fx.seduxtress', 'CSS_EveStellarBlade_eins0fx_P'),
    'EveKnit': ('EveTest/ABP_KnitFeet1', 'eins0fx.seduxtress', 'CSS_EveStellarBlade_eins0fx_P'),
    'Genessa': ('UnholyGenessa/ABP_Wings2', 'eins0fx.unholygenessa', 'CSS_UnholyGenessa_eins0fx_P'),
    'CommanderWhite': ('CWTest/ABP_BodyHairB4', 'eins0fx.commanderwhite', 'CSS_CommanderWhite_eins0fx_P'),
}
FIELDS = {'CSSPerformanceSequence': 'ObjectProperty', 'CSSPerformanceEnabled': 'BoolProperty',
          'CSSPerformanceTime': 'FloatProperty', 'CSSPerformanceWeight': 'FloatProperty'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def exports(rows):
    defaults = [row for row in rows if row['Name'].startswith('Default__')]
    classes = [row for row in rows if row['Type'] == 'AnimBlueprintGeneratedClass']
    require(len(defaults) == len(classes) == 1, 'Expected one class and its defaults')
    return defaults[0]['Properties'], classes[0]


def verify(source, adapter):
    old, old_class = exports(source)
    new, new_class = exports(adapter)
    fields = {p['Name']: p for p in new_class['ChildProperties']}
    require({name: fields[name]['Type'] for name in FIELDS} == FIELDS, 'Runtime control types differ')
    require(fields['CSSPerformanceSequence']['PropertyClass'] == {
        'ObjectName': "Class'AnimSequence'", 'ObjectPath': '/Script/Engine'}, 'Sequence field class differs')
    for name, default in zip(FIELDS, (None, False, 0.0, 0.0)):
        require(new.get(name, default) == default, 'Adapter does not start inactive')
    css = lambda properties: {k: v for k, v in properties.items()
                              if k.startswith('CSS') and not k.startswith('CSSPerformance')}
    require(css(old) == css(new), 'An existing CSS default changed')
    skeleton = old_class['Properties']['TargetSkeleton']
    require(skeleton == new_class['Properties']['TargetSkeleton'], 'Target skeleton changed')
    node_names = [p['Name'] for p in new_class['ChildProperties'] if p['Name'].startswith('AnimGraphNode_')]
    blend_name, evaluator_name = 'AnimGraphNode_TwoWayBlend', 'AnimGraphNode_SequenceEvaluator'
    require(new[blend_name]['bResetChildOnActivation'] is True, 'Blend child reset is disabled')
    require(new[blend_name]['AlphaInputType'] == 'EAnimAlphaInputType::Float', 'Blend uses a separate bool timer')
    require(new[blend_name]['B']['LinkID'] == node_names.index(evaluator_name), 'Blend does not sample evaluator')
    rewired = []
    for name, settings in old.items():
        if not name.startswith('AnimGraphNode_'):
            continue
        candidate = dict(new[name])
        expected = dict(settings)
        if candidate.get('Source') != expected.get('Source'):
            require(candidate['Source']['LinkID'] == node_names.index(blend_name), 'Unexpected pose rewiring')
            require(new[blend_name]['A'] == expected['Source'], 'Original idle is not the base pose')
            candidate.pop('Source'); expected.pop('Source'); rewired.append(name)
        if 'SourcePropertyNames' in expected:
            for a, b in zip(expected['SourcePropertyNames'], candidate['SourcePropertyNames'], strict=True):
                normalize = lambda value: re.sub(r'_[A-F0-9]{32}$', '', value)
                require(normalize(a) == normalize(b), 'A rig input mapping changed')
                require(old.get(a) == new.get(b), 'A rig input default changed')
            candidate.pop('SourcePropertyNames'); expected.pop('SourcePropertyNames')
        require(candidate == expected, 'An original graph node changed: ' + name)
    require(len(rewired) == 1, 'Expected one insertion before secondary motion')
    path = skeleton['ObjectPath'].rsplit('.', 1)[0]
    return path + '.' + path.rsplit('/', 1)[-1], len(css(old))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sources', type=Path)
    parser.add_argument('adapters', type=Path)
    parser.add_argument('package_receipt', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'Output already exists')
    sources, adapters, receipt = [json.loads(path.read_text()) for path in
                                 (args.sources, args.adapters, args.package_receipt)]
    require(receipt['passed'] is True and len(adapters) == len(sources) == 5, 'Incomplete package readback')
    results, bindings = [], []
    for family, (source, outfit, stem) in FAMILIES.items():
        source_path = '/Game/CSS/' + source
        adapter_path = '/Game/CSS/Locomotion/Adapters/ABP_' + family
        skeleton, controls = verify(sources[source_path], adapters[adapter_path])
        dependencies = [(Path(path), checksum) for path, checksum in receipt['dependencies'].items()
                        if Path(path).name == stem + '.utoc']
        require(len(dependencies) == 1, 'Missing audited source container')
        toc, checksum = dependencies[0]
        require(hashlib.sha256(toc.read_bytes()).hexdigest() == checksum, 'Source container changed')
        bindings.append(dict(source_outfit=outfit, source_class=source_path + '.' + source.rsplit('/', 1)[-1] + '_C',
                             target_class=adapter_path + '.ABP_' + family + '_C', skeleton=skeleton,
                             source_toc_sha256=checksum, source_bulk_bytes=toc.with_suffix('.ucas').stat().st_size))
        results.append(dict(family=family, preserved_cooked_css_defaults=controls, passed=True))
    result = dict(passed=True, contract='explicit-weight-v1', adapters=bindings, results=results,
                  evidence={str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest()
                            for path in (args.sources, args.adapters, args.package_receipt)})
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Five cooked adapters match their installed source controls, rigs and pose connections')


if __name__ == '__main__':
    main()
