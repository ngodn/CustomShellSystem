"""Compare decoded skeletons: preserve everything except camera translation modes."""
import argparse
import copy
import json
from pathlib import Path


def validate(before, after):
    expected = copy.deepcopy(before)
    skeleton = next(x for x in expected if x['Type'] == 'Skeleton')
    bones = skeleton['ReferenceSkeleton']['FinalRefBoneInfo']
    changed = []
    for i, bone in enumerate(bones):
        if bone['Name'] in ('camera_pivot', 'camera_target'):
            skeleton['Properties']['BoneTree'][i]['TranslationRetargetingMode'] = (
                'EBoneTranslationRetargetingMode::Animation')
            changed.append(bone['Name'])
    assert len(changed) == 2
    assert after == expected, 'Decoded package changed beyond the two camera translation modes'
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    args = parser.parse_args()
    changed = validate(json.loads(args.before.read_text()), json.loads(args.after.read_text()))
    print('PASS: only camera helper modes changed:', ', '.join(changed))
