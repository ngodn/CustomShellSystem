using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse_Conversion.Animations;
using Newtonsoft.Json;

// Keep the source key times. Resampling through CAnimSequence.FramesPerSecond
// uses NumFrames/duration, which is not the endpoint sample rate for these clips.
internal static class SourceTracks
{
    public static void Export(UAnimSequence animation, string game, string directory)
    {
        if (animation.AdditiveAnimType != EAdditiveAnimationType.AAT_None)
            throw new ArgumentException("Source tracks require a non-additive sequence");
        if (animation.CompressedDataStructure is not FUECompressedAnimData data ||
            data.KeyEncodingFormat != AnimationKeyFormat.AKF_PerTrackCompression)
            throw new ArgumentException("Source tracks currently support UE per-track compression only");
        if (animation.NumFrames < 2 || animation.NumFrames > 100000 ||
            !float.IsFinite(animation.SequenceLength) || animation.SequenceLength <= 0 ||
            !float.IsFinite(animation.RateScale) || animation.RateScale <= 0)
            throw new InvalidDataException("Invalid source timeline");
        var converted = animation.ConvertAnims();
        if (converted.Sequences.Count != 1) throw new InvalidDataException("Expected one source sequence");
        var sequence = converted.Sequences[0];
        var skeleton = converted.Skeleton;
        var info = skeleton.ReferenceSkeleton.FinalRefBoneInfo;
        var rest = skeleton.ReferenceSkeleton.FinalRefBonePose;
        var map = animation.GetTrackMap();
        if (info.Length == 0 || info.Length > 4096 || rest.Length != info.Length ||
            sequence.Tracks.Count != info.Length ||
            info.Select(b => b.Name.Text).Distinct(StringComparer.OrdinalIgnoreCase).Count() != info.Length ||
            map.Length == 0 || map.Any(t => t.BoneTreeIndex < 0 || t.BoneTreeIndex >= info.Length) ||
            map.Select(t => t.BoneTreeIndex).Distinct().Count() != map.Length)
            throw new InvalidDataException("Invalid source skeleton or track map");
        var bones = new List<object>();
        double maxQuaternionError = 0;
        for (int i = 0; i < info.Length; i++) {
            var bone = info[i]; var pose = rest[i]; var track = sequence.Tracks[i];
            if (bone.ParentIndex < -1 || bone.ParentIndex >= i)
                throw new InvalidDataException("Source skeleton is not parent ordered");
            if (track.KeyTime.Length != 0) throw new InvalidDataException("Unexpected shared time keys");
            CheckTimes(track.KeyQuatTime, track.KeyQuat.Length, animation.NumFrames);
            CheckTimes(track.KeyPosTime, track.KeyPos.Length, animation.NumFrames);
            CheckTimes(track.KeyScaleTime, track.KeyScale.Length, animation.NumFrames);
            foreach (var q in track.KeyQuat.Append(pose.Rotation)) {
                double norm = Math.Sqrt(q.X*q.X + q.Y*q.Y + q.Z*q.Z + q.W*q.W);
                if (!double.IsFinite(norm) || Math.Abs(norm-1) > .001)
                    throw new InvalidDataException("Invalid source quaternion");
                maxQuaternionError = Math.Max(maxQuaternionError, Math.Abs(norm-1));
            }
            foreach (var v in track.KeyPos.Concat(track.KeyScale).Append(pose.Translation).Append(pose.Scale3D))
                if (!float.IsFinite(v.X) || !float.IsFinite(v.Y) || !float.IsFinite(v.Z))
                    throw new InvalidDataException("Non-finite source vector");
            bones.Add(new {
                name = bone.Name.Text, parent = bone.ParentIndex, reference = pose,
                animated = animation.FindTrackForBoneIndex(i) >= 0,
                rotations = track.KeyQuat, rotationTimes = track.KeyQuatTime,
                translations = track.KeyPos, translationTimes = track.KeyPosTime,
                scales = track.KeyScale, scaleTimes = track.KeyScaleTime
            });
        }
        var output = new {
            schema = 1, sourceGame = game, asset = animation.GetPathName(),
            skeleton = skeleton.GetPathName(), frameCount = animation.NumFrames,
            duration = animation.SequenceLength, rateScale = animation.RateScale,
            interpolation = animation.Interpolation.ToString(),
            endpointSampleRate = (animation.NumFrames-1)/animation.SequenceLength,
            retargetSource = animation.RetargetSource.Text, retargetBasePose = sequence.RetargetBasePose,
            trackCount = map.Length, maxQuaternionError, bones,
            scope = "Decoded source local keys in Unreal coordinates and centimeters. Time arrays are frame indices; absent time arrays mean uniformly spaced keys over frame 0 through frameCount-1. No retargeting, gameplay curves, notifies, root-motion extraction or visual acceptance."
        };
        using var file = new StreamWriter(new FileStream(Path.Combine(directory, "source-tracks.json"), FileMode.CreateNew));
        file.Write(JsonConvert.SerializeObject(output, Formatting.Indented));
        Console.WriteLine($"Decoded {map.Length} source tracks across {info.Length} bones and {animation.NumFrames} frames");
    }

    private static void CheckTimes(float[] times, int keys, int frames)
    {
        if (keys > frames || (times.Length != 0 && times.Length != keys))
            throw new InvalidDataException("Invalid source key count");
        float previous = -1;
        foreach (float time in times) {
            if (!float.IsFinite(time) || time < 0 || time > frames-1 || time <= previous)
                throw new InvalidDataException("Invalid source key time");
            previous = time;
        }
    }
}
