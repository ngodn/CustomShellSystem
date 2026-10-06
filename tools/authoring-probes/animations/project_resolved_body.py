"""Prepare a body-only audition while retaining every unbound source track."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path

from prepare_eve import select_bones
from sample_timeline import timeline


def checked_keys(track, frames):
    for channel, times, axes in (
        ('rotations', 'rotationTimes', 'XYZW'),
        ('translations', 'translationTimes', 'XYZ'),
        ('scales', 'scaleTimes', 'XYZ'),
    ):
        keys = track[channel]
        clock = track[times]
        if len(keys) > frames or (clock and len(clock) != len(keys)):
            raise ValueError('Invalid source channel dimensions')
        if any(not math.isfinite(t) or not 0 <= t < frames for t in clock):
            raise ValueError('Invalid source key time')
        if any(a >= b for a, b in zip(clock, clock[1:])):
            raise ValueError('Source key times are not increasing')
        for key in keys:
            values = [key[axis] for axis in axes]
            if not all(math.isfinite(value) for value in values):
                raise ValueError('Nonfinite source key')
            if channel == 'rotations' and abs(math.sqrt(sum(v*v for v in values))-1) > .001:
                raise ValueError('Invalid source quaternion')


def project(audit, metadata, skeleton, required):
    props = metadata['Properties']
    if props.get('AdditiveAnimType', 'AAT_None') not in ('AAT_None', 'EAdditiveAnimationType::AAT_None'):
        raise ValueError('Additive projection requires a separate base-pose audit')
    if props.get('RateScale', 1) != 1 or props.get('Interpolation', 'Linear') not in (
        'Linear', 'EAnimInterpolationType::Linear'
    ) or props.get('RetargetSource', 'None') != 'None':
        raise ValueError('Unsupported source playback settings')
    if props['NumFrames'] != audit['frames'] or props['SequenceLength'] != audit['duration']:
        raise ValueError('Source timeline disagrees with the track audit')
    clock = timeline(audit['frames'], audit['duration'])
    if audit['asset'].split('.')[0] != metadata['Package']:
        raise ValueError('Metadata belongs to another animation')
    if audit['skeleton'].split('.')[0] != skeleton['Package']:
        raise ValueError('Reference belongs to another skeleton')
    ref = skeleton['ReferenceSkeleton']
    info, poses = ref['FinalRefBoneInfo'], ref['FinalRefBonePose']
    if len(info) != audit['skeletonBones'] or len(info) != len(poses):
        raise ValueError('Reference skeleton size disagrees with audit')
    bones = [dict(name=b['Name'], parent=b['ParentIndex'], reference=p)
             for b, p in zip(info, poses)]
    selected = select_bones(bones, set(required))
    if len(set(required)) != len(required) or not required:
        raise ValueError('Required body set must be unique and nonempty')
    indices = [t['boneIndex'] for t in audit['tracks']]
    if any(type(i) is not int or i < 0 for i in indices) or len(set(indices)) != len(indices):
        raise ValueError('Invalid or duplicate source bone binding')
    if [t['trackIndex'] for t in audit['tracks']] != list(range(len(indices))):
        raise ValueError('Source track indices are not complete')
    for field in ('TrackToSkeletonMapTable',):
        if [t['BoneTreeIndex'] for t in props[field]] != indices:
            raise ValueError('Metadata track bindings disagree with audit')
    if metadata['CompressedTrackToSkeletonMapTable'] != indices:
        raise ValueError('Compressed track bindings disagree with audit')
    tracks = {t['boneIndex']: t for t in audit['tracks']}
    for index, track in tracks.items():
        expected = info[index]['Name'] if index < len(info) else None
        if track['boneName'] != expected:
            raise ValueError('Audited bone name disagrees with skeleton index')
        checked_keys(track, audit['frames'])
    if not set(selected) <= tracks.keys():
        raise ValueError('Required body joint has no decoded source track')
    remap = {old: new for new, old in enumerate(selected)}
    output_bones = []
    channels = ('rotations', 'rotationTimes', 'translations', 'translationTimes', 'scales', 'scaleTimes')
    for index in selected:
        bone = copy.deepcopy(bones[index])
        pose = bone['reference']
        checked_keys(dict(rotations=[pose['Rotation']], rotationTimes=[],
                          translations=[pose['Translation']], translationTimes=[],
                          scales=[pose['Scale3D']], scaleTimes=[]), audit['frames'])
        bone['parent'] = remap[bone['parent']] if bone['parent'] >= 0 else -1
        bone['animated'] = True
        bone.update({key: copy.deepcopy(tracks[index][key]) for key in channels})
        output_bones.append(bone)
    retained = [copy.deepcopy(t) for t in audit['tracks'] if t['boneIndex'] not in remap]
    unknown = [t['boneIndex'] for t in retained if t['boneName'] is None]
    result = dict(schema=1, sourceGame='GAME_StellarBlade', asset=audit['asset'],
                  skeleton=audit['skeleton'], frameCount=audit['frames'], duration=audit['duration'],
                  rateScale=1, interpolation='Linear', endpointSampleRate=clock.fps,
                  retargetSource='None', retargetBasePose=None, trackCount=len(output_bones),
                  bones=output_bones, auditionOnly=True, unresolvedSourceBoneIndices=unknown,
                  scope='Resolved body ancestor closure only. All other decoded tracks are retained '
                        'in preserved-tracks.json. This does not identify unknown bones or establish '
                        'full source compatibility, facial completion or live acceptance.')
    preservation = dict(asset=audit['asset'], sourceTrackCount=len(indices),
                        projectedSourceIndices=selected, tracks=retained,
                        unresolvedSourceBoneIndices=unknown)
    return result, preservation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('audit', 'metadata', 'skeleton', 'body_reference', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    inputs = {name: getattr(args, name).resolve(strict=True) for name in (
        'audit', 'metadata', 'skeleton', 'body_reference')}
    data = {name: json.loads(path.read_text()) for name, path in inputs.items()}
    result, retained = project(data['audit'], data['metadata'], data['skeleton'],
                               [b['name'] for b in data['body_reference']['bones']])
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs.values()}
    args.output.mkdir(parents=True, exist_ok=False)
    for name, value in (('source-tracks.json', result), ('preserved-tracks.json', retained),
                        ('projection-receipt.json', dict(inputs=hashes, audition_only=True,
                          selected_tracks=len(result['bones']), retained_tracks=len(retained['tracks']),
                          unknown_indices=retained['unresolvedSourceBoneIndices']))):
        (args.output/name).write_text(json.dumps(value, separators=(',', ':'))+'\n')
    if any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != h for p, h in hashes.items()):
        raise RuntimeError('Source changed during projection')
    print(json.dumps(dict(output=str(args.output), selected=len(result['bones']),
                         retained=len(retained['tracks']), unknown=retained['unresolvedSourceBoneIndices'])))


if __name__ == '__main__':
    main()
