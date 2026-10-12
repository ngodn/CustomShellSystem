"""Check one-time body-preset propagation and update counts in the editor fixture."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(data):
    expected = {(name, fps) for name in ('SK_PeonyV2', 'SK_PeonyV2_Face') for fps in (30, 60, 120)}
    observed = set()
    frames = 0
    for case in data['cases']:
        identity = (case['mesh'].split('.')[-1], case['fps'])
        if identity in observed or identity not in expected or case['passed'] is not True:
            raise ValueError('Duplicate, failed or unexpected case')
        observed.add(identity)
        fps = case['fps']
        rows = case['samples']
        if len(rows) != 8 * fps + 1:
            raise ValueError('Incomplete frame coverage')
        for frame, row in enumerate(rows):
            if row['frame'] != frame or row['phase'] != min(frame // (2 * fps), 3):
                raise ValueError('Incorrect preset transition coverage')
            if row['rig_count'] != 3:
                raise ValueError('Expected body, hair and hand rigs')
            if frame:
                previous = rows[frame - 1]
                if row['SeenUpdate'] - previous['SeenUpdate'] != 1:
                    raise ValueError('Body rig did not consume exactly one animation update')
                changed = row['phase'] != previous['phase']
                if row['ResetCount'] - previous['ResetCount'] != int(changed):
                    raise ValueError('Unexpected body reset')
                expected_steps = 0 if changed else 120 // fps
                if row['Substeps'] != expected_steps or row['TotalSteps'] - previous['TotalSteps'] != expected_steps:
                    raise ValueError('Body simulation did not advance at its 120 Hz clock')
        frames += len(rows)
    if observed != expected or data['passed'] is not True:
        raise ValueError('Missing cases or failed native input transfer')
    return dict(passed=True, cases=len(observed), frames=frames,
                scope='Saved Peony AnimBP forwards one-time region writes without extra updates/resets; no game or playback-adapter coverage.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    path = args.stage / 'graph-inputs.json'
    raw = path.read_bytes()
    result = verify(json.loads(raw))
    for source, digest in json.loads((args.stage / 'inputs.json').read_text()).items():
        if hashlib.sha256(Path(source).read_bytes()).hexdigest() != digest:
            raise ValueError('Changed source: ' + source)
    if json.loads((args.stage / 'exit.json').read_text())['returncode'] != 0:
        raise ValueError('Editor did not exit successfully')
    result['response_sha256'] = hashlib.sha256(raw).hexdigest()
    result['checker_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    output = args.output or args.stage / 'verified.json'
    with output.open('x') as handle:
        handle.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
