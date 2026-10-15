"""Check preview settings and secondary-rig recovery across release appearances."""
import argparse
import json
import math
from pathlib import Path
import shutil
import time
import traceback

from capture_eve_surface_variant import ROOT, describe, identity, settled, processes, sha, atomic
from check_peony_live_presets import customize, engine, request, GAME
from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import MOD, geometry_restored
from css import wait_json
import css_capture


def finite(value):
    if isinstance(value, dict):
        return all(finite(v) for v in value.values())
    if isinstance(value, list):
        return all(finite(v) for v in value)
    return not isinstance(value, float) or math.isfinite(value)


def compare_settings(world, preview):
    assert world['asset'] == preview['asset'], 'Preview mesh differs from world mesh'
    assert world['post']['class'] == preview['post']['class'], 'Preview graph differs from world graph'
    assert world['public'] == preview['public'], 'Preview body settings differ'
    assert world['morphs'] == preview['morphs'], 'Preview morph values differ'
    def hidden_sections(lods):
        # UE 5.6.1 IsMaterialSectionShown treats an absent mask entry as visible.
        return {(lod, index) for lod, row in enumerate(lods)
                for index, hidden in enumerate(row['HiddenMaterials']) if hidden}
    assert hidden_sections(world['lod_info']) == hidden_sections(preview['lod_info']), 'Preview section visibility differs'


def verify_updates(before, after):
    assert before['post'] == after['post'], 'Post-process owner changed between counter samples'
    assert set(before['rigs']) == set(after['rigs']), 'Rig identities changed between samples'
    measured = []
    for name, a in before['rigs'].items():
        b = after['rigs'][name]
        assert a['object'] == b['object'] and finite(b['values'])
        assert a['values'].get('Enabled', True) == b['values'].get('Enabled', True)
        if not a['values'].get('Enabled', True):
            continue
        counters = [key for key in ('TotalSteps', 'SeenUpdate', 'UpdateSerial') if key in a['values']]
        for key in counters:
            assert b['values'][key] > a['values'][key], f'{name}: {key} stopped updating'
        if counters:
            measured.append(name)
    assert measured, 'No enabled secondary rig has an observed update counter'
    return measured


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--variants', nargs='+', choices=[v for values in OUTFITS.values() for v in values])
    args = parser.parse_args()
    out = args.output.resolve()
    assert not out.exists()
    original, pids = describe(), processes()
    assert len(pids) == 1 and not original['busy'] and not original['idle_override']
    assert not original['expression']['performance'] and not any(engine({'op': 'menu.status'}).values())
    core = json.loads((MOD / 'core.json').read_text())
    assert sha(MOD / 'cores' / core['file']) == args.core_sha256
    assert core['file'] in Path(f'/proc/{pids[0]}/maps').read_text()
    state = json.loads((MOD / 'state/state.json').read_text())
    memories = state['remembered_custom']
    restore_variants = {}
    for outfit, variants in OUTFITS.items():
        assert all(outfit + '/' + v in memories for v in variants)
        restore_variants[outfit] = next(v for v in variants if memories[outfit + '/' + v] == memories[outfit])
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result']
    buttons = {c['id']: c.get('enabled', True) for s in cine['sections'] for c in s['controls']}
    assert buttons.get('record') and buttons.get('capture')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', state)
    atomic(out / 'cine-before.json', cine)
    log = (out / 'requests.jsonl').open('x')
    signatures = set()

    def send(value):
        log.write(json.dumps({'at_ns': time.monotonic_ns(), 'request': value}) + '\n'); log.flush()
        result = engine(value)
        log.write(json.dumps({'at_ns': time.monotonic_ns(), 'result': result}) + '\n'); log.flush()
        return result

    def get(target, name):
        return send({'op': 'get', 'target': target, 'property': name})

    def call(target, name, **fields):
        key = json.dumps(target, sort_keys=True), name
        if key not in signatures:
            send({'op': 'describe', 'target': target, 'function': name})
            signatures.add(key)
        return send({'op': 'call', 'target': target, 'function': name, 'args': fields}).get('ReturnValue')

    player = send({'op': 'player'})

    def geometry():
        return {k: call(player[t], f) for k, t, f in (
            ('position', 'pawn', 'K2_GetActorLocation'), ('rotation', 'pawn', 'K2_GetActorRotation'),
            ('control_rotation', 'controller', 'GetControlRotation'), ('view_target', 'controller', 'GetViewTarget'))}

    def resources():
        health = get(player['pawn'], 'HealthComponent')
        assert not call(health, 'IsDeadOrDying')
        values = get(health, 'HealthSet')
        return {n: get(values, n) for n in ('Health', 'Resolve')}

    def keys():
        return send({'op': 'input.keys', 'target': player['controller'], 'keys':
                     ['W', 'A', 'S', 'D', 'F', 'Q', 'C', 'SpaceBar', 'LeftControl', 'LeftShift',
                      'LeftMouseButton', 'RightMouseButton']})

    def sample(character, settings=True):
        mesh = get(character, 'Mesh')
        asset = call(mesh, 'GetSkeletalMeshAsset')
        post = call(mesh, 'GetPostProcessInstance')
        assert post
        props = send({'op': 'properties', 'target': post, 'inherited': True})
        public = {p['name']: get(post, p['name']) for p in props
                  if settings and p['name'].startswith('CSSBody') and p['name'] != 'CSSBodyResetEpoch'}
        rigs = {}
        for prop in props:
            if not prop['name'].startswith('AnimGraphNode_ControlRig'):
                continue
            node = get(post, prop['name'])
            rig = node.get('ControlRig')
            if not rig:
                continue
            names = {p['name'] for p in send({'op': 'properties', 'target': rig, 'inherited': True})}
            fields = ('Enabled', 'TotalSteps', 'SeenUpdate', 'UpdateSerial', 'ResetCount', 'Limits',
                      'MaxTravel', 'Display', 'Omega', 'orientation')
            rigs[prop['name']] = dict(object=rig, values={n: get(rig, n) for n in fields if n in names})
        morphs = {}
        if settings:
            assert public.get('CSSBodyGeometryJson'), 'Expected authored body geometry'
            for name in json.loads(public['CSSBodyGeometryJson'])['morphs']:
                morphs[name] = call(mesh, 'GetMorphTarget', MorphTargetName=name)
        row = dict(character=character, mesh=mesh, asset=asset, post=post, public=public, rigs=rigs,
                   morphs=morphs, lod_info=get(mesh, 'LODInfo') if settings else None)
        assert call(mesh, 'GetPostProcessInstance') == post and call(mesh, 'GetSkeletalMeshAsset') == asset
        assert finite(row), 'Nonfinite sampled secondary state'
        return row

    baseline, original_resources = geometry(), resources()
    target, expected, idle = identity(original), original['customize'], original['idle_override']
    report = dict(passed=False, original=original, player=player, core=core, core_sha256=args.core_sha256,
                  geometry_before=baseline, resources_before=original_resources, cases=[],
                  gameplay_input=False, camera_input=False, scope='Preview settings parity and sampled rig update recovery')
    initial_animation = json.loads((MOD / 'runtime/status.json').read_text())['animation']
    report['animation_before'] = initial_animation
    atomic(out / 'result.json', report)
    owned_menu = False

    def guard(menu=False):
        current = describe()
        assert processes() == pids and send({'op': 'player'}) == player
        assert identity(current) == target and current['customize'] == expected and current['idle_override'] == idle
        assert not current['busy'] and not current['expression']['performance']
        assert resources() == original_resources and not any(keys().values())
        assert json.loads((MOD / 'core.json').read_text()) == core
        if not menu:
            assert not any(send({'op': 'menu.status'}).values())
            assert geometry_restored(baseline, geometry())

    def close_owned_menu():
        nonlocal owned_menu
        if owned_menu:
            assert processes() == pids and send({'op': 'player'}) == player
            result = request(GAME, {'op': 'menu.close'}); assert result['ok'], result
            time.sleep(1.5)
            assert not any(send({'op': 'menu.status'}).values())
            owned_menu = False

    try:
        guard()
        customize({'action': 'idle', 'value': 'eve'})
        idle = 'eve'
        for outfit, variants in OUTFITS.items():
            for variant in variants:
                if args.variants and variant not in args.variants:
                    continue
                guard()
                target = original['shell'], outfit, variant
                expected = memories[outfit + '/' + variant]
                customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                           'customize': expected, 'persist': False})
                settled(target, custom=expected)
                time.sleep(.5)
                guard()
                row = dict(outfit=outfit, variant=variant, passed=False)
                report['cases'].append(row)
                row['world_before'] = sample(player['pawn'])
                atomic(out / 'result.json', report)
                owned_menu = True
                result = request(GAME, {'op': 'menu.open'}); assert result['ok'], result
                time.sleep(2)
                # CSSX opens its own tab; CSS is after the three native tabs.
                css_capture.command('inventory_select', index=3)
                wait_json(MOD / 'runtime/status.json', lambda s: s['inventory']['active'])
                time.sleep(1)
                guard(menu=True)
                assert send({'op': 'menu.status'})['game_menu_open']
                handler = get(player['controller'], 'User Interface Handler Component')
                character = call(get(handler, 'ActiveDisplayMenu'), 'GetDisplayMenuCharacter')
                assert character and character != player['pawn']
                row['preview_before'] = sample(character)
                compare_settings(row['world_before'], row['preview_before'])
                time.sleep(.5)
                row['preview_after'] = sample(character, settings=False)
                row['preview_updating_rigs'] = verify_updates(row['preview_before'], row['preview_after'])
                close_owned_menu()
                guard()
                row['world_after'] = sample(player['pawn'])
                compare_settings(row['world_before'], row['world_after'])
                time.sleep(.5)
                row['world_after_later'] = sample(player['pawn'], settings=False)
                row['world_updating_rigs'] = verify_updates(row['world_after'], row['world_after_later'])
                assert not call(player['controller'], 'IsMoveInputIgnored')
                assert not call(player['controller'], 'IsLookInputIgnored')
                row['passed'] = True
                atomic(out / 'result.json', report)
                print(outfit, variant, 'preview and world recovery pass', flush=True)
        report['cases_passed'] = True
    except Exception as exc:
        report['error'] = repr(exc)
        report['traceback'] = traceback.format_exc()
    finally:
        try:
            close_owned_menu()
            guard()
            for outfit, variant in restore_variants.items():
                customize({'action': 'wear', 'outfit': outfit, 'variant': variant,
                           'customize': memories[outfit], 'persist': False})
                settled((original['shell'], outfit, variant), custom=memories[outfit])
            customize({'action': 'idle', 'value': original['idle_override']})
            customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                       'customize': original['customize'], 'persist': True})
            settled(identity(original), custom=original['customize'])
            target, expected, idle = identity(original), original['customize'], original['idle_override']
            time.sleep(1)
            guard()
            final_status = wait_json(MOD / 'runtime/status.json', lambda s:
                s['animation']['engaged'] == initial_animation['engaged']
                and s['animation']['shared']['idle'] == initial_animation['shared']['idle'], timeout=20)
            report['animation_after'] = final_status['animation']
            report['restoration'] = dict(
                state=json.loads((MOD / 'state/state.json').read_text()) == state,
                cine=request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine,
                geometry=geometry_restored(baseline, geometry()), resources=resources() == original_resources,
                input=not call(player['controller'], 'IsMoveInputIgnored') and not call(player['controller'], 'IsLookInputIgnored'),
                process=processes() == pids, keys=not any(keys().values()))
            report['passed'] = bool(report.get('cases_passed') and all(report['restoration'].values()))
        except Exception as exc:
            report['restore_error'] = repr(exc)
            report['restore_traceback'] = traceback.format_exc()
        atomic(out / 'state-after.json', json.loads((MOD / 'state/state.json').read_text()))
        atomic(out / 'result.json', report)
        log.close()
    print(json.dumps({k:v for k,v in report.items() if k in ('passed', 'error', 'restore_error', 'restoration')}))
    assert report['passed']


if __name__ == '__main__':
    main()
