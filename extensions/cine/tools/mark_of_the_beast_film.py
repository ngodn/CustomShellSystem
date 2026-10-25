"""Write the Mark of the Beast takes: ten presets for one film on Polyphia's "Mark of the Beast",
for Profane Proxima and Unholy Genessa. Python 3.14, standard library.

  tools/mark_of_the_beast_film.py

The design, the song map, the lore and the live checklist are in docs/BEAST-GOAL.md. Each take
is one preset timed to its own slice of the song (tools/mark_of_the_beast_audio.py cuts the
slices), so a retake is one take; tools/mark_of_the_beast_post.py assembles the film, with the
narration and the title cards. Proxima's takes are shot in the KnightLady shell at the
Shattered Beacon, Genessa's in the Genessa shell at Marrow Keep. Every take is shot in the one shell: the
outfits, the two original shells included, are CSS looks the take wears and gives back.

Framing rule (YouTube): the body only from behind, within 20 degrees; front and side shots on
the face and neck. Everything she does restores at the end of each take.
"""
import json
from pathlib import Path

PRESETS = Path(__file__).resolve().parent.parent / 'presets'
BPM = 92.3                    # the groove (librosa over the whole song); the grid only sets the bar unit
BAR = 4 * 60.0 / BPM          # 2.6002 s
SONG_END = 269.35
IDLE = 'css.eve.idle.default'
SEATED_AT = 255.259           # 4:15.259: the Seated Recline for both endings (the user's choice)

PP = 'eins0fx.thornyproxima'
UG = 'eins0fx.unholygenessa'
ORIGINAL = 'css.original_shells'       # the game's own shells as CSS looks: 'knightlady' is Proxima, 'genessa' is Genessa
PX = 'css.profaneproxima.face.'
GX = 'css.unholygenessa.face.'
PERF = 'css.eve.performance.'
AFTER_BRUTE = PERF + 'afterbrutebattle'          # 13.67 s
P801 = PERF + 'example801'                       # 18.85 s
BLACK_HOLE = 'css.eve.dance.blackhole'           # 20 s
BASS_DOWN_LOW = 'css.eve.dance.bassdownlow'      # 12.67 s
SPIN = 'css.eve.dance.jangwonyoungspin'          # 3.67 s
IRIS_OUT = 'css.eve.dance.irisout'               # 16.67 s at 136 BPM
LILY_CATWALK = 'css.eve.catwalk.lilycatwalk'     # 12 s
IRIS_RATE = round(BPM / 136.0, 4)                # the dance at the song's tempo: 0.6787

# The game's own moves (SKEL_Human_Skeleton montages; see docs/BEAST-GOAL.md for their lengths).
SHARED = '/Game/Sparta/Characters/Shells/_Shared/Animation/'
KNIGHT = '/Game/Sparta/Characters/Shells/KnightLady/Animation/Ability/'


def asset(folder, name):
    return f'{folder}{name}.{name.split("/")[-1]}'


KNEEL = asset(SHARED, 'Actions/A_Shared_Actions_Kneel_Montage')                                              # 4.27 s
KNEEL_LOOP = asset(SHARED, 'Actions/A_Player_Kneel_Loop_Montage')                                            # 8.5 s, loops
BLOCK = asset(SHARED, 'Actions/Block/Axeatana/A_Shared_Actions_BlockParry_Axeatana_Perfect_Block_01_Montage')  # 2.23 s
PARRY = asset(SHARED, 'Parry/AM_Player_Seal_Parry_Successful_01_A')                                          # 1.44 s
RIPOSTE = asset(SHARED, 'Parry/AM_Shells_InfSeal_Riposte_Brig_01_Shell')                                     # 1.9 s
SEAL = asset(SHARED, 'SlayerSeal/A_Shared_Attacks_SlayerSeal_Charge_Montage')                                # 1.67 s
BIO_START = asset(KNIGHT, 'A_KnightLady_Attacks_Ability_Start_Montage')                                     # 0.6 s
BIO_STAB = asset(KNIGHT, 'A_KnightLady_Attacks_Ability_Stab_Montage')                                       # 3.03 s


def pose(azimuth, distance, height, aim, roll=0, fov=40):
    return {'azimuth': azimuth, 'distance': distance, 'height': height, 'aim': aim, 'fov': fov, 'roll': roll}


class Take:
    """One preset: shots and look steps placed in song seconds, stored in bars of the take."""

    def __init__(self, id, name, start, end, who, about):
        self.id, self.name, self.start, self.end, self.who, self.about = id, name, start, min(end, SONG_END), who, about
        self.shots, self.looks = [], []

    def at(self, song_time):
        return round((song_time - self.start) / BAR + 1, 4)

    def shot(self, t, a, b, track, fit, fstop=None, fixed=False, ease='smooth'):
        s = {'at': self.at(t), 'from': a, 'to': b, 'ease': ease, 'track': track, 'fit': fit}
        if fstop:
            s['fstop'] = fstop
        if fixed:
            s['fixed'] = True
        self.shots.append(s)

    def look(self, t, **step):
        self.looks.append({'at': self.at(t), **step})

    def skin(self, t, variant, palette, **more):
        """Profane Proxima's skin lands on its beat, uncovered: a flash goes on it in post."""
        self.look(t, outfit=PP, variant=variant, palette=palette, cover=False, wipe=0.01, hold=0.12, **more)

    def preset(self):
        self.looks.sort(key=lambda l: l['at'])
        self.shots.sort(key=lambda s: s['at'])
        return {
            'schema': 1, 'id': self.id, 'name': self.name,
            'description': f'Mark of the Beast, {self.who}: {self.about} Song {self.start:.1f} to {self.end:.1f} s. '
                           f'Track: audio/{self.id}.wav in the CINE folder.',
            'kind': 'sequence', 'reference': 'capsule',
            'tempo': {'bpm': BPM, 'offset': 0.0, 'beats_per_bar': 4},
            'end': self.at(self.end), 'audio': f'audio/{self.id}.wav', 'idle': IDLE, 'performance': 'off',
            'shots': self.shots, 'looks': self.looks,
        }


# ---------------------------------------------------------------- the intros (the game's own shells)

def intro(t, mirror, shell):
    """The narration over the game's own shell (a CSS look), then the kneel under the title.
    `mirror` flips the sides so the two intros intercut without repeating an angle."""
    m = -1 if mirror else 1
    crown = shell == 'genessa'    # her crown of thorns fills a close shot from behind or above: keep those wider and to the side
    t.look(0.0, outfit=ORIGINAL, variant=shell)
    if crown:
        t.shot(0.0, pose(m * 205, 300, 35, -6), pose(m * 190, 270, 25, -4), 'neck', 100, 2.0, fixed=True)  # the crown against the sky, with her shoulders
    else:
        t.shot(0.0, pose(m * 205, 220, 55, -4), pose(m * 190, 200, 40, -2), 'head', 60, 2.0, fixed=True)   # the horns, from high behind
    t.shot(5.1, pose(-6 * m, 150, 4, 0), pose(0, 135, 3, 0), 'head', 36, 1.8)                             # "the Beast": the face
    t.shot(7.1, pose(95 * m, 150, 0, 0, 4 * m), pose(80 * m, 140, 0, 0, 2 * m), 'head', 34, 1.8)          # "the Man of Sin": profile
    if crown:
        t.shot(9.4, pose(135 * m, 200, 30, -6), pose(120 * m, 185, 20, -4), 'neck', 60, 2.0, fixed=True)   # "Daughter of Perdition": the crown in profile, from high
    else:
        t.shot(9.4, pose(170 * m, 180, 70, -6), pose(160 * m, 170, 50, -4), 'neck', 50, 2.0, fixed=True)   # "Daughter of Perdition": from above
    t.shot(11.6, pose(12 * m, 120, 2, 0, -3 * m), pose(4 * m, 105, 2, 0, 0), 'head', 26, 1.8, fixed=True)  # "the Antichrist": the eyes
    t.shot(13.9, pose(180, 700, -10, 0), pose(180, 420, -6, 0), 'body', 200, fixed=True)                   # "getting closer": the long push
    t.shot(24.3, pose(-20 * m, 140, 2, 0, -5 * m), pose(-8 * m, 128, 2, 0, -2 * m), 'head', 32, 1.8)     # "deliverance"
    # The bell: she kneels, and stays kneeling under the title (the CSS kneeling idle: the
    # game's own kneel montage is a kneel-and-rise action, it does not hold).
    t.look(29.5, idle='css.eve.idle.kneeling_bow')
    t.shot(29.5, pose(150 * m, 300, 10, -10), pose(115 * m, 260, -20, -8), 'body', 160, fixed=True)
    t.shot(36.0, pose(-60 * m, 200, 20, -14), pose(-30 * m, 180, 10, -10), 'head', 60, 2.4)              # the bowed crown from the side
    t.shot(41.2, pose(0, 260, 30, -20), pose(0, 200, 24, -16), 'head', 70 if crown else 50, 2.0)          # the name card: the bowed crown from above the grass
    # Her name card lands on her own look, not the game's shell: Profane Proxima on the Proxima
    # slam, Unholy Genessa on the Genessa slam. Uncovered, the slam's white flash hides the change.
    if crown:
        t.look(49.5, outfit=UG, variant='genessa', palette='moon', idle='css.eve.idle.kneeling_bow', expression=GX + 'serious', cover=False, wipe=0.01, hold=0.12)
    else:
        t.skin(41.2, 'proxima', 'devout_embers', idle='css.eve.idle.kneeling_bow', expression=PX + 'serious')
    t.shot(49.5, pose(-150 * m, 240, 20, -6), pose(-175 * m, 230, 10, -4), 'body', 170, fixed=True)
    t.look(52.5, idle=IDLE)                                                                                 # up before the groove


def p0():
    t = Take('beast-p0', 'Beast P0 Intro', 0.0, 54.0, "the game's own Proxima", 'the narration, then the kneel under the title.')
    intro(t, mirror=False, shell='knightlady')
    return t


def g0():
    t = Take('beast-g0', 'Beast G0 Intro', 0.0, 54.0, "the game's own Genessa", 'the narration, then the kneel under the title.')
    intro(t, mirror=True, shell='genessa')
    return t


# ---------------------------------------------------------------- Proxima

def p1():
    t = Take('beast-p1', 'Beast P1', 54.0, 96.0, 'Profane Proxima', 'After Brute Battle on the groove, the glow cooling from embers to silver, Low Lean, Side Kneel, ATOOL 7, the Reach pose on the accent.')
    t.skin(54.0, 'proxima', 'devout_embers', performance=AFTER_BRUTE, expression=PX + 'serious')
    # After Brute Battle leaps metres in a beat: wide frames, the camera high behind her.
    t.shot(54.0, pose(200, 520, 40, 4), pose(165, 480, 20, 6), 'body', 250, fixed=True)
    t.shot(60.0, pose(200, 300, 40, 0, -4), pose(165, 280, 30, 0, -1), 'neck', 110, 2.4, fixed=True)   # over her shoulder: her skin shows a lot from the front
    t.shot(64.0, pose(150, 420, -10, -4), pose(178, 400, -30, 0), 'body', 220, fixed=True)
    t.look(67.7, idle='css.eve.idle.low_lean', palette='ember', expression=PX + 'contempt')
    t.shot(67.9, pose(180, 260, 0, -6), pose(165, 250, 14, -4), 'neck', 60, 2.4, fixed=True)
    t.look(74.1, palette='tiel_bronze', expression=PX + 'sulk')
    t.shot(74.1, pose(-60, 150, 6, 0, 5), pose(-45, 138, 4, 0, 2), 'head', 30, 1.8)
    t.look(76.0, idle='css.eve.idle.side_kneel')
    t.shot(76.0, pose(190, 300, 30, -14), pose(168, 280, 24, -12), 'pelvis', 95, 4.0, fixed=True)   # the side kneel from above the grass
    t.look(80.0, performance=PERF + 'atool7', idle=IDLE)
    t.shot(80.0, pose(-170, 330, -10, 0), pose(-150, 300, -14, 0), 'body', 185, fixed=True)
    t.look(84.0, expression=PX + 'smile')
    t.shot(84.0, pose(20, 150, 2, 0, -3), pose(8, 132, 2, 0, 0), 'head', 32, 1.8)
    t.look(86.7, palette='proxima_silver', expression=PX + 'serious')
    t.shot(88.9, pose(180, 500, 10, 12), pose(180, 440, 0, 14), 'body', 205, fixed=True)   # low but above the grass
    t.look(92.6, performance='css.eve.photo.reach', expression=PX + 'surprised')
    t.shot(92.6, pose(-25, 170, 10, 0, -6), pose(-12, 150, 8, 0, -2), 'head', 40, 2.0)
    return t


def p2():
    t = Take('beast-p2', 'Beast P2', 150.8, 177.7, 'Profane Proxima', 'the hunt: block, parry, riposte, the Slayer Seal and the Biosampler on the double-time; then the breath, reclining.')
    t.skin(150.8, 'proxima', 'cold_starlight', montage=BLOCK, expression=PX + 'angry')
    t.shot(150.8, pose(-150, 260, -10, -4), pose(-130, 240, -14, -4), 'body', 170, fixed=True)
    t.shot(151.7, pose(160, 230, 10, -6), pose(175, 220, 4, -4), 'neck', 60, 2.4, fixed=True)
    t.look(153.1, montage=PARRY)
    t.shot(153.1, pose(-20, 170, 6, 0, -6), pose(-8, 150, 4, 0, -2), 'head', 38, 2.0)
    t.look(154.7, montage=RIPOSTE)
    t.shot(154.7, pose(200, 300, -30, -4), pose(170, 280, -40, -2), 'body', 180, fixed=True)
    t.look(156.9, montage=SEAL)
    t.shot(156.9, pose(-170, 260, 20, -8), pose(-150, 240, 10, -6), 'body', 175, fixed=True)
    t.look(158.7, montage=BIO_START)
    t.shot(158.7, pose(120, 220, 0, -4), pose(150, 210, -6, -2), 'chest', 80, 2.8, fixed=True)
    t.look(159.3, montage=BIO_STAB)
    t.shot(160.5, pose(-30, 150, 2, 0, -5), pose(-15, 135, 2, 0, -2), 'head', 34, 1.8)
    t.look(162.5, montage=BLOCK)
    t.shot(162.5, pose(180, 420, -60, 14), pose(180, 380, -70, 16), 'body', 200, fixed=True)
    # The breath: she reclines on the ground, in grass where the user sets her, so every camera
    # sits well above her and looks down; nothing at ground level.
    t.skin(164.7, 'human', 'ruined_mist', montage='', idle='css.eve.idle.reclining', expression=PX + 'daydream')
    t.shot(164.7, pose(95, 330, 120, -40, 3), pose(70, 300, 110, -40, 0), 'body', 170, 2.8, fixed=True)   # her profile from above
    t.shot(168.5, pose(25, 180, 95, -30, -4), pose(10, 160, 85, -30, -1), 'head', 40, 1.8)                 # her face from above
    t.look(171.2, expression=PX + 'kiss')
    t.shot(172.0, pose(-95, 330, 130, -45, 4), pose(-70, 300, 120, -45, 1), 'body', 160, 2.8, fixed=True)  # the other profile, higher
    t.look(175.4, expression=PX + 'tease-ya')
    t.shot(175.4, pose(5, 160, 85, -30, 2), pose(0, 140, 75, -30, 0), 'head', 32, 1.8)
    return t


def p3():
    t = Take('beast-p3', 'Beast P3', 209.7, 246.6, 'Profane Proxima', 'the fast section and the outro: Bass Down Low, the Spin, the Lily Catwalk, the Confident pose on the last accent.')
    t.skin(209.7, 'proxima', 'ember', idle=IDLE, expression=PX + 'serious')
    t.shot(209.7, pose(180, 520, -40, 8), pose(180, 440, -50, 10), 'body', 200, fixed=True)
    t.look(214.0, performance=BASS_DOWN_LOW)
    t.shot(214.0, pose(-150, 320, -20, 0), pose(-125, 290, -26, 0), 'body', 185, fixed=True)
    t.shot(217.5, pose(170, 250, -30, -6), pose(190, 240, -36, -6), 'pelvis', 88, 4.0, fixed=True)
    t.shot(221.0, pose(-25, 160, 6, 0, -5), pose(-10, 142, 4, 0, -2), 'head', 34, 1.8)
    t.look(224.4, palette='devout_embers', expression=PX + 'contempt')
    t.shot(224.4, pose(180, 450, -60, 14), pose(180, 400, -70, 16), 'body', 205, fixed=True)
    t.look(226.7, performance=SPIN)
    t.shot(226.7, pose(200, 330, -10, 0), pose(165, 300, -14, 0), 'body', 180, fixed=True)
    t.look(231.6, performance=LILY_CATWALK)
    t.shot(231.6, pose(180, 420, -30, 4), pose(180, 300, -24, 2), 'body', 190, fixed=True)
    t.shot(236.0, pose(-160, 260, -40, -6), pose(-175, 250, -46, -4), 'pelvis', 90, 4.0, fixed=True)
    t.shot(240.0, pose(-30, 150, 4, 0, -4), pose(-12, 135, 3, 0, -1), 'head', 34, 1.8)
    t.look(243.6, idle=IDLE)
    t.look(245.8, performance='css.eve.photo.confident', expression=PX + 'contempt')
    t.shot(245.8, pose(10, 140, 2, 0, 2), pose(0, 126, 2, 0, 0), 'head', 30, 1.8)
    return t


def p4():
    t = Take('beast-p4', 'Beast P4', 246.6, SONG_END, 'Profane Proxima', 'the drop-out on her face, the ending from the ground, then the Seated Recline.')
    t.skin(246.6, 'human', 'devout_embers', idle=IDLE, expression=PX + 'contempt')
    t.shot(246.6, pose(0, 140, 2, 0, -2), pose(0, 118, 2, 0, 0), 'head', 30, 1.8)
    t.look(250.0, expression=PX + 'smile')
    t.skin(253.6, 'proxima', 'devout_embers', expression=PX + 'serious')
    t.shot(253.6, pose(180, 560, 10, 12), pose(180, 500, 0, 14), 'body', 210, fixed=True)   # low but above the grass
    # 4:15.26: the Seated Recline for the sensual ending (the user's choice).
    t.look(SEATED_AT, idle='css.eve.idle.seated_recline', expression=PX + 'daydream')
    t.shot(SEATED_AT, pose(95, 340, -20, -30, 3), pose(70, 300, -10, -30, 0), 'body', 160, 2.8, fixed=True)   # her profile, reclining
    t.shot(259.0, pose(25, 170, 40, -10, -4), pose(10, 150, 30, -10, -1), 'head', 36, 1.8)                     # her face from above
    t.look(263.0, expression=PX + 'kiss')
    t.shot(263.0, pose(-75, 160, 10, -4, 4), pose(-50, 140, 6, -4, 0), 'head', 30, 2.0)                        # the other profile, close
    return t


# ---------------------------------------------------------------- Genessa

def g1():
    t = Take('beast-g1', 'Beast G1', 95.9, 150.8, 'Unholy Genessa', 'After Brute Battle in the arena, the quiet on her knees, the rise, Performance 801, ember on the accent, ATOOL 3.')
    t.look(95.9, outfit=UG, variant='genessa', performance=AFTER_BRUTE, palette='moon', expression=GX + 'serious')
    t.shot(95.9, pose(160, 520, 40, 4), pose(195, 480, 20, 6), 'body', 250, fixed=True)
    t.shot(101.5, pose(-200, 300, 40, 0, 4), pose(-165, 280, 30, 0, 1), 'neck', 110, 2.4, fixed=True)
    t.shot(105.5, pose(-150, 420, -10, -4), pose(-178, 400, -30, 0), 'body', 220, fixed=True)
    # The quiet: she kneels.
    t.look(109.6, idle='css.eve.idle.deep_kneeling_bow', expression=GX + 'look-down')
    t.shot(109.6, pose(180, 380, -30, -10), pose(165, 340, -40, -8), 'body', 170, fixed=True)
    t.shot(113.0, pose(-30, 150, -10, -4, -4), pose(-12, 135, -12, -4, -1), 'head', 34, 1.8)
    t.look(116.0, expression=GX + 'gloomy', palette='mist')
    t.shot(116.0, pose(140, 240, 30, -14), pose(160, 230, 40, -12), 'neck', 60, 2.4, fixed=True)
    t.look(120.0, controls={'toggle_hood': 0}, expression=GX + 'contempt')
    t.shot(120.0, pose(-150, 230, 12, -6), pose(-170, 220, 4, -4), 'neck', 56, 2.4, fixed=True)
    t.look(123.0, performance=P801, idle=IDLE)
    t.shot(123.0, pose(-160, 330, -12, 0), pose(-140, 300, -16, 0), 'body', 185, fixed=True)
    t.look(126.6, expression=GX + 'surprised')
    t.shot(126.6, pose(30, 150, 4, 0, -4), pose(15, 135, 3, 0, -1), 'head', 32, 1.8)
    t.shot(130.0, pose(170, 280, -36, -6), pose(190, 270, -42, -6), 'pelvis', 95, 4.0, fixed=True)
    t.shot(132.8, pose(-120, 240, 18, 4, 4), pose(-100, 220, 14, 4, 2), 'neck', 44, 2.4)
    t.look(138.2, palette='ember', expression=GX + 'angry')
    t.shot(138.2, pose(0, 130, 2, 0, 0), pose(0, 118, 2, 0, 0), 'head', 28, 1.8)
    t.look(142.0, performance=PERF + 'atool3')
    t.shot(142.0, pose(180, 480, -60, 16), pose(180, 430, -70, 18), 'body', 205, fixed=True)
    t.shot(146.0, pose(-40, 160, 8, 0, 5), pose(-25, 145, 6, 0, 2), 'head', 34, 1.8)
    t.look(148.4, expression=GX + 'contempt')
    t.shot(148.4, pose(180, 300, 0, -4), pose(176, 240, 6, -4), 'neck', 54, 2.4, fixed=True)
    t.look(148.7, idle=IDLE)
    return t


def g2():
    t = Take('beast-g2', 'Beast G2', 177.7, 209.7, 'Unholy Genessa', 'the build and the loudest part: wings on the accent, Black Hole, gold, glow 8 on the accent, the Hush pose.')
    t.look(177.7, outfit=UG, variant='genessa', idle=IDLE, palette='moon', expression=GX + 'serious')
    t.shot(177.7, pose(185, 520, -40, 8), pose(180, 440, -50, 10), 'body', 200, fixed=True)
    t.shot(182.0, pose(-150, 230, 12, -6), pose(-170, 220, 4, -4), 'neck', 58, 2.4, fixed=True)
    t.look(186.7, performance=BLACK_HOLE)
    t.shot(186.7, pose(-160, 360, -20, 0), pose(-135, 330, -26, 0), 'body', 190, fixed=True)
    t.look(190.3, controls={'toggle_wings': 1}, palette='ember', expression=GX + 'laugh')
    t.shot(190.3, pose(180, 600, -30, 10), pose(180, 520, -40, 12), 'body', 240, fixed=True)
    t.shot(193.6, pose(-30, 170, 6, 0, -5), pose(-12, 150, 4, 0, -2), 'head', 36, 1.8)
    t.look(196.7, palette='gold', expression=GX + 'angry')
    t.shot(196.7, pose(150, 300, -40, -6), pose(178, 290, -50, -4), 'pelvis', 95, 4.0, fixed=True)
    t.shot(197.8, pose(180, 480, -70, 20), pose(180, 420, -80, 22), 'body', 220, fixed=True)
    t.look(202.7, controls={'outfit_glow': 8}, expression=GX + 'laugh')
    t.shot(202.7, pose(0, 140, 4, 0, 0), pose(0, 124, 3, 0, 0), 'head', 30, 1.8)
    t.look(206.7, performance='css.eve.photo.hush', expression=GX + 'tease-ya')
    t.shot(206.7, pose(-20, 150, 2, 0, -3), pose(-8, 136, 2, 0, 0), 'head', 32, 1.8)
    t.shot(209.2, pose(185, 300, 0, -4), pose(180, 250, 6, -4), 'neck', 54, 2.4, fixed=True)
    return t


def g3():
    t = Take('beast-g3', 'Beast G3', 209.7, 246.6, 'Unholy Genessa', f'Iris Out danced at the song\'s tempo (rate {IRIS_RATE}), then the Charm and Hush poses and a laugh on the last accent.')
    t.look(209.7, outfit=UG, variant='genessa', idle=IDLE, palette='moon', expression=GX + 'neutral')
    t.shot(209.7, pose(180, 520, -40, 8), pose(180, 440, -50, 10), 'body', 200, fixed=True)
    t.look(214.0, performance=IRIS_OUT, performance_rate=IRIS_RATE)
    t.shot(214.0, pose(-160, 330, -16, 0), pose(-135, 300, -22, 0), 'body', 185, fixed=True)
    t.shot(217.5, pose(-20, 160, 4, 0, -4), pose(-8, 142, 3, 0, -1), 'head', 34, 1.8)
    t.shot(221.0, pose(165, 260, -34, -6), pose(190, 250, -40, -6), 'pelvis', 90, 4.0, fixed=True)
    t.look(224.4, palette='ember', expression=GX + 'smile')
    t.shot(224.4, pose(180, 470, -64, 16), pose(180, 420, -74, 18), 'body', 205, fixed=True)
    t.shot(228.0, pose(-150, 240, 14, -6), pose(-170, 230, 6, -4), 'neck', 56, 2.4, fixed=True)
    t.shot(231.6, pose(-30, 150, 6, 0, -5), pose(-12, 136, 4, 0, -2), 'head', 32, 1.8)
    t.shot(235.0, pose(185, 300, -20, -4), pose(176, 260, -14, -4), 'chest', 85, 2.8, fixed=True)
    t.look(238.6, performance='css.eve.photo.charm', palette='gold', expression=GX + 'smile')
    t.shot(238.6, pose(0, 150, 2, 0, 0), pose(0, 130, 2, 0, 0), 'head', 30, 1.8)
    t.look(242.0, performance='css.eve.photo.hush', expression=GX + 'wink')
    t.shot(242.0, pose(-40, 150, 4, 0, 5), pose(-25, 136, 3, 0, 2), 'head', 32, 1.8)
    t.look(245.8, expression=GX + 'laugh')
    t.shot(245.8, pose(180, 450, -60, 14), pose(180, 400, -70, 16), 'body', 205, fixed=True)
    return t


def g4():
    t = Take('beast-g4', 'Beast G4', 246.6, SONG_END, 'Unholy Genessa', 'the drop-out on her face, the ending from the ground, then the Seated Recline.')
    t.look(246.6, outfit=UG, variant='genessa', idle=IDLE, palette='moon', expression=GX + 'gloomy')
    t.shot(246.6, pose(0, 140, 2, 0, 2), pose(0, 118, 2, 0, 0), 'head', 30, 1.8)
    t.look(250.0, expression=GX + 'smile')
    t.look(253.6, palette='ember', expression=GX + 'serious')
    t.shot(253.6, pose(180, 560, -72, 20), pose(180, 500, -78, 22), 'body', 210, fixed=True)
    t.look(SEATED_AT, idle='css.eve.idle.seated_recline', expression=GX + 'daydream')
    t.shot(SEATED_AT, pose(-95, 340, -20, -30, -3), pose(-70, 300, -10, -30, 0), 'body', 160, 2.8, fixed=True)
    t.shot(259.0, pose(-25, 170, 40, -10, 4), pose(-10, 150, 30, -10, 1), 'head', 36, 1.8)
    t.look(263.0, expression=GX + 'kiss')
    t.shot(263.0, pose(75, 160, 10, -4, -4), pose(50, 140, 6, -4, 0), 'head', 30, 2.0)
    return t


BUILDERS = [p0, g0, p1, g1, p2, g2, p3, g3, p4, g4]
TAKES = [{'id': t.id, 'song': (t.start, t.end), 'who': t.who} for t in (b() for b in BUILDERS)]


def main():
    for build in BUILDERS:
        take = build()
        path = PRESETS / f'{take.id}.cine.json'
        preset = take.preset()
        path.write_text(json.dumps(preset, indent=1) + '\n')
        print(f'{path.name:24s} {take.start:6.1f} to {take.end:6.1f} s  {len(preset["shots"]):2d} shots {len(preset["looks"]):2d} steps')


if __name__ == '__main__':
    main()
