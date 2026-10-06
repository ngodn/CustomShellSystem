using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using CUE4Parse.UE4.Localization;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Objects.UObject;
using Newtonsoft.Json;
using Serilog;
using System.Text.RegularExpressions;

// args: PAKDIR USMAP OUTDIR MODE [PATTERN]
// MODE: list | locres | export <regex> | skel <regex>  (skel: one JSON line per skeletal mesh / skeleton: path, skeleton, bones)
Log.Logger = new LoggerConfiguration().WriteTo.Console(restrictedToMinimumLevel: Serilog.Events.LogEventLevel.Warning).CreateLogger();
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly, new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[1]);
provider.ReadScriptData = true;
provider.Initialize();
provider.Mount();
Directory.CreateDirectory(args[2]);
var mode = args[3];
if (mode == "list") {
    File.WriteAllLines(Path.Combine(args[2], "files.txt"), provider.Files.Keys.OrderBy(k => k));
    Console.WriteLine($"listed {provider.Files.Count}");
} else if (mode == "locres") {
    foreach (var (path, file) in provider.Files) {
        if (!path.EndsWith(".locres", StringComparison.OrdinalIgnoreCase)) continue;
        try {
            var loc = new FTextLocalizationResource(file.CreateReader());
            var dict = new Dictionary<string, Dictionary<string,string>>();
            foreach (var (ns, entries) in loc.Entries) {
                var d = new Dictionary<string,string>();
                foreach (var (key, e) in entries) d[key.Str] = e.LocalizedString;
                dict[ns.Str] = d;
            }
            var outp = Path.Combine(args[2], path.Replace('/', '_') + ".json");
            File.WriteAllText(outp, JsonConvert.SerializeObject(dict, Formatting.Indented));
            Console.WriteLine($"{path}: {dict.Sum(x => x.Value.Count)} entries");
        } catch (Exception ex) { Console.WriteLine($"{path}: FAIL {ex.Message}"); }
    }
} else if (mode == "export") {
    var re = new Regex(args[4], RegexOptions.IgnoreCase);
    int n = 0, fail = 0;
    foreach (var path in provider.Files.Keys.ToArray()) {
        if (!re.IsMatch(path)) continue;
        if (!path.EndsWith(".uasset") && !path.EndsWith(".umap")) continue;
        try {
            var exports = provider.LoadPackage(path).GetExports().ToArray();
            var outp = Path.Combine(args[2], path.Replace('/', '_') + ".json");
            File.WriteAllText(outp, JsonConvert.SerializeObject(exports, Formatting.Indented));
            n++;
        } catch (Exception ex) { fail++; Console.WriteLine($"{path}: FAIL {ex.Message.Split('\n')[0]}"); }
    }
    Console.WriteLine($"exported {n}, failed {fail}");
} else if (mode == "raw") {
    // raw <regex>: write matching non-asset files (ini, uplugin, json...) byte-for-byte under OUTDIR/<path>
    var re = new Regex(args[4], RegexOptions.IgnoreCase);
    int n = 0, fail = 0;
    foreach (var (path, file) in provider.Files) {
        if (!re.IsMatch(path)) continue;
        try {
            var bytes = file.Read();
            var outp = Path.Combine(args[2], path);
            Directory.CreateDirectory(Path.GetDirectoryName(outp)!);
            File.WriteAllBytes(outp, bytes);
            n++;
        } catch (Exception ex) { fail++; Console.WriteLine($"{path}: FAIL {ex.Message.Split('\n')[0]}"); }
    }
    Console.WriteLine($"raw {n}, failed {fail}");
} else if (mode == "skel") {
    var re = new Regex(args[4], RegexOptions.IgnoreCase);
    int n = 0, fail = 0;
    using var w = new StreamWriter(Path.Combine(args[2], "skeletons.jsonl"));
    foreach (var path in provider.Files.Keys.ToArray()) {
        if (!re.IsMatch(path) || !path.EndsWith(".uasset")) continue;
        try {
            foreach (var e in provider.LoadPackage(path).GetExports()) {
                if (e is USkeletalMesh mesh) {
                    var skel = mesh.GetOrDefault<FPackageIndex>("Skeleton");
                    var bones = mesh.ReferenceSkeleton.FinalRefBoneInfo.Select(b => b.Name.Text).ToArray();
                    w.WriteLine(JsonConvert.SerializeObject(new { kind = "mesh", path, skeleton = skel?.ResolvedObject?.GetPathName(), bones }));
                    n++;
                } else if (e is USkeleton sk) {
                    var bones = sk.ReferenceSkeleton.FinalRefBoneInfo.Select(b => b.Name.Text).ToArray();
                    w.WriteLine(JsonConvert.SerializeObject(new { kind = "skeleton", path, bones }));
                    n++;
                }
            }
        } catch (Exception ex) { fail++; Console.WriteLine($"{path}: FAIL {ex.Message.Split('\n')[0]}"); }
    }
    Console.WriteLine($"skeletons {n}, failed {fail}");
}
