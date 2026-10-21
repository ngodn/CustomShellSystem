using CUE4Parse;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.IO;
using CUE4Parse.UE4.IO.Objects;
using CUE4Parse.UE4.Readers;
using CUE4Parse.UE4.Shaders;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

if (args.Length == 5 && args[0] == "--extract-tails")
    return MaterialShaderSlices.Extract(args[1..]);

if (args.Length != 4 || File.Exists(args[3]))
    throw new ArgumentException("ShaderDependencyAudit CONTAINERS MAPPINGS PACKAGE_LIST NEW_OUTPUT_JSON");
var packages = File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p)).ToArray();
if (packages.Length == 0 || packages.Distinct(StringComparer.OrdinalIgnoreCase).Count() != packages.Length)
    throw new ArgumentException("Expected a nonempty list of unique packages");
Globals.FatalObjectSerializationErrors = true;
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.ReadShaderMaps = true;
provider.Initialize();
provider.Mount();
var readers = provider.MountedVfs.OfType<IoStoreReader>().ToArray();
var libraries = new List<(string Owner, FIoStoreShaderCodeArchive Library)>();
var failures = new JArray();
foreach (var reader in readers)
foreach (var chunk in reader.TocResource.ChunkIds.Where(c => c.ChunkType == (byte)EIoChunkType5.ShaderCodeLibrary)) {
    try {
        using var archive = new FByteArchive(reader.Name, reader.Read(chunk), reader.Versions);
        if (new FShaderCodeArchive(archive).SerializedShaders is not FIoStoreShaderCodeArchive library)
            throw new InvalidDataException("Expected IoStore shader library");
        libraries.Add((reader.Name, library));
    } catch (Exception error) {
        failures.Add(new JObject { ["container"] = reader.Name, ["error"] = error.Message });
    }
}
var available = new Dictionary<string, List<(string Owner, FIoStoreShaderCodeArchive Library, int Index)>>(StringComparer.OrdinalIgnoreCase);
foreach (var (owner, library) in libraries)
for (int i = 0; i < library.ShaderMapHashes.Length; ++i) {
    var hash = library.ShaderMapHashes[i].ToString();
    if (!available.TryGetValue(hash, out var entries)) available.Add(hash, entries = []);
    entries.Add((owner, library, i));
}
var checkedHashes = new Dictionary<string, JObject>(StringComparer.OrdinalIgnoreCase);
var materials = new JArray();
var inspected = 0;
foreach (var path in packages) {
    try {
        var package = (IoPackage)provider.LoadPackage(path);
        // Inspect export classes before loading: meshes and textures can be hundreds of MB.
        for (int i = 0; i < package.ExportMap.Length; ++i) {
            var type = package.ResolveObjectIndex(package.ExportMap[i].ClassIndex)?.Name.Text
                ?? throw new InvalidDataException("Unresolved export class");
            if (type is not ("Material" or "MaterialInstanceConstant")) continue;
            if (package.ExportsLazy[i].Value is not UMaterialInterface material)
                throw new InvalidDataException("Material export was not decoded as a material");
            var maps = new JArray();
            foreach (var resource in material.LoadedMaterialResources) {
                var map = resource.LoadedShaderMap ?? throw new InvalidDataException("Invalid serialized shader map");
                var hash = map.ResourceHash?.ToString();
                if (hash is null && map.Code is null) throw new InvalidDataException("Shader map has no code or shared hash");
                if (hash is not null && !checkedHashes.ContainsKey(hash)) {
                    var owners = new JArray();
                    var absentGroups = new HashSet<string>();
                    if (available.TryGetValue(hash, out var entries)) foreach (var entry in entries) {
                        var descriptor = entry.Library.ShaderMapEntries[entry.Index];
                        if (descriptor.NumShaders == 0) throw new InvalidDataException("Empty shared shader map");
                        owners.Add(entry.Owner);
                        for (uint n = 0; n < descriptor.NumShaders; ++n) {
                            var shaderIndex = entry.Library.ShaderIndices[checked(descriptor.ShaderIndicesOffset + n)];
                            var shader = entry.Library.ShaderEntries[shaderIndex];
                            var group = entry.Library.ShaderGroupIoHashes[shader.ShaderGroupIndex];
                            if (!readers.Any(reader => reader.TryResolve(group, out _))) absentGroups.Add(group.ToString());
                        }
                    }
                    checkedHashes.Add(hash, new JObject {
                        ["found"] = owners.Count > 0, ["owners"] = owners,
                        ["missing_groups"] = new JArray(absentGroups.Order())
                    });
                }
                maps.Add(new JObject { ["platform"] = map.ShaderPlatform.ToString(),
                    ["storage"] = hash is null ? "inline" : "shared", ["hash"] = hash });
            }
            var properties = JObject.Parse(JsonConvert.SerializeObject(material))["Properties"]!;
            if (material.LoadedMaterialResources.Count == 0 &&
                (material is UMaterial || properties.Value<bool?>("bHasStaticPermutationResource") == true))
                throw new InvalidDataException("Material declares shaders but no resources were decoded");
            materials.Add(new JObject { ["package"] = path, ["export"] = material.Name,
                ["properties"] = properties, ["maps"] = maps });
        }
        inspected++;
    } catch (Exception error) {
        failures.Add(new JObject { ["package"] = path, ["error"] = error.ToString() });
    }
}
var missing = checkedHashes.Where(pair => !pair.Value.Value<bool>("found") ||
    ((JArray)pair.Value["missing_groups"]!).Count > 0).Select(pair => pair.Key).Order().ToArray();
var report = new JObject {
    ["passed"] = failures.Count == 0 && missing.Length == 0,
    ["scope"] = "Every requested package header and Material/MaterialInstanceConstant export; shared hash and shader-group presence in supplied containers. Inline code is parsed, not executed. Does not prove rendering, shader bytecode correctness, or game mount order.",
    ["packages_requested"] = packages.Length, ["packages_inspected"] = inspected,
    ["libraries"] = libraries.Count, ["materials"] = materials,
    ["shared_hashes"] = JObject.FromObject(checkedHashes),
    ["missing_hashes"] = new JArray(missing), ["failures"] = failures
};
using (var output = new StreamWriter(new FileStream(args[3], FileMode.CreateNew, FileAccess.Write)))
    output.WriteLine(report.ToString(Formatting.Indented));
Console.WriteLine($"Packages {inspected}/{packages.Length}; materials {materials.Count}; shared maps {checkedHashes.Count}; missing {missing.Length}; errors {failures.Count}");
return report.Value<bool>("passed") ? 0 : 1;
