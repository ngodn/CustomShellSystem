using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using CUE4Parse_Conversion;
using CUE4Parse_Conversion.Options;
using Newtonsoft.Json;
using Serilog;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Objects.UObject;

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
