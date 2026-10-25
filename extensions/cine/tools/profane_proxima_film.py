"""Write the Profane Proxima film preset: a close, sensual cut on the first 17 seconds of a
170 BPM track, ending where the drop hits (the title card is added afterwards, outside the game).
Python 3.14, standard library.

  tools/profane_proxima_film.py

presets/profane-proxima.cine.json, framed for the game window as it is, and
presets/profane-proxima-stills.cine.json, six held poses for screenshots (work/live/stills.py).

She stands in Eve's default idle the whole take. Only two things change, as asked: the glow
colour (the outfit's palettes) and the skin (Human or Proxima). Everything else stays as saved.

The song (Polyphia, "They Know Not What They Do", a line from the crucifixion, under a
painting of a fall from heaven) is cut on its own turning points, not on a bar grid: librosa
novelty peaks at 3.04, 6.33, 8.52, 10.44, 12.41, 14.88 and 15.98 s, and the hard drop, where
the music cuts out, at 16.85 s. Each section gets one shot and one change, held, so the changes
are spread across the film about two seconds apart. The glow warms as the song brightens
(silver, starlight, mist, bronze, Devout embers) and the skin turns Proxima, back to Human and
Proxima for good:

  0             Human, silver. High above and behind her, over her crown to her face in profile.
  3.04          Glow: cold starlight. Down her back to her hips from behind, the camera sinking.
  6.33          Skin: Proxima. Her face, front, tight, a slow push.
  8.52          Glow: ruined mist. Her hips from low behind.
  10.44         Skin: Human. Her whole figure from behind, square on like an altarpiece.
  12.41         Glow: Tiel bronze. Her face from three-quarters, tight.
  14.88         Skin Proxima, glow Devout embers: the idol, from the ground behind her, looking up.
  15.98         The last rise: a push into her face. (The finished film cuts in flashes here and
                slams the title onto the hard drop.) The push goes on under the title card.

Framing rule (the film goes to YouTube): nothing below her collarbone from the front or the
side. Front shots are tight on her face; the body is only ever seen from behind, within 20
degrees, and those shots are fixed, so a wall in the way brings the camera closer, never round
to her front. Azimuths never wrap, so the camera never spins across her front.
The skin swaps are not covered: CSS lands them within a few frames of their moment, and the
finished film puts a flash there.

Track: audio/profane-proxima.wav in the CINE folder (the song up to the drop). Never in the
repository.
"""
import json
from pathlib import Path

PRESETS = Path(__file__).resolve().parent.parent / 'presets'
BPM = 170.14                  # librosa beats over the intro: 48 beats from 0.093 s to the drop at 17.02 s
BEAT = 60.0 / BPM
BAR = 4 * BEAT                # 1.4106 s
OFFSET = 0.093                # the first downbeat
OUTFIT = 'eins0fx.thornyproxima'
IDLE = 'css.eve.idle.default'


def t(bar_no, beat=0.0):
    """Seconds of a bar (from 1) and beat (from 0) of the song."""
    return OFFSET + ((bar_no - 1) * 4 + beat) * BEAT


def at(seconds):
    """Preset times are bars from 1 on a grid with offset 0."""
    return round(seconds / BAR + 1, 4)


def pose(azimuth, distance, height, aim, roll=0, fov=40):
    return {'azimuth': azimuth, 'distance': distance, 'height': height, 'aim': aim, 'fov': fov, 'roll': roll}


def shot(seconds, start, end, track, fit, fstop=None, ease='smooth', fixed=False):
    s = {'at': at(seconds), 'from': start, 'to': end, 'ease': ease, 'track': track, 'fit': fit}
    if fstop:
        s['fstop'] = fstop
    if fixed:
        s['fixed'] = True
    return s


def glide(seconds, end, track, fit, fstop=None):
    s = {'at': at(seconds), 'flow': True, 'to': end, 'ease': 'smooth', 'track': track, 'fit': fit}
    if fstop:
        s['fstop'] = fstop
    return s


def look(seconds, **step):
    return {'at': at(seconds), **step}


def swap(seconds, variant, palette):
    """A bare skin swap that lands on the beat: CSS swaps a cached mesh in 10-130 ms."""
    return look(seconds, outfit=OUTFIT, variant=variant, palette=palette, cover=False, wipe=0.01, hold=0.12)


def build():
    shots, looks = [], []
    looks.append(swap(0.0, 'human', 'proxima_silver'))
    shots.append(shot(0.0, pose(176, 150, 95, 0, -8), pose(150, 125, 60, 0, -2), 'head', 36, 1.8, fixed=True))

    looks.append(look(3.04, palette='cold_starlight'))
    shots.append(shot(3.04, pose(192, 200, 40, -6, 3), pose(178, 230, -8, -6, 0), 'pelvis', 95, 2.8, fixed=True))

    looks.append(swap(6.33, 'proxima', 'cold_starlight'))
    shots.append(shot(6.33, pose(-6, 150, 4, 0, -3), pose(0, 128, 3, 0, 0), 'head', 30, 1.8))

    looks.append(look(8.52, palette='ruined_mist'))
    shots.append(shot(8.52, pose(172, 280, -24, -6), pose(190, 270, -30, -6), 'pelvis', 80, 4.0, fixed=True))

    looks.append(swap(10.44, 'human', 'ruined_mist'))
    shots.append(shot(10.44, pose(180, 470, 2, 0), pose(180, 410, -6, 0), 'body', 195, fixed=True))

    looks.append(look(12.41, palette='tiel_bronze'))
    shots.append(shot(12.41, pose(-40, 150, 6, 0, -4), pose(-26, 132, 4, 0, -1), 'head', 28, 1.8))

    looks.append(swap(14.88, 'proxima', 'devout_embers'))
    shots.append(shot(14.88, pose(180, 520, -70, 20), pose(180, 480, -80, 22), 'body', 205, fixed=True))

    shots.append(shot(15.98, pose(0, 150, 2, 0, -4), pose(0, 112, 2, 0, 0), 'head', 34, 2.0))

    end = 19.6       # 2.75 s past the hard drop, so the title card has picture under it
    return {
        'schema': 1,
        'id': 'profane-proxima',
        'name': 'Profane Proxima',
        'description': ('A close 17 second cut for Profane Proxima on the song\'s turning points, in Eve\'s default idle. '
                        'Only the glow colour and the skin change; body shots only from behind. '
                        'Track: audio/profane-proxima.wav in the CINE folder.'),
        'kind': 'sequence',
        'reference': 'capsule',
        'tempo': {'bpm': BPM, 'offset': 0.0, 'beats_per_bar': 4},
        'end': at(end),
        'audio': 'audio/profane-proxima.wav',
        'idle': IDLE,
        'shots': shots,
        'looks': looks,
    }


# The stills: six held poses for the release page, one every STILL seconds, each look landing
# at the start of its hold and captured (work/live/stills.py) once it has settled. The same
# framing rule as the film: the body from behind, the face tight.
STILL = 3.0
STILLS = [
    ('human', 'proxima_silver', pose(172, 140, 70, 0, -6), 'head', 36, 'crown'),
    ('human', 'cold_starlight', pose(180, 280, -26, -6), 'pelvis', 80, 'hips'),
    ('proxima', 'devout_embers', pose(-4, 130, 3, 0, -2), 'head', 30, 'face'),
    ('proxima', 'tiel_bronze', pose(-32, 135, 5, 0, -3), 'head', 28, 'profile'),
    ('proxima', 'devout_embers', pose(180, 480, -78, 22), 'body', 205, 'idol'),
    ('human', 'ember', pose(186, 230, 10, -6, 2), 'pelvis', 95, 'back'),
]


def build_stills():
    shots, looks = [], []
    for i, (variant, palette, at_pose, track, fit, _) in enumerate(STILLS):
        moment = i * STILL
        fixed = abs(abs(at_pose['azimuth']) - 180) <= 20
        shots.append(shot(moment, at_pose, at_pose, track, fit, 2.2, fixed=fixed))
        looks.append(swap(moment, variant, palette))
    return {
        'schema': 1,
        'id': 'profane-proxima-stills',
        'name': 'Profane Proxima Stills',
        'description': 'Six held poses of Profane Proxima for screenshots, three seconds each, in Eve\'s default idle. Body only from behind.',
        'kind': 'sequence',
        'reference': 'capsule',
        'tempo': {'bpm': BPM, 'offset': 0.0, 'beats_per_bar': 4},
        'end': at(len(STILLS) * STILL),
        'idle': IDLE,
        'shots': shots,
        'looks': looks,
    }


def main():
    preset = build()
    path = PRESETS / 'profane-proxima.cine.json'
    path.write_text(json.dumps(preset, indent=1) + '\n')
    print(path, len(preset['shots']), 'shots', len(preset['looks']), 'look steps', 'end bar', preset['end'])
    stills = build_stills()
    path = PRESETS / 'profane-proxima-stills.cine.json'
    path.write_text(json.dumps(stills, indent=1) + '\n')
    print(path, len(stills['shots']), 'stills')


if __name__ == '__main__':
    main()
