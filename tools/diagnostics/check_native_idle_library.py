"""Exercise native idle carriers through CSS and inspect the live player graph."""
import argparse
import json
import math
from pathlib import Path
import shutil
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, sha, processes
from check_peony_live_presets import customize, engine, request

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
MANIFEST = ROOT / 'build/eve-surface-package1/metadata/CSS_SharedAssets_P/MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json'
SKELETON = '/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton'
BONES = ('pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r')


def asset_path(value):
    return value['name'].split(' ', 1)[1] if value else ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--core-sha256', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    pids, original = processes(), describe()
    if out.exists() or len(pids) != 1 or original['busy'] or original['expression']['performance'] or original['idle_override']:
        raise RuntimeError('Need one settled player, no active performance/override and a new output')
    if original['shell'] != 'CharacterId.Player.Shell.Genessa':
        raise RuntimeError('This fixture requires Faithful Genessa')
    core = json.loads((MOD / 'core.json').read_text())
    if sha(MOD / 'cores' / core['file']) != args.core_sha256 or core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Unexpected installed or mapped core')
    before = json.loads((MOD / 'state/state.json').read_text())
    native_custom = before['remembered_custom']['css.original_shells/genessa']
    if before['remembered_custom']['css.original_shells'] != native_custom:
        raise RuntimeError('Native outfit-level memory differs; fixture cannot restore it')
    library = json.loads(MANIFEST.read_text())['catalog']['animation_library']
    entries = [(e, a) for e in library['entries'] for a in e['assets']
               if a.get('skeleton') == SKELETON and a.get('idle_blend_space')]
    if len(entries) != 60 or len({e['id'] for e, a in entries}) != 60:
        raise RuntimeError('Expected exactly 60 distinct native idle choices')
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok'] or not next(c for s in cine['result']['sections'] for c in s['controls'] if c['id'] == 'take').get('enabled', True):
        raise RuntimeError('CINE must be between takes')
    player = engine({'op': 'player'})

    def get(target, prop):
        return engine({'op': 'get', 'target': target, 'property': prop})

    def call(target, function, **fields):
        return engine({'op': 'call', 'target': target, 'function': function, 'args': fields})['ReturnValue']

    def geometry():
        return dict(position=call(player['pawn'], 'K2_GetActorLocation'),
                    rotation=call(player['pawn'], 'K2_GetActorRotation'),
                    view_target=call(player['controller'], 'GetViewTarget'),
                    control_rotation=call(player['controller'], 'GetControlRotation'))

    initial_geometry = geometry()
    target, expected, override = identity(original), original['customize'], ''

    def guard():
        current = describe()
        if processes() != pids or engine({'op': 'player'}) != player:
            raise RuntimeError('Player or process changed')
        if identity(current) != target or current['customize'] != expected or current['idle_override'] != override:
            raise RuntimeError('Concurrent appearance or idle change')
        if geometry() != initial_geometry:
            raise RuntimeError('Player or camera moved')
        keys = engine({'op': 'input.keys', 'target': player['controller'],
                       'keys': ['W', 'A', 'S', 'D', 'SpaceBar', 'LeftControl', 'LeftMouseButton', 'RightMouseButton']})
        if any(keys.values()):
            raise RuntimeError('Concurrent gameplay input')
        menu = engine({'op': 'menu.status'})
        if menu['menu_open'] or menu['game_menu_open']:
            raise RuntimeError('Menu opened')

    guard()
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    atomic(out / 'cine-before.json', cine)
    report = dict(passed=False, cases=[], original=original, core=core,
                  core_sha256=args.core_sha256, process_before=pids,
                  manifest_sha256=sha(MANIFEST), geometry_before=initial_geometry,
                  gameplay_input=False, camera_input=False,
                  scope='Native Genessa idle selection, live carrier bindings and sampled bone positions. No visual, contact, full-clip or gameplay-interruption acceptance.')
    atomic(out / 'result.json', report)
    changed = False
    try:
        changed = True
        customize({'action': 'wear', 'outfit': 'css.original_shells', 'variant': 'genessa',
                   'customize': native_custom, 'persist': False})
        target, expected = (original['shell'], 'css.original_shells', 'genessa'), native_custom
        native = settled(target, custom=expected)
        listed = {e['id'] for e in native['idles']}
        missing = sorted({e['id'] for e, a in entries} - listed)
        report['missing_choices'] = missing
        if missing:
            raise RuntimeError('Native menu does not expose every idle: ' + repr(missing))
        mesh = get(player['pawn'], 'Mesh')
        asset = call(mesh, 'GetSkeletalMeshAsset')
        if asset_path(get(asset, 'Skeleton')) != SKELETON:
            raise RuntimeError('Selected appearance is not the native human skeleton')
        report['native_asset'] = asset
        report['signatures'] = {n: engine({'op': 'describe', 'target': mesh, 'function': n})
                                for n in ('DoesSocketExist', 'GetSocketLocation')}
        for bone in BONES:
            if not call(mesh, 'DoesSocketExist', InSocketName=bone):
                raise RuntimeError('Missing sampled bone ' + bone)
        guard()
        for entry, alternative in entries:
            guard()
            row = dict(id=entry['id'], name=entry['name'], expected=alternative, passed=False)
            report['cases'].append(row)
            override = entry['id']
            customize({'action': 'idle', 'value': override})
            deadline = time.monotonic() + 12
            while True:
                animation = json.loads((MOD / 'runtime/status.json').read_text())['animation']
                shared = animation.get('shared', {}).get('idle', {})
                anim = call(mesh, 'GetAnimInstance')
                active = get(anim, 'ActiveBlendSpace')
                enabled = get(anim, 'UseActiveBlendspace')
                if shared.get('asset') == alternative['asset'] and shared.get('available') and enabled and asset_path(active) == alternative['idle_blend_space']:
                    break
                if time.monotonic() > deadline:
                    row.update(animation=animation, active=active, enabled=enabled)
                    raise TimeoutError('Native carrier did not engage for ' + override)
                time.sleep(.1)
            row.update(animation=animation, anim=anim, carrier=active,
                       carrier_samples=get(active, 'SampleData'), poses=[])
            for index in range(3):
                time.sleep(.25)
                pose = {bone: call(mesh, 'GetSocketLocation', InSocketName=bone) for bone in BONES}
                if not all(math.isfinite(v) for xyz in pose.values() for v in xyz.values()):
                    raise RuntimeError('Nonfinite bone position')
                row['poses'].append(dict(time_ns=time.time_ns(), bones=pose))
            guard()
            if get(anim, 'ActiveBlendSpace') != active or not get(anim, 'UseActiveBlendspace') or call(mesh, 'GetSkeletalMeshAsset') != asset:
                raise RuntimeError('Carrier or body changed during sample')
            row['max_sampled_displacement_cm'] = max(
                math.dist(tuple(row['poses'][0]['bones'][bone].values()), tuple(pose['bones'][bone].values()))
                for bone in BONES for pose in row['poses'][1:])
            row['passed'] = True
            atomic(out / 'result.json', report)
            print(len(report['cases']), entry['id'], 'pass', flush=True)
        report['cases_passed'] = True
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        try:
            if changed:
                guard()
                customize({'action': 'idle', 'value': original['idle_override']})
                override = original['idle_override']
                customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                           'customize': original['customize'], 'persist': True})
                target, expected = identity(original), original['customize']
                settled(target, custom=expected)
                time.sleep(1.2)
                guard()
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(out / 'state-after.json', after)
            cine_after = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
            atomic(out / 'cine-after.json', cine_after)
            report.update(saved_state_equal=before == after, cine_model_equal=cine['result'] == cine_after['result'],
                          geometry_after=geometry(), process_after=processes())
            report['restored'] = (report['saved_state_equal'] and report['cine_model_equal'] and
                                  report['geometry_after'] == initial_geometry and processes() == pids)
            report['passed'] = bool(report.get('cases_passed') and report['restored'])
        except Exception as error:
            report['restore_error'] = repr(error)
            raise
        finally:
            atomic(out / 'result.json', report)
    if not report['passed']:
        raise RuntimeError('Library validation or restoration failed')


if __name__ == '__main__':
    main()
