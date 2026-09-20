using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using CUE4Parse_Conversion;
using CUE4Parse_Conversion.Options;
using Newtonsoft.Json;
using Serilog;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Objects.UObject;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse_Conversion.Animations;

if (args.Length < 4) throw new ArgumentException("MeshExport CONTAINERS MAPPINGS OBJECT_PATH OUTPUT [EXTRA_PACKAGE...]");
Log.Logger = new LoggerConfiguration().WriteTo.Console().CreateLogger();
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly, new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.ReadScriptData = true;
provider.Initialize();
provider.Mount();
var mesh = provider.LoadPackageObject(args[2]);
Directory.CreateDirectory(args[3]);
File.WriteAllText(Path.Combine(args[3], "source-mesh.json"), JsonConvert.SerializeObject(mesh, Formatting.Indented));
if (Environment.GetEnvironmentVariable("CSS_ABSOLUTE_TRACKS") == "1") {
    if (Environment.GetEnvironmentVariable("CSS_ADDITIVE_POSE_DELTAS") == "1" ||
        Environment.GetEnvironmentVariable("CSS_ANIMATION_POSES") == "1")
        throw new ArgumentException("Choose one animation export mode");
    if (mesh is not UAnimSequence absolute) throw new ArgumentException("Track export requires an AnimSequence");
    RawAbsoluteTracks.Export(absolute, args[3]);
    return;
}
if (Environment.GetEnvironmentVariable("CSS_ADDITIVE_POSE_DELTAS") == "1") {
    if (Environment.GetEnvironmentVariable("CSS_ANIMATION_POSES") == "1")
        throw new ArgumentException("Choose additive deltas or absolute pose sampling, not both");
    if (mesh is not UAnimSequence additive) throw new ArgumentException("Additive export requires an AnimSequence");
    RawAdditivePose.Export(additive, args[3]);
    return;
}
if (Environment.GetEnvironmentVariable("CSS_ANIMATION_POSES") == "1") {
    if (mesh is not UAnimSequence animation) throw new ArgumentException("Pose export requires one AnimSequence");
    if (animation.IsValidAdditive()) throw new ArgumentException("Additive animations require their gameplay blend context");
    if (animation.CompressedDataStructure is not CUE4Parse.UE4.Assets.Exports.Animation.ACL.FACLCompressedAnimData acl)
        throw new ArgumentException("Exact pose sampling currently requires ACL compression");
    var compressedTracks = acl.GetCompressedTracks();
    if (compressedTracks.IsValid(true) is string aclError) throw new InvalidOperationException(aclError);
    var header = compressedTracks.GetTracksHeader();
    var sampleCount = checked((int)header.NumSamples);
    if (sampleCount < 1 || sampleCount > 100000 || !float.IsFinite(header.SampleRate) || header.SampleRate <= 0)
        throw new InvalidOperationException("Invalid ACL sample dimensions");
    var converted = animation.ConvertAnims();
    if (converted.Sequences.Count != 1) throw new InvalidOperationException("Expected one decoded sequence");
    var sequence = converted.Sequences[0];
    var reference = converted.Skeleton.ReferenceSkeleton;
    var count = converted.Skeleton.BoneCount;
    // The game's merged human Skeleton has 1199 entries, beyond the old 1024 guard.
    if (count < 1 || count > 4096 || sequence.NumFrames < 1 || sequence.NumFrames > 100000 ||
        reference.FinalRefBonePose.Length != count || sequence.Tracks.Count != count)
        throw new InvalidOperationException($"Unexpected animation dimensions: {count} bones, {sequence.NumFrames} frames, {sequence.FramesPerSecond} fps");
    var fullHand = Environment.GetEnvironmentVariable("CSS_ANIMATION_FULL_HAND") == "1";
    var allNames = reference.FinalRefBoneInfo.Select(b => b.Name.Text).ToArray();
    var handPrefixes = new[] { "thumb_", "index_", "middle_", "ring_", "pinky_" };
    var selectedBones = Enumerable.Range(0, count).Where(i => !fullHand ||
        allNames[i].Equals("hand_l", StringComparison.OrdinalIgnoreCase) ||
        (allNames[i].EndsWith("_l", StringComparison.OrdinalIgnoreCase) &&
         handPrefixes.Any(prefix => allNames[i].StartsWith(prefix, StringComparison.OrdinalIgnoreCase)))).ToArray();
    if (fullHand && selectedBones.Length != 20)
        throw new InvalidOperationException($"Expected left wrist and 19 finger/metacarpal bones, got {selectedBones.Length}");
    var names = selectedBones.Select(i => allNames[i]).ToArray();
    var samples = fullHand ? Enumerable.Range(0, sampleCount).ToArray() :
        Enumerable.Range(0, 5).Select(i => (sampleCount - 1) * i / 4).Distinct().ToArray();
    var frames = new List<object>();
    foreach (var frame in samples) {
        var transforms = new List<object>();
        foreach (var i in selectedBones) {
            var rest = reference.FinalRefBonePose[i];
            var position = rest.Translation;
            var rotation = rest.Rotation;
            var scale = rest.Scale3D;
            if (animation.FindTrackForBoneIndex(i) >= 0) {
                var track = sequence.Tracks[i];
                var expectedKeys = sampleCount + (header.GetIsWrapOptimized() ? 1 : 0);
                if (track.KeyQuat.Length != expectedKeys || track.KeyPos.Length != expectedKeys || track.KeyScale.Length != expectedKeys)
                    throw new InvalidOperationException("Decoded ACL track dimensions differ from the header");
                // Read exact native-decoded samples, avoiding the legacy exporter's interpolation/time remapping.
                rotation = track.KeyQuat[frame]; position = track.KeyPos[frame]; scale = track.KeyScale[frame];
            }
            if (!float.IsFinite(rotation.X) || !float.IsFinite(rotation.Y) || !float.IsFinite(rotation.Z) ||
                !float.IsFinite(rotation.W) || !float.IsFinite(position.X) || !float.IsFinite(position.Y) ||
                !float.IsFinite(position.Z) || !float.IsFinite(scale.X) || !float.IsFinite(scale.Y) || !float.IsFinite(scale.Z))
                throw new InvalidOperationException("Decoded animation contains a non-finite transform");
            transforms.Add(new { Translation = new { position.X, position.Y, position.Z },
                Rotation = new { rotation.X, rotation.Y, rotation.Z, rotation.W },
                Scale3D = new { scale.X, scale.Y, scale.Z } });
        }
        frames.Add(new { frame, timeSeconds = frame / header.SampleRate, BoneNames = names, LocalTransforms = transforms, bIsValid = true });
    }
    var retargetBasePose = sequence.RetargetBasePose;
    if (fullHand) {
        var sourceBase = retargetBasePose ?? throw new InvalidOperationException("Full-hand sampling requires an authored retarget base");
        // The authored base includes the nine virtual-bone suffix entries;
        // converted animation tracks expose only the raw source bones.
        if (sourceBase.Length < count) throw new InvalidOperationException("Retarget base does not cover every source bone");
        retargetBasePose = selectedBones.Select(i => sourceBase[i]).ToArray();
    }
    var output = new { schema = fullHand ? 3 : 2, asset = args[2], sequence.NumFrames, sampleCount, sampleRate = header.SampleRate,
        boneCount = selectedBones.Length, sourceBoneCount = count, frames, retargetSource = animation.RetargetSource.Text,
        retargetBasePose,
        scope = fullHand ? "Every exact native-decoded ACL sample for the left wrist and 19 finger/metacarpal bones, in source local coordinates. Parent bones are omitted; this is not a complete pose. Before gameplay blending, IK or retargeting." :
            "Up to five exact native-decoded ACL samples in Unreal local coordinates, before gameplay blending, IK or retargeting." };
    File.WriteAllText(Path.Combine(args[3], "animation-poses.json"), JsonConvert.SerializeObject(output, Formatting.Indented));
    Console.WriteLine($"Exported {frames.Count} animation poses with {selectedBones.Length} of {count} source bones");
    return;
}
// Optional author recipe restores component overrides for an offline portrait.
// This changes the export in memory only, never the source containers.
var recipePath = Environment.GetEnvironmentVariable("CSS_MATERIALS");
if (!string.IsNullOrEmpty(recipePath)) {
    var recipe = JsonConvert.DeserializeObject<Dictionary<int,string>>(File.ReadAllText(recipePath))!;
    if (mesh is not USkeletalMesh skeletal) throw new ArgumentException("Material recipe requires a skeletal mesh");
    foreach (var (slot, path) in recipe) {
        if (slot < 0 || slot >= skeletal.SkeletalMaterials.Length) throw new ArgumentException("Material slot outside mesh");
        var material = provider.LoadPackageObject(path);
        var exports = material.Owner!.GetExports().ToArray();
        var index = Array.IndexOf(exports, material);
        if (index < 0) throw new InvalidOperationException("Material export identity was not resolved");
        var reference = new FPackageIndex(material.Owner!, index + 1);
        if (reference.Load() != material) throw new InvalidOperationException("Material reference read-back failed");
        skeletal.SkeletalMaterials[slot].MaterialInterface = reference;
        skeletal.Materials[slot] = reference;
    }
}
if (Environment.GetEnvironmentVariable("CSS_AUDIT_ONLY") == "1") {
    File.WriteAllText(Path.Combine(args[3], "effective-mesh.json"), JsonConvert.SerializeObject(mesh, Formatting.Indented));
    Console.WriteLine("Resolved mesh and configured material overrides");
    return;
}
var session = new ExportSession { MaxDegreeOfParallelism = 2 };
if (Environment.GetEnvironmentVariable("CSS_TEXTURES_ONLY") != "1") session.Add(mesh);
foreach (var path in args.Skip(4)) {
    var exports = provider.LoadPackage(path).GetExports().ToArray();
    File.WriteAllText(Path.Combine(args[3], Path.GetFileName(path)+".json"),JsonConvert.SerializeObject(exports,Formatting.Indented));
    foreach (var export in exports) if (export is CUE4Parse.UE4.Assets.Exports.Texture.UTexture) session.Add(export);
}
var results = await session.RunAsync(args[3], new ExportOptions(meshFormat: EMeshFormat.Gltf2));
File.WriteAllText(Path.Combine(args[3], "export-results.json"), JsonConvert.SerializeObject(results, Formatting.Indented));
if (results.Any(result => !result.Success)) throw new InvalidOperationException("One or more mesh resources failed to export");
Console.WriteLine($"Exported {results.Count} resources");
