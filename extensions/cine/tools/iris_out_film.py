"""Write the Iris Out film preset: one sensual cut timed to the dance, danced four times.
Python 3.14, standard library.

  tools/iris_out_film.py [--eve VARIANT]

presets/iris-out.cine.json (the game window as it is) and presets/iris-out-9x16.cine.json (laid out
for a 9:16 frame). Same cut, same looks, same timing. Four passes of the same
16.67 s dance (the shared CSS performance css.eve.dance.irisout), each pass with the whole cut:

  1. The bones alone on Eve: skeleton, virtual, IK and spring bones and sockets, her body
     hidden; no clay, no wireframe.
  2. Eve (the outfit she wears when the take starts), 3. Unholy Genessa, 4. Commander White:
     each opens on the wireframe over clay, then clay, then the finished look, two seconds
     each, and dances the rest in the finished look.

Every look is worn by its id only: no palette, no controls, no reset, so each outfit keeps the
customization saved on it in CSS. Between passes the wireframe sweeps over the picture for
one bar while the next outfit lands, and the dance starts again from the top the
moment the picture is back.

The cut follows the choreography of the reference clip (work/iris-out/sheet.png):
hand-to-face gestures at 0-2 s and 13-15 s (portrait, face and neckline), arms up with the
chest forward at 2-5 s and 10-11.6 s (chest framing), the hip sway, hand on hip and knee lift
at 5-7.6 s (hips from behind, low), the lean forward at 11.6-13 s (from a little above), the
arms-up V and the final crouch at 15-16.7 s (whole figure).

Track: audio/iris-out-x4.wav in the CINE folder, built from the clip's audio with a one-bar
gap between passes filled with its own drums (tools/iris_out_audio.sh). Never in the repository.
"""
import argparse
import json
from pathlib import Path

PRESETS = Path(__file__).resolve().parent.parent / 'presets'
BPM = 136.0                   # librosa's beat tracker on the full song
BAR = 4 * 60.0 / BPM          # 1.7647 s
CLIP = 16.667                 # the performance, 1000 frames at 60 Hz
GAP = 23.127 - 16.667         # one pass period (the beat-matched jump in tools/iris_out_audio.py) less the dance: 6.46 s
PASS = CLIP + GAP
WHOLE = 200                   # cm of height in frame for a whole-figure shot
UNHOLY = ('eins0fx.unholygenessa', 'genessa')
EVE = 'eins0fx.seduxtress'
WHITE = ('eins0fx.commanderwhite', 'original')
PERFORMANCE = 'css.eve.dance.irisout'


def bar(seconds):
    """Preset times are bars from 1; the tempo's offset is 0, bar 1 is the take's first frame."""
    return round(seconds / BAR + 1, 4)


def pose(azimuth, distance, height, aim, fov=40, roll=0):
    return {'azimuth': azimuth, 'distance': distance, 'height': height, 'aim': aim, 'fov': fov, 'roll': roll}


def framed(shot, track, fstop, fit, fixed):
    if track:
        shot['track'] = track
    if fit:
        shot['fit'] = fit
    if fstop:
        shot['fstop'] = fstop
    if fixed:
        shot['fixed'] = True
    return shot


def cut(at, start, end, track=None, fstop=None, fit=None, fixed=False):
    return framed({'at': bar(at), 'from': start, 'to': end, 'ease': 'smooth'}, track, fstop, fit, fixed)


def glide(at, end, track=None, fstop=None, fit=None, fixed=False):
    return framed({'at': bar(at), 'flow': True, 'to': end, 'ease': 'smooth'}, track, fstop, fit, fixed)


def pass_shots(start, scale=1.0, wings=False):
    """The cut for one pass. `start` is the pass's first second.

    Reviewed against the take of 7 Oct (21:22): chest framings are measured from her neck so
    her face and the neckline down to under the bust share the frame; the hips are taken from
    a side-back three-quarter sweeping behind, which keeps a long ponytail off them, and that
    shot may turn away from something in the way; whole-figure shots fill the frame. `wings`:
    the look has wings on her back (Unholy Genessa), so her hips are taken lower and more from
    the side, under them: from straight behind a wing sweeps across the lens."""
    s = start
    f = lambda cm: round(cm * scale)
    return [
        # 0-2: hand to face. Face and neckline, close, the camera drifting across her front.
        cut(s + 0.0, pose(20, 300, 4, 0), pose(-10, 295, 4, 0), 'head', 2.8, f(55)),
        # 2-5: arms up, chest forward, hips side to side. Face down to under the bust, from a
        # little below her eye line, coming round to her other side.
        glide(s + 2.0, pose(-25, 280, -6, -10), 'neck', 2.8, f(75)),
        # 5-7.6: hand on hip, the sway, the knee lift. Hips from her side-back, low, sweeping
        # behind her as she sways.
        # The sweep stops short of straight behind her: there the lens meets whatever stands
        # at her back (a rock at the 7 Oct spot blacked out the shot's last second).
        (cut(s + 5.0, pose(105, 300, -10, -4), pose(145, 290, -10, -4), 'pelvis', 4.0, f(90)) if wings else
         cut(s + 5.0, pose(130, 300, -8, -6), pose(165, 290, -8, -6), 'pelvis', 4.0, f(90))),
        # 7.6-10: pointing and face gestures. Whole figure, three-quarter front.
        cut(s + 7.6, pose(35, 520, 2, 0), pose(10, 510, 2, 0), 'body', fit=185),
        # 10-11.6: arms crossed over her chest, then stretched behind her head. Face and chest,
        # straight on, pushing in.
        cut(s + 10.0, pose(0, 300, -4, -8), pose(8, 260, -4, -8), 'neck', 2.8, f(80)),
        # 11.6-13: the lean forward, knees together. From a little above, looking down to her.
        glide(s + 11.6, pose(15, 240, 22, -6), 'neck', 2.8, f(68)),
        # 13-15: fingers by her face. Portrait, the camera crossing her front the other way.
        cut(s + 13.0, pose(-15, 290, 4, 0), pose(25, 285, 4, 0), 'head', 2.8, f(55)),
        # 15-16.7: arms up in a V, then the crouch. Whole figure, a little low.
        cut(s + 15.0, pose(-5, 540, -2, 0), pose(5, 530, -4, 0), 'body', fit=185),
    ]


TITLES = {'rig': 'Rig', 'eve': 'Eve', 'unholy': 'Unholy Genessa', 'white': 'Commander White'}
OUTFITS = {'eve': ('current', None), 'unholy': UNHOLY, 'white': WHITE}


def film(order, preset_id, name, audio, static=False):
    """One film from a pass order: 'rig' (the bones alone on the outfit she wears, the
    instrumental), 'eve' (the outfit she wears at F8), 'unholy', 'white'. `static`: one camera
    for the whole take, the one framed in Cine World at F8 ("view"), like the reference clip."""
    starts = [k * PASS for k in range(len(order))]
    shots, looks, steps = [], [], []
    bones = ['no_body', 'skeleton', 'virtual_bones', 'ik_bones', 'springs', 'sockets']
    wire, clay = ['clay', 'wireframe'], ['clay']
    for k, (kind, s) in enumerate(zip(order, starts)):
        title = TITLES[kind]
        if k == 0:
            # The opening look: the outfit she wears at F8 (Eve, and the rig on it), or the
            # film's own outfit, worn as saved in CSS.
            outfit, variant = OUTFITS.get(kind, ('current', None))
            opening = {'at': bar(s), 'outfit': outfit, 'title': title}
            if variant: opening['variant'] = variant
            looks.append(opening)
            if kind == 'rig':
                steps.append({'at': bar(0), 'layers': bones, 'beats': 1})
        else:
            g = s - GAP
            # The gap before the pass: one static shot, front three-quarter at her eye line, the
            # whole of her. The next look is staged on it before she dances.
            shots.append(cut(g, pose(18, 560, 8, 6), pose(18, 560, 8, 6), 'body', fit=205, fixed=True))
            prev = order[k - 1]
            same_outfit = kind == 'rig' or (kind == 'eve' and prev in ('rig', 'eve'))
            if same_outfit:
                looks.append({'at': bar(g + 0.6), 'title': title})
                landed = g + 0.6
            else:
                # The cover sweeps in from g + 0.6 s, the outfit lands under it, the picture is back at g + 2.2 s.
                outfit, variant = OUTFITS[kind]
                step = {'at': bar(g + 2.2), 'outfit': outfit, 'title': title, 'wipe': 0.5, 'hold': 1.1, 'performance': False}
                if variant: step['variant'] = variant
                looks.append(step)
                landed = g + 2.2
            looks.append({'at': bar(s), 'performance': True})
            if kind == 'rig':
                # From her look to the bones: the wireframe, then her body goes and the bones stay.
                steps += [{'at': bar(landed), 'layers': wire, 'beats': 2, 'labels': False},
                          {'at': bar(s - 1.6), 'layers': bones, 'beats': 2}]
            else:
                # The wireframe (from the bones, or on the outfit just landed), clay, then the
                # look, complete before the pass's first beat.
                steps += [{'at': bar(landed), 'layers': wire, 'beats': 2, 'labels': False},
                          {'at': bar(s - 3.0), 'layers': clay, 'beats': 2, 'labels': False},
                          {'at': bar(s - 1.6), 'layers': [], 'beats': 2}]
        if kind == 'rig':
            # The bones alone, one locked shot straight on, the way the clip is filmed. Set back
            # and a little high: the head bones and sockets reach above her head.
            shots.append(cut(s, pose(0, 600, 10, 6), pose(0, 600, 10, 6), 'body', fit=235, fixed=True))
        else:
            shots += pass_shots(s, wings=(kind == 'unholy'))
    if order[-1] == 'rig':
        # Ending on the bones: they fade back into her finished look over the song's last two bars.
        steps.append({'at': bar(starts[-1] + CLIP + 0.2), 'layers': [], 'beats': 3})
    if static:
        shots = [{'at': bar(0), 'view': True}]
    names = [TITLES[k] if k != 'rig' else 'the bones alone (on the instrumental)' for k in order]
    return {
        'schema': 1,
        'id': preset_id,
        'name': name,
        'description': ('The Iris Out dance ' + {2: 'twice', 3: 'three times', 4: 'four times'}[len(order)] + ', ' + ('on one still camera, the view you framed before rolling: ' if static else 'framed for the game window as it is: ') + ', then '.join(names) + '. Each look is staged before she dances '
                        '(wireframe, clay, then the look) and worn as saved in CSS; Eve is the outfit worn at F8. '
                        'Needs CSS, the shared locomotion pack and the three outfits. Set Breakdown to From the preset.'),
        'kind': 'sequence',
        'reference': 'capsule',
        'tempo': {'bpm': BPM, 'offset': 0.0, 'beats_per_bar': 4},
        'end': bar(starts[-1] + CLIP + 2 * BAR),   # the song's two-bar ending fades out before the take ends
        'audio': audio,
        'performance': PERFORMANCE,
        'idle': 'eve',
        'cover': [['wireframe']],
        'shots': shots,
        'looks': looks,
        'breakdown': {'steps': steps},
    }


FILMS = [
    (('rig', 'eve', 'unholy', 'white'), 'iris-out', 'Iris Out', 'audio/iris-out-x4.wav'),
    (('eve', 'rig', 'unholy', 'white'), 'iris-out-2', 'Iris Out 2', 'audio/iris-out-2-x4.wav'),
    (('eve', 'rig', 'unholy', 'white'), 'iris-out-static', 'Iris Out Static', 'audio/iris-out-2-x4.wav', True),
    (('rig', 'eve', 'unholy', 'white'), 'iris-out-static-1', 'Iris Out Static 1', 'audio/iris-out-x4.wav', True),
    # One look, then the bones of that same look.
    (('unholy', 'rig'), 'iris-out-unholy-rig', 'Iris Out Unholy Genessa + Rig', 'audio/iris-out-look-rig.wav'),
    (('eve', 'rig'), 'iris-out-eve-rig', 'Iris Out Eve + Rig', 'audio/iris-out-look-rig.wav'),
    (('white', 'rig'), 'iris-out-white-rig', 'Iris Out Commander White + Rig', 'audio/iris-out-look-rig.wav'),
]


def portrait(wide):
    """The same film laid out for a 9:16 frame: every shot is framed by height or on her head,
    so it fits a narrow frame as it is."""
    import copy
    tall = copy.deepcopy(wide)
    tall['id'], tall['name'] = 'iris-out-9x16', 'Iris Out 9:16'
    tall['description'] = tall['description'].replace('framed for the game window as it is', 'in a 9:16 frame (Reels): record the whole screen and crop to the middle')
    keys = list(tall)
    at = keys.index('performance') + 1
    return {**{k: tall[k] for k in keys[:at]}, 'frame': '9:16', **{k: tall[k] for k in keys[at:]}}


def dump(preset):
    out, keys = ['{'], list(preset)
    for key in keys:
        value, tail = preset[key], '' if key == keys[-1] else ','
        if key in ('shots', 'looks'):
            out.append(f'  "{key}": [')
            out += ['    ' + json.dumps(item) + (',' if i < len(value) - 1 else '') for i, item in enumerate(value)]
            out.append('  ]' + tail)
        elif key == 'breakdown':
            out.append('  "breakdown": {"steps": [')
            steps = value['steps']
            out += ['    ' + json.dumps(item) + (',' if i < len(steps) - 1 else '') for i, item in enumerate(steps)]
            out.append('  ]}' + tail)
        else:
            out.append(f'  "{key}": {json.dumps(value)}{tail}')
    out.append('}')
    return '\n'.join(out) + '\n'


def main():
    args = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    args.parse_args()
    built = []
    for order, preset_id, name, audio, *static in FILMS:
        wide = film(order, preset_id, name, audio, *static)
        built.append(wide)
        if preset_id == 'iris-out':
            built.append(portrait(wide))
    for preset in built:
        if len(preset['description']) > 512:
            raise SystemExit(f"{preset['id']}: description is {len(preset['description'])} characters, the limit is 512")
        (PRESETS / f"{preset['id']}.cine.json").write_text(dump(preset))
        print(preset['id'], len(preset['shots']), 'shots', len(preset['looks']), 'look steps, end bar', preset['end'])


if __name__ == '__main__':
    main()
