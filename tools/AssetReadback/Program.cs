// Read cooked package properties independently of the Retoc writer. .NET 10.
// One JSON document keyed by full package path avoids duplicate leaf filenames.
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.Animation.ACL;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.Security.Cryptography;

if (args.Length is not (4 or 5) || (args.Length == 5 && args[4] is not ("--shader-maps" or "--animation-digests" or "--mesh-digests" or "--texture-digests")))
    throw new ArgumentException("AssetReadback CONTAINERS MAPPINGS PACKAGE_LIST OUTPUT_JSON [--shader-maps|--animation-digests|--mesh-digests|--texture-digests]");
var readShaderMaps = args.Length == 5 && args[4] == "--shader-maps";
var readAnimationDigests = args.Length == 5 && args[4] == "--animation-digests";
var readMeshDigests = args.Length == 5 && args[4] == "--mesh-digests";
var readTextureDigests = args.Length == 5 && args[4] == "--texture-digests";
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
    if (readMeshDigests) {
        if (!exports.Any(item => item is USkeletalMesh))
            throw new InvalidDataException($"Expected a skeletal mesh: {path}");
        for (int i = 0; i < exports.Length; ++i)
            if (exports[i] is USkeletalMesh mesh)
                decoded[i]["OfflineMeshDigest"] = MeshDigest(mesh);
    }
    if (readTextureDigests) {
        if (exports.Length != 1 || exports[0] is not UTexture2D texture)
            throw new InvalidDataException($"Expected one Texture2D export: {path}");
        decoded[0]["OfflineTextureDigest"] = TextureDigest(texture);
    }
    results.Add(path, decoded);
}
using var output = new StreamWriter(new FileStream(args[3], FileMode.CreateNew, FileAccess.Write));
output.Write(results.ToString(Formatting.Indented));
Console.WriteLine($"Decoded {results.Count} packages");

static JObject TextureDigest(UTexture2D texture) {
    var platform = texture.PlatformData;
    if (platform.Mips.Length == 0 || platform.VTData is not null || platform.CPUCopy is not null)
        throw new InvalidDataException("Expected ordinary cooked texture mips");
    var rows = new JArray();
    foreach (var mip in platform.Mips) {
        var bulk = mip.BulkData ?? throw new InvalidDataException("Missing mip bulk data");
        var data = bulk.ReadDataOnce() ?? throw new InvalidDataException("Unreadable mip payload");
        if (data.Length == 0 || data.Length != bulk.GetDataSize() ||
            mip.SizeX <= 0 || mip.SizeY <= 0 || mip.SizeZ <= 0)
            throw new InvalidDataException("Incomplete texture mip");
        rows.Add(JObject.FromObject(new {
            mip.SizeX, mip.SizeY, mip.SizeZ,
            bytes = data.Length, sha256 = Convert.ToHexStringLower(SHA256.HashData(data))
        }));
    }
    return new JObject {
        ["mips"] = rows,
        ["scope"] = "All cooked mip payloads read and hashed, including external bulk data. No GPU rendering."
    };
}

static JObject MeshDigest(USkeletalMesh mesh) {
    var lods = mesh.LODModels ?? throw new InvalidDataException("No render LODs");
    if (lods.Length == 0) throw new InvalidDataException("No render LODs");
    var rows = new JArray();
    foreach (var lod in lods) {
        if (lod.NumVertices <= 0 || lod.VertexBufferGPUSkin.VertsFloat.Length != lod.NumVertices ||
            lod.Indices?.Buffer is not { Length: > 0 })
            throw new InvalidDataException("Expected complete float render vertices and topology");
        rows.Add(JObject.FromObject(new {
            vertices = lod.NumVertices,
            indices = lod.Indices.Buffer.Length,
            // The library's summary converters omit full vertex payloads.
            vertex_sha256 = Digest(lod.VertexBufferGPUSkin.VertsFloat.Select(v => new { v.Pos, v.Normal, v.Infs, v.UVs })),
            index_sha256 = Digest(lod.Indices.Buffer),
            colors_sha256 = Digest(lod.ColorVertexBuffer),
            sections_sha256 = Digest(lod.Sections),
            active_bones_sha256 = Digest(lod.ActiveBoneIndices),
            required_bones_sha256 = Digest(lod.RequiredBones),
            cloth_mapping_sha256 = Digest(lod.ClothVertexBuffer),
            attributes_sha256 = Digest(lod.VertexAttributeBuffers)
        }));
    }
    mesh.PopulateMorphTargetVerticesData();
    var morphs = new JObject();
    foreach (var reference in mesh.MorphTargets) {
        var morph = reference.Load<UMorphTarget>() ?? throw new InvalidDataException("Missing morph");
        if (morphs.ContainsKey(morph.Name)) throw new InvalidDataException("Duplicate morph name");
        morphs.Add(morph.Name, JToken.FromObject(morph.MorphLODModels.Select(lod => new {
            deltas = lod.Vertices.Length, sha256 = Digest(lod.Vertices)
        })));
    }
    return new JObject {
        ["reference_skeleton_sha256"] = Digest(mesh.ReferenceSkeleton),
        ["lods"] = rows, ["morphs"] = morphs,
        ["scope"] = "Decoded bind skeleton, render geometry, skin weights, cloth mappings and morph deltas; no simulation or gameplay execution."
    };
}

static string Digest(object? value) {
    using var hash = SHA256.Create();
    using (var stream = new CryptoStream(Stream.Null, hash, CryptoStreamMode.Write))
    using (var text = new StreamWriter(stream))
    using (var json = new JsonTextWriter(text)) JsonSerializer.CreateDefault().Serialize(json, value);
    return Convert.ToHexStringLower(hash.Hash!);
}
