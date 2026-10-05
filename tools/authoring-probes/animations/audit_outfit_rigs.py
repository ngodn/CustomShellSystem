"""Compare isolated installed-package readbacks. Does not load or modify the game."""
import argparse
import hashlib
import json
from pathlib import Path


def package_path(reference):
    return reference['ObjectPath'].rsplit('.', 1)[0]


def one(exports, kind):
    matches = [e for e in exports if e['Type'] == kind]
    if len(matches) != 1:
        raise ValueError(f'Expected one {kind}, found {len(matches)}')
    return matches[0]


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def audit(root):
    baseline = None
    results = []
    # Eve defines the shared base. Read each package in isolation to avoid pak precedence.
    for stem in ('CSS_EveStellarBlade_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P',
                 'CSS_CommanderWhite_eins0fx_P'):
        directory = root / stem
        manifest = json.loads((directory / 'manifest.json').read_text())
        dependencies = json.loads((directory / 'dependencies.json').read_text())
        meshes = json.loads((directory / 'meshes.json').read_text())
        for path, exports in meshes.items():
            mesh = one(exports, 'SkeletalMesh')
            props = mesh['Properties']
            skeleton_path = package_path(props['Skeleton'])
            skeleton = one(dependencies[skeleton_path], 'Skeleton')
            reference = skeleton['ReferenceSkeleton']
            bones = reference['FinalRefBoneInfo']
            poses = reference['FinalRefBonePose']
            if len(bones) != len(poses) or len({b['Name'] for b in bones}) != len(bones):
                raise ValueError(f'Invalid skeleton reference: {path}')
            virtuals = skeleton['Properties']['VirtualBones']
            if baseline is None:
                baseline = (bones, poses, virtuals)
            base_bones, base_poses, base_virtuals = baseline
            # Prefix equality covers names, parent indices and every serialized bind transform.
            if bones[:len(base_bones)] != base_bones or poses[:len(base_poses)] != base_poses:
                raise ValueError(f'Base reference differs: {path}')
            if virtuals != base_virtuals:
                raise ValueError(f'Virtual-bone definitions differ: {path}')
            post_path = package_path(props['PostProcessAnimBlueprint'])
            post = one(dependencies[post_path], 'AnimBlueprintGeneratedClass')
            fields = {p['Name']: p['Type'] for p in post['ChildProperties']}
            for name, kind in (('CSSIdleSequence', 'ObjectProperty'), ('CSSIdleEnabled', 'BoolProperty')):
                if fields.get(name) != kind:
                    raise ValueError(f'Missing idle contract {name}: {path}')
            if package_path(post['Properties']['TargetSkeleton']) != skeleton_path:
                raise ValueError(f'Post-process skeleton differs: {path}')
            defaults = [e for e in dependencies[post_path] if e['Name'].startswith('Default__')]
            if len(defaults) != 1:
                raise ValueError(f'Missing post-process defaults: {path}')
            graph = defaults[0]['Properties']
            modified = sorted(v['BoneToModify']['BoneName'] for k, v in graph.items()
                              if k.startswith('AnimGraphNode_ModifyBone'))
            extra = [b['Name'] for b in bones[len(base_bones):]]
            if any(name not in modified for name in extra):
                raise ValueError(f'Appended bones lack post-process nodes: {path}')
            results.append(dict(package=stem, version=manifest['version'], mesh=path,
                                skeleton=skeleton_path, bones=len(bones), appended_bones=extra,
                                base_reference_sha256=digest((base_bones, base_poses)),
                                virtual_bones_sha256=digest(virtuals), post_process=post_path,
                                physics=package_path(props['PhysicsAsset']),
                                control_rigs=[package_path(v['ControlRigClass']) for k, v in graph.items()
                                              if k.startswith('AnimGraphNode_ControlRig')],
                                modified_bones=modified, idle_contract=True))
    return dict(passed=True, results=results,
                scope='Cooked skeleton references and post-process contracts only. '
                      'Does not validate mesh skinning, new clip playback or live physics.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('readbacks', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    report = audit(args.readbacks)
    with args.output.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(f"Verified {len(report['results'])} installed mesh bindings")
