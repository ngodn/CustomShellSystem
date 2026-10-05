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

if (args.Length != 5 && args.Length != 6)
    throw new ArgumentException("FacialReadback CONTAINERS MAPPINGS STAGING_REPORT FACIAL_SHAPES OUTPUT [VERTEX_MAPS]");
if (File.Exists(args[4])) throw new IOException("Output already exists");
var staging = JObject.Parse(File.ReadAllText(args[2]));
var shapes = JObject.Parse(File.ReadAllText(args[3]));
if (staging.Value<bool>("complete") != true) throw new InvalidDataException("Incomplete staging");
var vertexMaps = args.Length == 6 ? JObject.Parse(File.ReadAllText(args[5])) : null;
if (vertexMaps != null && vertexMaps.Value<bool>("complete") != true) throw new InvalidDataException("Incomplete vertex maps");
var shapeNames = shapes["morph_targets"]!.Select(item => item.Value<string>("name")!).ToArray();
var expected = shapeNames.ToHashSet(StringComparer.Ordinal);
if (shapeNames.Length is < 1 or > 128 || expected.Count != shapeNames.Length ||
    shapeNames.Any(string.IsNullOrWhiteSpace) ||
    shapeNames.Distinct(StringComparer.OrdinalIgnoreCase).Count() != shapeNames.Length)
    throw new InvalidDataException("Expected a bounded, unique set of audited facial shapes");
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
        if (vertexMaps != null) {
            var map = vertexMaps["maps"]!.Single(m => m.Value<string>("mesh")!.Split('.')[0] == item.Value<string>("target"));
            row["source_delta_comparison"] = FacialError(target, newMorphs, shapes, map);
        }
        row["lods"] = lodRows; row["morphs"] = morphRows; row["preserved_morphs"] = oldMorphs.Count;
        row["passed"] = true;
    } catch (Exception error) {row["passed"] = false; row["error"] = error.ToString();}
    rows.Add(row);
    Console.WriteLine($"{item["target"]}: {row["passed"]}");
}
bool passed = rows.Count == 6 && rows.All(row => row.Value<bool>("passed"));
var report = new JObject { ["passed"] = passed, ["results"] = rows,
    ["source_deltas_checked"] = vertexMaps != null,
    ["shapes_sha256"] = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[3]))).ToLowerInvariant(),
    ["vertex_maps_sha256"] = vertexMaps == null ? null : Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(args[5]))).ToLowerInvariant(),
    ["scope"] = "Independent cooked geometry, bindings, existing body/hand deltas and facial GPU data. Optional source comparison covers every rendered vertex and all expressions. Live execution is separate." };
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

static JObject FacialError(USkeletalMesh mesh, Dictionary<string, UMorphTarget> morphs, JObject shapes, JToken map) {
    if (mesh.LODModels!.Length != 1) throw new InvalidDataException("Source map requires the audited single LOD");
    var lod = mesh.LODModels[0];
    var indices = map["source_vertices"]!.Values<int>().ToArray();
    if (indices.Length != lod.NumVertices || indices.Any(index => index < 0)) throw new InvalidDataException("Invalid render-to-source map");
    var points = shapes["body_points"]!.Select(p => new System.Numerics.Vector3((float)p[0]!, (float)p[1]!, (float)p[2]!)).ToArray();
    float pointError = 0;
    for (int i = 0; i < indices.Length; ++i) if (indices[i] < points.Length) {
        var p = lod.VertexBufferGPUSkin.VertsFloat[i].Pos;
        pointError = Math.Max(pointError, System.Numerics.Vector3.Distance(points[indices[i]], new(p.X,p.Y,p.Z)));
    }
    if (pointError > .0002f) throw new InvalidDataException("Source map does not match cooked body positions: " + pointError);
    float threshold = map.Value<float>("threshold_cm");
    float precision = lod.MorphTargetVertexInfoBuffers!.PositionPrecision;
    if (!float.IsFinite(threshold) || threshold < 0 || threshold > .015001f || !float.IsFinite(precision) || precision <= 0 || precision > .004001f)
        throw new InvalidDataException("Unexpected morph filtering or compression precision");
    float retainedLimit = (float)(Math.Sqrt(3) * precision / 2) + .0002f;
    var source = new Dictionary<string, System.Numerics.Vector3[]>();
    var errors = new Dictionary<string, float>();
    var shapeRows = new JArray();
    foreach (var shape in shapes["morph_targets"]!) {
        var name = shape.Value<string>("name")!;
        var wanted = new System.Numerics.Vector3[points.Length];
        foreach (var d in shape["deltas"]!) wanted[(int)d[0]!] = new((float)d[1]!, (float)d[2]!, (float)d[3]!);
        source[name] = wanted;
        var actual = new System.Numerics.Vector3[indices.Length];
        var present = new bool[indices.Length];
        foreach (var delta in morphs[name].MorphLODModels[0].Vertices) {
            actual[delta.SourceIdx] = new(delta.PositionDelta.X,delta.PositionDelta.Y,delta.PositionDelta.Z);
            present[delta.SourceIdx] = true;
        }
        float retained = 0, omitted = 0;
        for (int i = 0; i < indices.Length; ++i) {
            var expected = indices[i] < wanted.Length ? wanted[indices[i]] : default;
            float error = System.Numerics.Vector3.Distance(expected, actual[i]);
            if (present[i]) retained = Math.Max(retained, error); else omitted = Math.Max(omitted, error);
        }
        if (retained > retainedLimit || omitted > threshold + .0002f)
            throw new InvalidDataException($"{name}: retained error {retained} (limit {retainedLimit}), omitted {omitted} (threshold {threshold})");
        errors[name] = Math.Max(retained, omitted);
        shapeRows.Add(new JObject { ["name"] = name, ["max_retained_error_cm"] = retained, ["max_omitted_delta_cm"] = omitted });
    }
    var expressionRows = new JArray();
    foreach (var pose in shapes["expressions"]!) {
        var actual = new System.Numerics.Vector3[indices.Length];
        var wanted = new System.Numerics.Vector3[points.Length];
        float bound = 0;
        foreach (var entry in (JObject)pose["weights"]!) {
            float weight = (float)entry.Value!;
            bound += Math.Abs(weight) * errors[entry.Key];
            var offsets = source[entry.Key];
            for (int i = 0; i < wanted.Length; ++i) wanted[i] += offsets[i] * weight;
            foreach (var d in morphs[entry.Key].MorphLODModels[0].Vertices)
                actual[d.SourceIdx] += new System.Numerics.Vector3(d.PositionDelta.X,d.PositionDelta.Y,d.PositionDelta.Z) * weight;
        }
        float maximum = 0;
        for (int i = 0; i < indices.Length; ++i) {
            var expected = indices[i] < wanted.Length ? wanted[indices[i]] : default;
            maximum = Math.Max(maximum, System.Numerics.Vector3.Distance(actual[i], expected));
        }
        if (maximum > bound + .0002f) throw new InvalidDataException("Combined expression exceeds measured shape error bound");
        expressionRows.Add(new JObject { ["name"] = pose["name"], ["max_deformation_error_cm"] = maximum, ["weighted_shape_error_bound_cm"] = bound });
    }
    return new JObject { ["passed"] = true, ["render_vertices"] = indices.Length, ["max_source_position_error_cm"] = pointError,
        ["retained_delta_limit_cm"] = retainedLimit, ["filtered_delta_limit_cm"] = threshold + .0002f,
        ["shapes"] = shapeRows, ["expressions"] = expressionRows };
}
