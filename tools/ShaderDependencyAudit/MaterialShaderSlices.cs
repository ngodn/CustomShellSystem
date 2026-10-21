using System.Security.Cryptography;
using CUE4Parse;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Readers;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json.Linq;

static class MaterialShaderSlices
{
    public static int Extract(string[] args)
    {
        if (Directory.Exists(args[3]) || File.Exists(args[3]))
            throw new ArgumentException("Shader slice output directory must be new");
        Globals.FatalObjectSerializationErrors = true;
        ObjectTypeRegistry.RegisterClass("MaterialInstanceConstant", typeof(MaterialWithShaderSlice));
        using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
            new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
        provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
        provider.ReadShaderMaps = true;
        provider.Initialize();
        provider.Mount();
        var rows = new JArray();
        var slices = new List<byte[]>();
        foreach (var path in File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p))) {
            var exports = provider.LoadPackage(path).GetExports().ToArray();
            if (exports.Length != 1 || exports[0] is not MaterialWithShaderSlice material)
                throw new InvalidDataException("Expected one material instance: " + path);
            if (material.Slice.Length < 1024 || material.LoadedMaterialResources.Count != 2 ||
                material.LoadedMaterialResources.Any(r => r.LoadedShaderMap?.ResourceHash is null))
                throw new InvalidDataException("Expected two shared shader maps: " + path);
            var file = $"{slices.Count:D3}.bin";
            rows.Add(new JObject { ["package"] = path, ["file"] = file,
                ["bytes"] = material.Slice.Length,
                ["sha256"] = Convert.ToHexStringLower(SHA256.HashData(material.Slice)),
                ["resources"] = JArray.FromObject(material.LoadedMaterialResources) });
            slices.Add(material.Slice);
        }
        if (slices.Count == 0) throw new InvalidDataException("No requested materials");
        Directory.CreateDirectory(args[3]);
        for (int i = 0; i < slices.Count; ++i)
            File.WriteAllBytes(Path.Combine(args[3], $"{i:D3}.bin"), slices[i]);
        File.WriteAllText(Path.Combine(args[3], "slices.json"), rows.ToString() + "\n");
        Console.WriteLine($"Extracted {slices.Count} material shader tails");
        return 0;
    }
}

// Mirrors the UE5.6 material-instance suffix only, after the standard property
// decoder. Kept separate so the ordinary independent readback remains unchanged.
public class MaterialWithShaderSlice : UMaterialInterface
{
    public byte[] Slice = [];

    public override void Deserialize(FAssetArchive archive, long validPos)
    {
        base.Deserialize(archive, validPos);
        if (FUE5MainStreamObjectVersion.Get(archive) < FUE5MainStreamObjectVersion.Type.MaterialSavedCachedData)
            throw new InvalidDataException("Expected UE5.6 material cached-data layout");
        if (archive.ReadBoolean()) _ = new FStructFallback(archive, "MaterialInstanceCachedData");
        if (!GetOrDefault<bool>("bHasStaticPermutationResource"))
            throw new InvalidDataException("Material has no static permutation");
        var start = archive.Position;
        DeserializeInlineShaderMaps(archive, LoadedMaterialResources);
        if (archive.Position != validPos)
            throw new InvalidDataException("Shader resources do not consume the complete export suffix");
        archive.Position = start;
        Slice = archive.ReadBytes(checked((int)(validPos - start)));
    }
}
