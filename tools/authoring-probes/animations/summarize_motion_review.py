"""Make timestamped sheets and contact summaries from verified Blender replay renders."""
import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def summarize(directory, output):
    report_path = directory / 'report.json'
    report = json.loads(report_path.read_text())
    motion_path = Path(report['motion']).resolve(strict=True)
    if digest(motion_path) != report['motion_sha256']:
        raise ValueError('Motion changed after rendering: ' + str(motion_path))
    if report['measure_only']:
        raise ValueError('This report has no rendered frames')
    motion = json.loads(motion_path.read_text())
    fps = motion['fps']
    rows = report['errors']
    if not rows or not math.isfinite(fps) or fps <= 0:
        raise ValueError('Missing samples or invalid sample rate')
    frames = [row['frame'] for row in rows]
    if frames != sorted(set(frames)) or frames[0] != 0 or frames[-1] >= len(motion['frames']):
        raise ValueError('Unexpected replay sample ordering')
    for row in rows:
        if any(not math.isfinite(row[key]) for key in
               ('position_cm', 'angle_rad', 'contact_lowest_cm')):
            raise ValueError('Nonfinite replay measurement')
    images = [directory / f'{i:03d}.png' for i in range(len(rows))]
    if any(not image.is_file() for image in images):
        raise ValueError('A rendered sample is missing')
    output.mkdir(parents=True, exist_ok=False)
    pages = []
    for start in range(0, len(rows), 20):
        count = min(20, len(rows) - start)
        sheet = Image.new('RGB', (1200, math.ceil(count / 5) * 280), '#1c1c1c')
        draw = ImageDraw.Draw(sheet)
        for tile, index in enumerate(range(start, start + count)):
            with Image.open(images[index]) as original:
                thumb = original.convert('RGB')
                thumb.thumbnail((240, 250))
            x, y = tile % 5 * 240, tile // 5 * 280
            sheet.paste(thumb, (x + (240 - thumb.width) // 2, y))
            row = rows[index]
            draw.text((x + 8, y + 252),
                      f"{row['frame'] / fps:.3f}s | floor {row['contact_lowest_cm']:+.2f} cm",
                      fill='white')
        path = output / f'sheet-{start:04d}.jpg'
        sheet.save(path, quality=88)
        pages.append({'file': path.name, 'sha256': digest(path),
                      'first_frame': frames[start], 'last_frame': frames[start + count - 1]})
    result = dict(render_report=str(report_path.resolve()), report_sha256=digest(report_path),
                  motion=str(motion_path), motion_sha256=report['motion_sha256'],
                  source_hz=fps, source_frames=len(motion['frames']), rendered_samples=len(rows),
                  duration_seconds=(len(motion['frames']) - 1) / fps,
                  last_review_seconds=frames[-1] / fps,
                  unrendered_tail_seconds=(len(motion['frames']) - 1 - frames[-1]) / fps,
                  max_position_error_cm=max(row['position_cm'] for row in rows),
                  max_angle_error_rad=max(row['angle_rad'] for row in rows),
                  lowest_contact=min(rows, key=lambda row: row['contact_lowest_cm']),
                  highest_contact=max(rows, key=lambda row: row['contact_lowest_cm']),
                  sheets=pages,
                  replay_scope=report['scope'],
                  scope='Sampled replay geometry. Floor is the authoring reference plane; '
                        'contact extrema are not automatic failures or gameplay acceptance.')
    (output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(directory.name, len(rows), 'samples;', len(pages), 'sheets')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('renders', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    summarize(args.renders.resolve(strict=True), args.output.resolve())
