"""Exercise a temporary Default visibility profile, restoring the user's state."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, identity, processes, sha
from check_peony_live_presets import engine, request
sys.path.insert(0, str(ROOT/'tools'))
from css import atomic, wait_json

MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
CORE_SHA = '7004a5a913f6c8ce31fb26b64e6b055134b684269074faa7eb42079b286b3dc1'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', default=CORE_SHA, help='Expected installed and mapped core SHA-256')
    parser.add_argument('--menu', action='store_true', help='Also verify the already-open wardrobe preview')
    args = parser.parse_args()
    if len(args.core_sha256) != 64 or any(c not in '0123456789abcdef' for c in args.core_sha256):
        parser.error('--core-sha256 must be a lowercase SHA-256 digest')
    out = args.output.resolve()
    if out.exists() or len(processes()) != 1:
        raise RuntimeError('Need one running game and a new output directory')
    pids, original = processes(), describe()
    core = json.loads((MOD/'core.json').read_text())
    if sha(MOD/'cores'/core['file']) != args.core_sha256 or core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Profile correction is not the mapped core')
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    buttons = {c['id']: c.get('enabled', True) for s in cine['result']['sections'] for c in s['controls']}
    if not cine['ok'] or not buttons['take'] or original['busy'] or original['expression']['performance']:
        raise RuntimeError('Appearance or CINE is busy')
    before = json.loads((MOD/'state/state.json').read_text())
    name = 'css-diagnostic-default-' + str(time.time_ns())
    if name in before['presets'] or len(before['presets']) >= 64:
        raise RuntimeError('No free temporary profile slot')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    atomic(out/'state-before.json', before)
    atomic(out/'cine-before.json', cine)
    report = dict(passed=False, process_before=pids, core=core, core_sha256=args.core_sha256,
                  temporary_profile=name, gameplay_input=False, camera_input=False, samples={})
    expected = copy.deepcopy(before)
    player = engine({'op': 'player'})

    def geometry():
        return {key: engine({'op': 'call', 'target': target, 'function': function, 'args': {}})['ReturnValue']
                for key, target, function in (
                    ('position', player['pawn'], 'K2_GetActorLocation'),
                    ('rotation', player['pawn'], 'K2_GetActorRotation'),
                    ('control_rotation', player['controller'], 'GetControlRotation'),
                    ('view_target', player['controller'], 'GetViewTarget'))}

    initial_geometry = report['geometry_before'] = geometry()

    def guard():
        current = describe()
        if processes() != pids or engine({'op': 'player'}) != player or geometry() != initial_geometry:
            raise RuntimeError('Player or camera changed')
        if identity(current) != identity(original) or current['customize'] != original['customize'] or current['idle_override'] != original['idle_override']:
            raise RuntimeError('Appearance changed')
        if json.loads((MOD/'state/state.json').read_text()) != expected:
            raise RuntimeError('Concurrent saved-state change')

    with (out/'requests.jsonl').open('x') as log:
        def menu_items():
            def get(target, name):
                return engine({'op': 'get', 'target': target, 'property': name})
            def call(target, function, **fields):
                return engine({'op': 'call', 'target': target, 'function': function, 'args': fields})['ReturnValue']
            handler = get(player['controller'], 'User Interface Handler Component')
            display = get(handler, 'ActiveDisplayMenu')
            engine({'op': 'describe', 'target': display, 'function': 'GetDisplayMenuCharacter'})
            character = call(display, 'GetDisplayMenuCharacter')
            mesh = get(character, 'Mesh')
            engine({'op': 'describe', 'target': mesh, 'function': 'GetChildrenComponents'})
            children = engine({'op': 'call', 'target': mesh, 'function': 'GetChildrenComponents',
                               'args': {'bIncludeAllDescendants': False}})['Children']
            rows = []
            for child in children:
                if 'MeshComponent' not in child['class']:
                    continue
                owner = call(child, 'GetOwner')
                if not owner or owner == character:
                    continue
                owner_name = owner['class'].rsplit('.', 1)[-1]
                rows.append(dict(owner=owner_name, socket=call(child, 'GetAttachSocketName'),
                    actor_bHidden=int(get(owner, 'bHidden')),
                    comp_bHiddenInGame=int(get(child, 'bHiddenInGame')),
                    comp_bVisible=int(get(child, 'bVisible')),
                    rest_category={'WP_NailShotgun_C': 'sidearm', 'WP_InfiniteSeal_C': 'seal'}.get(owner_name, '')))
            if not {'sidearm', 'seal'} <= {r['rest_category'] for r in rows}:
                raise RuntimeError('Wardrobe preview lacks the expected sidearm and seal')
            log.write(json.dumps({'menu_character': character, 'menu_items': rows})+'\n'); log.flush()
            return rows

        def command(action, **fields):
            rid = str(time.time_ns())
            value = dict(id=rid, action=action, **fields)
            log.write(json.dumps({'request': value})+'\n'); log.flush()
            atomic(MOD/'request.json', value)
            status = wait_json(MOD/'runtime/status.json', lambda s: s.get('last_request') == rid)
            log.write(json.dumps({'status': status})+'\n'); log.flush()
            return rid, status

        def mutate(action, **fields):
            guard()
            target = copy.deepcopy(expected)
            if action == 'misc_reset':
                target['misc_rules'] = {}
            elif action == 'misc_mode':
                target['misc_rules'][fields['category']] = {'mode': fields['mode']}
            elif action == 'save_profile':
                target['presets'][name] = {key: copy.deepcopy(expected[key]) for key in
                    ('selections', 'walk_animation', 'animation_choices', 'misc_rules', 'doubles')}
            elif action == 'load_profile':
                target['misc_rules'] = copy.deepcopy(expected['presets'][name]['misc_rules'])
            elif action == 'delete_profile':
                del target['presets'][name]
            else:
                raise ValueError(action)
            command(action, **fields)
            wait_json(MOD/'state/state.json', lambda s: s == target, timeout=15)
            expected.clear(); expected.update(target)
            deadline = time.monotonic()+20
            while describe()['busy']:
                if time.monotonic() > deadline:
                    raise TimeoutError('Appearance did not settle')
                time.sleep(.2)
            guard()

        def inspect(label):
            guard()
            time.sleep(1.2)
            rid, status = command('inspect')
            result = wait_json(MOD/'runtime/inspection.json', lambda r: r.get('id') == rid)
            if args.menu:
                result['menu_items'] = menu_items()
            atomic(out/(label+'.json'), result)
            report['samples'][label] = dict(misc=status['misc'], items=result['misc_items']['items'])
            if args.menu:
                report['samples'][label]['menu_items'] = result['menu_items']
            return result['misc_items']['items'] + result.get('menu_items', [])

        def visibility(rows):
            return sorted((r['owner'], r['socket'], r['actor_bHidden'], r['comp_bHiddenInGame'], r['comp_bVisible'])
                          for r in rows)

        try:
            initial_items = inspect('original')
            mutate('misc_reset')
            default_items = inspect('default-before')
            assert report['samples']['default-before']['misc']['rules'] == 0
            mutate('save_profile', name=name)
            mutate('misc_mode', category='sidearm', mode='hidden')
            mutate('misc_mode', category='seal', mode='hidden')
            hidden = inspect('hidden')
            affected = [r for r in hidden if r['rest_category'] in ('sidearm', 'seal')]
            assert {r['rest_category'] for r in affected} == {'sidearm', 'seal'}, 'Required equipped props absent'
            assert all(r['comp_bHiddenInGame'] == 1 for r in affected), 'Overrides did not reach props'
            assert visibility(hidden) != visibility(default_items), 'No actual visibility change to verify'
            mutate('load_profile', name=name)
            restored = inspect('profile-loaded')
            assert report['samples']['profile-loaded']['misc']['rules'] == 0
            assert visibility(restored) == visibility(default_items), 'Default profile did not restore component visibility'
            mutate('load_profile', name=name)
            repeated = inspect('profile-reloaded')
            assert visibility(repeated) == visibility(default_items), 'Repeated profile load changed visibility'
            report['checks_passed'] = True
        except Exception as error:
            report['error'] = repr(error)
            raise
        finally:
            try:
                guard()
                mutate('misc_reset')
                for category, rule in before['misc_rules'].items():
                    mutate('misc_mode', category=category, mode=rule['mode'])
                if name in expected['presets']:
                    mutate('delete_profile', name=name)
                after_items = inspect('original-restored')
                report['saved_state_equal'] = expected == before
                report['all_visibility_flags_equal'] = visibility(after_items) == visibility(initial_items)
                report['visibility_scope'] = 'world component/actor flags'
                report['visibility_restored'] = report['all_visibility_flags_equal']
                if args.menu:
                    report['visibility_scope'] = 'preview component/actor flags; world effective visibility while preview is open'
                    initial_sample = report['samples']['original']
                    final_sample = report['samples']['original-restored']
                    def effective(rows):
                        return sorted((r['owner'], r['socket'], bool(r['comp_bVisible'])
                                       and not r['comp_bHiddenInGame'] and not r['actor_bHidden']) for r in rows)
                    report['menu_flags_equal'] = visibility(initial_sample['menu_items']) == visibility(final_sample['menu_items'])
                    report['world_effective_visibility_equal'] = effective(initial_sample['items']) == effective(final_sample['items'])
                    report['visibility_restored'] = report['menu_flags_equal'] and report['world_effective_visibility_equal']
                report['geometry_after'] = geometry()
                report['cine_model_equal'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
                report['process_after'] = processes()
                report['passed'] = bool(report.get('checks_passed') and report['saved_state_equal']
                    and report['visibility_restored'] and report['cine_model_equal'] and report['process_after'] == pids
                    and report['geometry_after'] == initial_geometry)
            except Exception as error:
                report['restore_error'] = repr(error)
            atomic(out/'state-after.json', json.loads((MOD/'state/state.json').read_text()))
            atomic(out/'result.json', report)
    print(json.dumps({k:v for k,v in report.items() if k not in ('samples', 'geometry_before', 'geometry_after')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
