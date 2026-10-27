"""Write the two Commander White release presets from one cut. Python 3.14, standard library.

  tools/commander_white_film.py

presets/commander-white-release.cine.json is the film for a wide frame;
presets/commander-white-release-9x16.cine.json is the same cut for a 9:16 frame (Reels). They
share every look step, title, beat and shot. Edit the cut here and run this again, so the two
never drift apart.

Times are bars of the 124 BPM track (bar 1 starts 0.87 s in).

What the owner of the outfit asked for, and this file keeps:
- the frame split on her body, the mesh side taking 37.5 percent of what is seen of her, on the
  whole-figure parts only; every close shot is the finished picture, no layers;
- only the white (the original palette), crimson and obsidian; no figure sliders; motion presets
  natural on the chest and soft on the rest;
- no look step uses "reset": it re-applies her whole look and the game falls back to its own
  idle for a moment before hers returns;
- the lace is toggled only inside the neckline framing, which ends above the bust line; the
  top is never toggled;
- the seated recline is shot only from her open left side and behind, ending on a close
  profile of her face and shoulders, and is "fixed" so nothing can turn the camera to her front.
"""
import copy
import json
from pathlib import Path

PRESETS = Path(__file__).resolve().parent.parent / 'presets'
OUTFIT = 'eins0fx.commanderwhite'
WHOLE = 205        # cm of height in frame for a whole-figure shot: she is about 180 in her boots
SHARE = 0.375      # of her width on screen that the mesh side takes


def pose(azimuth, distance, height, aim, fov=40, roll=0):
    return {'azimuth': azimuth, 'distance': distance, 'height': height, 'aim': aim, 'fov': fov, 'roll': roll}


def framed(shot, track, fstop, fit):
    """track: the part of her the shot is measured from. fit: the height of picture at her, in cm."""
    if track:
        shot['track'] = track
    if fit:
        shot['fit'] = fit
    if fstop:
        shot['fstop'] = fstop
    return shot


def cut(at, start, end, track=None, fstop=None, fit=None):
    return framed({'at': at, 'from': start, 'to': end, 'ease': 'smooth'}, track, fstop, fit)


def glide(at, end, track=None, fstop=None, fit=None):
    """Flows on from the shot before, as one spline with it."""
    return framed({'at': at, 'flow': True, 'to': end, 'ease': 'smooth'}, track, fstop, fit)


def look(at, title=None, palette=None, controls=None, idle=None, physics=None):
    step = {'at': at}
    if physics:
        step['physics'] = physics
    if palette:
        step['palette'] = palette
    if controls:
        step['controls'] = controls
    if idle:
        step['idle'] = idle
    if title is not None:
        step['title'] = title
    return step


def dump(preset):
    """One shot, look step or breakdown step per line."""
    out, keys = ['{'], list(preset)
    for key in keys:
        value, tail = preset[key], '' if key == keys[-1] else ','
        if key in ('shots', 'looks'):
            out.append(f'  "{key}": [')
            out += ['    ' + json.dumps(item) + (',' if i < len(value) - 1 else '') for i, item in enumerate(value)]
            out.append('  ]' + tail)
        elif key == 'breakdown':
            steps = value['steps']
            out.append('  "breakdown": {"steps": [')
            out += ['    ' + json.dumps(item) + (',' if i < len(steps) - 1 else '') for i, item in enumerate(steps)]
            out.append('  ]}' + tail)
        else:
            out.append(f'  "{key}": {json.dumps(value)}' + tail)
    out.append('}')
    return '\n'.join(out) + '\n'


def film():
    tempo = {'bpm': 124, 'offset': 0.87, 'beats_per_bar': 4}
    # Every shot is framed on a part of her body ("track"): its height and aim are centimetres
    # above that part, and the camera follows it. Head is the joint under her skull (her eyes
    # are about 8 above it), neck the base of her neck, chest bust height, pelvis her hips,
    # knees the middle between her knees, body the middle of her from head to feet.
    # "fit" is the height of picture at her: the lens follows from it, so the camera stands
    # well back on a long lens and a whole-figure shot shows her from head to foot.
    shots = [
        # intro: a portrait. It opens on her hair and ponytail from behind and comes slowly round
        # her, past her profile, to settle on her face: head, hair and shoulders in frame the
        # whole way, the background soft.
        cut(1, pose(205, 300, 6, 0), pose(150, 295, 5, 0), 'head', 2.8, 85),
        glide(3.25, pose(95, 290, 3, 0), 'head', 2.8, 85),
        glide(5.5, pose(45, 280, 2, 1), 'head', 2.8, 80),
        glide(7.75, pose(15, 260, 0, 6), 'head', 3.2, 58),
        # palettes: the whole figure, a new side for each, and each change exactly on its cut.
        # A palette change costs the wardrobe one long frame (about 45 ms): on a cut nobody
        # sees it, in the middle of a move it shows as a tick.
        cut(10, pose(40, 560, 0, 0), pose(22, 550, 0, 0), 'body', fit=WHOLE),
        cut(11, pose(-20, 550, 0, 0), pose(-38, 545, 0, 0), 'body', fit=WHOLE),
        cut(12, pose(150, 560, 2, 0), pose(168, 550, 2, 0), 'body', fit=WHOLE),
        cut(13, pose(-70, 555, 4, 0), pose(-88, 550, 4, 0), 'body', fit=WHOLE),
        # pieces: one pass down her body, each piece coming off as the camera reaches it
        cut(14, pose(24, 330, -4, -4), pose(8, 320, -2, -4), 'chest', fit=105),     # coat, gloves: shoulders, arms, hands
        glide(15, pose(34, 230, -6, 4), 'head', fit=50),                            # hair pieces: her head, from a little below
        glide(15.75, pose(22, 190, 4, 4), 'neck', fit=30),                          # collar, lace: the neckline; the frame ends above the bust line
        glide(16.5, pose(10, 190, 4, 4), 'neck', fit=30),                           # held there while the lace goes and comes back
        glide(17, pose(18, 320, 0, 2), 'pelvis', fit=90),                           # belt, skirt: waist and hips
        glide(18, pose(6, 330, 0, 2), 'knees', fit=100),                            # boots: legs
        # dressed again, in full view: half a circle round her
        cut(19, pose(200, 560, 0, 0), pose(110, 545, 0, 0), 'body', fit=WHOLE),
        glide(20.5, pose(20, 540, 0, 0), 'body', fit=WHOLE),
        # the rig: the other half
        cut(22, pose(0, 540, 0, 0), pose(-90, 535, 0, 0), 'body', fit=WHOLE),
        glide(24, pose(-180, 530, 0, 0), 'body', fit=WHOLE),
        # head to toe: one slow rise from her boots to her face, each part named as it passes
        cut(26, pose(20, 330, 0, 0), pose(10, 325, 0, 2), 'knees', fit=100),
        glide(27, pose(-5, 320, 0, 2), 'pelvis', fit=95),
        glide(28, pose(-18, 320, 0, 0), 'chest', fit=95),
        glide(29, pose(-28, 290, 2, 2), 'head', fit=70),
        # mixed looks: a look a bar, the whole figure
        cut(30, pose(-42, 560, 0, 0), pose(-12, 540, 0, 0), 'body', fit=WHOLE),
        cut(31, pose(202, 560, 0, 0), pose(162, 545, 0, 0), 'body', fit=WHOLE),
        cut(32, pose(30, 560, 0, 0), pose(2, 545, 0, 0), 'body', fit=WHOLE),
        cut(33, pose(-22, 520, -70, 0, 40, 4), pose(10, 500, -68, 0, 40, 2), 'body', fit=215),
        # two kneeling idles, the whole of her: framed on her hips, wherever the pose puts her
        cut(34, pose(34, 450, 40, 10), pose(0, 430, 40, 10), 'pelvis', fit=170),
        cut(35, pose(150, 450, 45, 10), pose(186, 435, 45, 10), 'pelvis', fit=170),
        # Seated recline, two bars, shot the way the approved sitting takes were: from her open
        # left side and behind only, measured from her head, starting high behind her shoulder
        # and gliding down to a close profile of her face and shoulders.
        dict(cut(36, pose(240, 300, 100, -25, 44), pose(288, 175, 3, -2, 32), 'head', 2.8), fixed=True),
        # outro: one slow move in to the front, all of her in frame
        cut(38, pose(50, 580, 2, 0), pose(15, 560, 2, 0), 'body', fit=210),
        glide(40, pose(0, 540, 2, 0), 'body', fit=WHOLE),
    ]
    assert all(a['at'] < b['at'] for a, b in zip(shots, shots[1:]))
    pieces = ('coat', 'top', 'skirt', 'leotard', 'lace', 'collar', 'belt', 'glove_left', 'glove_right', 'boot_left', 'boot_right',
              'hair_band', 'ornament_left', 'ornament_right', 'outfit_visible')
    everything_on = {p: 1 for p in pieces}
    looks = [
        # intro: her as she ships. Motion for the whole film: natural on the chest, soft on the
        # rest. Sizes and muscles are never touched.
        # White is the outfit's original palette.
        {'at': 0.55, 'outfit': OUTFIT, 'variant': 'original', 'palette': 'original', 'controls': everything_on, 'title': 'Commander White',
         'physics': {'chest_motion': 'natural', 'glute_motion': 'soft', 'thigh_motion': 'soft', 'belly_motion': 'soft', 'hair_motion': 'natural'}},
        # the three palettes
        look(10, 'Palette  //  Crimson', 'crimson'),
        look(11, 'Palette  //  Obsidian', 'obsidian'),
        look(12, 'Palette  //  White', 'original'),
        look(13, 'Palette  //  Obsidian', 'obsidian'),
        # the pieces come off as the camera reaches them. The lace goes and comes back inside
        # the neckline framing, before the camera pulls back. The top stays on.
        look(14.1, 'Pieces  //  Coat off', controls={'coat': 0}),
        look(14.55, 'Pieces  //  Gloves off', controls={'glove_left': 0, 'glove_right': 0}),
        look(15.4, 'Pieces  //  Hair pieces off', controls={'hair_band': 0, 'ornament_left': 0, 'ornament_right': 0}),
        look(15.9, 'Pieces  //  Collar off', controls={'collar': 0}),
        look(16.3, 'Pieces  //  Lace off', controls={'lace': 0}),
        look(16.8, 'Pieces  //  Lace on', controls={'lace': 1}),
        look(17.55, 'Pieces  //  Belt off', controls={'belt': 0, 'lace': 1}),
        look(17.8, 'Pieces  //  Skirt off', controls={'skirt': 0}),
        look(18.3, 'Pieces  //  Boots off', controls={'boot_left': 0, 'boot_right': 0}),
        # Boots, skirt and belt go back on while the camera is still on her legs: the wide shot
        # that follows opens from behind her, and she is covered before it does.
        look(18.7, 'Pieces  //  Boots, skirt and belt on', controls={'lace': 1, 'boot_left': 1, 'boot_right': 1, 'skirt': 1, 'belt': 1}),
        # the rest back on, in full view
        look(19.5, 'Pieces  //  Collar and gloves on', controls={'collar': 1, 'glove_left': 1, 'glove_right': 1}),
        look(20.25, 'Pieces  //  Coat on', controls={'coat': 1}),
        look(20.9, 'Pieces  //  Hair pieces on', controls={'hair_band': 1, 'ornament_left': 1, 'ornament_right': 1}),
        # the rig, in white
        look(22, 'Rig', 'original'),
        # head to toe, in white: each part named as the camera passes it
        look(26, 'Boots'),
        look(27, 'Belt and skirt'),
        look(28, 'Coat and gloves'),
        look(29, 'Collar and hair pieces'),
        # mixed looks: a palette with pieces left off
        look(30, 'Obsidian  //  No coat', 'obsidian', {'coat': 0}),
        look(31, 'Crimson  //  No skirt', 'crimson', {'coat': 1, 'skirt': 0}),
        look(32, 'White  //  No gloves, no collar', 'original', {'skirt': 1, 'glove_left': 0, 'glove_right': 0, 'collar': 0}),
        look(33, 'Obsidian  //  No coat, no skirt, no gloves', 'obsidian', {'coat': 0, 'skirt': 0, 'glove_left': 0, 'glove_right': 0, 'collar': 0, 'belt': 0}),
        # the idles, in white with every piece on but the skirt: on the ground it bunches and
        # clips through her legs. Pieces are set by name, never by a reset.
        look(34, 'Idle  //  Kneeling Bow', 'original', {**everything_on, 'skirt': 0}, idle='eve_idle_703'),
        look(35, 'Idle  //  Side Kneel', idle='eve_idle_713'),
        look(36, 'Idle  //  Seated Recline', idle='eve_idle_702'),
        # outro: clean, standing, the skirt back on
        look(38, '', controls={'skirt': 1}, idle='eve'),
    ]
    # What is on her body. IK and virtual bones float beside it, and a label for them points at air.
    everything = ['wireframe', 'skeleton', 'springs']
    split = {'layers': everything, 'split': 'body', 'share': SHARE, 'beats': 1}
    return {
        'schema': 1, 'id': 'commander-white-release', 'name': 'Commander White Release',
        'description': ('An 80 second release film for Commander White on a 124 BPM track, in long gliding moves: a portrait, her three palettes, each piece coming off as the '
                        'camera passes it, the rig, a rise from boots to face, mixed looks and three idles, each named by a title. On whole-figure parts the frame is split on her '
                        'body, mesh and rig beside the finished look; close shots are clean. Needs the Commander White outfit and CSS. Set Breakdown to From the preset.'),
        'kind': 'sequence', 'reference': 'capsule', 'tempo': tempo, 'end': 42, 'audio': 'audio/showreel.wav', 'idle': 'eve',
        'cover': [['wireframe']], 'shots': shots, 'looks': looks,
        # Mesh and rig on the whole-figure parts only. Close shots (the pass down her body, the
        # rise from boots to face, the recline) and the intro and outro are the finished picture.
        'breakdown': {'steps': [
            {'at': 10, **split},
            {'at': 13.75, 'layers': [], 'beats': 1},
            {'at': 19, **split},
            {'at': 25.75, 'layers': [], 'beats': 1},
            {'at': 30, **split},
            {'at': 35.75, 'layers': [], 'beats': 1},
        ]},
    }


def portrait(wide):
    """The same cut for a 9:16 frame. Every shot is framed by height ("fit") or on her head, so
    it fits a narrow frame as it is."""
    tall = copy.deepcopy(wide)
    tall['id'], tall['name'] = 'commander-white-release-9x16', 'Commander White Release 9:16'
    tall['description'] = ('The Commander White release film framed for 9:16 (Reels): the same cut as Commander White Release. Titles, labels and signature are laid out inside the '
                           '9:16 frame whatever guide is chosen; record the whole screen and crop to the middle. Needs the Commander White outfit and CSS. Set Breakdown to '
                           'From the preset.')
    keys = list(tall)
    at = keys.index('idle') + 1
    return {**{k: tall[k] for k in keys[:at]}, 'frame': '9:16', **{k: tall[k] for k in keys[at:]}}


def main():
    wide = film()
    for preset in (wide, portrait(wide)):
        if len(preset['description']) > 512:
            raise SystemExit(f"{preset['id']}: description is {len(preset['description'])} characters, the limit is 512")
        (PRESETS / f"{preset['id']}.cine.json").write_text(dump(preset))
        print(preset['id'], len(preset['shots']), 'shots', len(preset['looks']), 'look steps')


if __name__ == '__main__':
    main()
