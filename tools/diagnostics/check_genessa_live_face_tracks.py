"""Compare Genessa's live facial weights with installed tracks, then restore CSS."""
import argparse
import bisect
import json
from pathlib import Path
import shutil
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, sha, processes
from check_peony_live_presets import customize, engine, request

METADATA = ROOT/'build/eve-surface-package1/metadata/CSS_SharedAssets_P/MortalShell2/Content/CSS/Packages/css.locomotion'
MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def evaluate(keys, seconds):
    i = bisect.bisect_right([k[0] for k in keys], seconds)
    if i == 0:
        return keys[0][1]
    if i == len(keys):
        return keys[-1][1]
    a, b = keys[i-1], keys[i]
    return a[1]+(b[1]-a[1])*(seconds-a[0])/(b[0]-a[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    original, pids = describe(), processes()
    performance = lambda **fields: customize({'action': 'performance', **fields})
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok'] or not cine['result']['status'].startswith('Take finished.'):
        raise RuntimeError('This fixture requires CINE to be between takes')
    if out.exists() or len(pids) != 1 or original['busy'] or performance(op='status')['active']:
        raise RuntimeError('Need one settled game, no active performance and a new output directory')
    face_path = METADATA/'css.unholygenessa.face.face.json'
    manifest = METADATA/'manifest.json'
    library = json.loads(manifest.read_text())['catalog']['animation_library']
    face = next(f for f in library['faces'] if f['id'] == 'css.unholygenessa.face')
    tracks = [p for p in json.loads(face_path.read_text())['performances']
              if p['clip'] in ('css.eve.dance.irisout', 'css.eve.dance.comecloser')]
    assert len(tracks) == 2
    names = face['morphs']
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    before = json.loads((MOD/'state/state.json').read_text())
    atomic(out/'state-before.json', before)
    report = dict(passed=False, original=original, process_before=pids, cine_before=cine,
                  inputs={str(p): sha(p) for p in (face_path, manifest)}, cases=[],
                  camera_input=False, movement_input=False, started_ns=time.time_ns())
    token, expected, target = None, original['customize'], identity(original)
    idle_changed = False
    with (out/'requests.jsonl').open('x') as log:
        def probe(**value):
            log.write(json.dumps({'request': value})+'\n'); log.flush()
            result = engine(value)
            log.write(json.dumps({'result': result})+'\n'); log.flush()
            return result

        def weights(mesh):
            return {name: probe(op='call', target=mesh, function='GetMorphTarget',
                                args={'MorphTargetName': name})['ReturnValue'] for name in names}

        def guard():
            current = describe()
            if processes() != pids or identity(current) != target or current['customize'] != expected:
                raise RuntimeError('Concurrent appearance/process change')

        def stop_owned():
            if token is None:
                return
            state = performance(op='status')
            if state['token'] != token:
                raise RuntimeError('Performance owner changed; do not stop its replacement')
            if state['active']:
                performance(op='stop', token=token)
            deadline = time.monotonic()+8
            while performance(op='status')['active']:
                if time.monotonic() >= deadline:
                    raise TimeoutError('Owned performance did not finish stopping')
                time.sleep(.1)

        try:
            assert any(i['id'] == 'eve' for i in original['idles']), 'Body-only Eve idle is not available'
            customize({'action': 'idle', 'value': 'eve'})
            idle_changed = True
            for variant in ('eve', 'genessa'):
                guard()
                target = original['shell'], 'eins0fx.unholygenessa', variant
                customize({'action': 'wear', 'outfit': target[1], 'variant': variant, 'persist': False})
                active = settled(target)
                expected = active['customize']
                assert any(i['id'] == 'eve' for i in active['idles'])
                for track in tracks:
                    guard()
                    pawn = probe(op='player')['pawn']
                    mesh = probe(op='get', target=pawn, property='Mesh')
                    case = dict(variant=variant, clip=track['clip'], baseline=weights(mesh), samples=[])
                    report['cases'].append(case)
                    started = performance(op='start', id=track['clip'], seconds=.5, rate=1.)
                    token = started['token']
                    time.sleep(.4)
                    paused = performance(op='pause', token=token, paused=True)
                    assert paused['active'] and paused['weight'] == 1
                    post = probe(op='get', target=mesh, property='PostProcessAnimInstance')
                    case['post'] = post
                    case['mesh'] = probe(op='call', target=mesh, function='GetSkeletalMeshAsset', args={})['ReturnValue']
                    # Include the most expressive authored instant, rather than only neutral endpoints.
                    times = sorted({k[0] for c in track['curves'] for k in c['keys']})
                    peak = max(times, key=lambda t: sum(abs(evaluate(c['keys'], t)) for c in track['curves']))
                    for seconds in (0., peak, track['duration']*.8):
                        guard()
                        clock = performance(op='seek', token=token, seconds=seconds)
                        actual = weights(mesh)
                        wanted = dict.fromkeys(names, 0.)
                        wanted.update({c['morph']: evaluate(c['keys'], clock['seconds']) for c in track['curves']})
                        maximum = max(abs(actual[n]-wanted[n]) for n in names)
                        row = dict(clock=clock, actual=actual, expected=wanted, max_error=maximum,
                                   body_time=probe(op='get', target=post, property='CSSPerformanceTime'))
                        case['samples'].append(row)
                        atomic(out/'result.json', report)
                        assert maximum < 2e-5, f'Facial weight mismatch {variant}/{track["clip"]}: {maximum}'
                        assert abs(row['body_time']-clock['seconds']) < 2e-5, 'Body and face clocks disagree'
                    performance(op='seek', token=token, seconds=1.)
                    performance(op='pause', token=token, paused=False)
                    time.sleep(.6)
                    case['advancing'] = performance(op='status')
                    assert case['advancing']['active'] and case['advancing']['seconds'] > 1.1
                    stop_owned()
                    token = None
                    settled(target, custom=expected)
                    time.sleep(.3)
                    case['restored_weights'] = weights(mesh)
                    case['restored_error'] = max(abs(case['restored_weights'][n]-case['baseline'][n]) for n in names)
                    assert case['restored_error'] < 2e-5, 'Facial baseline did not restore'
                    case['passed'] = True
                    atomic(out/'result.json', report)
                    print(variant, track['clip'], 'weights, clocks and restoration pass', flush=True)
        except Exception as error:
            report['error'] = repr(error)
            raise
        finally:
            try:
                stop_owned()
                guard()
                if idle_changed:
                    if describe()['idle_override'] != 'eve':
                        raise RuntimeError('Idle override changed independently; automatic restore skipped')
                    customize({'action': 'idle', 'value': original['idle_override']})
                customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                           'customize': original['customize'], 'persist': True})
                report['after_restore'] = settled(identity(original), custom=original['customize'])
                time.sleep(1.2)
                after = json.loads((MOD/'state/state.json').read_text())
                atomic(out/'state-after.json', after)
                report['saved_state_equal'] = before == after
                assert report['saved_state_equal']
                report['restored'] = True
                report['cine_after'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
                assert report['cine_after']['result'] == cine['result'], 'CINE model changed during test'
                assert all(sha(Path(p)) == digest for p, digest in report['inputs'].items())
            except Exception as error:
                report['restore_error'] = repr(error)
                raise
            finally:
                report['process_after'] = processes()
                report['finished_ns'] = time.time_ns()
                report['passed'] = ('error' not in report and 'restore_error' not in report and
                                    len(report['cases']) == 4 and all(c.get('passed') for c in report['cases']))
                atomic(out/'result.json', report)
    print(json.dumps({k: report[k] for k in ('passed', 'restored', 'saved_state_equal')}))


if __name__ == '__main__':
    main()
