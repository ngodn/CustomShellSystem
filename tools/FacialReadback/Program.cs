using System.Security.Cryptography;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.UObject;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

if (args.Length != 5)
    throw new ArgumentException("FacialReadback CONTAINERS MAPPINGS STAGING_REPORT FACIAL_SHAPES OUTPUT");
if (File.Exists(args[4])) throw new IOException("Output already exists");
var staging = JObject.Parse(File.ReadAllText(args[2]));
var shapes = JObject.Parse(File.ReadAllText(args[3]));
if (staging.Value<bool>("complete") != true) throw new InvalidDataException("Incomplete staging");
var expected = shapes["morph_targets"]!.Select(item => item.Value<string>("name")!).ToHashSet(StringComparer.Ordinal);
if (expected.Count != 99) throw new InvalidDataException("Expected the audited 99 Eve facial shapes");
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.Initialize(); provider.Mount();
var rows = new JArray();
foreach (var item in staging["results"]!) {
    var row = new JObject { ["source"] = item["source"], ["target"] = item["target"] };
    try {
        var source = provider.LoadPackageObject<USkeletalMesh>(item.Value<string>("source")!);
        var target = provider.LoadPackageObject<USkeletalMesh>(item.Value<string>("target")!);
        foreach (var field in new[] { "Skeleton", "PhysicsAsset", "ShadowPhysicsAsset", "PostProcessAnimBlueprint" }) {
            var a = source.GetOrDefault(field, new FPackageIndex()).ToString();
            var b = target.GetOrDefault(field, new FPackageIndex()).ToString();
            if (a != b) throw new InvalidDataException("Binding changed: " + field);
            row[field] = b;
        }
        Same(source.ReferenceSkeleton, target.ReferenceSkeleton, "Reference skeleton");
        Same(source.SkeletalMaterials, target.SkeletalMaterials, "Material slots");
        Same(source.Sockets, target.Sockets, "Sockets");
        var oldLods = source.LODModels ?? throw new InvalidDataException("No source render LODs");
        var newLods = target.LODModels ?? throw new InvalidDataException("No facial render LODs");
        if (oldLods.Length != newLods.Length) throw new InvalidDataException("LOD count changed");
        var lodRows = new JArray();
        for (int i = 0; i < oldLods.Length; i++) {
            var a = oldLods[i]; var b = newLods[i];
            if (a.NumVertices != b.NumVertices || a.VertexBufferGPUSkin.VertsFloat.Length != a.NumVertices ||
                b.VertexBufferGPUSkin.VertsFloat.Length != b.NumVertices)
                throw new InvalidDataException("Expected complete float render vertices");
            Same(a.VertexBufferGPUSkin, b.VertexBufferGPUSkin, "Render vertices, normals, UVs or skin weights");
            Same(a.VertexBufferGPUSkin.VertsFloat.Select(v => new {v.Pos, v.Normal, v.Infs, v.UVs}),
                 b.VertexBufferGPUSkin.VertsFloat.Select(v => new {v.Pos, v.Normal, v.Infs, v.UVs}), "Render vertex payload");
            Same(a.Indices?.Buffer, b.Indices?.Buffer, "Render topology");
            Same(a.Sections, b.Sections, "Material/cloth sections");
            Same(a.ActiveBoneIndices, b.ActiveBoneIndices, "Active bones");
            Same(a.RequiredBones, b.RequiredBones, "Required bones");
            Same(a.ClothVertexBuffer, b.ClothVertexBuffer, "Cloth index mappings");
            lodRows.Add(new JObject { ["lod"] = i, ["vertices"] = b.NumVertices,
                ["position_precision_cm"] = b.MorphTargetVertexInfoBuffers?.PositionPrecision });
        }
        source.PopulateMorphTargetVerticesData(); target.PopulateMorphTargetVerticesData();
        var oldMorphs = source.MorphTargets.Select(p => p.Load<UMorphTarget>() ?? throw new InvalidDataException("Missing source morph")).ToDictionary(m => m.Name, StringComparer.Ordinal);
        var newMorphs = target.MorphTargets.Select(p => p.Load<UMorphTarget>() ?? throw new InvalidDataException("Missing target morph")).ToDictionary(m => m.Name, StringComparer.Ordinal);
        if (oldMorphs.Count != 22 || !newMorphs.Keys.ToHashSet().SetEquals(oldMorphs.Keys.Concat(expected)))
            throw new InvalidDataException("Cooked morph names differ from the audited set");
        foreach (var (name, oldMorph) in oldMorphs) {
            var current = newMorphs[name];
            if (oldMorph.MorphLODModels.Length != current.MorphLODModels.Length)
                throw new InvalidDataException("Existing morph LODs changed: " + name);
            for (int i = 0; i < oldMorph.MorphLODModels.Length; i++)
                Same(oldMorph.MorphLODModels[i].Vertices, current.MorphLODModels[i].Vertices,
                     "Existing body/hand cooked morph changed: " + name);
        }
        var morphRows = new JArray();
        foreach (var name in expected.Order()) {
            var morph = newMorphs[name];
            if (morph.MorphLODModels.Length != newLods.Length) throw new InvalidDataException("Incomplete facial LODs: " + name);
            int count = 0;
            for (int i = 0; i < morph.MorphLODModels.Length; i++) {
                var indices = new HashSet<uint>();
                foreach (var delta in morph.MorphLODModels[i].Vertices) {
                    if (delta.SourceIdx >= newLods[i].NumVertices || !indices.Add(delta.SourceIdx) ||
                        !Finite(delta.PositionDelta) || !Finite(delta.TangentZDelta))
                        throw new InvalidDataException("Invalid cooked facial delta: " + name);
                    count++;
                }
            }
            if (count == 0) throw new InvalidDataException("Facial morph has no decoded GPU deltas: " + name);
            morphRows.Add(new JObject { ["name"] = name, ["deltas"] = count });
        }
        row["lods"] = lodRows; row["morphs"] = morphRows; row["preserved_morphs"] = oldMorphs.Count;
        row["passed"] = true;
    } catch (Exception error) {row["passed"] = false; row["error"] = error.ToString();}
    rows.Add(row);
    Console.WriteLine($"{item["target"]}: {row["passed"]}");
}
bool passed = rows.Count == 6 && rows.All(row => row.Value<bool>("passed"));
var report = new JObject { ["passed"] = passed, ["results"] = rows,
    ["scope"] = "Independent cooked base geometry, bindings, existing body/hand deltas, and facial GPU delta validity. Facial source-delta error and live execution are separate checks." };
using (var output = new StreamWriter(new FileStream(args[4], FileMode.CreateNew, FileAccess.Write)))
    output.Write(report.ToString(Formatting.Indented));
return passed ? 0 : 1;

static bool Finite(FVector v) => float.IsFinite(v.X) && float.IsFinite(v.Y) && float.IsFinite(v.Z);
static void Same(object? a, object? b, string label) {
    if (Digest(a) != Digest(b)) throw new InvalidDataException(label + " differs");
}
static string Digest(object? value) {
    using var sha = SHA256.Create();
    using (var stream = new CryptoStream(Stream.Null, sha, CryptoStreamMode.Write))
    using (var text = new StreamWriter(stream))
    using (var json = new JsonTextWriter(text)) JsonSerializer.CreateDefault().Serialize(json, value);
    return Convert.ToHexString(sha.Hash!);
}
