using System.Runtime.InteropServices;
using CUE4Parse.ACL;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.Animation.ACL;
using CUE4Parse.UE4.Objects.Core.Math;
using Newtonsoft.Json;

// Same native ABI and allocation layout as CUE4Parse's AnimConverter.
// This mode deliberately returns deltas, never a plausible-looking absolute pose.
internal static class RawAdditivePose
{
    [DllImport(ACLNative.LIB_NAME)]
    private static extern unsafe void nReadACLData(IntPtr tracks, FTransform* reference,
        FTrackToSkeletonMap* map, FTransform* output);

    public static unsafe void Export(UAnimSequence animation, string directory)
    {
        if (!animation.IsValidAdditive() || animation.AdditiveAnimType != EAdditiveAnimationType.AAT_LocalSpaceBase)
            throw new ArgumentException("Expected a valid local-space additive sequence");
        if (animation.CompressedDataStructure is not FACLCompressedAnimData acl)
            throw new ArgumentException("Expected ACL-compressed additive data");
        var skeleton = animation.Skeleton?.Load<USkeleton>() ?? throw new InvalidOperationException("Missing Skeleton");
        var tracks = acl.GetCompressedTracks();
        if (tracks.IsValid(true) is string error) throw new InvalidOperationException(error);
        var header = tracks.GetTracksHeader();
        var map = animation.GetTrackMap();
        var bones = skeleton.ReferenceSkeleton.FinalRefBoneInfo;
        var names = bones.Select(b => b.Name.Text)
            .Concat(skeleton.VirtualBones.Select(b => b.VirtualBoneName.Text)).ToArray();
        var reference = skeleton.AnimRetargetSources.TryGetValue(animation.RetargetSource, out var authored)
            ? authored.ReferencePose : skeleton.ReferenceSkeleton.FinalRefBonePose;
        var sampleCount = checked((int)header.NumSamples);
        if (sampleCount < 1 || sampleCount > 100000 || map.Length != header.NumTracks ||
            names.Length < 1 || names.Length > 4096 || reference.Length != names.Length ||
            names.Distinct(StringComparer.OrdinalIgnoreCase).Count() != names.Length ||
            map.Any(t => t.BoneTreeIndex < 0 || t.BoneTreeIndex >= names.Length) ||
            map.Select(t => t.BoneTreeIndex).Distinct().Count() != map.Length ||
            !float.IsFinite(header.SampleRate) || header.SampleRate <= 0)
            throw new InvalidOperationException($"Invalid additive dimensions: {names.Length} named bones, {reference.Length} reference poses, {map.Length} mapped/{header.NumTracks} ACL tracks, {sampleCount} samples");
        int count = checked(map.Length * sampleCount);
        if (count > 2000000) throw new InvalidOperationException("Additive diagnostic allocation limit exceeded");
        var atoms = new FTransform[count];
        // Additive omitted rotations/translations/scales use identity/zero/zero,
        // as in AnimConverter's ACL branch, rather than the Skeleton bind pose.
        tracks.SetDefaultScale(0);
        fixed (FTransform* referencePtr = reference)
        fixed (FTrackToSkeletonMap* mapPtr = map)
        fixed (FTransform* atomsPtr = atoms)
            nReadACLData(tracks.Handle, referencePtr, mapPtr, atomsPtr);
        GC.KeepAlive(tracks);
        var rows = new List<object>();
        double maxNormError = 0;
        foreach (int sample in Enumerable.Range(0, 5).Select(i => (sampleCount - 1) * i / 4).Distinct())
        {
            var deltas = new Dictionary<string, object>();
            for (int i = 0; i < map.Length; i++)
            {
                var t = atoms[i * sampleCount + sample];
                float[] values = [t.Rotation.X, t.Rotation.Y, t.Rotation.Z, t.Rotation.W,
                    t.Translation.X, t.Translation.Y, t.Translation.Z, t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z];
                if (values.Any(v => !float.IsFinite(v))) throw new InvalidOperationException("Non-finite additive delta");
                var q = t.Rotation;
                maxNormError = Math.Max(maxNormError, Math.Abs(Math.Sqrt(q.X*q.X + q.Y*q.Y + q.Z*q.Z + q.W*q.W) - 1));
                deltas.Add(names[map[i].BoneTreeIndex], new {
                    Rotation = new {q.X, q.Y, q.Z, q.W},
                    Translation = new {t.Translation.X, t.Translation.Y, t.Translation.Z},
                    ScaleDelta = new {t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z}
                });
            }
            rows.Add(new {sample, timeSeconds = sample / header.SampleRate, deltas});
        }
        if (maxNormError >= .001) throw new InvalidOperationException("Non-unit additive rotations");
        File.WriteAllText(Path.Combine(directory, "additive-deltas.json"), JsonConvert.SerializeObject(new {
            schema = 1, poseKind = "local-space-additive-deltas", asset = animation.GetPathName(),
            sampleCount, sampleRate = header.SampleRate, maxNormError, rows,
            scope = "Raw additive deltas. Requires a base pose and actual blend alpha; not an absolute pose."
        }, Formatting.Indented));
        Console.WriteLine($"Decoded {rows.Count} additive samples across {map.Length} tracks");
    }
}
