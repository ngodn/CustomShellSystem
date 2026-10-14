"""Verify retained Eve supporting cooks and replay the compiled graph relocation."""
import json
from pathlib import Path
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'work/three-outfit-rig-physics'
OUT = ROOT / 'build/eve-supporting-rig-source-check1'
PACKAGES = ('CR_Hair', 'CR_Hand', 'Fit/ABP_BikiniFeet2', 'Fit/ABP_KnitFeet1')
PREFIX = 'MortalShell2/Content/CSS/eins0fx/EveSB/'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def main():
    require(not OUT.exists(), 'Use a new output directory')
    pins = {}

    def pin(path, expected=None):
        path = Path(path).resolve(strict=True)
        digest = sha(path)
        require(expected is None or digest == expected, 'Hash differs: ' + str(path))
        pins[str(path)] = digest
        return digest

    def read(path, expected=None):
        pin(path, expected)
        return json.loads(Path(path).read_text())

    previous = read(WORK / 'eve-rig-pack2/verification.json')
    require(previous['passed'], 'Earlier rig assembly failed')

    def prior(path):
        pin(path, previous['protected'][str(path)])

    editor = WORK / 'eve-rig-editor2'
    cook = WORK / 'eve-rig-cook1'
    for path in (editor/'input.json', editor/'migration.json', editor/'reload.json',
                 editor/'reload-exit.json', cook/'exit.json', cook/'packages.txt'):
        prior(path)
    original = read(editor/'input.json')['assets']
    migration = read(editor/'migration.json')['assets']
    require(read(editor/'reload.json')['passed'] and
            read(editor/'reload-exit.json')['returncode'] == 0 and
            read(cook/'exit.json')['returncode'] == 0, 'Reload/cook did not pass')
    command = read(cook/'command.json')
    require(command[command.index('--ro-bind', command.index('--die-with-parent'))+1:
                    command.index('--ro-bind', command.index('--die-with-parent'))+3] ==
            [str(editor/'Content'), str(cook/'project/Content')],
            'Cook did not mount the retained editor content read-only')
    plan = read(ROOT/'build/eve-surface-package1/plan.json')
    candidate = read(ROOT/'build/eve-surface-package1/verified.json')
    require(candidate['passed'] and candidate['plan_sha256'] ==
            sha(ROOT/'build/eve-surface-package1/plan.json'), 'Candidate proof differs')
    rows = []
    for name in PACKAGES:
        package = '/Game/CSS/eins0fx/EveSB/' + name
        source, = [r for r in original if r['new_package'] == package]
        migrated, = [r for r in migration if r['package'] == package]
        pin(source['source_file'], source['source_sha256'])
        editable = editor/'Content/CSS/eins0fx/EveSB'/(name+'.uasset')
        prior(editable)
        pin(editable, migrated['sha256'])
        require(package in (cook/'packages.txt').read_text().splitlines(), 'Package not cooked')
        payloads = {}
        for suffix in ('.uasset', '.uexp'):
            file = cook/'cooked/CSSAuthoring/Content/CSS/eins0fx/EveSB'/(name+suffix)
            relative = PREFIX+name+suffix
            prior(file)
            digest = pin(file, previous['payloads'][relative])
            require(plan['payloads'][relative]['sha256'] == digest, 'Candidate cook differs')
            pin(plan['payloads'][relative]['path'], digest)
            payloads[relative] = digest
        rows.append(dict(package=package, original_source=source, editor_source=str(editable),
                         shipping_payloads=payloads))

    secondary = WORK/'eve-secondary-namespace1'
    preserved = read(secondary/'verification.json')
    prior(secondary/'verification.json')
    request = read(secondary/'request.json')
    require(preserved['passed'] and request['animation_blueprints'] and len(request['jobs']) == 1,
            'Unexpected compiled graph recipe')
    original_header = Path(request['jobs'][0]['input'])
    pin(original_header)
    pin(original_header.with_suffix('.uexp'), preserved['export_payload_sha256'])
    writer = ROOT/'build/retoc-css-target/release/examples/rewrite_material'
    source = ROOT/'tools/asset-paths/rewrite_material.rs'
    require(pin(source) == pin(ROOT/'build/retoc-css/retoc/examples/rewrite_material.rs'),
            'Writer source differs from the retained build checkout')
    pin(writer)
    pin(__file__)
    prelist = ROOT/'docs/development/cleanup-2026-10-14-eve-supporting-rig-replay.md'
    pin(prelist)
    OUT.mkdir()
    stage = OUT/'stage'
    stage.mkdir()
    request['jobs'][0]['output'] = str(stage/'ABP_Secondary.uasset')
    request['report'] = str(OUT/'relocation.json')
    write(OUT/'request.json', request)
    pin(OUT/'request.json')
    write(OUT/'inputs.json', pins)
    command = [str(writer), str(OUT/'request.json')]
    write(OUT/'command.json', command)
    with (OUT/'replay.log').open('x') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    write(OUT/'exit.json', dict(returncode=result.returncode))
    result.check_returncode()
    report = read(OUT/'relocation.json')
    require(report['passed'] and len(report['animation_blueprints']) == 1, 'Graph rewrite failed')
    row, = report['animation_blueprints']
    require(row['inverse_roundtrip'] and row['unchanged_roundtrip'], 'Header roundtrip failed')
    replay = {}
    require({p.name for p in stage.iterdir()} == {'ABP_Secondary.uasset', 'ABP_Secondary.uexp'},
            'Unexpected replay files')
    for file in stage.iterdir():
        relative = PREFIX+file.name
        digest = sha(file)
        require(digest == preserved['payloads'][relative] == plan['payloads'][relative]['sha256'],
                'Replayed graph differs from shipping data')
        pin(plan['payloads'][relative]['path'], digest)
        replay[relative] = digest
    require(all(sha(Path(p)) == h for p,h in pins.items()), 'Input changed during replay')
    write(OUT/'verified.json', dict(passed=True, supporting_cooks=rows, replayed_payloads=replay,
        inputs=pins, scope='Four retained supporting source/cook chains verified; current writer reproduces the compiled secondary graph exactly. No fresh recook or live test.'))
    cleanup = {str(p):dict(sha256=sha(p), bytes=p.stat().st_size) for p in stage.iterdir()}
    write(OUT/'cleanup-plan.json', cleanup)
    for path, record in cleanup.items():
        file = Path(path)
        require(sha(file) == record['sha256'], 'Replay output changed before cleanup')
        file.unlink()
    stage.rmdir()
    write(OUT/'cleanup-completed.json', dict(files=len(cleanup), logical_bytes=sum(r['bytes'] for r in cleanup.values())))
    print('Verified four retained supporting cooks; reproduced both compiled secondary payloads exactly')


if __name__ == '__main__':
    main()
