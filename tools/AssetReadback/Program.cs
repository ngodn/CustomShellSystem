// Read cooked package properties independently of the Retoc writer. .NET 10.
// One JSON document keyed by full package path avoids duplicate leaf filenames.
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

if (args.Length != 4)
    throw new ArgumentException("AssetReadback CONTAINERS MAPPINGS PACKAGE_LIST OUTPUT_JSON");
if (File.Exists(args[3])) throw new IOException("Output already exists");
var packages = File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p)).ToArray();
if (packages.Length == 0 || packages.Distinct(StringComparer.OrdinalIgnoreCase).Count() != packages.Length)
    throw new ArgumentException("Expected a nonempty list of unique packages");
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.ReadScriptData = true;
provider.Initialize();
provider.Mount();
var results = new JObject();
foreach (var path in packages) {
    var package = provider.LoadPackage(path);
    var exports = package.GetExports().ToArray();
    if (exports.Length == 0) throw new InvalidDataException($"No exports: {path}");
    results.Add(path, JArray.Parse(JsonConvert.SerializeObject(exports)));
}
using var output = new StreamWriter(new FileStream(args[3], FileMode.CreateNew, FileAccess.Write));
output.Write(results.ToString(Formatting.Indented));
Console.WriteLine($"Decoded {results.Count} packages");
