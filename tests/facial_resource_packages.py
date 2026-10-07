"""Test facial-resource loading in real metadata paks with synthetic IoStore companions."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css_facial_resources import encode, externalize, resolve


def fixture(performance):
    rig = dict(id='test.face', morphs=[c['morph'] for c in performance['curves']],
               expressions=[dict(id='test.neutral', name='Neutral', weights={})],
               adapters=[dict(source_outfit='test.outfit', source_mesh='/Game/Test/Body.Body',
                              target_mesh='/Game/CSS/Locomotion/Faces/Body.Body',
                              skeleton='/Game/Test/Skeleton.Skeleton', source_toc_sha256='a'*64,
                              source_bulk_bytes=123)], performances=[performance])
    library = dict(id='test.library', name='Test', author='Test', version='1', faces=[rig],
                   entries=[dict(id=performance['clip'], name='Test', category='performance', playback='once',
                                 assets=[dict(asset='/Game/CSS/Locomotion/Test.Test',
                                              skeleton='/Game/Test/Skeleton.Skeleton')])])
    return dict(format='CSS.Package', format_version=1, game='MortalShell2', engine='5.6',
                kind='animation_library', id='test.library', name='Test', author='Test', version='1',
                catalog=dict(schema=1, animation_library=library))


def run(args, root):
    performance = dict(clip='test.clip', duration=1., curves=[dict(morph='Blink', keys=[[0., 0.], [1., 1.]])])
    original = fixture(performance)
    valid, resources = externalize(original)
    cases = []
    python_rejections = 0

    def write_case(name, manifest, files, expected=None, python_reject=True):
        nonlocal python_rejections
        if expected is not None:
            if resolve(manifest, files) != expected:
                raise AssertionError('Python round-trip lost facial data: '+name)
        elif python_reject:
            try:
                resolve(manifest, files)
            except (ValueError, KeyError, TypeError):
                python_rejections += 1
            else:
                raise AssertionError('Python accepted invalid resource: '+name)
        directory = root/name
        directory.mkdir()
        manifest = copy.deepcopy(manifest)
        manifest['containers'] = {}
        stem = 'CSS_Test_P'
        for suffix in ('.utoc', '.ucas'):
            data = b'Synthetic metadata test, not a playable Unreal container.\n'
            (directory/(stem+suffix)).write_bytes(data)
            manifest['containers'][suffix] = dict(file=stem+suffix, bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        staging = root/(name+'-stage')
        metadata = staging/'MortalShell2/Content/CSS/Packages/test.library'
        metadata.mkdir(parents=True)
        (metadata/'manifest.json').write_bytes(encode(manifest))
        for file, data in files.items():
            (metadata/file).write_bytes(data)
        subprocess.run([str(args.repak.resolve()), 'pack', str(staging), str(directory/(stem+'.pak')),
                        '--version', 'V8B'], check=True, stdout=subprocess.DEVNULL)
        row = dict(name=name, packages=int(expected is not None))
        if expected is not None:
            row['catalog'] = expected['catalog']
        cases.append(row)

    write_case('external', valid, resources, original)
    write_case('inline', original, {}, original)

    def invalid(name, edit, python_reject=True):
        manifest, files = copy.deepcopy(valid), dict(resources)
        edit(manifest, files)
        write_case(name, manifest, files, python_reject=python_reject)

    def descriptor(manifest):
        return manifest['facial_resources'][0]

    def payload_case(name, edit, python_reject=True):
        def change(manifest, files):
            item = descriptor(manifest)
            payload = json.loads(files[item['file']])
            edit(payload)
            data = encode(payload)
            files[item['file']] = data
            item.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
        invalid(name, change, python_reject)

    invalid('missing', lambda m, f: f.clear())
    invalid('wrong_hash', lambda m, f: descriptor(m).update(sha256='0'*64))
    invalid('wrong_size', lambda m, f: descriptor(m).update(bytes=1))
    invalid('float_size', lambda m, f: descriptor(m).update(bytes=float(descriptor(m)['bytes'])))
    invalid('negative_size', lambda m, f: descriptor(m).update(bytes=-1))
    invalid('empty', lambda m, f: f.update({'test.face.face.json': b''}))
    invalid('duplicate', lambda m, f: m['facial_resources'].append(copy.deepcopy(descriptor(m))))
    invalid('too_many', lambda m, f: m.update(facial_resources=m['facial_resources']*17))
    invalid('no_descriptors', lambda m, f: m.update(facial_resources=[]))
    invalid('extra_descriptor', lambda m, f: descriptor(m).update(extra=True))
    invalid('missing_descriptor_field', lambda m, f: descriptor(m).pop('sha256'))
    invalid('path_escape', lambda m, f: descriptor(m).update(file='../test.face.face.json'))
    invalid('other_rig', lambda m, f: descriptor(m).update(rig='test.other', file='test.other.face.json'))
    invalid('duplicate_rig', lambda m, f: m['catalog']['animation_library']['faces'].append(
        copy.deepcopy(m['catalog']['animation_library']['faces'][0])))
    invalid('mixed', lambda m, f: m['catalog']['animation_library']['faces'][0].update(performances=[]))
    invalid('wrong_kind', lambda m, f: m.update(kind='outfit'))
    invalid('oversized_resource', lambda m, f: f.update({'test.face.face.json': b' '*(16*1024*1024+1)}))
    invalid('oversized_total', lambda m, f: f.update({f'test.extra{i}.face.json': b' '*(16*1024*1024)
                                                   for i in range(4)}), python_reject=False)
    invalid('oversized_manifest', lambda m, f: m.update(padding='x'*(256*1024)), python_reject=False)
    payload_case('wrong_schema', lambda p: p.update(schema=2))
    payload_case('float_schema', lambda p: p.update(schema=1.0))
    payload_case('wrong_payload_rig', lambda p: p.update(rig='test.other'))
    payload_case('extra_payload', lambda p: p.update(extra=True))
    payload_case('bad_performance_array', lambda p: p.update(performances={}))
    payload_case('too_many_performances', lambda p: p.update(performances=p['performances']*65))
    def malformed(manifest, files):
        data = b'{broken json'
        files['test.face.face.json'] = data
        descriptor(manifest).update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    invalid('malformed_json', malformed)
    # These must reach the existing native facial parser after resource verification.
    payload_case('unknown_morph', lambda p: p['performances'][0]['curves'][0].update(morph='Unknown'), False)
    payload_case('unordered_keys', lambda p: p['performances'][0]['curves'][0].update(keys=[[1., 1.], [0., 0.]]), False)
    payload_case('missing_body', lambda p: p['performances'][0].update(clip='test.absent'), False)

    if args.performance:
        large = fixture(json.loads(args.performance.read_text()))
        if len(encode(large)) <= 256*1024:
            raise ValueError('Expected a real performance exceeding the old manifest limit')
        manifest, data = externalize(large)
        write_case('large_performance', manifest, data, large)
    (root/'expected.json').write_bytes(encode(cases))
    subprocess.run([str(args.test_binary.resolve()), str(root)], check=True)
    report = dict(passed=True, cases=len(cases), authoring_rejections=python_rejections,
                  real_performance=str(args.performance) if args.performance else None,
                  scope='Metadata and catalog validation only; no Unreal asset or live playback acceptance.')
    if args.report:
        args.report.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repak', type=Path, required=True)
    parser.add_argument('--test-binary', type=Path, required=True)
    parser.add_argument('--performance', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='css-facial-resources-') as directory:
        run(args, Path(directory))


if __name__ == '__main__':
    main()
