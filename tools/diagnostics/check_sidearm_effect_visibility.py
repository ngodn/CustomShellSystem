"""Compare stowed Burn rendering under Default, mesh-only hiding and Only when in use."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, describe, processes, sha
from check_peony_live_presets import engine, request
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic, wait_json
import css_capture

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


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
    if before['misc_rules'].get('sidearm') != {'mode': 'in_use'}:
        raise RuntimeError('Need the existing Only when in use sidearm rule')
    expected = copy.deepcopy(before)
    original = describe()
    player = engine({'op': 'player'})
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok'] or not next(c for s in cine['result']['sections'] for c in s['controls'] if c['id'] == 'take').get('enabled', True):
        raise RuntimeError('CINE must be between takes')

    def get(target, prop):
        return engine({'op': 'get', 'target': target, 'property': prop})

    def call(target, function, **fields):
        return engine({'op': 'call', 'target': target, 'function': function, 'args': fields}).get('ReturnValue')

    def geometry():
        return {key: call(target, function) for key, target, function in (
            ('position', player['pawn'], 'K2_GetActorLocation'),
            ('rotation', player['pawn'], 'K2_GetActorRotation'),
            ('view_target', player['controller'], 'GetViewTarget'),
            ('control_rotation', player['controller'], 'GetControlRotation'))}

    baseline_geometry = geometry()
    wc = get(player['pawn'], 'WeaponsComponent')
    equipped = get(wc, 'EquippedWeapons')
    weapon = next(e['value'] for e in equipped['$map'] if e['key']['TagName'] == 'Weapon.Slot.Sidearm')
    if not weapon['class'].endswith('.WP_NailShotgun_C'):
        raise RuntimeError('Need the equipped Naylshotte')
    effects = get(weapon, 'BPC_WeaponEffects')
    effect = get(effects, 'SpawnedPermanent')
    if not effect or call(effect, 'GetOwner') != weapon:
        raise RuntimeError('Need an existing weapon-owned permanent effect')
    mesh_class = engine({'op': 'find', 'path': '/Script/Engine.MeshComponent'})
    engine({'op': 'describe', 'target': weapon, 'function': 'K2_GetComponentsByClass'})
    meshes = call(weapon, 'K2_GetComponentsByClass', ComponentClass=mesh_class)
    if not meshes or len(meshes) > 16:
        raise RuntimeError('Unexpected weapon mesh count')
    signatures = {}
    for target, function in ((effect, 'WasRecentlyRendered'), (weapon, 'SetActorHiddenInGame'),
                             *((mesh, 'SetHiddenInGame') for mesh in meshes)):
        signatures[target['name'] + ':' + function] = engine({'op': 'describe', 'target': target, 'function': function})
    render_sig = signatures[effect['name'] + ':WasRecentlyRendered']
    if set(render_sig) != {'Tolerance', 'ReturnValue'} or render_sig['Tolerance']['size'] != 4:
        raise RuntimeError('Unexpected rendering observation signature')
    for mesh in meshes:
        sig = signatures[mesh['name'] + ':SetHiddenInGame']
        if set(sig) != {'NewHidden', 'bPropagateToChildren'} or any(v['size'] != 1 for v in sig.values()):
            raise RuntimeError('Unexpected mesh setter signature')

    def infusion():
        return dict(tags=get(weapon, 'WeaponEffectTags'), effect=get(effects, 'SpawnedPermanent'),
                    asset=get(effect, 'Asset'), active=get(effect, 'bIsActive'),
                    hidden=get(effect, 'bHiddenInGame'), visible=get(effect, 'bVisible'))

    original_infusion = infusion()
    if 'PS_Nailshot_Burn' not in original_infusion['asset']['name'] or not original_infusion['active']:
        raise RuntimeError('Need the active permanent Burn effect')

    def flags():
        return dict(actor_hidden=get(weapon, 'bHidden'), meshes=[dict(object=m,
                    hidden=get(m, 'bHiddenInGame'), visible=get(m, 'bVisible')) for m in meshes])

    original_flags = flags()
    if not original_flags['actor_hidden'] or not all(m['hidden'] for m in original_flags['meshes']):
        raise RuntimeError('Expected stowed actor and meshes hidden at start')

    def guard():
        if processes() != pids or engine({'op': 'player'}) != player or geometry() != baseline_geometry:
            raise RuntimeError('Player or camera changed')
        if json.loads((MOD / 'state/state.json').read_text()) != expected:
            raise RuntimeError('Concurrent saved-state change')
        current = describe()
        if any(current[k] != original[k] for k in ('shell', 'outfit', 'variant', 'customize', 'idle_override')) or current['busy'] or current['expression']['performance']:
            raise RuntimeError('Appearance changed or busy')
        if get(wc, 'EquippedWeapons') != equipped or infusion() != original_infusion:
            raise RuntimeError('Equipped weapon or infusion changed')
        menu = engine({'op': 'menu.status'})
        if menu['menu_open'] or menu['game_menu_open']:
            raise RuntimeError('Menu opened')

    guard()
    out.mkdir(parents=True)
    css_capture.MEDIA = out
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', before)
    atomic(out / 'signatures.json', signatures)
    report = dict(passed=False, process_before=pids, core=core, core_sha256=args.core_sha256,
                  gameplay_input=False, camera_input=False, original_infusion=original_infusion,
                  original_flags=original_flags, geometry_before=baseline_geometry, cases=[])
    with (out / 'requests.jsonl').open('x') as log:
        def mode(value):
            guard()
            command = dict(id=str(time.time_ns()), action='misc_mode', category='sidearm', mode=value)
            log.write(json.dumps(command) + '\n'); log.flush()
            atomic(MOD / 'request.json', command)
            wait_json(MOD / 'runtime/status.json', lambda s: s.get('last_request') == command['id'])
            expected['misc_rules']['sidearm'] = {'mode': value}
            wait_json(MOD / 'state/state.json', lambda s: s == expected)

        def sample(label):
            guard()
            time.sleep(.7)
            row = dict(label=label, flags=flags(), infusion=infusion(), recently_rendered=[])
            for _ in range(3):
                row['recently_rendered'].append(call(effect, 'WasRecentlyRendered', Tolerance=.2))
                time.sleep(.25)
            row['screenshot'] = str(css_capture.shot(label))
            guard()
            report['cases'].append(row)
            atomic(out / 'result.json', report)

        try:
            sample('initial-hidden')
            mode('default')
            sample('default-visible')
            for mesh in meshes:
                log.write(json.dumps({'mesh_only_hidden': mesh}) + '\n'); log.flush()
                call(mesh, 'SetHiddenInGame', NewHidden=True, bPropagateToChildren=False)
            sample('mesh-only-hidden')
            mode('in_use')
            sample('owner-hidden')
            mode('default')
            sample('default-again')
            mode('in_use')
            sample('hidden-again')
            cases = {r['label']: r for r in report['cases']}
            report['render_response_matches'] = all(
                all(r['recently_rendered']) if label in ('default-visible', 'mesh-only-hidden', 'default-again')
                else not any(r['recently_rendered']) for label, r in cases.items())
            report['mesh_only_control_valid'] = not cases['mesh-only-hidden']['flags']['actor_hidden'] and all(
                m['hidden'] for m in cases['mesh-only-hidden']['flags']['meshes'])
        except Exception as error:
            report['error'] = repr(error)
        finally:
            try:
                guard()
                if expected != before:
                    mode(before['misc_rules']['sidearm']['mode'])
                time.sleep(.7)
                report['state_restored'] = json.loads((MOD / 'state/state.json').read_text()) == before
                report['flags_restored'] = flags() == original_flags
                report['infusion_preserved'] = infusion() == original_infusion
                report['geometry_after'] = geometry()
                report['cine_restored'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
                report['process_after'] = processes()
                report['passed'] = bool(report.get('render_response_matches') and report.get('mesh_only_control_valid') and
                    report['state_restored'] and report['flags_restored'] and report['infusion_preserved'] and
                    report['cine_restored'] and geometry() == baseline_geometry and processes() == pids)
            except Exception as error:
                report['restore_error'] = repr(error)
            atomic(out / 'result.json', report)
    print(json.dumps({k: v for k, v in report.items() if k not in ('original_infusion', 'original_flags', 'cases', 'geometry_before', 'geometry_after')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
