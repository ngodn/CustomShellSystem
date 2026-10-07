"""Run bounded independent full-frame reviews and verify their encoded clocks."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import subprocess
import time


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    plan = json.loads((work / 'plan.json').read_text())
    if not 1 <= len(plan) <= 32 or (work / 'result.json').exists():
        raise ValueError('Use a fresh bounded review batch')
    labels = [row['label'] for row in plan]
    if len(set(labels)) != len(labels) or any(Path(label).name != label for label in labels):
        raise ValueError('Invalid or duplicate review label')
    for row in plan:
        command = row['command']
        if (command[command.index('--stride') + 1] != '1' or '--video' not in command or
                command[command.index('--python-exit-code') + 1] != '1' or
                Path(command[command.index('--output') + 1]).resolve() != work / row['label'] or
                (work / row['label']).exists() or (work / (row['label'] + '-exit.json')).exists()):
            raise ValueError('Each command must produce a fresh full-frame video with Python failure propagation')

    def render(row):
        label, command = row['label'], row['command']
        motion = Path(command[command.index('--motion') + 1]).resolve(strict=True)
        source_hash = sha(motion)
        source = json.loads(motion.read_text())
        frame_count, fps = len(source['frames']), source['fps']
        del source
        start = time.monotonic()
        result = dict(label=label, command=command, motion=str(motion),
                      motion_sha256=source_hash, expected_frames=frame_count, expected_fps=fps,
                      passed=False, scope='Full-frame neutral fitted-mesh render, not visual or gameplay acceptance.')
        try:
            with (work / (label + '.log')).open('x') as stream:
                process = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
            result['exit_code'] = process.returncode
            if process.returncode:
                raise ValueError('Renderer failed')
            folder = work / label
            report = json.loads((folder / 'report.json').read_text())
            if (not report.get('video') or report['measure_only'] or report['motion_sha256'] != source_hash or
                    report['fps'] != fps or [r['frame'] for r in report['errors']] != list(range(frame_count))):
                raise ValueError('Missing or incomplete full-frame render report')
            movie = folder / 'review.mp4'
            probe = subprocess.run(['ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v:0',
                '-show_entries', 'stream=nb_read_frames,r_frame_rate,duration,width,height', '-of', 'json', str(movie)],
                capture_output=True, text=True, check=True)
            video = json.loads(probe.stdout)['streams']
            if (len(video) != 1 or int(video[0]['nb_read_frames']) != frame_count or
                    abs(float(Fraction(video[0]['r_frame_rate'])) - fps) > .0001 or
                    abs(float(video[0]['duration']) - frame_count / fps) > .002):
                raise ValueError('Encoded video omits frames or changes the motion clock')
            if sha(motion) != source_hash:
                raise ValueError('Motion changed while rendering')
            result.update(passed=True, video=video[0], movie_sha256=sha(movie),
                          movie_bytes=movie.stat().st_size, report_sha256=sha(folder / 'report.json'),
                          max_position_error_cm=max(r['position_cm'] for r in report['errors']),
                          max_angle_error_rad=max(r['angle_rad'] for r in report['errors']))
        except Exception as error:
            result['error'] = str(error)
        result['seconds'] = time.monotonic() - start
        (work / (label + '-exit.json')).write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({k: result[k] for k in ('label', 'passed', 'seconds')}), flush=True)
        return result

    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(render, plan))
    passed = all(row['passed'] for row in results)
    (work / 'result.json').write_text(json.dumps(dict(complete=True, passed=passed, results=results), indent=2) + '\n')
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
