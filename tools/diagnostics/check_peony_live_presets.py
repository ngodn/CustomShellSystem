"""Check Peony's live regional inputs and restore the original customization."""
import argparse
import copy
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, atomic, processes, request


def engine(value):
    reply = request(GAME, {'op': 'engine', 'request': value})
    if not reply['ok']:
        raise RuntimeError(reply['error'])
    return reply['result']


def customize(value):
    return engine({'op': 'service.call', 'service': 'css.customize',
                   'version': 1, 'request': value})


def describe():
    value = customize({'action': 'describe'})
    if (value['outfit'], value['variant']) != ('eins0fx.seduxtress', 'peony'):
        raise RuntimeError('Peony is no longer selected')
    return value


def settled(expected):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        value = describe()
        if value['customize'] != expected:
            raise RuntimeError('Customization changed during the diagnostic')
        if not value['busy']:
            return value
        time.sleep(.2)
    raise TimeoutError('CSS has not applied the requested preset')


def sample():
    get = lambda target, name: engine({'op': 'get', 'target': target, 'property': name})
    pawn = engine({'op': 'player'})['pawn']
    component = get(pawn, 'Mesh')
    post = engine({'op': 'call', 'target': component,
                   'function': 'GetPostProcessInstance', 'args': {}})['ReturnValue']
    nodes = engine({'op': 'properties', 'target': post, 'inherited': True})
    rigs = []
    for node in nodes:
        if node['name'].startswith('AnimGraphNode_ControlRig'):
            value = get(post, node['name'])
            if value.get('ControlRig'):
                rigs.append(value['ControlRig'])
    body = [rig for rig in rigs if '/CR_Body.CR_Body_C' in rig['class']]
    if len(body) != 1:
        raise RuntimeError(f'Expected one active body rig, found {len(body)}')
    fields = ('UseRegionSettings', 'RegionFrequencies', 'RegionDampingRatios',
              'RegionMotionAmounts', 'RegionEnabled')
    row = dict(post=post, rigs=rigs, public={name: get(post, 'CSSBody'+name) for name in fields},
               rig={name: get(body[0], name) for name in fields})
    for name in ('SeenUpdate', 'TotalSteps', 'ResetCount', 'SubSteps', 'StepSeconds',
                 'Limits', 'MaxTravel', 'orientation', 'Display', 'Omega'):
        row['rig'][name] = get(body[0], name)
    after = engine({'op': 'call', 'target': component,
                    'function': 'GetPostProcessInstance', 'args': {}})['ReturnValue']
    if after != post:
        raise RuntimeError('Animation instance changed during capture')
    if any(row['public'][name] != row['rig'][name] for name in fields):
        raise AssertionError('Public settings and live rig settings differ')
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--idle', help='Temporarily test a listed idle animation')
    args = parser.parse_args()
    if args.output.exists() or len(processes()) != 1:
        raise RuntimeError('Need one running game and a new output path')
    original = describe()
    if original['busy']:
        raise RuntimeError('CSS is still applying the current appearance')
    if args.idle and args.idle not in {row['id'] for row in original['idles']}:
        raise ValueError('Idle is not listed for Peony')
    report = dict(original=original, cases=[], restored=False, gameplay_input=False)
    expected = original['customize']
    changed = False
    atomic(args.output, report)
    try:
        if args.idle:
            customize({'action': 'idle', 'value': args.idle})
            report['temporary_idle'] = args.idle
            time.sleep(3)
        for name, chest, glute in (
            ('firm', [2.6, .65, .6, 1], [2.5, .65, .65, 1]),
            ('natural', [2.15, .48, 1, 1], [2.1, .5, 1, 1]),
            ('disabled', [2.6, .65, .6, 0], [2.5, .65, .65, 0]),
        ):
            if describe()['customize'] != expected:
                raise RuntimeError('Customization changed before applying the diagnostic preset')
            target = copy.deepcopy(original['customize'])
            target.setdefault('values', {}).update(chest_motion=chest, glute_motion=glute)
            changed = True
            applied = customize({'action': 'apply', 'persist': False,
                                 'commands': [{'action': 'restore', 'customize': target}]})
            expected = applied['customize']
            settled(expected)
            case = dict(name=name, customize=expected, samples=[])
            report['cases'].append(case)
            for _ in range(3):
                time.sleep(.5)
                row = sample()
                for field, index in (('RegionFrequencies', 0), ('RegionDampingRatios', 1),
                                     ('RegionMotionAmounts', 2), ('RegionEnabled', 3)):
                    wanted = [chest[index]]*2 + [glute[index]]*2
                    if any(abs(float(a)-float(b)) > 1e-5
                           for a, b in zip(row['rig'][field][:4], wanted)):
                        raise AssertionError(f'{name}: {field} did not reach the rig')
                case['samples'].append(row)
                atomic(args.output, report)
            print(name, 'passed', flush=True)
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        try:
            if args.idle:
                customize({'action': 'idle', 'value': original['idle_override']})
                report['idle_restored'] = True
            if changed:
                current = describe()
                if current['customize'] != expected:
                    raise RuntimeError('Concurrent customization change; preserved it')
                customize({'action': 'apply', 'commands': [
                    {'action': 'restore', 'customize': original['customize']}]})
                report['after'] = settled(original['customize'])
                report['restored'] = True
        except Exception as error:
            report['restore_error'] = str(error)
            if 'error' not in report:
                raise
        finally:
            atomic(args.output, report)
    print('Original customization restored', flush=True)


if __name__ == '__main__':
    main()
