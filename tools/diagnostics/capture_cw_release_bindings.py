"""Check final CW walk references and belt inputs without movement or camera input."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, sha, processes
from check_peony_live_presets import customize, engine

CW = ROOT.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P'
BASE = '/Game/CSS/eins0fx/CommanderWhite/Animations/Locomotion/Walk/'
MOD = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'


def object_path(handle):
    return handle['name'].split(' ', 1)[1] if handle else ''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    original = describe()
    pids = processes()
    if out.exists() or len(pids) != 1 or original['busy'] or original['outfit'] == 'eins0fx.commanderwhite':
        raise RuntimeError('Need one game, a settled non-CW look and a new evidence directory')
    inputs = [CW/'work/walk-final-paths1/saved.json',
              CW/'work/walk-final-paths1/assembly/installed.json',
              CW/'work/foot-walk-orientation3/blendbound/result.json']
    saved, installed, accepted = [json.loads(p.read_text()) for p in inputs]
    pins = {str(p): sha(p) for p in inputs}
    for name, digest in installed['files'].items():
        if name.startswith('CSS_CommanderWhite_'):
            path = GAME/'Content/Paks/~mods/CSS_CommanderWhite_eins0fx_P'/name
            if sha(path) != digest:
                raise RuntimeError('Installed CW differs from accepted final-path receipt')
            pins[str(path)] = digest
    if json.loads((MOD/'core.json').read_text()) != installed['core']:
        raise RuntimeError('Core selector differs from accepted CW build')
    core = MOD/'cores'/installed['core']['file']
    if sha(core) != installed['core_sha256']:
        raise RuntimeError('Core digest differs')
    pins[str(core)] = sha(core)
    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    before = json.loads((MOD/'state/state.json').read_text())
    atomic(out/'state-before.json', before)
    report = dict(passed=False, restored=False, original=original, process_before=pids,
                  inputs=pins, recipe_sha256=sha(Path(__file__)), movement_input=False,
                  camera_input=False, started_ns=time.time_ns())
    target = original['shell'], 'eins0fx.commanderwhite', 'original'
    expected = None
    changed = False
    with (out/'requests.jsonl').open('x') as log:
        def probe(**request):
            log.write(json.dumps({'request': request})+'\n'); log.flush()
            result = engine(request)
            log.write(json.dumps({'result': result})+'\n'); log.flush()
            return result

        def get(target, prop):
            return probe(op='get', target=target, property=prop)

        try:
            if describe() != original:
                raise RuntimeError('Appearance changed before capture')
            changed = True
            customize({'action': 'wear', 'outfit': target[1], 'variant': target[2], 'persist': False})
            active = report['active'] = settled(target)
            expected = active['customize']
            player = report['player'] = probe(op='player')
            mesh = get(player['pawn'], 'Mesh')
            post = report['post'] = get(mesh, 'PostProcessAnimInstance')
            report['asset'] = probe(op='call', target=mesh, function='GetSkeletalMeshAsset', args={})['ReturnValue']
            properties = probe(op='properties', target=post, inherited=True)
            rigs = [get(post, p['name'])['ControlRig'] for p in properties
                    if p['name'].startswith('AnimGraphNode_ControlRig')]
            belts = [r for r in rigs if r and '/CommanderWhite/Rig/CR_Belt20.' in r['class']]
            if len(belts) != 1:
                raise RuntimeError(f'Expected one Belt20 rig, found {len(belts)}')
            report['rigs'] = rigs
            report['belt'] = {name: get(belts[0], name) for name in
                              ('Enabled', 'Initialized', 'SupportedScale', 'TotalSteps', 'ResetCount')}
            report['post_shapes'] = [get(post, 'CSSBodyShape'+str(i)) for i in range(6)]
            report['belt_shapes'] = [get(belts[0], 'BridgeShape'+str(i)) for i in range(6)]
            assert report['post_shapes'] == report['belt_shapes'], 'Belt shape bridge mismatch'
            assert all(report['belt'][n] for n in ('Enabled', 'Initialized', 'SupportedScale'))
            blend_path = BASE+'BS_CW_Walk.BS_CW_Walk'
            blend = probe(op='find', path=blend_path)
            report['blend_already_loaded'] = bool(blend)
            if not blend:
                blend = probe(op='load', path=blend_path)
            assert object_path(blend) == blend_path, 'Final walking BlendSpace did not resolve'
            report['blend'] = blend
            fields = probe(op='properties', target=blend, inherited=True)
            assert 'SampleData' in {p['name'] for p in fields}, 'No reflected BlendSpace samples'
            samples = report['samples'] = get(blend, 'SampleData')
            assert len(samples) == len(accepted['blend']['samples'])
            for actual, old in zip(samples, accepted['blend']['samples'], strict=True):
                path = saved['mapping'][old['candidate'].split('.')[0]]
                assert object_path(actual['Animation']) == path+'.'+path.rsplit('/', 1)[1]
                assert [actual['SampleValue'][k] for k in 'XYZ'] == old['point']
                assert actual['RateScale'] == old['rate']
            report['unique_clips'] = sorted({object_path(s['Animation']) for s in samples})
            assert len(report['unique_clips']) == 8
            report['status'] = json.loads((MOD/'runtime/status.json').read_text())
            command = [sys.executable, str(ROOT/'tools/diagnostics/check_live_animation_owner.py'),
                       '--catalog', str(ROOT/'build/eve-surface-package1/metadata/CSS_SharedAssets_P/MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json'),
                       '--output', str(out/'owner.json')]
            with (out/'owner.log').open('x') as owner_log:
                subprocess.run(command, stdout=owner_log, stderr=subprocess.STDOUT, check=True)
            assert get(mesh, 'PostProcessAnimInstance') == post and probe(op='player') == player
            assert processes() == pids
            assert all(sha(Path(p)) == digest for p, digest in pins.items())
            report['bindings_passed'] = True
        except Exception as error:
            report['error'] = repr(error)
            raise
        finally:
            try:
                if changed:
                    current = describe()
                    if processes() != pids or identity(current) != target or (expected is not None and current['customize'] != expected):
                        raise RuntimeError('Concurrent appearance/process change; automatic restore skipped')
                    customize({'action': 'wear', 'outfit': original['outfit'], 'variant': original['variant'],
                               'customize': original['customize'], 'persist': True})
                    report['after_restore'] = settled(identity(original), custom=original['customize'])
                    assert report['after_restore']['idle_override'] == original['idle_override']
                    report['restored'] = True
                    time.sleep(1.2)
                    after = json.loads((MOD/'state/state.json').read_text())
                    atomic(out/'state-after.json', after)
                    report['saved_state_equal'] = before == after
                    assert report['saved_state_equal'], 'CSS saved state differs after restoration'
            except Exception as error:
                report['restore_error'] = repr(error)
                raise
            finally:
                report['finished_ns'] = time.time_ns()
                report['process_after'] = processes()
                report['passed'] = bool(report.get('bindings_passed') and report['restored'] and report.get('saved_state_equal'))
                atomic(out/'result.json', report)
    print(json.dumps({k: report[k] for k in ('passed', 'blend_already_loaded', 'restored', 'saved_state_equal')}))


if __name__ == '__main__':
    main()
