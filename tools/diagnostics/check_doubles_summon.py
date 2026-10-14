"""Send one normal shell-ability press and record owned doubles until they expire."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, describe, processes, sha
from check_peony_live_presets import engine, request, GAME
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic
from css_live_snapshot import MOD, Probe
import css_capture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--details', action='store_true', help='Read the first active visual materials and physics inputs')
    parser.add_argument('--customization', action='store_true', help='Compare current Peony morphs, hidden sections and six material families')
    args = parser.parse_args()
    if args.details and args.customization:
        parser.error('Choose one capture scope per short-lived summon')
    out = args.output.resolve()
    pids = processes()
    if out.exists() or len(pids) != 1 or not out.is_relative_to((ROOT / 'work').resolve()):
        raise RuntimeError('Need one game and a new evidence directory under work')
    core = json.loads((MOD / 'core.json').read_text())
    if sha(MOD / 'cores' / core['file']) != args.core_sha256 or core['file'] not in Path(f'/proc/{pids[0]}/maps').read_text():
        raise RuntimeError('Expected core is not installed and mapped')
    original = describe()
    if original['busy'] or original['expression']['performance']:
        raise RuntimeError('Appearance or facial performance is busy')
    saved = json.loads((MOD / 'state/state.json').read_text())
    cine = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError('Cannot inspect CINE')
    controls = {c['id']: c for s in cine['result']['sections'] for c in s['controls']}
    if not controls['take'].get('enabled', True) and controls['exit'].get('enabled', True):
        raise RuntimeError('CINE take is active')
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out / 'recipe.py')
    atomic(out / 'state-before.json', saved)
    report = dict(passed=False, core=core, core_sha256=args.core_sha256, pids=pids,
                  original=original, input_sent=False, active_samples=0, actor_ids=[])
    with (out / 'requests.jsonl').open('x') as log, (out / 'samples.jsonl').open('x') as samples:
        p = Probe(log)
        player = p.send('player')
        get = lambda target, name: p.send('get', target=target, property=name)

        def call(target, function, **fields):
            p.send('describe', target=target, function=function)
            return p.call(target, function, **fields)

        def geometry():
            return {k: call(t, f) for k, t, f in (
                ('position', player['pawn'], 'K2_GetActorLocation'),
                ('rotation', player['pawn'], 'K2_GetActorRotation'),
                ('view_target', player['controller'], 'GetViewTarget'),
                ('control_rotation', player['controller'], 'GetControlRotation'))}

        def keys():
            return p.send('input.keys', target=player['controller'],
                          keys=['F', 'W', 'A', 'S', 'D', 'SpaceBar', 'LeftControl', 'LeftMouseButton', 'RightMouseButton'])

        def details(actor):
            rows = []
            source_meshes = {c['mesh']['name'] for c in report['source']['components']}
            poses = sorted(actor.get('poses', []), key=lambda row: row['mesh']['name'] not in source_meshes)
            for pose in poses:
                component = pose['component']
                materials = get(component, 'OverrideMaterials')
                post = call(component, 'GetPostProcessInstance')
                fields = p.send('properties', target=post, inherited=True) if post else []
                public = {f['name']: get(post, f['name']) for f in fields if f['name'].startswith('CSSBody')}
                rows.append(dict(component=component, mesh=pose['mesh'], post=post, inputs=public,
                    materials=[dict(material=m, parent=get(m, 'Parent')) if m else None for m in materials],
                    flags={name: get(component, name) for name in
                        ('bVisible', 'bHiddenInGame', 'bDisableClothSimulation', 'bDisableRigidBodyAnimNode')},
                    collision=call(component, 'GetCollisionEnabled')))
            return rows

        uniform_fields = ('ScalarParameterValues', 'VectorParameterValues', 'TextureParameterValues')

        def uniforms(material):
            return {name: get(material, name) for name in uniform_fields}

        def customization(actor):
            source = report['source']['components'][0]
            visual = next(row for row in actor['poses'] if row['mesh']['name'] == source['mesh']['name'])
            component = visual['component']
            materials = get(component, 'OverrideMaterials')
            result = dict(component=component, materials=materials, lod_info=get(component, 'LODInfo'),
                morphs={name: p.call(component, 'GetMorphTarget', MorphTargetName=name) for name in source['morphs']},
                families=[])
            # Serialize each completed read before a later object can expire.
            report['customization'] = result
            atomic(out / 'result.json', report)
            for row in report['customization_source']:
                material = materials[row['slot']]
                captured = dict(slot=row['slot'], material=material, parent=get(material, 'Parent'), uniforms=uniforms(material))
                result['families'].append(captured)
                atomic(out / 'result.json', report)
            return result

        def guard():
            if processes() != pids or p.send('player') != player:
                raise RuntimeError('Player or game changed')
            current = describe()
            if any(current[k] != original[k] for k in ('shell', 'outfit', 'variant', 'customize', 'idle_override')):
                raise RuntimeError('Appearance changed')
            if json.loads((MOD / 'state/state.json').read_text()) != saved:
                raise RuntimeError('Saved settings changed')
            if any(keys().values()):
                raise RuntimeError('Concurrent input detected')
            menu = engine({'op': 'menu.status'})
            if menu['menu_open'] or menu['game_menu_open']:
                raise RuntimeError('Menu opened')

        pressed = False
        try:
            guard()
            window = json.loads(subprocess.check_output(['hyprctl', 'activewindow', '-j']))
            if window.get('pid') != pids[0] or window.get('class') != 'steam_app_2584270':
                raise RuntimeError('Game is not focused')
            inp = get(player['controller'], 'PlayerInput')
            mappings = get(inp, 'EnhancedActionMappings')
            atomic(out / 'input-mappings.json', mappings)
            if not any(r['Key']['KeyName'] == 'F' and r['Action']['name'].endswith('.IA_Shell_Ability') for r in mappings):
                raise RuntimeError('Expected F shell-ability mapping is absent')
            asc = get(player['pawn'], 'AbilitySystemComponent')
            report['abilities_before'] = get(asc, 'ActivatableAbilities')
            health = get(player['pawn'], 'HealthComponent')
            health_set = get(health, 'HealthSet')
            if call(health, 'IsDeadOrDying'):
                raise RuntimeError('Player is not alive')
            anim = call(get(player['pawn'], 'Mesh'), 'GetAnimInstance')
            if get(anim, 'Speed') > .05 or call(anim, 'IsAnyMontagePlaying'):
                raise RuntimeError('Player is moving or playing a montage')
            report['resources_before'] = {n: get(health_set, n) for n in ('Health', 'Resolve')}
            report['geometry_before'] = geometry()
            report['source'] = p.send('astral.source')
            if args.customization:
                source = report['source']['components'][0]
                if '/PeonyV2/' not in source['mesh']['name']:
                    raise RuntimeError('Customization fixture requires Peony V2')
                p.send('describe', target=source['component'], function='GetMorphTarget')
                adapters = p.send('astral.adapters.probe')
                atomic(out / 'adapters.json', adapters)
                by_source = {row['source']: row['companion'] for row in adapters['materials']}
                captured, companions = [], set()
                for slot, material in enumerate(source['materials']):
                    companion = by_source[material['name'].split(' ',1)[1]]
                    if '/M_PeonyV2_' not in companion or companion in companions:
                        continue
                    companions.add(companion)
                    captured.append(dict(slot=slot, material=material, companion=companion, uniforms=uniforms(material)))
                if len(captured) != 6:
                    raise RuntimeError('Expected all six Peony material families')
                report['customization_source'] = captured
            initial = p.send('astral.observe', poses=True)
            if initial['status'] != 'observed' or any(a['active'] for a in initial['actors']):
                raise RuntimeError('Need no active doubles before the press')
            report['initial'] = initial
            report['status_before'] = json.loads((MOD / 'runtime/status.json').read_text()).get('astral')
            atomic(out / 'result.json', report)
            guard()
            pressed = True
            report['input_sent'] = True
            atomic(out / 'result.json', report)
            css_capture.command('inventory_test_key', key='F', down=True)
            time.sleep(.12)
            report['key_observed'] = keys()['F']
            css_capture.command('inventory_test_key', key='F', down=False)
            pressed = False
            start = time.monotonic()
            inactive_since = None
            while time.monotonic() - start < 60:
                guard()
                observed = p.send('astral.observe', poses=True)
                status = json.loads((MOD / 'runtime/status.json').read_text()).get('astral')
                active = [a for a in observed['actors'] if a['active']]
                row = dict(elapsed=time.monotonic()-start, observed=observed, status=status)
                samples.write(json.dumps(row)+'\n'); samples.flush()
                if active:
                    report['active_samples'] += 1
                    report['actor_ids'] = sorted(set(report['actor_ids']) | {a['id'] for a in active})
                    inactive_since = None
                    if args.details and 'details' not in report:
                        css_capture.MEDIA = out
                        report['screenshot'] = str(css_capture.shot('summon'))
                        report['details'] = details(active[0])
                        report['observed_after_details'] = p.send('astral.observe', poses=True)
                        atomic(out / 'result.json', report)
                    if args.customization and 'customization' not in report:
                        customization(active[0])
                        report['observed_after_customization'] = p.send('astral.observe', poses=True)
                        atomic(out / 'result.json', report)
                elif report['active_samples']:
                    inactive_since = inactive_since or time.monotonic()
                    if time.monotonic()-inactive_since > 3 and status['active'] == 0 and status['cached'] == 0:
                        break
                elif time.monotonic()-start > 12:
                    raise RuntimeError('No active double observed after the normal press')
                time.sleep(.3)
            report['final_observed'] = p.send('astral.observe', poses=True)
            report['status_after'] = json.loads((MOD / 'runtime/status.json').read_text()).get('astral')
            report['resources_after'] = {n: get(health_set, n) for n in ('Health', 'Resolve')}
            report['abilities_after'] = get(asc, 'ActivatableAbilities')
        except Exception as error:
            report['error'] = repr(error)
        finally:
            try:
                if pressed:
                    css_capture.command('inventory_test_key', key='F', down=False)
                report['keys_released'] = not any(keys().values())
                report['geometry_after'] = geometry()
                after = json.loads((MOD / 'state/state.json').read_text())
                atomic(out / 'state-after.json', after)
                report['state_unchanged'] = after == saved
                report['cine_unchanged'] = request(GAME, {'op': 'model', 'extension': 'eins0fx.cine'})['result'] == cine['result']
                report['process_unchanged'] = processes() == pids
                if 'geometry_before' in report:
                    report['view_unchanged'] = all(report['geometry_after'][k] == report['geometry_before'][k]
                                                   for k in ('view_target', 'control_rotation'))
                    report['position_unchanged'] = report['geometry_after']['position'] == report['geometry_before']['position']
                else:
                    report['view_unchanged'] = report['position_unchanged'] = None
                status = report.get('status_after', {})
                report['passed'] = bool('error' not in report and report['active_samples'] and report.get('key_observed')
                    and status.get('active') == 0 and status.get('cached') == 0 and not status.get('error')
                    and all(report[k] for k in ('keys_released', 'state_unchanged', 'cine_unchanged', 'process_unchanged', 'view_unchanged')))
            except Exception as error:
                report['cleanup_error'] = repr(error)
            atomic(out / 'result.json', report)
    print(json.dumps({k: report.get(k) for k in ('passed', 'input_sent', 'active_samples', 'actor_ids', 'error', 'cleanup_error', 'keys_released', 'state_unchanged', 'cine_unchanged', 'view_unchanged')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
