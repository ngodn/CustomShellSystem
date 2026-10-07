// Read cooked package properties independently of the Retoc writer. .NET 10.
// One JSON document keyed by full package path avoids duplicate leaf filenames.
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.Animation.ACL;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.Security.Cryptography;

if (args.Length is not (4 or 5) || (args.Length == 5 && args[4] is not ("--shader-maps" or "--animation-digests")))
    throw new ArgumentException("AssetReadback CONTAINERS MAPPINGS PACKAGE_LIST OUTPUT_JSON [--shader-maps|--animation-digests]");
var readShaderMaps = args.Length == 5 && args[4] == "--shader-maps";
var readAnimationDigests = args.Length == 5 && args[4] == "--animation-digests";
if (File.Exists(args[3])) throw new IOException("Output already exists");
var packages = File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p)).ToArray();
if (packages.Length == 0 || packages.Distinct(StringComparer.OrdinalIgnoreCase).Count() != packages.Length)
    throw new ArgumentException("Expected a nonempty list of unique packages");
var sourceGame = Environment.GetEnvironmentVariable("CSS_SOURCE_GAME") switch {
    null or "" or "GAME_UE5_6" => EGame.GAME_UE5_6,
    "GAME_StellarBlade" => EGame.GAME_StellarBlade,
    var unsupported => throw new ArgumentException($"Unsupported source game: {unsupported}")
};
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(sourceGame), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.ReadScriptData = true;
provider.ReadShaderMaps = readShaderMaps;
provider.Initialize();
provider.Mount();
var results = new JObject();
foreach (var path in packages) {
    var package = provider.LoadPackage(path);
    var exports = package.GetExports().ToArray();
    if (exports.Length == 0) throw new InvalidDataException($"No exports: {path}");
    if (readShaderMaps) foreach (var material in exports.OfType<UMaterialInterface>()) {
        if (material is UMaterial && material.LoadedMaterialResources.Count == 0)
            throw new InvalidDataException($"No material shader resources decoded: {path}");
        if (material.LoadedMaterialResources.Any(resource => resource.LoadedShaderMap is null))
            throw new InvalidDataException($"Invalid material shader map: {path}");
    }
    var decoded = JArray.Parse(JsonConvert.SerializeObject(exports));
    if (readAnimationDigests) for (int i = 0; i < exports.Length; ++i) {
        if (exports[i] is not UAnimSequence sequence) continue;
        if (sequence.CompressedDataStructure is not FACLCompressedAnimData acl || acl.CompressedByteStream.Length == 0)
            throw new InvalidDataException($"Expected nonempty standalone ACL data: {path}");
        var curves = sequence.CompressedCurveByteStream;
        decoded[i]["OfflineAnimationDigest"] = JObject.FromObject(new {
            codec=sequence.CompressedDataStructure.GetType().Name,
            bone_bytes=acl.CompressedByteStream.Length,
            bone_sha256=Convert.ToHexString(SHA256.HashData(acl.CompressedByteStream)),
            curve_bytes=curves?.Length ?? 0,
            curve_sha256=curves == null ? null : Convert.ToHexString(SHA256.HashData(curves))
        });
    }
    results.Add(path, decoded);
}
using var output = new StreamWriter(new FileStream(args[3], FileMode.CreateNew, FileAccess.Write));
output.Write(results.ToString(Formatting.Indented));
Console.WriteLine($"Decoded {results.Count} packages");
