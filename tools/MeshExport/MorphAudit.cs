using System.Security.Cryptography;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Objects.Core.Math;
using Newtonsoft.Json;

internal static class MorphAudit
{
    public static void Export(USkeletalMesh mesh, string asset, string output)
    {
        var lods = mesh.LODModels ?? throw new InvalidDataException("Missing render LODs");
        if (lods.Length != 1 || lods[0].NumVertices is < 1 or > 1000000)
            throw new InvalidDataException("Morph audit requires one bounded render LOD");
        var lod = lods[0];
        var vertices = lod.VertexBufferGPUSkin.VertsFloat;
        if (vertices.Length != lod.NumVertices)
            throw new InvalidDataException("Expected complete float render vertices");
        var paths = new[] { "morph-vertices.bin", "morph-deltas.bin", "morph-indices.bin", "morph-audit.json" }
            .Select(name => Path.Combine(output, name)).ToArray();
        if (paths.Any(File.Exists)) throw new IOException("Use a fresh morph audit directory");
        mesh.PopulateMorphTargetVerticesData();
        var morphs = mesh.MorphTargets.Select(reference => reference.Load<UMorphTarget>() ??
            throw new InvalidDataException("Unresolved morph")).ToArray();
        if (morphs.Length is < 1 or > 1024 || morphs.Select(m => m.Name)
            .Distinct(StringComparer.OrdinalIgnoreCase).Count() != morphs.Length)
            throw new InvalidDataException("Unexpected or duplicate morph names");
        using (var writer = new BinaryWriter(File.Open(paths[0], FileMode.CreateNew))) {
            foreach (var vertex in vertices) {
                WriteVector(writer, vertex.Pos);
                if (vertex.UVs.Length == 0 || !float.IsFinite(vertex.UVs[0].U) || !float.IsFinite(vertex.UVs[0].V))
                    throw new InvalidDataException("Missing or nonfinite primary UV");
                writer.Write(vertex.UVs[0].U); writer.Write(vertex.UVs[0].V);
            }
        }
        var records = new List<object>();
        using (var writer = new BinaryWriter(File.Open(paths[1], FileMode.CreateNew))) {
            foreach (var morph in morphs) {
                if (morph.MorphLODModels.Length != 1)
                    throw new InvalidDataException("Incomplete morph LOD: " + morph.Name);
                var deltas = morph.MorphLODModels[0].Vertices;
                var seen = new HashSet<uint>();
                long offset = writer.BaseStream.Position;
                foreach (var delta in deltas) {
                    if (delta.SourceIdx >= vertices.Length || !seen.Add(delta.SourceIdx))
                        throw new InvalidDataException("Invalid or repeated morph vertex: " + morph.Name);
                    writer.Write(delta.SourceIdx);
                    WriteVector(writer, delta.PositionDelta);
                    WriteVector(writer, delta.TangentZDelta);
                }
                records.Add(new { name = morph.Name, offset_bytes = offset, count = deltas.Length });
            }
        }
        var indices = lod.Indices?.Buffer ?? throw new InvalidDataException("Missing triangle indices");
        if (indices.Length == 0 || indices.Length % 3 != 0)
            throw new InvalidDataException("Expected triangle topology");
        using (var writer = new BinaryWriter(File.Open(paths[2], FileMode.CreateNew)))
            foreach (var index in indices) {
                if (index >= vertices.Length) throw new InvalidDataException("Invalid triangle vertex");
                writer.Write(index);
            }
        var report = new {
            schema = 1, asset, vertex_count = vertices.Length, index_count = indices.Length,
            vertex_stride = 20, delta_stride = 28,
            vertex_layout = "Little endian float32 position_cm[3], uv0[2]. Original render vertex order.",
            delta_layout = "Little endian uint32 source_vertex, float32 position_delta_cm[3], tangent_z_delta[3].",
            index_layout = "Little endian uint32 original triangle indices.",
            sections = (lod.Sections ?? throw new InvalidDataException("Missing render sections"))
                .Select(s => new { s.MaterialIndex, s.BaseVertexIndex, s.NumVertices }),
            materials = mesh.SkeletalMaterials.Select(m => new { name = m.MaterialSlotName.Text,
                asset = m.MaterialInterface?.ToString() }),
            morphs = records,
            files = paths.Take(3).Select(path => new { name = Path.GetFileName(path),
                bytes = new FileInfo(path).Length, sha256 = Hash(path) }),
            scope = "Decoded cooked morphs with original render indices. No position welding, axis conversion, filtering or alias mapping."
        };
        using var json = new StreamWriter(File.Open(paths[3], FileMode.CreateNew));
        json.WriteLine(JsonConvert.SerializeObject(report, Formatting.Indented));
        Console.WriteLine($"Audited {vertices.Length} original render vertices and {morphs.Length} morphs");
    }

    private static void WriteVector(BinaryWriter writer, FVector value)
    {
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z))
            throw new InvalidDataException("Nonfinite morph geometry");
        writer.Write(value.X); writer.Write(value.Y); writer.Write(value.Z);
    }

    private static string Hash(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }
}
