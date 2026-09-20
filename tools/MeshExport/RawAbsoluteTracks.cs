using System.Runtime.InteropServices;
using CUE4Parse.ACL;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.Animation.ACL;
using CUE4Parse.UE4.Objects.Core.Math;
using Newtonsoft.Json;

// Diagnostic samples of every compressed track, including virtual bones that
// ConvertAnims omits when projecting onto the raw reference-bone array.
internal static class RawAbsoluteTracks
{
    [DllImport(ACLNative.LIB_NAME)]
    private static extern unsafe void nReadACLData(IntPtr tracks, FTransform* reference,
        FTrackToSkeletonMap* map, FTransform* output);

    public static unsafe void Export(UAnimSequence animation, string directory)
    {
        if (animation.IsValidAdditive()) throw new ArgumentException("Absolute tracks reject additive sequences");
        if (animation.CompressedDataStructure is not FACLCompressedAnimData acl)
            throw new ArgumentException("Expected ACL-compressed animation");
        var skeleton = animation.Skeleton?.Load<USkeleton>() ?? throw new InvalidOperationException("Missing Skeleton");
        var tracks = acl.GetCompressedTracks();
        if (tracks.IsValid(true) is string error) throw new InvalidOperationException(error);
        var header = tracks.GetTracksHeader();
        var map = animation.GetTrackMap();
        var rawNames = skeleton.ReferenceSkeleton.FinalRefBoneInfo.Select(b => b.Name.Text).ToArray();
        var names = rawNames.Concat(skeleton.VirtualBones.Select(b => b.VirtualBoneName.Text)).ToArray();
        var reference = skeleton.AnimRetargetSources.TryGetValue(animation.RetargetSource, out var authored)
            ? authored.ReferencePose : skeleton.ReferenceSkeleton.FinalRefBonePose;
        var sampleCount = checked((int)header.NumSamples);
        if (sampleCount < 1 || sampleCount > 100000 || map.Length != header.NumTracks || map.Length == 0 ||
            names.Length > 4096 || names.Distinct(StringComparer.OrdinalIgnoreCase).Count() != names.Length ||
            map.Any(t => t.BoneTreeIndex < 0 || t.BoneTreeIndex >= names.Length || t.BoneTreeIndex >= reference.Length) ||
            map.Select(t => t.BoneTreeIndex).Distinct().Count() != map.Length ||
            !float.IsFinite(header.SampleRate) || header.SampleRate <= 0)
            throw new InvalidOperationException("Invalid absolute-track dimensions or reference coverage");
        int count = checked(map.Length * sampleCount);
        if (count > 2000000) throw new InvalidOperationException("Absolute-track diagnostic allocation limit exceeded");
        var atoms = new FTransform[count];
        fixed (FTransform* referencePtr = reference)
        fixed (FTrackToSkeletonMap* mapPtr = map)
        fixed (FTransform* atomsPtr = atoms)
            nReadACLData(tracks.Handle, referencePtr, mapPtr, atomsPtr);
        GC.KeepAlive(tracks);
        var selectedNames = map.Select(t => names[t.BoneTreeIndex]).ToArray();
        var frames = new List<object>();
        double maxNormError = 0;
        foreach (int sample in Enumerable.Range(0, 5).Select(i => (sampleCount - 1) * i / 4).Distinct())
        {
            var transforms = new List<object>();
            for (int i = 0; i < map.Length; i++)
            {
                var t = atoms[i * sampleCount + sample];
                float[] values = [t.Rotation.X, t.Rotation.Y, t.Rotation.Z, t.Rotation.W,
                    t.Translation.X, t.Translation.Y, t.Translation.Z, t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z];
                if (values.Any(v => !float.IsFinite(v))) throw new InvalidOperationException("Non-finite absolute transform");
                var q = t.Rotation;
                maxNormError = Math.Max(maxNormError, Math.Abs(Math.Sqrt(q.X*q.X + q.Y*q.Y + q.Z*q.Z + q.W*q.W) - 1));
                transforms.Add(new {
                    Rotation = new {q.X, q.Y, q.Z, q.W},
                    Translation = new {t.Translation.X, t.Translation.Y, t.Translation.Z},
                    Scale3D = new {t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z}
                });
            }
            frames.Add(new {frame = sample, timeSeconds = sample / header.SampleRate,
                BoneNames = selectedNames, LocalTransforms = transforms});
        }
        if (maxNormError >= .001) throw new InvalidOperationException("Non-unit absolute rotations");
        File.WriteAllText(Path.Combine(directory, "absolute-tracks.json"), JsonConvert.SerializeObject(new {
            schema = 1, asset = animation.GetPathName(), sampleCount, sampleRate = header.SampleRate,
            retargetSource = animation.RetargetSource.Text, referencePose = reference,
            rawBoneCount = rawNames.Length, trackCount = map.Length,
            virtualTrackCount = map.Count(t => t.BoneTreeIndex >= rawNames.Length), maxNormError, frames,
            scope = "Five native-decoded ACL samples of mapped tracks only, including animated virtual bones. Unmapped bones are omitted. Before retargeting, blending and game IK."
        }, Formatting.Indented));
        Console.WriteLine($"Decoded {frames.Count} absolute samples across {map.Length} tracks, including virtual bones");
    }
}
