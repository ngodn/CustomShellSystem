"""Replay Genessa's facial authoring chain in an isolated Blender workspace."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys

from verify_genessa_face_inputs import AUTHOR, ROOT, WORK, read, sha


def compare_payload(expected, generated, stage, preview):
    if sha(expected) == sha(generated):
        return 'byte_exact'
    if stage not in ('backing', 'export'):
        raise ValueError(stage + ' does not reproduce the accepted data')
    old, new = read(expected), read(generated)
    original_inputs = old['inputs'] if stage == 'backing' else old['backing']['inputs']
    actual_inputs = new['inputs'] if stage == 'backing' else new['backing']['inputs']
    original_preview = str(AUTHOR / 'work/face-articulation5/preview.blend')
    assert sha(Path(original_preview)) == original_inputs[original_preview]
    expected_inputs = dict(original_inputs)
    del expected_inputs[original_preview]
    expected_inputs[str(preview)] = sha(preview)
    assert actual_inputs == expected_inputs, 'Unexpected backing provenance change'
    normalized = copy.deepcopy(new)
    if stage == 'backing':
        normalized['inputs'] = original_inputs
    else:
        normalized['backing']['inputs'] = original_inputs
    assert normalized == old, 'Payload differs beyond the regenerated preview provenance'
    return 'preview_provenance_only'


def link_packets(out):
    original = AUTHOR / 'work/face-mapping1/mapping.json'
    mapping = read(original)
    patch = out / 'export/head-patch.json'
    compare_payload(AUTHOR / 'work/face-patch1/head-patch.json', patch, 'export',
                    out / 'articulation/preview.blend')
    old_patch = 'work/face-patch1/head-patch.json'
    assert mapping['inputs'][old_patch] == sha(AUTHOR / old_patch)
    before = copy.deepcopy(mapping)
    del mapping['inputs'][old_patch]
    mapping['inputs'][str(patch)] = sha(patch)
    assert {k: v for k, v in mapping.items() if k != 'inputs'} == {
        k: v for k, v in before.items() if k != 'inputs'}
    for path, digest in mapping['inputs'].items():
        assert sha(AUTHOR / path) == digest, path
    rebound = out / 'mapping.json'
    with rebound.open('x') as stream:
        stream.write(json.dumps(mapping, indent=2) + '\n')
    recipe = AUTHOR / 'work/face-integration-input3/recipe.py'
    command = [sys.executable, str(recipe), '--mapping', str(rebound), '--patch', str(patch),
               '--audit', str(AUTHOR / 'work/face-mesh-audit1'), '--output', str(out / 'integration')]
    provenance = dict(original_mapping_sha256=sha(original), mapping_sha256=sha(rebound),
                      input_change={'from': old_patch, 'to': str(patch), 'sha256': sha(patch)},
                      recipe_sha256=sha(recipe), command=command)
    with (out / 'integration-command.json').open('x') as stream:
        stream.write(json.dumps(provenance, indent=2) + '\n')
    with (out / 'integration.log').open('x') as log:
        result = subprocess.run(command, cwd=AUTHOR, stdout=log, stderr=subprocess.STDOUT)
    (out / 'integration-exit.json').write_text(json.dumps({'exit_code': result.returncode}) + '\n')
    result.check_returncode()
    expected = read(AUTHOR / 'work/face-integration-input3/report.json')['outputs']
    actual = read(out / 'integration/report.json')['outputs']
    assert actual == expected
    for row in actual:
        assert sha(out / 'integration' / (row['mesh'] + '.json')) == row['sha256']
    for path, digest in mapping['inputs'].items():
        assert sha(AUTHOR / path) == digest, path
    assert sha(original) == provenance['original_mapping_sha256']
    return dict(passed=True, packets=actual, provenance=provenance)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--resume', action='store_true', help='Reuse the terminal backing-provenance failure after checking its retained outputs')
    args = parser.parse_args()
    out = args.output.resolve()
    if (out.exists() and not args.resume) or out == WORK.resolve() or not out.is_relative_to(WORK.resolve()):
        raise ValueError('Use a new directory beneath the goal workspace')
    original = AUTHOR / 'work'
    source = AUTHOR / 'authoring/GownEye1.blend'
    donor = ROOT / 'work/cine-animation-port/facial-author-head-export-v1/facial-shapes.json'
    topology = ROOT / 'work/cine-animation-port/author-head-source-v1/eve-correspondence-v2.npz'
    blender = ROOT.parent / 'CSS-eins0fx-collections/reference-tools/blender-5.2.2-linux-x64/blender'
    version = subprocess.check_output([str(blender), '--version'], text=True)
    assert version.splitlines()[0] == 'Blender 5.2.2 LTS'
    recipes = {
        'surface': AUTHOR / 'tools/inspect_expression_surface.py',
        'transfer': AUTHOR / 'tools/trial_expression_transfer.py',
        'articulation': original / 'face-articulation5/recipe.py',
        'backing': original / 'face-backing2/recipe.py',
        'export': original / 'face-patch1/recipe.py',
    }
    protected = {}
    for name, field in (('face-transfer1', 'protected'), ('face-articulation5', 'inputs'),
                        ('face-backing2', 'inputs'), ('face-patch1', 'inputs')):
        report = read(original / name / 'report.json')
        for path, digest in report[field].items():
            assert sha(Path(path)) == digest, path
            protected[path] = digest
    expected = {
        'surface': original / 'face-controls1/surfaces.json',
        'transfer': original / 'face-transfer1/morphs.json',
        'articulation': original / 'face-articulation5/morphs.json',
        'backing': original / 'face-backing2/backing.json',
        'export': original / 'face-patch1/head-patch.json',
    }
    for path in [*recipes.values(), *expected.values(), blender]:
        protected[str(path)] = sha(path)
    previous = None
    if args.resume:
        previous = read(out / 'result.json')
        assert previous.get('error') == "AssertionError('backing does not reproduce the accepted data')"
        assert not previous['passed'] and previous['changed_inputs'] == []
        assert [r['stage'] for r in previous['stages']] == ['surface', 'transfer', 'articulation', 'backing']
        assert all(r['returncode'] == 0 for r in previous['stages'])
        assert previous['protected'] == protected
        assert not (out / 'failed-attempt1.json').exists()
        shutil.copyfile(out / 'result.json', out / 'failed-attempt1.json')
        shutil.copyfile(__file__, out / 'resume-recipe.py')
    else:
        out.mkdir()
        shutil.copyfile(__file__, out / 'recipe.py')
    (out / 'blender-version.txt').write_text(version)
    (out / 'inputs.json').write_text(json.dumps(protected, indent=2) + '\n')
    report = dict(passed=False, stages=[], original_source=str(source), protected=protected)

    def save():
        temporary = out / 'result.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(out / 'result.json')

    common = ['bwrap', '--unshare-net', '--ro-bind', '/', '/', '--bind', str(out), str(out),
              '--dev-bind', '/dev', '/dev', '--tmpfs', '/tmp', '--die-with-parent', '--',
              str(blender), '--background', '--factory-startup', '--disable-autoexec',
              '--threads', '2', '--python-exit-code', '1']
    specifications = [
        ('surface', ['--source', source]),
        ('transfer', ['--surface', out / 'surface/surfaces.json', '--donor', donor, '--topology', topology]),
        ('articulation', ['--surface', out / 'surface/surfaces.json', '--transfer', out / 'transfer/morphs.json',
                          '--donor', donor, '--topology', topology]),
        ('backing', ['--source', source, '--candidate', out / 'articulation', '--donor', donor, '--topology', topology]),
        ('export', ['--source', source, '--candidate', out / 'articulation', '--backing', out / 'backing/backing.json',
                    '--topology', topology]),
    ]
    save()
    try:
        for name, arguments in specifications:
            command = common + ['--python', str(recipes[name]), '--'] + list(map(str, arguments)) + ['--output', str(out / name)]
            row = dict(stage=name, command=command, recipe_sha256=sha(recipes[name]), expected=str(expected[name]))
            report['stages'].append(row)
            save()
            prior = next((r for r in previous['stages'] if r['stage'] == name), None) if previous else None
            if prior:
                assert prior['command'] == command and prior['recipe_sha256'] == row['recipe_sha256']
                assert sha(out / name / expected[name].name) == prior['sha256']
                row.update(returncode=0, reused_completed_process=True)
            else:
                print('Running', name, flush=True)
                with (out / (name + '.log')).open('x') as log:
                    result = subprocess.run(command, cwd=AUTHOR, stdout=log, stderr=subprocess.STDOUT)
                row['returncode'] = result.returncode
                save()
                result.check_returncode()
            payload = out / name / expected[name].name
            row['sha256'] = sha(payload)
            row['expected_sha256'] = protected[str(expected[name])]
            row['exact_payload'] = row['sha256'] == row['expected_sha256']
            row['bytes'] = payload.stat().st_size
            row['comparison'] = compare_payload(expected[name], payload, name, out / 'articulation/preview.blend')
            row['data_preserved'] = True
            save()
            print(name, row['comparison'], flush=True)
        report['integration'] = link_packets(out)
        report['chain_replayed'] = True
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        report['changed_inputs'] = [path for path, digest in protected.items() if sha(Path(path)) != digest]
        report['passed'] = bool(report.get('chain_replayed') and not report.get('error') and not report['changed_inputs'])
        report['scope'] = ('Five-stage facial data replay and two exact W3 packets from GownEye1 and retained Eve donor data. '
                           'No outfit export, UE cook, runtime change or new visual acceptance.')
        save()
    print(json.dumps(dict(passed=report['passed'], stages=len(report['stages']), protected_inputs=len(protected))))


if __name__ == '__main__':
    main()
