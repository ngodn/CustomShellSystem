using CUE4Parse;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets.Readers;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Objects.Properties;
using CUE4Parse.UE4.Readers;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using System.Security.Cryptography;

static class SocketAdjustmentSlices
{
    public static void Run(string[] args) {
#if !DEBUG
        throw new InvalidOperationException("Socket spans require a Debug build, which retains CUE4Parse property offsets");
#else
        if (File.Exists(args[3])) throw new IOException("Output already exists");
        Globals.FatalObjectSerializationErrors = true;
        using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
            new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
        provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
        provider.Initialize(); provider.Mount();
        var rows = new List<object>();
        foreach (var path in File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p))) {
            var file = provider[path];
            var bytes = file.Read();
            var package = provider.LoadPackage(file);
            var owner = package.GetExports().Single(e => e.Properties.Any(p => p.Name.Text == "SocketAdjustments"));
            var tag = owner.Properties.Single(p => p.Name.Text == "SocketAdjustments");
            if (tag.TagData?.InnerType != "NameProperty" || tag.TagData.ValueType != "StructProperty" ||
                tag.TagData.ValueTypeData?.StructType != "Transform")
                throw new InvalidDataException("Expected a name-to-transform socket map");
            using var archive = new FAssetArchive(new FByteArchive(file.Path, bytes, provider.Versions), package);
            archive.Position = tag.Position;
            if (archive.Read<int>() != 0) throw new InvalidDataException("Socket map has removed keys");
            var count = archive.Read<int>();
            if (count < 1 || count > 256) throw new InvalidDataException("Unexpected socket count");
            var slices = new List<object>();
            var names = new HashSet<string>();
            for (var i = 0; i < count; ++i) {
                var name = archive.ReadFName().Text;
                if (!names.Add(name)) throw new InvalidDataException("Duplicate socket name");
                var offset = checked((int)archive.Position);
                var transform = new StructProperty(archive, tag.TagData.ValueTypeData, ReadType.MAP);
                var size = checked((int)archive.Position - offset);
                if (size < 40 || size > 256) throw new InvalidDataException("Unexpected transform span");
                slices.Add(new { name, offset, hex = Convert.ToHexStringLower(bytes.AsSpan(offset, size)), transform });
            }
            if (archive.Position != tag.Position + tag.Size) throw new InvalidDataException("Socket map span mismatch");
            rows.Add(new { package = path, export = owner.Name, sha256 = Convert.ToHexStringLower(SHA256.HashData(bytes)), slices });
        }
        if (rows.Count == 0) throw new InvalidDataException("No socket packages");
        File.WriteAllText(args[3], JsonConvert.SerializeObject(rows, Formatting.Indented) + "\n");
        Console.WriteLine($"Read socket transform spans in {rows.Count} packages");
#endif
    }
}
