// Offline UE 5.6 shared-library readback. Never writes to input containers.
using System.Buffers.Binary;
using System.Security.Cryptography;
using CUE4Parse.Compression;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.IO;
using CUE4Parse.UE4.IO.Objects;
using CUE4Parse.UE4.Readers;
using CUE4Parse.UE4.Shaders;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

if (args.Length is not (3 or 4 or 6) || Directory.Exists(args[2]) || File.Exists(args[2]) ||
    (args.Length >= 4 && args[3] is not ("--distortion" or "--vertex" or "--all-vertex" or "--pixel")) ||
    (args.Length == 6 && args[4] != "--inline-oodle"))
    throw new ArgumentException("ShaderReadback CONTAINERS SHADER_MAP_JSON NEW_DIRECTORY [--distortion|--vertex|--all-vertex|--pixel] [--inline-oodle MAPPINGS]");
bool inlineEnabled = args.Length == 6;
bool distortion = args.Length >= 4 && args[3] == "--distortion";
bool allVertex = args.Length >= 4 && args[3] == "--all-vertex";
bool vertex = allVertex || (args.Length >= 4 && args[3] == "--vertex");
string frequency = vertex ? "SF_Vertex" : "SF_Pixel";
var requests = new List<(string Package, string Platform, string Hash, int[] Indices, bool Inline)>();
foreach (var package in JObject.Parse(File.ReadAllText(args[1])).Properties())
foreach (var export in (JArray)package.Value)
foreach (var resource in export["LoadedMaterialResources"] as JArray ?? new JArray()) {
    var map = resource["LoadedShaderMap"] ?? throw new InvalidDataException("Missing shader map");
    bool inline = map["Code"] is JObject;
    if (inline && !inlineEnabled) throw new InvalidDataException("Inline shader code requires --inline-oodle MAPPINGS");
    var hash = (inline ? map["Code"]!.Value<string>("ResourceHash") : map.Value<string>("ResourceHash")) ??
        throw new InvalidDataException("Missing shader resource hash");
    if (hash.Length != 40 || !hash.All(Uri.IsHexDigit)) throw new InvalidDataException("Invalid shader-map hash");
    var content = map["Content"] ?? throw new InvalidDataException("Missing shader content");
    var shaders = (content["Shaders"] as JArray ?? new JArray()).Concat(
        (content["OrderedMeshShaderMaps"] as JArray ?? new JArray()).SelectMany(mesh => (JArray)mesh["Shaders"]!));
    var indices = shaders.Where(shader => shader["Target"]?.Value<string>("Frequency") == frequency &&
            (allVertex || (distortion ? shader.Value<string>("Type") == "FDistortionMeshPS" :
             (shader.Value<string>("Type") ?? "").StartsWith(vertex ? "TBasePassVS" : "TBasePassPS", StringComparison.Ordinal))))
        .Select(shader => shader.Value<int>("ResourceIndex")).Distinct().Order().ToArray();
    if (indices.Length == 0 || indices.Any(index => index < 0)) throw new InvalidDataException("No matching shaders");
    requests.Add((package.Name, map.Value<string>("ShaderPlatform")!, hash, indices, inline));
}
if (requests.Count == 0) throw new InvalidDataException("No material shader requests");
Directory.CreateDirectory(args[2]);
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
if (inlineEnabled) {
    provider.MappingsContainer = new FileUsmapTypeMappingsProvider(args[5]);
    provider.ReadScriptData = true;
    provider.ReadShaderMaps = true;
}
provider.Initialize(); provider.Mount();
var readers = provider.MountedVfs.OfType<IoStoreReader>().ToArray();
var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
var rows = new JArray();
foreach (var request in requests.Where(request => request.Inline)) {
    var maps = provider.LoadPackage(request.Package).GetExports().OfType<UMaterialInterface>()
        .SelectMany(material => material.LoadedMaterialResources)
        .Select(resource => resource.LoadedShaderMap?.Code)
        .Where(code => code != null && code.ResourceHash.ToString().Equals(request.Hash, StringComparison.OrdinalIgnoreCase))
        .ToArray();
    if (maps.Length != 1 || !found.Add(request.Hash)) throw new InvalidDataException("Expected one matching inline shader map");
    var map = maps[0]!;
    if (map.ShaderHashes.Length != map.ShaderCodeResources.Length) throw new InvalidDataException("Inline shader hash count mismatch");
    foreach (int resourceIndex in request.Indices) {
        if (resourceIndex >= map.ShaderCodeResources.Length) throw new InvalidDataException("Inline resource index exceeds map");
        var shader = map.ShaderCodeResources[resourceIndex];
        var header = shader.Header;
        var stored = shader.Code.Data;
        if (header.Frequency.ToString() != frequency || shader.Code.Len != stored.Length ||
            header.UncompressedSize <= 0 || header.UncompressedSize > 128 * 1024 * 1024 ||
            stored.Length > header.UncompressedSize || header.ShaderCodeSize <= 0 ||
            header.ShaderCodeSize > header.UncompressedSize)
            throw new InvalidDataException("Invalid inline shader header");
        var code = stored.Length == header.UncompressedSize ? stored :
            Compression.Decompress(stored, header.UncompressedSize, CompressionMethod.Oodle);
        var (offset, dxbc) = ExtractContainer(code[..header.ShaderCodeSize]);
        var filename = request.Hash + "-" + resourceIndex;
        WriteNew(Path.Combine(args[2], filename + ".uecode"), code);
        WriteNew(Path.Combine(args[2], filename + ".dxbc"), dxbc);
        rows.Add(JObject.FromObject(new {
            package=request.Package, platform=request.Platform, map_hash=request.Hash,
            resource_index=resourceIndex, shader_hash=map.ShaderHashes[resourceIndex].ToString(),
            storage="inline", compression=stored.Length == header.UncompressedSize ? "None" : "Oodle",
            stored_bytes=stored.Length, stored_sha256=Convert.ToHexString(SHA256.HashData(stored)),
            uecode=filename + ".uecode", uecode_sha256=Convert.ToHexString(SHA256.HashData(code)),
            container=filename + ".dxbc", container_offset=offset, container_bytes=dxbc.Length,
            container_sha256=Convert.ToHexString(SHA256.HashData(dxbc))
        }));
    }
}
foreach (var reader in readers)
foreach (var chunk in reader.TocResource.ChunkIds.Where(chunk => chunk.ChunkType == (byte)EIoChunkType5.ShaderCodeLibrary)) {
    using var archive = new FByteArchive(reader.Name, reader.Read(chunk), reader.Versions);
    if (new FShaderCodeArchive(archive).SerializedShaders is not FIoStoreShaderCodeArchive library)
        throw new InvalidDataException("Expected UE5 IoStore shader library");
    foreach (var request in requests.Where(request => !request.Inline)) {
        var mapIndex = Array.FindIndex(library.ShaderMapHashes, hash => hash.ToString().Equals(request.Hash, StringComparison.OrdinalIgnoreCase));
        if (mapIndex < 0) continue;
        if (!found.Add(request.Hash)) throw new InvalidDataException("Shader-map hash occurs in multiple libraries");
        var map = library.ShaderMapEntries[mapIndex];
        foreach (var resourceIndex in request.Indices) {
            if (resourceIndex >= map.NumShaders) throw new InvalidDataException("Shader resource index exceeds map");
            var shaderIndex = library.ShaderIndices[checked((int)map.ShaderIndicesOffset + resourceIndex)];
            var shader = library.ShaderEntries[shaderIndex];
            if (shader.Frequency.ToString() != frequency) throw new InvalidDataException("Mapped shader frequency differs from request");
            var group = library.ShaderGroupEntries[shader.ShaderGroupIndex];
            if (group.UncompressedSize == 0 || group.UncompressedSize > 128 * 1024 * 1024 || group.CompressedSize > group.UncompressedSize)
                throw new InvalidDataException("Shader group size invalid or exceeds 128 MiB");
            var groupId = library.ShaderGroupIoHashes[shader.ShaderGroupIndex];
            var owners = readers.Where(candidate => candidate.TryResolve(groupId, out _)).ToArray();
            if (owners.Length == 0) throw new InvalidDataException(
                $"Shader group {groupId} for map {request.Hash}, resource {resourceIndex} has {owners.Length} owners: " +
                string.Join(", ", owners.Select(owner => owner.Name)));
            var stored = owners[0].Read(groupId);
            if (stored.Length != group.CompressedSize) throw new InvalidDataException("Shader group stored size mismatch");
            foreach (var duplicate in owners.Skip(1))
                if (!stored.AsSpan().SequenceEqual(duplicate.Read(groupId)))
                    throw new InvalidDataException($"Conflicting shader group {groupId}: {owners[0].Name} and {duplicate.Name}");
            var decoded = group.CompressedSize == group.UncompressedSize ? stored :
                Compression.Decompress(stored, checked((int)group.UncompressedSize), CompressionMethod.Oodle);
            var members = library.ShaderIndices.AsSpan(checked((int)group.ShaderIndicesOffset), checked((int)group.NumShaders));
            var member = members.IndexOf(shaderIndex);
            if (member < 0) throw new InvalidDataException("Shader missing from its declared group");
            var end = member + 1 == members.Length ? group.UncompressedSize :
                library.ShaderEntries[members[member + 1]].UncompressedOffsetInGroup;
            var start = shader.UncompressedOffsetInGroup;
            if (end <= start || end > decoded.Length) throw new InvalidDataException("Shader range invalid");
            var code = decoded.AsSpan(checked((int)start), checked((int)(end - start))).ToArray();
            var (offset, dxbc) = ExtractContainer(code);
            var filename = request.Hash + "-" + resourceIndex;
            WriteNew(Path.Combine(args[2], filename + ".uecode"), code);
            WriteNew(Path.Combine(args[2], filename + ".dxbc"), dxbc);
            rows.Add(JObject.FromObject(new {
                package=request.Package, platform=request.Platform, map_hash=request.Hash,
                resource_index=resourceIndex, shader_index=shaderIndex, shader_hash=library.ShaderHashes[shaderIndex].ToString(),
                library=reader.Name, library_chunk=chunk.ToString(), group_chunk=groupId.ToString(),
                group_owners=owners.Select(owner => owner.Name).ToArray(),
                group_index=shader.ShaderGroupIndex, group_offset=start, group_end=end,
                uecode=filename + ".uecode", uecode_sha256=Convert.ToHexString(SHA256.HashData(code)),
                container=filename + ".dxbc", container_offset=offset, container_bytes=dxbc.Length,
                container_sha256=Convert.ToHexString(SHA256.HashData(dxbc))
            }));
        }
    }
}
if (found.Count != requests.Count) throw new InvalidDataException($"Located {found.Count} of {requests.Count} shader maps");
File.WriteAllText(Path.Combine(args[2], "manifest.json"), new JObject {
    ["scope"]=(allVertex ? "All" : distortion ? "Distortion" : "Base-pass") + (vertex ? " vertex" : " pixel") + " shader bytecode extracted from shared libraries or explicitly enabled inline Oodle code. No material reconstruction or runtime change.",
    ["shaders"]=rows
}.ToString(Formatting.Indented) + "\n");
Console.WriteLine($"Extracted {rows.Count} {(vertex ? "vertex" : "pixel")} shaders from {found.Count} maps");

static void WriteNew(string path, byte[] bytes) {
    using var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write);
    output.Write(bytes);
}
static (int Offset, byte[] Container) ExtractContainer(byte[] code) {
    var matches = new List<(int Offset, byte[] Container)>();
    for (int offset=0; offset<=code.Length-32; ++offset) {
        var bytes = code.AsSpan(offset);
        if (!bytes[..4].SequenceEqual("DXBC"u8)) continue;
        // Microsoft's DxilContainerHeader: size at 24, part count at 28.
        uint length=BinaryPrimitives.ReadUInt32LittleEndian(bytes[24..]);
        uint count=BinaryPrimitives.ReadUInt32LittleEndian(bytes[28..]);
        if (length<32 || length>bytes.Length || count==0 || count>(length-32)/4) continue;
        bool valid=true;
        uint tableEnd=32+count*4;
        for (int part=0;part<count;++part) {
            uint begin=BinaryPrimitives.ReadUInt32LittleEndian(bytes[(32+part*4)..]);
            if (begin<tableEnd || begin>length-8) { valid=false; break; }
            uint size=BinaryPrimitives.ReadUInt32LittleEndian(bytes[checked((int)begin+4)..]);
            if (size>length-begin-8) { valid=false; break; }
        }
        if (valid) matches.Add((offset,bytes[..checked((int)length)].ToArray()));
    }
    if (matches.Count!=1) throw new InvalidDataException("Expected exactly one bounded DXBC container in UE shader code");
    return matches[0];
}
