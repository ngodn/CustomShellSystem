"""Fit the Genesis 2 Female figure onto Mortal Shell II's 258-bone skeleton.

The figure and the game disagree about almost everything.

    DAZ / VaM              Mortal Shell II (as exported to glTF)
    left-handed, left -X   right-handed, left +X
    metres, Y up, +Z front metres, Y up, +Z front
    T-pose                 A-pose
    flat feet, ankle 6 cm  heeled, ankle 19 cm
    height 1.622 m to head 1.758 m to head
    80 DAZ bones           258 game bones

So the figure is not moved onto the skeleton; it is deformed to reach it. The
skeleton itself is never touched, because the game animates it and every other
CSS package already agrees with it.

The deformation is linear blend skinning run backwards. Each DAZ bone gets a
rigid frame from where it points, the matching game bone gets the same, and the
transform between the two pairs is applied to the mesh through a smooth weight
field. That does the pose change, the height change and the proportion change in
one pass, and because it is skinning it cannot tear: a vertex is always a convex
combination of rigid motions.

Weights come from distance to the bone segments, then are smoothed **over the
mesh graph rather than through space**. Spatial smoothing leaks the thigh bone
into the other thigh where they touch and the arm into the ribs, and those leaks
show up as the surface collapsing when the joint bends. Graph smoothing has to
travel through the pelvis to get from one thigh to the other, which is the path
the anatomy actually takes.

    tools/fit_g2f.py --mesh Genesis2Female_base_21556.obj \
        --daz-rig work/v1.0.0-body/daz/g2f_rig.json \
        --game-rig .../SK_Sester_Genessa_V6.refskel.json \
        --out work/v1.0.0-body/fit
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

import dual_quaternion
import mesh_health

# DAZ bone -> game bone. Bones with no distinct game counterpart ride the nearest
# one that does exist: the eyes, jaw and tongue move with the head, the pectorals
# with the upper chest. They still need an entry, or the vertices they own get no
# transform at all and stay behind at the DAZ rest position.
BONE_MAP = {
    'hip': 'pelvis', 'pelvis': 'pelvis',
    'abdomen': 'spine_01', 'abdomen2': 'spine_02', 'chest': 'spine_04',
    'neck': 'neck_01', 'head': 'head',
    'lEye': 'head', 'rEye': 'head', 'lowerJaw': 'head', 'upperJaw': 'head',
    'tongueBase': 'head', 'tongue01': 'head', 'tongue02': 'head', 'tongue03': 'head',
    'tongue04': 'head', 'tongue05': 'head', 'tongueTip': 'head',
    'lPectoral': 'spine_04', 'rPectoral': 'spine_04',
}
for side, tag in (('l', 'l'), ('r', 'r')):
    BONE_MAP[f'{side}Collar'] = f'clavicle_{tag}'
    BONE_MAP[f'{side}Shldr'] = f'upperarm_{tag}'
    BONE_MAP[f'{side}ForeArm'] = f'lowerarm_{tag}'
    BONE_MAP[f'{side}Hand'] = f'hand_{tag}'
    BONE_MAP[f'{side}Carpal1'] = f'index_metacarpal_{tag}'
    BONE_MAP[f'{side}Carpal2'] = f'pinky_metacarpal_{tag}'
    BONE_MAP[f'{side}Thigh'] = f'thigh_{tag}'
    BONE_MAP[f'{side}Shin'] = f'calf_{tag}'
    BONE_MAP[f'{side}Foot'] = f'foot_{tag}'
    BONE_MAP[f'{side}Toe'] = f'ball_{tag}'
    for toe in ('BigToe', 'SmallToe1', 'SmallToe2', 'SmallToe3', 'SmallToe4'):
        BONE_MAP[f'{side}{toe}'] = f'ball_{tag}'
    for daz, game in (('Index', 'index'), ('Mid', 'middle'), ('Ring', 'ring'),
                      ('Pinky', 'pinky'), ('Thumb', 'thumb')):
        for joint in (1, 2, 3):
            BONE_MAP[f'{side}{daz}{joint}'] = f'{game}_0{joint}_{tag}'

# Bones that take the body's overall scale instead of their own length ratio.
# The head is the one that matters: it has no mapped children, so its length
# ratio falls back to the neck's, and the game's neck is 27 percent longer than
# the figure's. Scaling the skull by that gives a head a quarter too big, which
# reads immediately as wrong however good the rest of the fit is.
ISOTROPIC = {'hand_l', 'hand_r', 'head', 'foot_l', 'foot_r', 'ball_l', 'ball_r', 'pelvis'}

# The pelvis is in that set for a different reason: its two children are a
# mirrored pair of thighs, so their mean direction points straight down the
# body's centreline and its length says nothing about how the hips should scale.
# Left to itself it pegs the clamp at 1.73 and widens the hips by three quarters.


def load_daz_rig(path) -> dict[str, dict]:
    """DAZ joints, converted to the game's handedness (left goes from -X to +X)."""
    bones = json.loads(Path(path).read_text())
    by_index = {i: b for i, b in enumerate(bones)}
    out = {}
    for i, bone in by_index.items():
        world = np.asarray(bone['world'], dtype=np.float64)
        parent = by_index[bone['parent']]['name'] if bone['parent'] >= 0 else None
        out[bone['name']] = {'world': np.array([-world[0], world[1], world[2]]),
                             'parent': parent}
    return out


def load_game_rig(path) -> dict[str, dict]:
    """Forward-kinematic the reference skeleton, then convert to the glTF frame.

    The refskel is Unreal centimetres with Z up; the exported meshes are metres
    with Y up. The conversion is (x, z, y) / 100, confirmed against the bone
    weighted centroids of the shipped Genessa mesh rather than assumed.
    """
    bones = json.loads(Path(path).read_text())
    matrices: list[np.ndarray] = [None] * len(bones)
    origins: list[np.ndarray] = [None] * len(bones)
    for i, bone in enumerate(bones):
        x, y, z, w = bone['rotation']
        rotation = np.array([
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        translation = np.asarray(bone['translation'], dtype=np.float64)
        parent = bone['parent']
        if parent < 0:
            matrices[i], origins[i] = rotation, translation
        else:
            matrices[i] = matrices[parent] @ rotation
            origins[i] = origins[parent] + matrices[parent] @ translation
    out = {}
    for i, bone in enumerate(bones):
        ue = origins[i]
        parent = bones[bone['parent']]['name'] if bone['parent'] >= 0 else None
        out[bone['name']] = {'world': np.array([ue[0], ue[2], ue[1]]) / 100.0,
                             'parent': parent}
    return out


def read_obj(path):
    points, faces = [], []
    for line in Path(path).read_text().splitlines():
        if line.startswith('v '):
            points.append([float(v) for v in line.split()[1:4]])
        elif line.startswith('f '):
            faces.append([int(t.split('/')[0]) - 1 for t in line.split()[1:]])
    return np.asarray(points, dtype=np.float64), faces


def _ancestors(name, rig):
    seen = set()
    while name in rig and name not in seen:
        seen.add(name)
        name = rig[name]['parent']
        if name in rig:
            yield name


def children_of(rig) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for name, bone in rig.items():
        if bone['parent']:
            out.setdefault(bone['parent'], []).append(name)
    return out


def weighted_similarity(source, target, weights):
    """Umeyama with per-landmark weights. Reflection is forbidden, as ever.

    Weighting is what makes a per-bone fit work at all. An unweighted fit over a
    bone's neighbourhood is dominated by whichever part of the body contributed
    the most joints, so fitting the upper arm ends up solving for the torso and
    the arm never leaves the T-pose it started in.
    """
    weights = np.asarray(weights, dtype=np.float64)
    weights = weights / weights.sum()
    source_mean = (source * weights[:, None]).sum(axis=0)
    target_mean = (target * weights[:, None]).sum(axis=0)
    source_centred = source - source_mean
    target_centred = target - target_mean
    variance = float((weights[:, None] * source_centred ** 2).sum())
    if variance < 1e-12:
        raise ValueError('landmarks are coincident')
    covariance = (target_centred * weights[:, None]).T @ source_centred
    u, singular, vt = np.linalg.svd(covariance)
    correction = np.eye(3)
    if np.linalg.det(u) * np.linalg.det(vt) < 0:
        correction[2, 2] = -1.0
    rotation = u @ correction @ vt
    scale = float((singular * np.diag(correction)).sum() / variance)
    return rotation, scale


def align_direction(source_direction, target_direction) -> np.ndarray:
    """Smallest rotation carrying one direction onto the other (Rodrigues)."""
    a = source_direction / max(np.linalg.norm(source_direction), 1e-12)
    b = target_direction / max(np.linalg.norm(target_direction), 1e-12)
    axis = np.cross(a, b)
    sine = float(np.linalg.norm(axis))
    cosine = float(a @ b)
    if sine < 1e-9:
        if cosine > 0:
            return np.eye(3)
        # Opposed: any perpendicular axis will do, so take a stable one.
        perpendicular = np.cross(a, np.eye(3)[np.argmin(np.abs(a))])
        perpendicular /= np.linalg.norm(perpendicular)
        return -np.eye(3) + 2 * np.outer(perpendicular, perpendicular)
    axis = axis / sine
    cross = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
    return np.eye(3) + sine * cross + (1 - cosine) * (cross @ cross)


def best_roll(axis, aligned, target, weights) -> np.ndarray:
    """The rotation about `axis` that best lines `aligned` up with `target`.

    Splitting the fit this way is the whole trick. A bone's direction is a hard
    constraint, because getting it wrong leaves the limb pointing somewhere the
    animation does not expect; its roll is a soft one, because nothing but the
    surrounding anatomy says where the elbow crease should face. Solving both at
    once, as a single least-squares over the neighbourhood, trades away the hard
    constraint to buy a better fit on the soft one, which is how the shoulder
    ended up 16 degrees short of the pose the game animates.

    Maximising the weighted alignment reduces to A*cos(t) + B*sin(t), so the best
    angle is atan2(B, A) with no search.
    """
    axis = axis / max(np.linalg.norm(axis), 1e-12)
    along_a = aligned @ axis
    along_t = target @ axis
    a_term = float((weights * ((aligned * target).sum(axis=1) - along_a * along_t)).sum())
    b_term = float((weights * (np.cross(axis, aligned) * target).sum(axis=1)).sum())
    angle = np.arctan2(b_term, a_term)
    cross = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
    return np.eye(3) + np.sin(angle) * cross + (1 - np.cos(angle)) * (cross @ cross)


# Beyond this the two skeletons disagree about which way a bone points, which
# means the pairing is telling us nothing rather than telling us to turn.
MAX_TURN_DEGREES = 75.0


def plausible(source, target) -> bool:
    """Whether a source/target direction pair is worth trusting.

    The hip is why this exists. Genesis 2 puts `abdomen` a centimetre *below*
    `hip`, while the game puts `spine_01` three centimetres above `pelvis`, so
    the two directions point opposite ways and aligning them rolls the whole
    pelvis through 140 degrees. That was 516 inverted triangles through the
    groin, and nothing in the silhouette showed it.
    """
    source_length = float(np.linalg.norm(source))
    target_length = float(np.linalg.norm(target))
    if source_length < 5e-3 or target_length < 5e-3:
        return False
    cosine = float(source @ target) / (source_length * target_length)
    return cosine > np.cos(np.radians(MAX_TURN_DEGREES))


def paired_direction(name, daz_rig, daz_children, game_rig, bone_map):
    """The bone's direction in both skeletons, measured between the same joints.

    The target direction must not be read off the game skeleton's own children.
    `upperarm_l` has three: `lowerarm_l` and two twist bones that sit partway
    along the limb, so averaging them points the upper arm at its own midpoint,
    shrinks it to three quarters length and leaves the shoulder twelve degrees
    short. `thigh_l`, `calf_l` and `lowerarm_l` all have the same problem.

    Measuring between the game bones that the DAZ bone and its DAZ children map
    to keeps the two directions describing the same anatomy. Children mapping
    onto the bone's own game bone are skipped, or the head, whose eyes and jaw
    all map to `head`, would come out with no direction at all.
    """
    game_name = bone_map.get(name)
    if game_name not in game_rig:
        return None, None
    kids = [k for k in daz_children.get(name, [])
            if bone_map.get(k) in game_rig and bone_map.get(k) != game_name]
    if kids:
        source = np.mean([daz_rig[k]['world'] for k in kids], axis=0) - daz_rig[name]['world']
        target = (np.mean([game_rig[bone_map[k]]['world'] for k in kids], axis=0)
                  - game_rig[game_name]['world'])
        if plausible(source, target):
            return source, target

    parent = daz_rig[name]['parent']
    game_parent = bone_map.get(parent)
    if game_parent in game_rig and game_parent != game_name:
        source = daz_rig[name]['world'] - daz_rig[parent]['world']
        target = game_rig[game_name]['world'] - game_rig[game_parent]['world']
        if plausible(source, target):
            return source, target
    return None, None


def neighbourhood(name, rig, children, depth=2) -> list[str]:
    """The bone plus the joints around it, out to `depth` links.

    A bone's rotation cannot be read from the bone alone: a single joint pair
    fixes a position, and a single direction fixes only two of three angles, so
    the twist around the bone is left to whatever reference axis the code picks.
    Picking one from the world axes puts a singularity on every vertical bone and
    silently rolls the torso. Taking the surrounding joints instead lets the
    anatomy decide: the finger joints fix the roll of the hand, the shoulder and
    ribs fix the roll of the upper arm.
    """
    found = {name: 0}
    frontier = [name]
    for step in range(1, depth + 1):
        nxt = []
        for bone in frontier:
            parent = rig[bone]['parent']
            if parent in rig:
                nxt.append(parent)
                nxt += children.get(parent, [])          # siblings
            nxt += children.get(bone, [])
        frontier = [b for b in dict.fromkeys(nxt) if b not in found]
        for bone in frontier:
            found[bone] = step
    return found


def bone_transforms(daz_rig, game_rig, bone_map, global_scale):
    """One similarity transform per mapped DAZ bone, from matched joint clouds.

    The bone's own joint is then pinned exactly onto its game counterpart. The
    surrounding joints decide the rotation and the scale, the bone's own joint
    decides the position, so the skeleton the game animates is hit on the nose
    while the limb around it still turns the right way.
    """
    daz_children = children_of(daz_rig)
    transforms = {}
    # Parents before children, so a bone that has to inherit its parent's
    # transform finds it already built.
    ordered = sorted(bone_map, key=lambda n: len(list(_ancestors(n, daz_rig))))
    for daz_name in ordered:
        game_name = bone_map[daz_name]
        if daz_name not in daz_rig or game_name not in game_rig:
            continue

        # A bone sharing its parent's game bone has nothing of its own to solve
        # for: every landmark around it maps onto the same target point, and
        # fitting a spread-out source cloud onto a single target returns whatever
        # rotation the SVD happens to produce. The eyes, jaw and tongue all map
        # to `head`, and the garbage rotations they came back with were enough to
        # flatten the skull. They ride the parent instead.
        parent = daz_rig[daz_name]['parent']
        if bone_map.get(parent) == game_name and parent in transforms:
            transforms[daz_name] = dict(transforms[parent])
            continue

        for depth in (2, 3, 4):
            picked, seen = [], set()
            reached = neighbourhood(daz_name, daz_rig, daz_children, depth)
            for bone, steps in reached.items():
                game = bone_map.get(bone)
                # Several DAZ bones share one game bone (every toe maps to the
                # ball, the jaw and tongue to the head). Keeping both sides of
                # such a pair would ask the fit to send two different source
                # points to one target, so only the first is kept.
                if game in game_rig and game not in seen:
                    seen.add(game)
                    picked.append((bone, game, steps))
            if len(picked) >= 4:
                source = np.array([daz_rig[b]['world'] for b, _, _ in picked])
                target = np.array([game_rig[g]['world'] for _, g, _ in picked])
                # Nearby joints decide, distant ones only steady the fit.
                influence = np.array([0.3 ** steps for _, _, steps in picked])
                spread = np.linalg.svd(source - source.mean(0), compute_uv=False)
                if spread[2] > 1e-4:                     # not coplanar
                    break
        else:
            continue
        source_direction, target_direction = paired_direction(
            daz_name, daz_rig, daz_children, game_rig, bone_map)
        axis = None
        centred_source = source - daz_rig[daz_name]['world']
        centred_target = target - game_rig[game_name]['world']

        if source_direction is None or target_direction is None:
            try:
                rotation, scale = weighted_similarity(source, target, influence)
            except ValueError:
                continue
        else:
            straight = align_direction(source_direction, target_direction)
            rolled = best_roll(target_direction, centred_source @ straight.T,
                               centred_target, influence)
            rotation = rolled @ straight
            scale = (np.linalg.norm(target_direction)
                     / max(np.linalg.norm(source_direction), 1e-9))
            axis = target_direction / np.linalg.norm(target_direction)

        # Stretch along the bone, not in every direction. A forearm whose game
        # counterpart is a quarter longer needs to be a quarter longer; it does
        # not need to be a quarter thicker, and scaling it uniformly makes the
        # arms heavy while the limb lengths are still right, so nothing in the
        # numbers says anything is wrong.
        along = float(np.clip(scale, global_scale * 0.6, global_scale * 1.6))
        across = global_scale
        if game_name in ISOTROPIC or axis is None:
            along = across = global_scale
            axis = np.array([0.0, 1.0, 0.0])
        source_axis = (source_direction / np.linalg.norm(source_direction)
                       if source_direction is not None else np.array([0.0, 1.0, 0.0]))
        transforms[daz_name] = {'rotation': rotation, 'along': along, 'across': across,
                                'axis': axis, 'source_axis': source_axis,
                                'source': daz_rig[daz_name]['world'],
                                'target': game_rig[game_name]['world'],
                                'game': game_name, 'landmarks': len(picked)}
    return transforms


def segment_distance(points: np.ndarray, start: np.ndarray, end: np.ndarray) -> np.ndarray:
    """Distance from every point to the line segment start..end."""
    axis = end - start
    length = float(axis @ axis)
    if length < 1e-12:
        return np.linalg.norm(points - start, axis=1)
    t = np.clip((points - start) @ axis / length, 0.0, 1.0)
    return np.linalg.norm(points - (start + t[:, None] * axis), axis=1)


def proximity_weights(points, daz_rig, transforms, neighbours, influences=8, blend=0.035):
    """Weight every vertex against the bones it is nearest to, then smooth.

    Distance is to the bone's own span, its joint to its children. Weight is by
    distance beyond the nearest bone, so the field is scale-free: the nearest
    bone always scores 1 and nothing is orphaned, whatever the limb's size. A
    short graph smoothing then spreads each bone across its joint so the boundary
    does not pinch. This is the weighting that gave the best body.
    """
    names = list(transforms)
    children = children_of(daz_rig)
    starts, ends = [], []
    for name in names:
        here = daz_rig[name]['world']
        kids = [k for k in children.get(name, []) if k in transforms]
        # A bone deforms the flesh between its own joint and its children, not
        # between its parent and itself. Getting this backwards hands the upper
        # arm to the forearm's transform, and the arm folds at the shoulder.
        if kids:
            ends.append(np.mean([daz_rig[k]['world'] for k in kids], axis=0))
        else:
            parent = daz_rig[name]['parent']
            reach = here - daz_rig[parent]['world'] if parent in daz_rig else np.zeros(3)
            ends.append(here + reach * 0.5)
        starts.append(here)
    starts, ends = np.asarray(starts), np.asarray(ends)

    distances = np.stack([segment_distance(points, starts[i], ends[i])
                          for i in range(len(names))], axis=1)
    nearest = distances.min(axis=1, keepdims=True)
    weights = np.exp(-((distances - nearest) / blend) ** 2)

    if influences < weights.shape[1]:
        cutoff = np.partition(weights, -influences, axis=1)[:, -influences][:, None]
        weights = np.where(weights >= cutoff, weights, 0.0)
    weights /= weights.sum(axis=1, keepdims=True)

    for _ in range(10):
        averaged = np.zeros_like(weights)
        counts = np.zeros(len(points))
        for a, b in neighbours:
            averaged[a] += weights[b]
            averaged[b] += weights[a]
            counts[a] += 1
            counts[b] += 1
        busy = counts > 0
        averaged[busy] /= counts[busy, None]
        weights[busy] = 0.5 * weights[busy] + 0.5 * averaged[busy]
        weights /= np.maximum(weights.sum(axis=1, keepdims=True), 1e-12)
    return names, weights


def edges_of(faces, count) -> np.ndarray:
    pairs = set()
    for face in faces:
        for i in range(len(face)):
            a, b = face[i], face[(i + 1) % len(face)]
            pairs.add((a, b) if a < b else (b, a))
    return np.asarray(sorted(pairs), dtype=np.int64)


def skin(points, names, weights, transforms) -> np.ndarray:
    """Linear blend skinning with the stretch kept separate from the rotation.

    Each bone rotates the flesh about its joint, stretches it along its own axis
    (long the bone, across the bone) and moves it to the game joint; the results
    are averaged by weight. Plain LBS, which is what produced the best body so
    far. Dual quaternions were tried here to cure the joint collapse and made it
    worse, because the scale has to be blended too and a dual quaternion carries
    only a rigid motion; the collapse is better dealt with by relaxing the few
    pinched triangles afterwards than by changing the whole blend.
    """
    out = np.zeros_like(points)
    for index, name in enumerate(names):
        share = weights[:, index]
        active = share > 1e-6
        if not active.any():
            continue
        entry = transforms[name]
        moved = (points[active] - entry['source']) @ entry['rotation'].T
        axis = entry['axis']
        along = (moved @ axis)[:, None] * axis
        moved = entry['across'] * (moved - along) + entry['along'] * along + entry['target']
        out[active] += moved * share[active, None]
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--mesh', required=True, type=Path)
    parser.add_argument('--daz-rig', required=True, type=Path)
    parser.add_argument('--game-rig', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--blend', type=float, default=0.035,
                        help='metres over which two bones share a vertex')
    args = parser.parse_args(argv)

    points, faces = read_obj(args.mesh)
    # Into the game's handedness. The winding flips with it, or every face ends
    # up inside out and the model renders as a silhouette of its own back faces.
    points = np.stack([-points[:, 0], points[:, 1], points[:, 2]], axis=1)
    faces = [list(reversed(f)) for f in faces]

    daz_rig = load_daz_rig(args.daz_rig)
    game_rig = load_game_rig(args.game_rig)

    global_scale = float(game_rig['head']['world'][1] / daz_rig['head']['world'][1])
    transforms = bone_transforms(daz_rig, game_rig, BONE_MAP, global_scale)
    print(f'{len(transforms)} bones mapped, global scale {global_scale:.4f}')

    neighbours = edges_of(faces, len(points))
    names, weights = proximity_weights(points, daz_rig, transforms, neighbours,
                                       blend=args.blend)
    fitted = skin(points, names, weights, transforms)

    triangles = np.array([[face[0], face[i], face[i + 1]]
                          for face in faces for i in range(1, len(face) - 1)])
    health = mesh_health.report(points, fitted, triangles)
    fitted, left = mesh_health.relax(fitted, triangles, neighbours)
    health['inverted_after_relax'] = left

    args.out.mkdir(parents=True, exist_ok=True)
    lines = [f'# Genesis 2 Female fitted to SKEL_Human_Skeleton, {len(fitted)} vertices']
    lines += [f'v {x:.6f} {y:.6f} {z:.6f}' for x, y, z in fitted]
    lines += ['f ' + ' '.join(str(v + 1) for v in face) for face in faces]
    (args.out / 'g2f_fitted.obj').write_text('\n'.join(lines) + '\n')

    checks = {}
    for daz_name, game_name in (('head', 'head'), ('lHand', 'hand_l'), ('lFoot', 'foot_l'),
                                ('lShin', 'calf_l'), ('chest', 'spine_04'), ('hip', 'pelvis')):
        if daz_name in transforms:
            entry = transforms[daz_name]
            checks[game_name] = {'along': round(entry['along'], 3),
                                 'across': round(entry['across'], 3),
                                 'landmarks': entry['landmarks']}
    report = {'vertices': len(fitted), 'bones_mapped': len(transforms),
              'global_scale': round(global_scale, 5),
              'height_before_m': round(float(points[:, 1].max() - points[:, 1].min()), 4),
              'height_after_m': round(float(fitted[:, 1].max() - fitted[:, 1].min()), 4),
              'health': health, 'bone_scales': checks}
    (args.out / 'fit.report.json').write_text(json.dumps(report, indent=2) + '\n')
    np.save(args.out / 'fit_weights.npy', weights)
    (args.out / 'fit_bones.json').write_text(json.dumps(names, indent=1) + '\n')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
