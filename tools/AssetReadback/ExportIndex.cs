using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;

static class ExportIndex
{
    public static void Run(string[] args) {
        if (File.Exists(args[3])) throw new IOException("Output already exists");
        using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
            new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
        provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
        provider.Initialize(); provider.Mount();
        var rows = new List<object>();
        foreach (var path in File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p))) {
            var package = provider.LoadPackage(path) as IoPackage
                ?? throw new InvalidDataException("Expected IoStore package: " + path);
            var exports = package.ExportMap.Select(e => new {
                name = package.CreateFNameFromMappedName(e.ObjectName).Text,
                type = package.ResolveObjectIndex(e.ClassIndex)?.Name.Text
                    ?? throw new InvalidDataException("Unresolved export class: " + path)
            }).ToArray();
            rows.Add(new { path, exports });
        }
        if (rows.Count == 0) throw new InvalidDataException("No packages supplied");
        File.WriteAllText(args[3], JsonConvert.SerializeObject(rows, Formatting.Indented) + "\n");
        Console.WriteLine($"Indexed export headers in {rows.Count} packages");
    }
}
