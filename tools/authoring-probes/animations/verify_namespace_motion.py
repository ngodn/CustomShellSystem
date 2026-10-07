"""UE 5.6.1: compare every decoded ACL key with migrated raw animation poses."""
import hashlib
import json
import math
import os
from pathlib import Path
import struct

import unreal


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    stage = Path(os.environ['CSS_NAMESPACE_STAGE']).resolve(strict=True)
    work = Path(os.environ['CSS_NAMESPACE_COOK']).resolve(strict=True)
    content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
    if content != stage/'Content' or not unreal.SystemLibrary.get_engine_version().startswith('5.6.1-'):
        raise ValueError('Expected the isolated UE 5.6.1 migration project')
    report = stage/'eve-motion-comparison.json'
    if report.exists():
        raise FileExistsError(report)
    entries = json.loads((work/'catalog.json').read_text())['animation_library']['entries']
    decoded = json.loads((work/'eve-decoded/decoded.json').read_text())
    if not decoded['complete'] or len(entries) != len(decoded['results']):
        raise ValueError('Incomplete decoded batch')
    migration = json.loads((stage/'migration.json').read_text())
    hashes = {r['package']: r['sha256'] for r in migration['assets']}
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', False)
    options.set_editor_property('extract_root_motion', False)
    results = []
    for index, entry in enumerate(entries):
        package = entry['assets'][0]['asset'].split('.')[0]
        source = content/(package.removeprefix('/Game/')+'.uasset')
        if sha(source) != hashes[package]:
            raise ValueError('Migrated source changed: '+package)
        clip = unreal.load_asset(package)
        folder = work/'eve-decoded'/f'{index:02d}'
        sample = json.loads((folder/'absolute-tracks-full.json').read_text())
        binary = folder/sample['file']
        if (sha(binary) != sample['sha256'] or sample['components'] !=
                ['qx', 'qy', 'qz', 'qw', 'tx', 'ty', 'tz', 'sx', 'sy', 'sz'] or
                sample['roundingPolicy'] != 'none' or sample['substeps'] != 1):
            raise ValueError('Unexpected decoded sample format')
        names, frames, rate = sample['names'], sample['frames'], sample['sampleRate']
        if frames != unreal.AnimationLibrary.get_num_keys(clip) or not math.isclose(
                (frames-1)/rate, clip.get_play_length(), abs_tol=1e-5):
            raise ValueError('Cooked timing differs')
        data = binary.read_bytes()
        if len(data) != frames*len(names)*40:
            raise ValueError('Decoded binary length differs')
        maxima = dict(position_cm=0.0, quaternion_distance=0.0, scale=0.0)
        worst = {}
        for frame in range(frames):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame/rate, options)
            if not unreal.AnimPoseExtensions.is_valid(pose):
                raise ValueError('Invalid raw pose')
            pose_names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose)}
            if not set(names).issubset(pose_names):
                raise ValueError('Decoded track has no raw bone')
            for track, name in enumerate(names):
                expected = struct.unpack_from('<10f', data, (frame*len(names)+track)*40)
                actual = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
                q = [actual.rotation.x, actual.rotation.y, actual.rotation.z, actual.rotation.w]
                errors = dict(position_cm=math.dist(expected[4:7], [actual.translation.x, actual.translation.y, actual.translation.z]),
                    quaternion_distance=min(math.dist(expected[:4], q), math.dist(expected[:4], [-v for v in q])),
                    scale=math.dist(expected[7:], [actual.scale3d.x, actual.scale3d.y, actual.scale3d.z]))
                for key, value in errors.items():
                    if not math.isfinite(value):
                        raise ValueError('Nonfinite comparison')
                    if value > maxima[key]:
                        maxima[key], worst[key] = value, dict(frame=frame, bone=name)
        passed = maxima['position_cm'] < .05 and maxima['quaternion_distance'] < 2*math.sin(math.radians(.25)/4) and maxima['scale'] < .001
        results.append(dict(id=entry['id'], frames=frames, tracks=len(names), maxima=maxima,
                            worst=worst, passed=passed, source_sha256=hashes[package], decoded_sha256=sample['sha256']))
        print('CSS_NAMESPACE_MOTION', entry['id'], passed, maxima, flush=True)
        report.write_text(json.dumps(dict(complete=False, results=results), indent=2)+'\n')
    if any(sha(content/(package.removeprefix('/Game/')+'.uasset')) != digest for package, digest in hashes.items()):
        raise ValueError('Candidate source changed during verification')
    passed = all(row['passed'] for row in results)
    report.write_text(json.dumps(dict(complete=True, passed=passed, results=results,
        scope='Every source-key time, including mapped virtual tracks, compared against raw UE local poses without retargeting. No between-key or gameplay acceptance.'), indent=2)+'\n')
    if not passed:
        raise ValueError('Cooked motion exceeds review tolerances')


if __name__ == '__main__':
    main()
