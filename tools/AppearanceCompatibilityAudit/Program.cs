using CUE4Parse;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.IO;
using CUE4Parse.UE4.IO.Objects;
using CUE4Parse.UE4.Versions;
using CUE4Parse.UE4.VirtualFileSystem;
using Newtonsoft.Json;

if (args.Length != 4 || File.Exists(args[3]))
    throw new ArgumentException("AppearanceCompatibilityAudit CONTAINERS MAPPINGS REFERENCES NEW_OUTPUT_JSON");
var references = File.ReadAllLines(args[2]).Where(p => !string.IsNullOrWhiteSpace(p)).Distinct().ToArray();
if (references.Length == 0) throw new ArgumentException("No references supplied");
Globals.FatalObjectSerializationErrors = true;
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.Initialize();
provider.Mount();
// This retained CUE4Parse revision fills its ID index in concurrent mount order.
// Rebuild it by patch priority before resolving any imports or loading packages.
var mounted = provider.MountedVfs.OrderBy(v => v.ReadOrder).ThenBy(v => v.Name, StringComparer.OrdinalIgnoreCase).ToArray();
provider.Files.Clear();
foreach (var archive in mounted)
    provider.Files.AddFiles(archive.Files, archive.ReadOrder, archive is IoStoreReader io ? io.PackageIdIndex : null);
var overlaps = mounted.OfType<IoStoreReader>().SelectMany(v => v.PackageIdIndex.Select(p => new { id = p.Key, owner = v.Name, order = v.ReadOrder }))
    .GroupBy(p => p.id).Where(g => g.Count() > 1)
    .Select(g => new { package_id = g.Key.ToString(), owners = g.OrderByDescending(p => p.order).ToArray(), tied_priority = g.Count(p => p.order == g.Max(x => x.order)) > 1 }).ToArray();
var roots = references.GroupBy(p => p.Split('.')[0], StringComparer.OrdinalIgnoreCase)
    .ToDictionary(g => g.Key, g => g.ToArray(), StringComparer.OrdinalIgnoreCase);
var pending = new Queue<string>(roots.Keys);
var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
var rows = new List<object>();
var meshes = new List<object>();
var failures = new List<object>();
while (pending.TryDequeue(out var path)) {
    if (!visited.Add(path)) continue;
    if (visited.Count > 20000) throw new InvalidDataException("Dependency walk exceeded 20000 packages");
    try {
        // IoStore resolves by package ID even when the mod uses another project directory.
        var id = FPackageId.FromName(path);
        var file = provider.FilesById[id];
        var package = provider.LoadPackage(id);
        var header = (file as FIoStoreEntry)?.IoStoreReader.ContainerHeader;
        var store = header?.StoreEntries[Array.IndexOf(header.PackageIds, id)];
        var dependencies = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var import in package.ImportMap.Where(i => i.IsPackageImport)) {
            var resolved = package.ResolveObjectIndex(import);
            var key = import.AsPackageImportRef;
            var target = package.ImportedPackages.Value[key.ImportedPackageIndex];
            var expected = package.ImportedPublicExportHashes![key.ImportedPublicExportHashIndex];
            if (resolved is null || target is null || !target.ExportMap.Any(e => e.PublicExportHash == expected))
                failures.Add(new { package = path, error = "Unresolved public import in effective package", import = import.TypeAndId.ToString("x16"), target = target?.Name,
                    target_package_id = store?.ImportedPackages[key.ImportedPackageIndex].id.ToString("x16"), export_hash = expected.ToString("x16") });
            else if (resolved.Package.Name.StartsWith("/Game/", StringComparison.OrdinalIgnoreCase))
                dependencies.Add(resolved.Package.Name);
        }
        foreach (var dependency in dependencies) pending.Enqueue(dependency);
        if (roots.TryGetValue(path, out var requested)) {
            foreach (var reference in requested.Where(r => r.Contains('.')))
                if (package.GetExportIndex(reference[(reference.IndexOf('.') + 1)..]) < 0)
                    failures.Add(new { reference, error = "Missing requested export" });
            for (var i = 0; i < package.ExportMap.Length; ++i) {
                var type = package.ResolveObjectIndex(package.ExportMap[i].ClassIndex)?.Name.Text;
                if (type != "SkeletalMesh") continue;
                var mesh = package.ExportsLazy[i].Value as USkeletalMesh
                    ?? throw new InvalidDataException("Skeletal mesh did not deserialize");
                if (mesh.ReferenceSkeleton.FinalRefBoneInfo.Length == 0 || mesh.LODModels is not { Length: > 0 })
                    throw new InvalidDataException("Empty skeletal mesh");
                var skeleton = mesh.Skeleton.ResolvedObject?.GetPathName()
                    ?? throw new InvalidDataException("Missing skeleton");
                meshes.Add(new {
                    package = path, mesh.Name, skeleton,
                    bones = mesh.ReferenceSkeleton.FinalRefBoneInfo.Select(b => new { name = b.Name.Text, parent = b.ParentIndex }),
                    materials = mesh.SkeletalMaterials.Select((m, index) => new { index, slot = m.MaterialSlotName.Text, asset = m.MaterialInterface?.ResolvedObject?.GetPathName(), import_index = m.MaterialInterface?.Index }),
                    physics = mesh.PhysicsAsset.ResolvedObject?.GetPathName(),
                    lods = mesh.LODModels.Select(l => new { vertices = l.NumVertices, indices = l.Indices?.Buffer?.Length,
                        sections = l.Sections.Select(s => new { material = s.MaterialIndex, triangles = s.NumTriangles, disabled = s.bDisabled }) })
                });
            }
        }
        rows.Add(new { package = path, file = file.Path, container = (file as VfsEntry)?.Vfs.Name, dependencies,
            root_name_paths = roots.ContainsKey(path) ? package.NameMap.Select(n => n.Name).OfType<string>().Where(n => n.StartsWith("/Game/")).ToArray() : null });
    } catch (Exception error) {
        failures.Add(new { package = path, error = error.Message });
    }
    if (visited.Count % 100 == 0) Console.WriteLine($"Checked {visited.Count}, pending {pending.Count}, failures {failures.Count}");
}
File.WriteAllText(args[3], JsonConvert.SerializeObject(new {
    passed = failures.Count == 0, references, packages = rows, meshes, overlaps, failures,
    scope = "Effective IoStore package ownership, requested exports, cooked public import closure, root mesh skeletons/materials/render LODs. No gameplay, physics or visual acceptance."
}, Formatting.Indented) + "\n");
Console.WriteLine($"Packages {rows.Count}; meshes {meshes.Count}; failures {failures.Count}");
return failures.Count == 0 ? 0 : 1;
