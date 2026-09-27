using System.Reflection;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Readers;
using CUE4Parse_Conversion.Animations;
using CUE4Parse_Conversion.Writers.ActorX.Structs.Animations;
using Newtonsoft.Json;

internal static class SourceTrackAudit
{
    public static void Export(UAnimSequence animation, string directory)
    {
        if (animation.CompressedDataStructure is not FUECompressedAnimData data ||
            data.KeyEncodingFormat != AnimationKeyFormat.AKF_PerTrackCompression)
            throw new ArgumentException("Track audit supports per-track compression only");
        if (animation.Skeleton == null || !animation.Skeleton.TryLoad<USkeleton>(out var skeleton))
            throw new InvalidDataException("Source skeleton could not be loaded");
        var map = animation.GetTrackMap();
        var bones = skeleton.ReferenceSkeleton.FinalRefBoneInfo;
        if (data.CompressedTrackOffsets.Length != map.Length * 2)
            throw new InvalidDataException("Compressed offsets do not match the track map");
        // The normal converter iterates skeleton bones and omits out-of-range tracks.
        // Invoke its pinned decoder by track index for inspection, without inventing bones.
        var decode = typeof(AnimConverter).GetMethod("ReadPerTrackData", BindingFlags.NonPublic | BindingFlags.Static)
            ?? throw new MissingMethodException("Pinned CUE4Parse per-track decoder changed");
        using var reader = new FByteArchive("TrackAudit", data.CompressedByteStream);
        var tracks = new List<object>();
        for (int i = 0; i < map.Length; ++i) {
            var track = new CAnimTrack();
            decode.Invoke(null, new object[] { reader, animation, track, i });
            var boneIndex = map[i].BoneTreeIndex;
            tracks.Add(new {
                trackIndex = i, boneIndex,
                boneName = boneIndex >= 0 && boneIndex < bones.Length ? bones[boneIndex].Name.Text : null,
                rotations = track.KeyQuat, rotationTimes = track.KeyQuatTime,
                translations = track.KeyPos, translationTimes = track.KeyPosTime,
                scales = track.KeyScale, scaleTimes = track.KeyScaleTime
            });
        }
        File.WriteAllText(Path.Combine(directory, "track-audit.json"), JsonConvert.SerializeObject(new {
            purpose = "diagnostic only; unresolved tracks are not retargetable",
            asset = animation.GetPathName(), frames = animation.NumFrames,
            duration = animation.SequenceLength, skeleton = skeleton.GetPathName(),
            skeletonBones = bones.Length, tracks
        }, Formatting.Indented));
    }
}
