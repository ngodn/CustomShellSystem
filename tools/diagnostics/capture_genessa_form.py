"""Observe Genessa form requests across a save change without sending gameplay input. Python 3.14."""
import argparse
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, processes, request, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--expect-form', choices=('faithful', 'stray'))
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('seconds must be 1..300')
    pids = processes()
    if len(pids) != 1:
        parser.error('Need one running game; no request was written')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    last = None
    controller_fields = ('ActiveShellItemDefinition', 'ActiveDarkFormItemDefinition',
                         'ActiveShellCharacterData', 'ActiveDarkformCharacterData')

    def query(value):
        reply = request(GAME, dict(op='engine', request=value))
        if not reply['ok']:
            raise RuntimeError(reply)
        return reply['result']

    def read_property(target, name):
        return query(dict(op='get', target=target, property=name))

    def read_bool(target, name):
        signature = query(dict(op='describe', target=target, function=name))
        if (set(signature) != {'ReturnValue'} or not signature['ReturnValue']['return']
                or signature['ReturnValue']['size'] != 1):
            raise RuntimeError('Unexpected getter signature: ' + name)
        return query(dict(op='call', target=target, function=name, args={}))['ReturnValue']

    with args.output.open('x') as stream:
        def record(value):
            stream.write(json.dumps(value) + '\n')
            stream.flush()

        record(dict(kind='start', pids=pids, script_sha256=sha(Path(__file__)),
                    core_selector=json.loads((mod / 'core.json').read_text()),
                    gameplay_input=False, form_requested_by_capture=False))
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            if processes() != pids:
                record(dict(kind='stopped', reason='Game process changed'))
                raise SystemExit('Capture stopped: game process changed')
            row = dict(kind='sample', time_ns=time.time_ns())
            try:
                player = query(dict(op='player'))
                row['player'] = player
                controller, pawn = player.get('controller'), player.get('pawn')
                if controller and pawn:
                    row['definitions'] = {key: read_property(controller, key) for key in controller_fields}
                    row['in_menu'] = read_bool(controller, 'IsInGameMenu')
                    health = read_property(pawn, 'HealthComponent')
                    row['dead_or_dying'] = read_bool(health, 'IsDeadOrDying') if health else None
                    row['player_after'] = query(dict(op='player'))
                    row['same_player_during_sample'] = player == row['player_after']
                status = json.loads((mod / 'runtime/status.json').read_text())
                row['css_status'] = {key: status.get(key) for key in
                                     ('last_request', 'message', 'shell', 'applied', 'enabled')}
                last = row
            except (OSError, ValueError, KeyError, RuntimeError, TimeoutError) as error:
                row['observation_error'] = str(error)
                last = None
            record(row)
            time.sleep(1)
        expected = {'faithful': 'CharacterId.Player.Shell.Genessa',
                    'stray': 'CharacterId.Player.Darkform.CorruptedGenessa'}.get(args.expect_form)
        observed = last.get('player', {}).get('shell') if last else None
        matched = (bool(last) and observed == expected
                   and last.get('same_player_during_sample', False)
                   and last.get('in_menu') is False
                   and last.get('dead_or_dying') is False) if expected else None
        record(dict(kind='end', expected_shell=expected, observed_shell=observed, expected_form_observed=matched,
                    limitation='Observes current form and CSS messages; does not prove native ability activation or save identity.'))
        if expected and not matched:
            raise SystemExit('Expected form was not observed in the final stable sample')
    print(args.output)


if __name__ == '__main__':
    main()
