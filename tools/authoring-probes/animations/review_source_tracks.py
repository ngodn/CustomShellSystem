"""Plot decoded Eve source joints before retargeting. Python 3.14, NumPy, Matplotlib."""
import argparse
import json
from pathlib import Path

import numpy as np


def vector(value, quat=False):
    return np.array([value[k] for k in ('XYZW' if quat else 'XYZ')], dtype=float)


def sample(bone, channel, times, fallback, frame, frame_count):
    keys = bone[channel]
    quat = channel == 'rotations'
    if not keys:
        return vector(bone['reference'][fallback], quat)
    if len(keys) == 1:
        return vector(keys[0], quat)
    indices = bone[times] or np.linspace(0, frame_count - 1, len(keys)).tolist()
    end = min(max(int(np.searchsorted(indices, frame, side='right')), 1), len(keys)-1)
    alpha = np.clip((frame-indices[end-1])/(indices[end]-indices[end-1]), 0, 1)
    a, b = vector(keys[end-1], quat), vector(keys[end], quat)
    if quat and np.dot(a, b) < 0:
        b = -b
    result = a * (1-alpha) + b * alpha
    return result / np.linalg.norm(result) if quat else result


def rotation(q):
    x, y, z, w = q / np.linalg.norm(q)
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])


def selected_bones(data):
    names = {'Root', 'Bip001', 'Bip001-Pelvis', 'Bip001-Spine', 'Bip001-Spine1',
             'Bip001-Spine2', 'Bip001-Neck', 'Bip001-Head'}
    names.update(f'Bip001-{side}-{part}' for side in ('L', 'R')
                 for part in ('Clavicle', 'UpperArm', 'Forearm', 'Hand', 'Thigh', 'Calf', 'Foot', 'Toe0'))
    selected = [i for i, bone in enumerate(data['bones']) if bone['name'] in names]
    if len(selected) != len(names):
        raise ValueError('Source skeleton lacks expected Eve body joints')
    if any(data['bones'][i]['parent'] not in selected and data['bones'][i]['parent'] != -1 for i in selected):
        raise ValueError('Body joint selection is missing ancestors')
    return selected


def pose(data, selected, seconds):
    frame = min(seconds, data['duration']) / data['duration'] * (data['frameCount']-1)
    world = {}
    for i in selected:
        bone = data['bones'][i]
        q = sample(bone, 'rotations', 'rotationTimes', 'Rotation', frame, data['frameCount'])
        p = sample(bone, 'translations', 'translationTimes', 'Translation', frame, data['frameCount'])
        s = sample(bone, 'scales', 'scaleTimes', 'Scale3D', frame, data['frameCount'])
        if not np.allclose(s, 1, atol=0.0001):
            raise ValueError('Non-unit source body scale needs an Unreal transform evaluator')
        transform = np.eye(4)
        transform[:3, :3] = rotation(q)
        transform[:3, 3] = p
        world[i] = world[bone['parent']] @ transform if bone['parent'] >= 0 else transform
    return {i: value[:3, 3] for i, value in world.items()}


def main():
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.animation import FFMpegWriter

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--video', action='store_true')
    args = parser.parse_args()
    paths = sorted(args.work.glob('*/source-tracks.json'))
    if not paths or len(paths) > 8:
        raise ValueError('Expected one to eight source clips')
    clips = [(path.parent.name, json.loads(path.read_text())) for path in paths]
    fig = plt.figure(figsize=(16, 9), facecolor='#16191d')
    panels = []
    evidence = []
    for index, (name, data) in enumerate(clips):
        if data.get('interpolation', 'Linear') != 'Linear' or data['rateScale'] != 1:
            raise ValueError('Preview currently requires linear interpolation and source rate 1')
        selected = selected_bones(data)
        frames = [pose(data, selected, t) for t in np.linspace(0, data['duration'], data['frameCount'])]
        all_points = np.array([p for frame in frames for p in frame.values()])
        if not np.isfinite(all_points).all():
            raise ValueError('Non-finite joint positions')
        evidence.append({'clip': name, 'frames': data['frameCount'], 'duration': data['duration'],
                         'joints': len(selected), 'bounds_cm': [all_points.min(0).tolist(), all_points.max(0).tolist()]})
        ax = fig.add_subplot(2, 4, index+1, projection='3d', facecolor='#16191d')
        ax.set_title(f'{name}\n{data["duration"]:.2f} seconds', color='white', fontsize=10)
        center = (all_points.min(0)+all_points.max(0))/2
        extent = max(np.ptp(all_points, axis=0))*0.55
        ax.set(xlim=(center[0]-extent, center[0]+extent), ylim=(center[1]-extent, center[1]+extent),
               zlim=(center[2]-extent, center[2]+extent))
        ax.view_init(elev=12, azim=-55)
        ax.set_box_aspect((1, 1, 1)); ax.set_axis_off()
        lines = []
        for i in selected:
            parent = data['bones'][i]['parent']
            if parent < 0 or data['bones'][parent]['name'] in ('Root', 'Bip001'):
                continue
            color = '#69c7ef' if '-L-' in data['bones'][i]['name'] else '#efbe8c'
            line, = ax.plot([], [], [], color=color, linewidth=3, marker='o', markersize=3)
            lines.append((line, i, parent))
        panels.append((data, selected, lines))
    fig.suptitle('Eve source motion | joint preview, before retargeting or skinning', color='white')
    fig.tight_layout()

    def draw(seconds):
        for data, selected, lines in panels:
            positions = pose(data, selected, seconds % data['duration'])
            for line, i, parent in lines:
                points = np.stack([positions[parent], positions[i]])
                line.set_data_3d(points[:, 0], points[:, 1], points[:, 2])

    for t in (0, 1, 3, 5):
        draw(t); fig.savefig(args.work / f'joints-{t}.png', dpi=110)
    if args.video:
        writer = FFMpegWriter(fps=12)
        with writer.saving(fig, str(args.work/'source-motion.mp4'), dpi=90):
            for frame in range(96):
                draw(frame/12)  # Keep original clip timing, loop short cycles.
                writer.grab_frame()
    (args.work/'source-joints.json').write_text(json.dumps(evidence, indent=2)+'\n')


if __name__ == '__main__':
    main()
