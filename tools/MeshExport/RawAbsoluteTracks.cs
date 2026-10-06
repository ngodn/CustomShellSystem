using System.Runtime.InteropServices;
using System.Security.Cryptography;
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

    [DllImport("CSSACLStudy")]
    private static extern unsafe void nCSSReadACLData(IntPtr tracks, FTransform* reference,
        FTrackToSkeletonMap* map, FTransform* output);

    [DllImport("CSSACLStudy")]
    private static extern unsafe void nCSSReadACLDataDense(IntPtr tracks, FTransform* reference,
        FTrackToSkeletonMap* map, FTransform* output, uint substeps);

    public static unsafe void Export(UAnimSequence animation, string directory, bool full = false)
    {
        var runtimeSampling = Environment.GetEnvironmentVariable("CSS_ABSOLUTE_RUNTIME") switch {
            null or "" or "0" => false,
            "1" when full => true,
            _ => throw new ArgumentException("CSS_ABSOLUTE_RUNTIME=1 requires the full sample export")
        };
        var substeps = Environment.GetEnvironmentVariable("CSS_ABSOLUTE_SUBSTEPS") switch {
            null or "" or "1" => 1,
            "2" when runtimeSampling => 2,
            "4" when runtimeSampling => 4,
            _ => throw new ArgumentException("Substeps must be 1, or 2/4 with full runtime sampling")
        };
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
        // Match UE 5.6.1 UAnimSequence::GetRetargetTransforms, including the
        // sequence-owned reference used by imported clips with virtual bones.
        var reference = animation.RetargetSource.IsNone && animation.RetargetSourceAssetReferencePose is { Length: > 0 }
            ? animation.RetargetSourceAssetReferencePose
            : skeleton.AnimRetargetSources.TryGetValue(animation.RetargetSource, out var authored)
                ? authored.ReferencePose : skeleton.ReferenceSkeleton.FinalRefBonePose;
        var sampleCount = checked((int)header.NumSamples);
        if (sampleCount < 1 || sampleCount > 100000 || map.Length != header.NumTracks || map.Length == 0 ||
            names.Length > 4096 || names.Distinct(StringComparer.OrdinalIgnoreCase).Count() != names.Length ||
            map.Any(t => t.BoneTreeIndex < 0 || t.BoneTreeIndex >= names.Length || t.BoneTreeIndex >= reference.Length) ||
            map.Select(t => t.BoneTreeIndex).Distinct().Count() != map.Length ||
            !float.IsFinite(header.SampleRate) || header.SampleRate <= 0)
            throw new InvalidOperationException("Invalid absolute-track dimensions or reference coverage");
        var wrapOptimized = header.GetIsWrapOptimized();
        if (substeps > 1 && wrapOptimized)
            throw new ArgumentException("Dense sampling of wrap-optimized clips is not implemented");
        int decodedSamples = checked((sampleCount - 1) * substeps + 1);
        int count = checked(map.Length * decodedSamples);
        if (count > 2000000 * substeps) throw new InvalidOperationException("Absolute-track diagnostic allocation limit exceeded");
        var atoms = new FTransform[count];
        fixed (FTransform* referencePtr = reference)
        fixed (FTrackToSkeletonMap* mapPtr = map)
        fixed (FTransform* atomsPtr = atoms)
        {
            if (substeps > 1) nCSSReadACLDataDense(tracks.Handle, referencePtr, mapPtr, atomsPtr, (uint)substeps);
            else if (runtimeSampling) nCSSReadACLData(tracks.Handle, referencePtr, mapPtr, atomsPtr);
            else nReadACLData(tracks.Handle, referencePtr, mapPtr, atomsPtr);
        }
        GC.KeepAlive(tracks);
        var selectedNames = map.Select(t => names[t.BoneTreeIndex]).ToArray();
        var frames = new List<object>();
        double maxNormError = 0;
        var logicalSamples = sampleCount + (wrapOptimized ? 1 : 0);
        foreach (int sample in Enumerable.Range(0, 5).Select(i => (logicalSamples - 1) * i / 4).Distinct())
        {
            var transforms = new List<object>();
            for (int i = 0; i < map.Length; i++)
            {
                var t = atoms[i * decodedSamples + (sample == sampleCount ? 0 : sample * substeps)];
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
        if (full)
            ExportFull(atoms, decodedSamples, decodedSamples + (wrapOptimized ? 1 : 0), selectedNames,
                directory, animation.GetPathName(), header.SampleRate, runtimeSampling, substeps);
        File.WriteAllText(Path.Combine(directory, "absolute-tracks.json"), JsonConvert.SerializeObject(new {
            schema = 2, asset = animation.GetPathName(), sampleCount, logicalSamples, wrapOptimized,
            sampleRate = header.SampleRate,
            retargetSource = animation.RetargetSource.Text, referencePose = reference,
            rawBoneCount = rawNames.Length, trackCount = map.Length,
            virtualTrackCount = map.Count(t => t.BoneTreeIndex >= rawNames.Length), maxNormError, frames,
            scope = "Five native-decoded ACL samples of mapped tracks only, including animated virtual bones. Unmapped bones are omitted. Before retargeting, blending and game IK."
        }, Formatting.Indented));
        Console.WriteLine($"Decoded {frames.Count} absolute samples across {map.Length} tracks, including virtual bones");
    }

    private static void ExportFull(FTransform[] atoms, int sampleCount, int logicalSamples,
        string[] names, string directory, string asset, float sourceSampleRate, bool runtimeSampling, int substeps)
    {
        var path = Path.Combine(directory, "absolute-tracks.bin");
        double maxNormError = 0;
        using (var writer = new BinaryWriter(File.Open(path, FileMode.CreateNew)))
            for (int frame = 0; frame < logicalSamples; frame++)
                for (int track = 0; track < names.Length; track++)
                {
                    var t = atoms[track * sampleCount + (frame == sampleCount ? 0 : frame)];
                    ReadOnlySpan<float> values = [t.Rotation.X, t.Rotation.Y, t.Rotation.Z, t.Rotation.W,
                        t.Translation.X, t.Translation.Y, t.Translation.Z, t.Scale3D.X, t.Scale3D.Y, t.Scale3D.Z];
                    foreach (float value in values)
                    {
                        if (!float.IsFinite(value)) throw new InvalidOperationException("Non-finite absolute transform");
                        writer.Write(value);
                    }
                    var q = t.Rotation;
                    maxNormError = Math.Max(maxNormError, Math.Abs(Math.Sqrt(
                        (double)q.X*q.X + (double)q.Y*q.Y + (double)q.Z*q.Z + (double)q.W*q.W) - 1));
                }
        if (maxNormError >= .001) throw new InvalidOperationException("Non-unit full absolute rotations");
        using var stream = File.OpenRead(path);
        File.WriteAllText(Path.Combine(directory, "absolute-tracks-full.json"), JsonConvert.SerializeObject(new {
            schema = 1, asset, frames = logicalSamples, sampleRate = sourceSampleRate * substeps,
            sourceSampleRate, substeps, names, maxNormError,
            roundingPolicy = runtimeSampling ? "none" : "nearest",
            file = Path.GetFileName(path), bytes = stream.Length,
            sha256 = Convert.ToHexStringLower(SHA256.HashData(stream)),
            layout = "frame,track,component", componentType = "float32-le",
            components = new[] {"qx", "qy", "qz", "qw", "tx", "ty", "tz", "sx", "sy", "sz"},
            scope = substeps == 1
                ? "Every source-key time decoded with the stated rounding policy, with wrap endpoint restored. Mapped local tracks only, before retargeting, blending and game IK. No between-key evaluation."
                : "Dense non-looping ACL evaluation at the stated sample clock. Mapped local tracks only, before retargeting, blending and game IK."
        }, Formatting.Indented));
    }
}
