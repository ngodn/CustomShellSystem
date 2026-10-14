"""Trace accepted Eve motion inputs into the installed surface-repair package.

Python 3.14. This checks retained files and receipts, not live solver behavior.
"""
import argparse
import json
import math
from pathlib import Path

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'work/three-outfit-rig-physics'
PACKAGES = ('CR_Body', 'CR_Hair', 'CR_Hand', 'ABP_Secondary',
            'Fit/ABP_BikiniFeet2', 'Fit/ABP_KnitFeet1')
PREFIX = 'MortalShell2/Content/CSS/eins0fx/EveSB/'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Output already exists')
    inputs = {}

    def pin(path, expected=None):
        path = Path(path).resolve(strict=True)
        if str(path) not in inputs:
            inputs[str(path)] = sha(path)
        actual = inputs[str(path)]
        require(expected is None or actual == expected, 'Hash differs: ' + str(path))
        return actual

    def read(path, expected=None):
        pin(path, expected)
        return json.loads(Path(path).read_text())

    pin(__file__)
    candidate = args.candidate.resolve(strict=True)
    proof = read(candidate / 'verified.json')
    plan = read(candidate / 'plan.json', proof['plan_sha256'])
    require(proof['passed'], 'Combined package has not passed verification')
    motion = WORK / 'peony-v2-motion-package1'
    accepted = read(motion / 'verified.json')
    assembly = read(motion / 'assembly.json', accepted['assembly_sha256'])
    decoded = read(motion / 'decoded.json', accepted['decoded_sha256'])
    require(accepted['passed'] and assembly['passed'], 'Accepted motion package failed')
    rig = WORK / 'peony-v2-body-limits1'
    authored = read(rig / 'authored.json')
    reloaded = read(rig / 'reloaded.json')
    verified = read(rig / 'verified.json')
    require(verified['passed'] and reloaded['passed'], 'Body source was not verified')
    require(authored['after'] == reloaded['after'], 'Reloaded body source differs')
    require(authored['before']['graph_sha256'] == authored['after']['graph_sha256'],
            'Body graph changed during limit authoring')
    source = rig / 'editor/Content/CSS/eins0fx/EveSB/CR_Body.uasset'
    pin(source, authored['sha256'])
    require(verified['candidate_sha256'] == authored['sha256'], 'Wrong body candidate')
    cook = rig / 'windows-cook1'
    cook_inputs = read(cook / 'inputs.json')
    require(read(cook / 'exit.json') == dict(returncode=0, sources_unchanged=True),
            'Body cook failed')
    require(cook_inputs['protected'][str(source.resolve())] == authored['sha256'],
            'Cook did not use the accepted body source')
    for path, digest in cook_inputs['protected'].items():
        pin(path, digest)
    defaults, = [e['Properties'] for e in decoded['/Game/CSS/eins0fx/EveSB/CR_Body']
                 if e['Name'] == 'Default__CR_Body_C']
    angles = [math.radians(v) for v in (6, 6, 10, 10, 3, 3, 5)]
    require(len(defaults['Limits']) == 7 and all(abs(a-b) < 1e-6 for a, b in
            zip(defaults['Limits'], angles, strict=True)), 'Unexpected angular limits')
    require(defaults['MaxTravel'] == [1.5, 1.5, 1, 1, .5, .5, .5],
            'Unexpected travel limits')
    live = read(motion / 'live-accepted.json')
    require(live['live_limits_verified'] and live['user_answer'] == 'Motion looks right now',
            'Historical motion acceptance missing')
    for name in ('Limits', 'MaxTravel'):
        observed = live['active_rig']['rig'][name]
        require(len(observed) == len(defaults[name]) and all(abs(a-b) < 1e-7
                for a, b in zip(observed, defaults[name], strict=True)),
                'Accepted live limits differ from packaged defaults')

    migrated = read(WORK / 'eve-rig-pack2/verification.json')
    secondary = read(WORK / 'eve-secondary-namespace1/verification.json')
    require(migrated['passed'] and secondary['passed'], 'Prior rig verification failed')
    payloads = {}
    for package in PACKAGES:
        for suffix in ('.uasset', '.uexp'):
            relative = PREFIX + package + suffix
            digest = assembly['payloads'][relative]
            row = plan['payloads'][relative]
            require(row['sha256'] == digest, 'Motion payload changed: ' + relative)
            pin(row['path'], digest)
            if package == 'CR_Body':
                pin(cook / 'project/Saved/Cooked/Windows/CSSShared/Content/CSS/eins0fx/EveSB' /
                    ('CR_Body' + suffix), digest)
            else:
                require(migrated['payloads'][relative] == digest,
                        'Supporting graph changed since namespace verification')
            if package == 'ABP_Secondary':
                require(secondary['payloads'][relative] == digest,
                        'Preserved compiled secondary graph differs')
            payloads[relative] = digest

    migration = read(WORK / 'eve-rig-editor2/migration.json')
    regression = WORK / 'eve-six-body-inputs1'
    tested_sources = read(regression / 'inputs.json')
    sources = {}
    for package in PACKAGES[1:]:
        asset = WORK / 'eve-rig-editor2/Content/CSS/eins0fx/EveSB' / (package + '.uasset')
        row, = [r for r in migration['assets'] if r['package'] == '/Game/CSS/eins0fx/EveSB/' + package]
        digest = pin(asset, row['sha256'])
        require(tested_sources[str(asset.resolve())] == digest, 'Preset fixture used another graph')
        sources[package] = dict(path=str(asset.resolve()), sha256=digest)
    require(tested_sources[str(source.resolve())] == authored['sha256'],
            'Preset fixture used another body rig')
    result = read(regression / 'verified.json')
    require(result['passed'] and result['cases'] == 36 and result['frames'] == 20196,
            'Unexpected preset regression coverage')
    pin(regression / 'graph-inputs.json', result['response_sha256'])
    pin(ROOT / 'tools/diagnostics/verify_body_graph_inputs.py', result['checker_sha256'])

    deployment = read(candidate / 'deployment.json')
    read(Path(deployment['transaction']) / 'plan.json', deployment['plan_sha256'])
    postcheck = read(candidate / 'installation-postcheck.json')
    require(deployment['installed'] and postcheck['passed'], 'Candidate is not installed')
    # Check the current files, not just the historical installation receipt.
    installed = Path(deployment['transaction']).parent / 'Paks/~mods/CSS_EveStellarBlade_eins0fx_P'
    for name, digest in postcheck['installed']['CSS_EveStellarBlade_eins0fx_P'].items():
        require(proof['files']['CSS_EveStellarBlade_eins0fx_P/' + name] == digest,
                'Installed receipt refers to another candidate')
        pin(installed / name, digest)
    require(all(sha(Path(path)) == digest for path, digest in inputs.items()),
            'Input changed during audit')
    args.output.write_text(json.dumps(dict(passed=True, payloads=payloads,
        supporting_editor_sources=sources, body_editor_source=str(source.resolve()),
        limits=defaults['Limits'], max_travel=defaults['MaxTravel'],
        historical_acceptance=live['scope'], preset_cases=result['cases'],
        preset_frames=result['frames'], inputs=inputs,
        scope='Six motion packages preserved in the installed shading candidate. Retained body source, cook, decoded limits and prior preset fixture match. Supporting editor sources are retained; compiled ABP_Secondary remains preserved, not recompiled. No new live test or complete supporting-graph rebuild is claimed.'), indent=2) + '\n')
    print('Verified 6 motion packages, 12 payloads, accepted body source and 36-case preset provenance')


if __name__ == '__main__':
    main()
