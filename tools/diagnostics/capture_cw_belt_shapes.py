"""Check installed CW shape inputs and restore the current settings. Python 3.14."""
import argparse
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import atomic, processes, sha, wait_json
from css_live_snapshot import MOD, Probe
import css_capture

MORPHS = ('FBMBodyTone', 'PBMBreastsSize', 'PBMGlutesSize', 'PBMHipSize',
          'PBMThighsTone', 'PBMWaistWidth')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to((ROOT / 'work').resolve()):
        parser.error('Output must be under CSS/work')
    pids = processes()
    if len(pids) != 1:
        parser.error('Need one running game')
    before = json.loads((MOD / 'state/state.json').read_text())
    status = json.loads((MOD / 'runtime/status.json').read_text())
    if status['applied'] != 'eins0fx.commanderwhite/original':
        parser.error('Select Commander White first')
    values = before['selections'][status['shell']]['customize']['values']
    if any('shape_' + name in values for name in MORPHS):
        parser.error('This fixture requires default body shapes; no settings changed')
    out.mkdir(parents=True, exist_ok=False)
    atomic(out / 'state-before.json', before)
    (out / 'capture-source.py').write_bytes(Path(__file__).read_bytes())
    report = dict(passed=False, cases=[], pids=pids,
                  script_sha256=sha(Path(__file__)),
                  core_selector=json.loads((MOD / 'core.json').read_text()),
                  scope='Live shape routing and recovery; not rendered clipping or performance acceptance')
    with (out / 'requests.jsonl').open('x') as log:
        p = Probe(log)
        player = p.send('player')
        mesh = p.send('get', target=player['pawn'], property='Mesh')
        report.update(player=player, component=mesh)
        for function in ('GetPostProcessInstance', 'GetMorphTarget'):
            p.send('describe', target=mesh, function=function)

        def check_owner():
            assert processes() == pids, 'Game process changed'
            assert p.send('player') == player, 'Player changed'
            current = json.loads((MOD / 'runtime/status.json').read_text())
            assert current['applied'] == status['applied'], 'Outfit changed'

        def control(action, name, **fields):
            check_owner()
            log.write(json.dumps(dict(control_request=dict(action=action, control=name, **fields))) + '\n')
            log.flush()
            reply = css_capture.command(action, control=name, **fields)
            assert not reply.get('maintenance_error'), reply.get('maintenance_error')

        def sample(label, expected):
            check_owner()
            post = p.call(mesh, 'GetPostProcessInstance')
            node = p.send('get', target=post, property='AnimGraphNode_ControlRig')
            rig = node['ControlRig']
            assert '/CommanderWhite/Rig/CR_Belt20.' in rig['class']
            row = dict(label=label, expected=expected, post=post, rig=rig)
            row['mesh_morphs'] = [p.call(mesh, 'GetMorphTarget', MorphTargetName=n) for n in MORPHS]
            row['post_shapes'] = [p.send('get', target=post, property='CSSBodyShape'+str(i)) for i in range(6)]
            row['belt_shapes'] = [p.send('get', target=rig, property='BridgeShape'+str(i)) for i in range(6)]
            row['rig_state'] = {k:p.send('get', target=rig, property=k) for k in
                                ('Enabled', 'Initialized', 'SupportedScale', 'TotalSteps', 'ResetCount')}
            report['cases'].append(row)
            atomic(out / 'result.json', report)
            for key in ('mesh_morphs', 'post_shapes', 'belt_shapes'):
                assert row[key] == expected, (label, key, row[key], expected)
            assert all(row['rig_state'][k] for k in ('Enabled', 'Initialized', 'SupportedScale'))
            print(label, 'mesh/post/belt inputs match', flush=True)

        changed = False
        try:
            sample('baseline', [0]*6)
            for i, name in enumerate(MORPHS):
                changed = True
                control('control', 'shape_'+name, value=1)
                time.sleep(.5)
                sample(name, [int(j == i) for j in range(6)])
                control('reset_control', 'shape_'+name)
            for name in MORPHS:
                control('control', 'shape_'+name, value=1)
            time.sleep(.5)
            sample('combined_maximum', [1]*6)
        except Exception as error:
            report['error'] = repr(error)
            raise
        finally:
            errors = []
            if changed:
                for name in MORPHS:
                    try:
                        control('reset_control', 'shape_'+name)
                    except Exception as error:
                        errors.append(repr(error))
            report['restore_errors'] = errors
            try:
                # Request acknowledgement can precede the saved-state write.
                wait_json(MOD / 'state/state.json', lambda value: value == before)
                report['state_equal'] = True
            except Exception as error:
                report['state_equal'] = False
                errors.append(repr(error))
            atomic(out / 'result.json', report)
        sample('restored', [0]*6)
        report['passed'] = not report['restore_errors'] and report['state_equal']
        atomic(out / 'result.json', report)
        assert report['passed'], report


if __name__ == '__main__':
    main()
