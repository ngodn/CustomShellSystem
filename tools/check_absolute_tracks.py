"""Compare absolute ACL tracks against the existing raw-bone sampler, Python 3.14."""
import argparse
import json
import math
from pathlib import Path


def check(tracks, poses, expected_virtual_tracks):
    errors = dict(rotation=0.0, translation=0.0, scale=0.0)
    virtual_names = set()
    checked = 0
    assert tracks["frames"] and len(tracks["frames"]) == len(poses["frames"])
    assert tracks["sampleCount"] == poses["sampleCount"]
    assert tracks["sampleRate"] == poses["sampleRate"]
    for frame, reference in zip(tracks["frames"], poses["frames"], strict=True):
        assert frame["frame"] == reference["frame"]
        assert frame["timeSeconds"] == reference["timeSeconds"]
        assert len(set(frame["BoneNames"])) == len(frame["BoneNames"]) == tracks["trackCount"]
        original = dict(zip(reference["BoneNames"], reference["LocalTransforms"], strict=True))
        for name, transform in zip(frame["BoneNames"], frame["LocalTransforms"], strict=True):
            if name not in original:
                assert name.casefold().startswith("vb "), name
                virtual_names.add(name)
                continue
            for label, field, axes in (
                ("rotation", "Rotation", "XYZW"),
                ("translation", "Translation", "XYZ"),
                ("scale", "Scale3D", "XYZ"),
            ):
                delta = math.dist(
                    [transform[field][axis] for axis in axes],
                    [original[name][field][axis] for axis in axes],
                )
                assert math.isfinite(delta)
                errors[label] = max(errors[label], delta)
            checked += 1
    assert len(virtual_names) == tracks["virtualTrackCount"] == expected_virtual_tracks
    assert checked == (tracks["trackCount"] - expected_virtual_tracks) * len(tracks["frames"])
    assert checked > 0 and max(errors.values()) < 1e-6, errors
    return dict(passed=True, checked_ordinary_samples=checked, max_errors=errors,
                virtual_tracks=sorted(virtual_names), track_count=tracks["trackCount"],
                scope="Ordinary-track differential check and virtual-track coverage, not game IK acceptance.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tracks", type=Path)
    parser.add_argument("poses", type=Path)
    parser.add_argument("--expected-virtual-tracks", type=int, required=True)
    args = parser.parse_args()
    print(json.dumps(check(json.loads(args.tracks.read_text()), json.loads(args.poses.read_text()),
                           args.expected_virtual_tracks), indent=2))
