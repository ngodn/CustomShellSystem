using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse_Conversion.Dto;
using CUE4Parse_Conversion.Options;
using Newtonsoft.Json;
using System.Security.Cryptography;

internal static class SkinAudit
{
    public static void Export(USkeletalMesh mesh, string asset, string output)
    {
        using var dto = new SkeletalMeshDto(mesh, EMeshQuality.Highest,
            ENaniteMeshFormat.NoNanite, exportMorphTarget: false);
        if (dto.LODs.Count != 1 || dto.LODs[0].IsNanite)
            throw new InvalidOperationException("Skin audit requires one conventional render LOD");
        var lod = dto.LODs[0];
        if (lod.Vertices.Length == 0 || dto.Bones.Length == 0)
            throw new InvalidOperationException("Empty skin audit input");
        var maxInfluences = lod.Vertices.Max(v => v.Influences.Length);
        if (maxInfluences is < 1 or > 256)
            throw new InvalidOperationException("Unexpected influence count");
        var vertexPath = Path.Combine(output, "skin-vertices.bin");
        var indexPath = Path.Combine(output, "skin-indices.bin");
        var manifestPath = Path.Combine(output, "skin-audit.json");
        if (new[] { vertexPath, indexPath, manifestPath }.Any(File.Exists))
            throw new IOException("Use a fresh skin audit output directory");
        var minWeightSum = double.PositiveInfinity;
        var maxWeightSum = double.NegativeInfinity;
        var histogram = new SortedDictionary<int, int>();
        using (var writer = new BinaryWriter(File.Open(vertexPath, FileMode.CreateNew)))
        {
            foreach (var vertex in lod.Vertices)
            {
                var p = vertex.Position;
                if (!float.IsFinite(p.X) || !float.IsFinite(p.Y) || !float.IsFinite(p.Z))
                    throw new InvalidOperationException("Non-finite vertex position");
                writer.Write(p.X); writer.Write(p.Y); writer.Write(p.Z);
                var influences = vertex.Influences;
                if (influences.Length == 0)
                    throw new InvalidOperationException("Unweighted vertex");
                histogram[influences.Length] = histogram.GetValueOrDefault(influences.Length) + 1;
                double sum = 0;
                for (var i = 0; i < maxInfluences; ++i)
                {
                    var influence = i < influences.Length ? influences[i] : default;
                    if (influence.Bone >= dto.Bones.Length || !float.IsFinite(influence.Weight) ||
                        influence.Weight < 0 || influence.Weight > 1)
                        throw new InvalidOperationException("Invalid skin influence");
                    writer.Write(influence.Bone);
                    writer.Write(influence.RawWeight);
                    writer.Write(influence.Weight);
                    sum += influence.Weight;
                }
                minWeightSum = Math.Min(minWeightSum, sum);
                maxWeightSum = Math.Max(maxWeightSum, sum);
            }
        }
        using (var writer = new BinaryWriter(File.Open(indexPath, FileMode.CreateNew)))
            foreach (var index in lod.Indices)
            {
                if (index >= lod.Vertices.Length) throw new InvalidOperationException("Invalid vertex index");
                writer.Write(index);
            }
        var bones = dto.Bones.Select(b => new {
            name = b.Name, parent = b.ParentIndex,
            translation = new[] { b.Transform.Translation.X, b.Transform.Translation.Y, b.Transform.Translation.Z },
            rotation = new[] { b.Transform.Rotation.X, b.Transform.Rotation.Y, b.Transform.Rotation.Z, b.Transform.Rotation.W },
            scale = new[] { b.Transform.Scale3D.X, b.Transform.Scale3D.Y, b.Transform.Scale3D.Z }
        }).ToArray();
        var manifest = new {
            schema = 1, asset, source_lod = lod.SourceLodIndex,
            vertex_count = lod.Vertices.Length, index_count = lod.Indices.Length,
            max_influences = maxInfluences, influence_count_histogram = histogram,
            min_weight_sum = minWeightSum, max_weight_sum = maxWeightSum,
            vertex_stride = 12 + maxInfluences * 8,
            binary_layout = "Little endian, unaligned: float32 position[3], then max_influences * (uint16 bone, uint16 raw_weight, float32 weight). Zero-pad unused influences. Indices are uint32.",
            scope = "Neutral render vertices in Unreal component coordinates (cm), all nonzero cooked influences, no axis conversion or weight normalization. No morph, cloth, physics, material displacement or post-process evaluation.",
            bones, sections = lod.Sections,
            material_slots = mesh.SkeletalMaterials.Select(m => m.MaterialSlotName.Text).ToArray(),
            files = new[] { vertexPath, indexPath }.Select(path => new {
                name = Path.GetFileName(path), bytes = new FileInfo(path).Length,
                sha256 = Hash(path)
            }).ToArray()
        };
        File.WriteAllText(manifestPath, JsonConvert.SerializeObject(manifest, Formatting.Indented));
        Console.WriteLine($"Audited {lod.Vertices.Length} vertices, up to {maxInfluences} influences, {dto.Bones.Length} bones");
    }

    private static string Hash(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }
}
