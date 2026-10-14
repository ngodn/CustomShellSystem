"""Clear hidden item actor flags, observe CSS enforcement, and restore the original state."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, processes, sha
from check_peony_live_presets import engine
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic, wait_json

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
TARGETS = {'WP_NailShotgun_C': 'sidearm', 'WP_InfiniteSeal_C': 'seal', 'WP_Genessa_Catalyst_C': 'accessories'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    pids = processes()
    if out.exists() or len(pids) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    core = json.loads((MOD / 'core.json').read_text())
    if sha(MOD / 'cores' / core['file']) != args.core_sha256 or not any(
            line.rstrip().endswith('/' + core['file']) for line in Path(f'/proc/{pids[0]}/maps').read_text().splitlines()):
        raise RuntimeError('Expected core is not installed and mapped')
    before = json.loads((MOD / 'state/state.json').read_text())
    expected = copy.deepcopy(before)
    original = describe()
    player = engine({'op': 'player'})

    def get(target, prop):
        return engine({'op': 'get', 'target': target, 'property': prop})

    def call(target, function, **fields):
        return engine({'op': 'call', 'target': target, 'function': function, 'args': fields})

    def geometry():
        return {key: call(target, function)['ReturnValue'] for key, target, function in (
            ('position', player['pawn'], 'K2_GetActorLocation'),
            ('rotation', player['pawn'], 'K2_GetActorRotation'),
            ('view_target', player['controller'], 'GetViewTarget'),
            ('control_rotation', player['controller'], 'GetControlRotation'))}

    baseline_geometry = geometry()

    def guard():
        if processes() != pids or engine({'op': 'player'}) != player or geometry() != baseline_geometry:
            raise RuntimeError('Player or camera changed')
        if json.loads((MOD / 'state/state.json').read_text()) != expected:
            raise RuntimeError('Concurrent saved-state change')
        current = describe()
        if any(current[k] != original[k] for k in ('shell', 'outfit', 'variant', 'customize', 'idle_override')):
            raise RuntimeError('Appearance changed')
        if current['busy'] or current['expression']['performance']:
            raise RuntimeError('Appearance or facial performance is active')
        menu = engine({'op': 'menu.status'})
        if menu['menu_open'] or menu['game_menu_open']:
            raise RuntimeError('Menu is open')

    guard()
    mesh = get(player['pawn'], 'Mesh')
    engine({'op': 'describe', 'target': mesh, 'function': 'GetChildrenComponents'})
    children = call(mesh, 'GetChildrenComponents', bIncludeAllDescendants=True)['Children']
    if len(children) > 512:
        raise RuntimeError('Attachment scan exceeds this fixture limit')
    targets = {}
    for component in children:
        if 'MeshComponent' not in component['class']:
            continue
        owner = call(component, 'GetOwner')['ReturnValue']
        if not owner or owner == player['pawn']:
            continue
        name = owner['class'].rsplit('.', 1)[-1]
        if name in TARGETS and name not in targets:
            socket = call(component, 'GetAttachSocketName')['ReturnValue']
            if 'stowed' not in str(socket).lower():
                continue
            signature = engine({'op': 'describe', 'target': owner, 'function': 'SetActorHiddenInGame'})
            if set(signature) != {'bNewHidden'} or signature['bNewHidden']['size'] != 1:
                raise RuntimeError('Unexpected actor visibility setter signature')
            targets[name] = dict(owner=owner, component=component, socket=socket, category=TARGETS[name])
    if set(targets) != set(TARGETS):
        raise RuntimeError('Expected stowed item actors were not all found')

    def flags(target):
        return dict(actor_hidden=bool(get(target['owner'], 'bHidden')),
                    mesh_hidden=bool(get(target['component'], 'bHiddenInGame')),
                    mesh_visible=bool(get(target['component'], 'bVisible')))

    originals = {name: flags(target) for name, target in targets.items()}
    if any(not row['actor_hidden'] or not row['mesh_hidden'] for row in originals.values()):
        raise RuntimeError('Start with all three actors and meshes hidden under existing rules')
    if any(before['misc_rules'].get(cat, {}).get('mode') != 'in_use' for cat in TARGETS.values()):
        raise RuntimeError('Fixture expects existing Only when in use rules')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    report = dict(passed=False, core=core, core_sha256=args.core_sha256, process_before=pids,
                  geometry_before=baseline_geometry, original_flags=originals, targets=targets,
                  movement_input=False, camera_input=False, injected_actor_flags=True, samples={})
    changed = set()
    with (out / 'requests.jsonl').open('x') as log:
        def set_hidden(name, value):
            target = targets[name]
            if call(target['component'], 'GetOwner')['ReturnValue'] != target['owner']:
                raise RuntimeError('Item owner changed')
            log.write(json.dumps({'actor': name, 'SetActorHiddenInGame': value}) + '\n'); log.flush()
            call(target['owner'], 'SetActorHiddenInGame', bNewHidden=value)

        def mode(category, value):
            guard()
            rid = str(time.time_ns())
            command = dict(id=rid, action='misc_mode', category=category, mode=value)
            log.write(json.dumps({'command': command}) + '\n'); log.flush()
            atomic(MOD / 'request.json', command)
            wait_json(MOD / 'runtime/status.json', lambda s: s.get('last_request') == rid)
            expected['misc_rules'][category] = {'mode': value}
            wait_json(MOD / 'state/state.json', lambda s: s == expected)

        try:
            for name, target in targets.items():
                guard()
                changed.add(name)
                set_hidden(name, False)
                start = time.monotonic()
                samples = []
                while time.monotonic() - start < 2:
                    row = flags(target)
                    samples.append(dict(elapsed_seconds=time.monotonic() - start, **row))
                    if row['actor_hidden'] and row['mesh_hidden']:
                        break
                    time.sleep(.05)
                report['samples'][name] = samples
                if not samples[-1]['actor_hidden'] or not samples[-1]['mesh_hidden']:
                    report.setdefault('unrepaired', []).append(name)
                set_hidden(name, originals[name]['actor_hidden'])
                changed.remove(name)
            mode('sidearm', 'default')
            target = targets['WP_NailShotgun_C']
            set_hidden('WP_NailShotgun_C', False)
            time.sleep(.5)
            report['default_control'] = flags(target)
            if report['default_control']['actor_hidden']:
                raise RuntimeError('Default mode unexpectedly enforced actor hiding')
            mode('sidearm', before['misc_rules']['sidearm']['mode'])
            report['checks_passed'] = not report.get('unrepaired')
        except Exception as error:
            report['error'] = repr(error)
        finally:
            try:
                guard()
                if expected['misc_rules'] != before['misc_rules']:
                    mode('sidearm', before['misc_rules']['sidearm']['mode'])
                for name in changed:
                    set_hidden(name, originals[name]['actor_hidden'])
                time.sleep(.5)
                report['final_flags'] = {name: flags(target) for name, target in targets.items()}
                report['flags_restored'] = report['final_flags'] == originals
                report['state_restored'] = json.loads((MOD / 'state/state.json').read_text()) == before
                report['geometry_after'] = geometry()
                report['process_after'] = processes()
                report['passed'] = bool(report.get('checks_passed') and report['flags_restored'] and
                    report['state_restored'] and report['geometry_after'] == baseline_geometry and processes() == pids)
            except Exception as error:
                report['restore_error'] = repr(error)
            atomic(out / 'result.json', report)
    print(json.dumps({k: v for k, v in report.items() if k not in
                     ('targets', 'samples', 'original_flags', 'final_flags', 'geometry_before', 'geometry_after')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
