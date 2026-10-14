"""Repeat normal/facial outfit switches and measure component retention."""
import argparse
from collections import Counter
import json
from pathlib import Path
import shutil
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, sha, processes
from check_peony_live_presets import customize, engine, request

MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
MANIFEST = ROOT/'build/eve-surface-package1/metadata/CSS_SharedAssets_P/MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json'
OUTFITS = {
    'eins0fx.seduxtress': ('black_pearl', 'prototype', 'skin_suit', 'bikini',
                         'casual_sweater', 'midsummer_alice', 'peony'),
    'eins0fx.unholygenessa': ('eve', 'genessa'),
    'eins0fx.commanderwhite': ('original',),
}


def path(handle, key='name'):
    return handle[key].split(' ', 1)[1] if handle else ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3, choices=range(2, 6))
    args = parser.parse_args()
    out = args.output.resolve()
    pids, original = processes(), describe()
    perf = lambda **fields: customize({'action': 'performance', **fields})
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError(cine)
    buttons = {c['id']: c.get('enabled', True) for s in cine['result']['sections'] for c in s['controls']}
    if out.exists() or len(pids) != 1 or original['busy'] or perf(op='status')['active'] or not buttons['take']:
        raise RuntimeError('Need one settled game, CINE between takes, no active performance and a new output')
    before = json.loads((MOD/'state/state.json').read_text())
    memories = before['remembered_custom']
    restore_variants = {}
    for outfit, variants in OUTFITS.items():
        if any(outfit+'/'+variant not in memories for variant in variants):
            raise RuntimeError('Fixture requires existing memories for every tested variant')
        restore_variants[outfit] = next((v for v in variants if memories[outfit+'/'+v] == memories[outfit]), None)
        if restore_variants[outfit] is None:
            raise RuntimeError('Cannot restore outfit-level memory through an existing variant')
    library = json.loads(MANIFEST.read_text())['catalog']['animation_library']
    core = json.loads((MOD/'core.json').read_text())
    if core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Selected core is not mapped')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    atomic(out/'state-before.json', before)
    atomic(out/'cine-model-before.json', cine)
    report = dict(passed=False, rounds=args.rounds, cases=[], original=original,
                  process_before=pids, core=core, core_sha256=sha(MOD/'cores'/core['file']),
                  manifest_sha256=sha(MANIFEST), movement_input=False, camera_input=False)
    expected, target, token = original['customize'], identity(original), None

    with (out/'requests.jsonl').open('x') as log:
        def probe(**value):
            log.write(json.dumps({'request': value})+'\n'); log.flush()
            result = engine(value)
            log.write(json.dumps({'result': result})+'\n'); log.flush()
            return result

        def get(obj, prop):
            return probe(op='get', target=obj, property=prop)

        def call(obj, name, **fields):
            return probe(op='call', target=obj, function=name, args=fields)['ReturnValue']

        player = probe(op='player')
        component_class = probe(op='find', path='/Script/Engine.ActorComponent')
        mesh = get(player['pawn'], 'Mesh')
        probe(op='describe', target=player['pawn'], function='K2_GetComponentsByClass')
        probe(op='describe', target=mesh, function='GetChildrenComponents')

        def geometry():
            return dict(position=call(player['pawn'], 'K2_GetActorLocation'),
                        rotation=call(player['pawn'], 'K2_GetActorRotation'),
                        view_target=call(player['controller'], 'GetViewTarget'),
                        control_rotation=call(player['controller'], 'GetControlRotation'))

        initial_geometry = report['geometry_before'] = geometry()

        def guard():
            current = describe()
            if processes() != pids or identity(current) != target or current['customize'] != expected:
                raise RuntimeError('Concurrent player or appearance change')

        def sample():
            guard()
            asset = call(mesh, 'GetSkeletalMeshAsset')
            authored = get(asset, 'PostProcessAnimBlueprint')
            post = get(mesh, 'PostProcessAnimInstance')
            skeleton = get(asset, 'Skeleton')
            adapters = [a for a in library['adapters'] if a['source_class'] == path(authored)
                        and a['skeleton'] == path(skeleton)]
            assert path(post, 'class') in {path(authored), *(a['target_class'] for a in adapters)}, 'Wrong post-process owner'
            owned = call(player['pawn'], 'K2_GetComponentsByClass', ComponentClass=component_class)
            children = probe(op='call', target=mesh, function='GetChildrenComponents',
                             args={'bIncludeAllDescendants': False})['Children']
            for values in (owned, children):
                assert all(values) and len({x['$object'] for x in values}) == len(values), 'Null or duplicate component'
            summary = dict(owned=dict(Counter(x['class'] for x in owned)),
                           children=dict(Counter(x['class'] for x in children)))
            assert probe(op='player') == player, 'Player changed during component sample'
            if call(mesh, 'GetSkeletalMeshAsset') != asset:
                raise RuntimeError('Mesh changed during sample: '+json.dumps(perf(op='status')))
            assert get(mesh, 'PostProcessAnimInstance') == post, 'Animation owner changed during sample'
            return dict(asset=asset, authored=authored, post=post, owned=owned, children=children, counts=summary)

        def stop_owned():
            if token is None:
                return
            p = perf(op='status')
            if p['token'] != token:
                raise RuntimeError('Performance owner changed; preserving the replacement')
            if p['active']:
                perf(op='stop', token=token)
            deadline = time.monotonic()+10
            while perf(op='status')['active']:
                if time.monotonic() > deadline:
                    raise TimeoutError('Owned performance did not stop')
                time.sleep(.1)

        try:
            customize({'action': 'idle', 'value': 'eve'})
            baselines = {}
            for cycle in range(args.rounds):
                for outfit, variants in OUTFITS.items():
                    for variant in variants:
                        guard()
                        if geometry() != initial_geometry:
                            raise RuntimeError('Player or camera moved; stationary fixture stopped')
                        target = original['shell'], outfit, variant
                        expected = memories[outfit+'/'+variant]
                        customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                                   'customize': expected, 'persist': False})
                        settled(target, custom=expected)
                        time.sleep(.4)
                        row = dict(cycle=cycle+1, outfit=outfit, variant=variant)
                        report['cases'].append(row)
                        row['before'] = sample()
                        match = [a for f in library['faces'] for a in f['adapters']
                                 if a['source_outfit'] == outfit and path(row['before']['asset'])
                                 in (a['source_mesh'], a['target_mesh'])]
                        assert len(match) == 1, 'Missing or ambiguous facial mapping'
                        started = perf(op='start', id='css.eve.dance.irisout', seconds=2., rate=1.)
                        token = started['token']
                        time.sleep(.4)
                        paused = perf(op='pause', token=token, paused=True)
                        assert paused['active'] and paused['weight'] == 1
                        row['performance'] = paused
                        row['during'] = sample()
                        assert path(row['during']['asset']) == match[0]['target_mesh'], 'Facial mesh did not activate'
                        post = row['during']['post']
                        row['adapter_inputs'] = {n:get(post,n) for n in ('CSSPerformanceEnabled', 'CSSPerformanceWeight', 'CSSPerformanceTime')}
                        assert row['adapter_inputs']['CSSPerformanceEnabled'] and row['adapter_inputs']['CSSPerformanceWeight'] == 1
                        stop_owned(); token = None
                        deadline = time.monotonic()+10
                        while call(mesh, 'GetSkeletalMeshAsset') != row['before']['asset']:
                            if time.monotonic() > deadline:
                                raise TimeoutError('Pre-performance mesh did not restore')
                            time.sleep(.1)
                        time.sleep(.4)
                        row['after'] = sample()
                        assert path(row['after']['post'], 'class') == path(row['before']['post'], 'class')
                        assert row['before']['counts'] == row['after']['counts'], 'Component counts changed across facial playback'
                        key = outfit+'/'+variant
                        if key in baselines:
                            assert baselines[key] == row['after']['counts'], 'Components accumulated across outfit switches'
                        else:
                            baselines[key] = row['after']['counts']
                        row['passed'] = True
                        atomic(out/'result.json', report)
                        print(cycle+1, key, 'pass', flush=True)
            report['geometry_after_cases'] = geometry()
            assert report['geometry_after_cases'] == initial_geometry
            report['cases_passed'] = True
        except Exception as error:
            report['error'] = repr(error)
            raise
        finally:
            try:
                guard()
                stop_owned()
                for outfit, variant in restore_variants.items():
                    customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                               'customize': memories[outfit], 'persist': False})
                    settled((original['shell'], outfit, variant), custom=memories[outfit])
                customize({'action': 'idle', 'value': original['idle_override']})
                customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                           'customize': original['customize'], 'persist': True})
                report['after_restore'] = settled(identity(original), custom=original['customize'])
                time.sleep(1.2)
                after = json.loads((MOD/'state/state.json').read_text())
                atomic(out/'state-after.json', after)
                report['saved_state_equal'] = before == after
                cine_after = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
                atomic(out/'cine-model-after.json', cine_after)
                report['cine_model_equal'] = cine['result'] == cine_after['result']
                report['geometry_after'] = geometry()
                assert report['saved_state_equal'] and report['cine_model_equal']
                assert report['geometry_after'] == initial_geometry
                report['restored'] = True
            except Exception as error:
                report['restore_error'] = repr(error)
                raise
            finally:
                report['process_after'] = processes()
                report['passed'] = bool(report.get('cases_passed') and report.get('restored') and report['process_after'] == pids)
                atomic(out/'result.json', report)
    print(json.dumps({k:report[k] for k in ('passed', 'restored', 'saved_state_equal', 'cine_model_equal')}))


if __name__ == '__main__':
    main()
