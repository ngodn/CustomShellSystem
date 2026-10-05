// Check cooked import hashes using only the containers supplied to this process.
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;

if (args.Length != 5)
    throw new ArgumentException("AssetDependencyAudit CONTAINERS MAPPINGS PACKAGES REFERENCES OUTPUT");
if (File.Exists(args[4])) throw new IOException("Output already exists");
var packages = File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p)).ToArray();
if (packages.Length == 0 || packages.Distinct(StringComparer.OrdinalIgnoreCase).Count() != packages.Length)
    throw new ArgumentException("Expected a nonempty list of unique packages");
var references = File.ReadAllLines(args[3]).Where(p => !string.IsNullOrWhiteSpace(p)).ToArray();
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.Initialize();
provider.Mount();
var failures = new List<object>();
var rows = new List<object>();
foreach (var path in packages) {
    try {
        var package = (IoPackage)provider.LoadPackage(path);
        var imports = 0;
        var dependencies = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var import in package.ImportMap) {
            if (!import.IsPackageImport) continue;
            imports++;
            var resolved = package.ResolveObjectIndex(import);
            if (resolved != null) dependencies.Add(resolved.Package.Name);
            if (resolved == null)
                failures.Add(new { package = path, error = "Unresolved package export", import = import.ToString() });
        }
        foreach (var reference in references.Where(r => r.Split('.')[0] == path)) {
            var dot = reference.IndexOf('.');
            if (dot >= 0 && package.GetExportIndex(reference[(dot + 1)..]) < 0)
                failures.Add(new { package = path, error = "Missing catalog export", reference });
        }
        rows.Add(new { package = path, exports = package.ExportMapLength, imports, dependencies });
    } catch (Exception error) {
        failures.Add(new { package = path, error = error.Message });
    }
}
foreach (var reference in references)
    if (!packages.Contains(reference.Split('.')[0], StringComparer.OrdinalIgnoreCase))
        failures.Add(new { reference, error = "Catalog package absent from container" });
var result = new { passed = failures.Count == 0, packages = rows, references, failures,
    scope = "Cooked package presence, catalog export names and public import hashes. Not in-game execution." };
File.WriteAllText(args[4], JsonConvert.SerializeObject(result, Formatting.Indented) + "\n");
Console.WriteLine($"Packages: {rows.Count}; references: {references.Length}; failures: {failures.Count}");
return failures.Count == 0 ? 0 : 1;
