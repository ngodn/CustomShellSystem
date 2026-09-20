"""Verify V42 body controls and morph geometry on player and preview instances."""
import argparse
import json
from pathlib import Path
import sys
import time

from css import ROOT, atomic, processes
from css_capture import MOD, command
import css_capture
from css_live_snapshot import Probe
from check_inventory_camera import checked_get, checked_call

AUTHOR = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(AUTHOR / 'tools'))
from body_morph_layout import inputs as layout_inputs
from body_contact_morph_inputs import inputs as contact_inputs

CONTROLS = ['chest_motion', 'glute_motion', 'thigh_motion', 'belly_motion']
ARRAYS = ['RegionFrequencies', 'RegionDampingRatios', 'RegionMotionAmounts', 'RegionEnabled',
          'Offsets', 'Moments', 'ContactCenters', 'ContactAxesX', 'ContactAxesY', 'ContactAxesZ']
FIELDS = ['CSSResetEpoch', 'CSSBodyResetEpoch', 'CSSBodyUseRegionSettings',
          'CSSStiffness', 'CSSDamping', 'CSSGravity', 'CSSEnabled'] + ['CSSBody' + name for name in ARRAYS]


def snapshot(probe):
    player = probe.send('player')
    inventory = command('inventory_inspect')
    if not player.get('pawn') or not inventory['css']['active']:
        raise RuntimeError('An active player and CSS preview are required')
    preview = probe.send('find', path=inventory['display_character']['path'])
    result = dict(player_identity=player, inventory=inventory)
    for name, actor in [('player', player['pawn']), ('preview', preview)]:
        mesh = checked_get(probe, actor, 'Mesh')
        anim = checked_call(probe, mesh, 'GetPostProcessInstance')['ReturnValue']
        if not anim or 'ABP_SeduXtress_BodyHairV42' not in anim['class']:
            raise RuntimeError(name + ' is missing the combined production graph')
        props = probe.send('properties', target=anim, inherited=True)
        if not set(FIELDS).issubset({p['name'] for p in props}):
            raise RuntimeError('Body input contract is missing')
        values = {field: probe.send('get', target=anim, property=field) for field in FIELDS}
        result[name] = dict(mesh=mesh, anim=anim, values=values)
    if probe.send('player') != player:
        raise RuntimeError('Player changed during the check')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists() or len(processes()) != 1:
        parser.error('Use a new workspace output with exactly one running game')
    state = json.loads((MOD / 'state/state.json').read_text())
    status = command('status')
    selected = state['selections'][status['shell']]
    if selected['outfit'] != 'eins0fx.seduxtress' or selected['variant'] != 'black_pearl':
        raise RuntimeError('Wear V42 Black Pearl first')
    if len(state['presets']) >= 64:
        raise RuntimeError('No room for a temporary profile')
    measured = json.loads((AUTHOR / 'work/nextgen-audit/body-morph-layout-v1.json').read_text())
    envelopes = json.loads((AUTHOR / 'work/nextgen-audit/body-contact-ellipsoids-v2.json').read_text())
    morph_controls = ['shape_' + name for name in measured['morphs']]
    touched = CONTROLS + morph_controls
    originals = {name: selected['customize']['values'].get(name) for name in touched}
    output.mkdir(parents=True)
    atomic(output / 'state-before.json', state)
    css_capture.MEDIA = output
    slot = 'css-body-check-' + str(time.time_ns())
    saved = False
    results = []
    with (output / 'requests.jsonl').open('x') as log:
        probe = Probe(log)
        initial = snapshot(probe)
        atomic(output / 'initial.json', initial)
        view = initial['inventory']['css']

        def send(action, **fields):
            log.write(json.dumps(dict(action=action, **fields)) + '\n'); log.flush()
            reply = command(action, **fields)
            log.write(json.dumps(reply) + '\n'); log.flush()
            return reply

        def setting(control, channel, value):
            send('control', control=control, channel=channel, value=value)

        def check(name, overrides, morphs=None):
            time.sleep(.5)
            sample = snapshot(probe)
            atomic(output / (name + '.json'), sample)
            expected = [[2., .7, 1., True] for _ in range(7)]
            for indices, value in overrides:
                for i in indices:
                    expected[i] = value
            max_geometry_error = 0.
            for owner in ('player', 'preview'):
                values = sample[owner]['values']
                for field in ('CSSResetEpoch', 'CSSStiffness', 'CSSDamping', 'CSSGravity', 'CSSEnabled'):
                    if values[field] != initial[owner]['values'][field]:
                        raise AssertionError(name + ': hair input/reset changed')
                for channel, field in enumerate(ARRAYS[:4]):
                    actual = values['CSSBody' + field]
                    if len(actual) != 7 or any(abs(float(v)-float(expected[i][channel])) > 1e-5 for i, v in enumerate(actual)):
                        raise AssertionError((name, owner, field, actual, expected))
                if overrides and not values['CSSBodyUseRegionSettings']:
                    raise AssertionError('Body region mode was not enabled')
                if morphs is not None:
                    geometry = dict(**layout_inputs(measured, morphs), **contact_inputs(envelopes, morphs))
                    for field, target in geometry.items():
                        actual = values['CSSBody' + field]
                        if len(actual) != 7:
                            raise AssertionError('Incomplete live geometry array')
                        for a, b in zip(actual, target, strict=True):
                            error = abs(a-b) if field == 'Moments' else max(abs(a[k]-v) for k, v in zip('XYZ', b, strict=True))
                            max_geometry_error = max(max_geometry_error, error)
                            if error > (1e-4 if field == 'Moments' else 1e-6):
                                raise AssertionError((name, owner, field, error))
                    for morph, weight in zip(measured['morphs'], morphs, strict=True):
                        actual = checked_call(probe, sample[owner]['mesh'], 'GetMorphTarget', MorphTargetName=morph)['ReturnValue']
                        if abs(actual-weight) > 1e-6:
                            raise AssertionError((owner, morph, actual, weight))
            results.append(dict(name=name, passed=True, maximum_geometry_error=max_geometry_error))
            return sample

        try:
            for name in touched:
                send('reset_control', control=name)
            check('defaults', [], [0.] * 6)
            setting('chest_motion', 0, 4)
            check('frequency', [([0, 1], [4, .7, 1, True])])
            setting('chest_motion', 1, 1.1)
            check('damping', [([0, 1], [4, 1.1, 1, True])])
            setting('chest_motion', 2, .2)
            check('motion', [([0, 1], [4, 1.1, .2, True])])
            setting('chest_motion', 3, 0)
            chest = ([0, 1], [4, 1.1, .2, False])
            check('disabled', [chest])
            belly = ([6], [3, .8, .4, True])
            for channel, value in enumerate(belly[1]):
                setting('belly_motion', channel, float(value))
            check('disjoint', [chest, belly])
            send('save_profile', name=slot); saved = True
            send('reset_control', control='chest_motion')
            check('remove-one', [belly])
            send('reset_control', control='belly_motion')
            check('reset', [])
            send('load_profile', name=slot)
            check('profile', [chest, belly])
            send('inventory_capture_row', section=1, row=18, offset=1000)
            css_capture.shot('profile-body-controls')
            for name in CONTROLS:
                send('reset_control', control=name)
            for label, weights in [('maximum-morphs', [1.] * 6), ('mixed-morphs', [.3, .8, .1, .7, .4, .6])]:
                for control, weight in zip(morph_controls, weights, strict=True):
                    setting(control, 0, weight)
                check(label, [], weights)
                css_capture.shot(label)
            send('inventory_select', index=0); time.sleep(.8)
            send('inventory_select', index=1)
            check('tab-reentry', [], [.3, .8, .1, .7, .4, .6])
        finally:
            for name, original in originals.items():
                if original is None:
                    send('reset_control', control=name)
                else:
                    for channel, value in enumerate(original[:4 if name in CONTROLS else 1]):
                        setting(name, channel, value)
            if saved:
                send('delete_profile', name=slot)
            send('inventory_capture_row', section=view['section'], row=view['row'], offset=1000)
            after = json.loads((MOD / 'state/state.json').read_text())
            atomic(output / 'state-after.json', after)
            if after != state:
                raise AssertionError('Original saved settings were not restored')
        restored = snapshot(probe)
        atomic(output / 'restored.json', restored)
        css_capture.shot('restored')
    atomic(output / 'result.json', dict(passed=True, cases=results, state_restored=True,
        media_review_pending=True, scope='Actual player/preview body tuning, morph geometry, profile and tab re-entry. No movement/contact/travel/death/FPS acceptance.'))
    print(json.dumps(dict(passed=True, cases=len(results), output=str(output))))


if __name__ == '__main__':
    main()
