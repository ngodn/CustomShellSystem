"""Exercise facial mesh restoration above 128 children with empty owned components."""
import argparse
import json
from pathlib import Path
import shutil
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, processes, sha
from check_peony_live_presets import engine, customize, request

MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
CORE_SHA = '7004a5a913f6c8ce31fb26b64e6b055134b684269074faa7eb42079b286b3dc1'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    pids, original = processes(), describe()
    perf = lambda **fields: customize({'action': 'performance', **fields})
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    buttons = {c['id']: c.get('enabled', True) for s in cine['result']['sections'] for c in s['controls']}
    core = json.loads((MOD/'core.json').read_text())
    if (out.exists() or len(pids) != 1 or original['busy'] or perf(op='status')['active']
            or not cine['ok'] or not buttons['take'] or identity(original)[1:] != ('eins0fx.seduxtress', 'peony')):
        raise RuntimeError('Need idle Peony, CINE between takes, one game and a new output directory')
    if sha(MOD/'cores'/core['file']) != CORE_SHA or core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Expected tested core is not mapped')
    if engine({'op': 'menu.status'})['menu_open']:
        raise RuntimeError('Close CSS before this stationary world check')
    saved = json.loads((MOD/'state/state.json').read_text())
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    atomic(out/'state-before.json', saved)
    atomic(out/'cine-before.json', cine)
    report = dict(passed=False, core=core, core_sha256=CORE_SHA, process_before=pids,
                  original=original, created=[], removed=[], cases=[], gameplay_input=False)
    token = None
    expected_idle = original['idle_override']
    with (out/'requests.jsonl').open('x') as log:
        def probe(**value):
            log.write(json.dumps({'request': value})+'\n'); log.flush()
            reply = engine(value)
            log.write(json.dumps({'result': reply})+'\n'); log.flush()
            return reply

        def get(obj, prop):
            return probe(op='get', target=obj, property=prop)

        def call(obj, name, **fields):
            return probe(op='call', target=obj, function=name, args=fields)

        player = probe(op='player')
        pawn, controller = player['pawn'], player['controller']
        mesh = get(pawn, 'Mesh')
        scene_class = probe(op='find', path='/Script/Engine.SceneComponent')
        actor_class = probe(op='find', path='/Script/Engine.ActorComponent')
        math = probe(op='find', path='/Script/Engine.Default__KismetMathLibrary')
        for obj, function in ((pawn, 'AddComponentByClass'), (pawn, 'K2_GetComponentsByClass'),
                              (mesh, 'GetChildrenComponents'), (mesh, 'K2_AttachToComponent'),
                              (mesh, 'K2_DestroyComponent'), (math, 'MakeTransform')):
            probe(op='describe', target=obj, function=function)
        transform = call(math, 'MakeTransform', Location=dict(X=0, Y=0, Z=0),
                         Rotation=dict(Pitch=0, Yaw=0, Roll=0), Scale=dict(X=1, Y=1, Z=1))['ReturnValue']

        def geometry():
            return {name: call(obj, function)['ReturnValue'] for name, obj, function in (
                ('position', pawn, 'K2_GetActorLocation'), ('rotation', pawn, 'K2_GetActorRotation'),
                ('view_target', controller, 'GetViewTarget'), ('control_rotation', controller, 'GetControlRotation'))}

        initial_geometry = report['geometry_before'] = geometry()

        def guard():
            current = describe()
            if (processes() != pids or probe(op='player') != player or geometry() != initial_geometry
                    or identity(current) != identity(original) or current['customize'] != original['customize']
                    or current['idle_override'] != expected_idle):
                raise RuntimeError('Concurrent player, view or appearance change')

        def sample():
            guard()
            children = call(mesh, 'GetChildrenComponents', bIncludeAllDescendants=False)['Children']
            owned = call(pawn, 'K2_GetComponentsByClass', ComponentClass=actor_class)['ReturnValue']
            asset = call(mesh, 'GetSkeletalMeshAsset')['ReturnValue']
            post = get(mesh, 'PostProcessAnimInstance')
            rows = [dict(child=c, parent=get(c, 'AttachParent'), socket=get(c, 'AttachSocketName')) for c in children]
            assert all(r['parent'] == mesh for r in rows)
            assert len({c['$object'] for c in children}) == len(children)
            return dict(children=children, owned=owned, attachments=rows, asset=asset, post=post)

        def handles(values):
            return {v['$object'] for v in values}

        def stop_owned():
            if token is not None:
                status = perf(op='status')
                if status['active']:
                    if status['token'] != token:
                        raise RuntimeError('Preserving a replacement performance')
                    perf(op='stop', token=token)

        baseline = report['baseline'] = sample()
        count = 129-len(baseline['children'])
        if not 1 <= count <= 100:
            raise RuntimeError('Unexpected baseline attachment count')
        atomic(out/'result.json', report)
        try:
            customize({'action': 'idle', 'value': 'eve'})
            expected_idle = 'eve'
            time.sleep(.5)
            for i in range(count):
                guard()
                component = call(pawn, 'AddComponentByClass', Class=scene_class, bManualAttachment=True,
                                 RelativeTransform=transform, bDeferredFinish=False)['ReturnValue']
                assert component and component['$object'] not in handles(baseline['owned'])
                report['created'].append(component)
                atomic(out/'result.json', report)
                assert call(component, 'GetOwner')['ReturnValue'] == pawn
                attached = call(component, 'K2_AttachToComponent', Parent=mesh, SocketName='None',
                                LocationRule=0, RotationRule=0, ScaleRule=0, bWeldSimulatedBodies=False)
                assert attached['ReturnValue'] and get(component, 'AttachParent') == mesh
            expanded = report['expanded'] = sample()
            assert len(expanded['children']) == 129
            assert handles(expanded['owned']) == handles(baseline['owned']) | handles(report['created'])
            print('Expanded to', len(expanded['children']), 'direct children', flush=True)
            for cycle in range(3):
                guard()
                row = dict(cycle=cycle+1)
                report['cases'].append(row)
                started = perf(op='start', id='css.eve.dance.irisout', seconds=2., rate=1.)
                token = started['token']
                time.sleep(.4)
                paused = row['performance'] = perf(op='pause', token=token, paused=True)
                assert paused['active'] and paused['weight'] == 1
                during = row['during'] = sample()
                assert during['asset'] != expanded['asset'] and '_Face' in during['asset']['name']
                row['inputs'] = {n:get(during['post'], n) for n in
                                 ('CSSPerformanceEnabled', 'CSSPerformanceWeight', 'CSSPerformanceTime')}
                assert row['inputs']['CSSPerformanceEnabled'] and row['inputs']['CSSPerformanceWeight'] == 1
                assert handles(during['children']) == handles(expanded['children'])
                assert handles(during['owned']) == handles(expanded['owned'])
                stop_owned(); token = None
                deadline = time.monotonic()+10
                while call(mesh, 'GetSkeletalMeshAsset')['ReturnValue'] != expanded['asset']:
                    if time.monotonic() > deadline:
                        raise TimeoutError('Normal mesh did not restore')
                    time.sleep(.1)
                after = row['after'] = sample()
                assert handles(after['owned']) == handles(expanded['owned'])
                assert after['attachments'] == expanded['attachments']
                assert after['post']['class'] == expanded['post']['class']
                row['passed'] = True
                atomic(out/'result.json', report)
                print('Facial cycle', cycle+1, 'passed', flush=True)
            report['checks_passed'] = True
        except Exception as error:
            report['error'] = repr(error)
            raise
        finally:
            errors = []
            try:
                stop_owned()
            except Exception as error:
                errors.append(repr(error))
            for component in reversed(report['created']):
                try:
                    if call(component, 'GetOwner')['ReturnValue'] != pawn:
                        raise RuntimeError('Component owner changed')
                    call(component, 'K2_DestroyComponent', Object=pawn)
                    report['removed'].append(component)
                    atomic(out/'result.json', report)
                except Exception as error:
                    errors.append(repr(error))
            try:
                if describe()['idle_override'] == expected_idle:
                    customize({'action': 'idle', 'value': original['idle_override']})
                    expected_idle = original['idle_override']
                time.sleep(1.3)
                final = report['after_cleanup'] = sample()
                report['component_handles_restored'] = handles(final['owned']) == handles(baseline['owned'])
                report['attachments_restored'] = final['attachments'] == baseline['attachments']
                report['saved_state_equal'] = saved == json.loads((MOD/'state/state.json').read_text())
                report['cine_model_equal'] = cine['result'] == request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
                report['geometry_after'] = geometry()
                report['passed'] = bool(report.get('checks_passed') and not errors and
                    report['component_handles_restored'] and report['attachments_restored'] and
                    report['saved_state_equal'] and report['cine_model_equal'] and
                    len(report['created']) == len(report['removed']))
            except Exception as error:
                errors.append(repr(error))
            report['cleanup_errors'] = errors
            atomic(out/'result.json', report)
    print(json.dumps({k: report.get(k) for k in ('passed', 'checks_passed', 'component_handles_restored',
          'attachments_restored', 'saved_state_equal', 'cine_model_equal', 'cleanup_errors')}, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
