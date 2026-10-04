// Offline UE 5.6 shared-library readback. Never writes to input containers.
using System.Buffers.Binary;
using System.Security.Cryptography;
using CUE4Parse.Compression;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.IO;
using CUE4Parse.UE4.IO.Objects;
using CUE4Parse.UE4.Readers;
using CUE4Parse.UE4.Shaders;
using CUE4Parse.UE4.Versions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

if (args.Length != 3 || Directory.Exists(args[2]) || File.Exists(args[2]))
    throw new ArgumentException("ShaderReadback CONTAINERS SHADER_MAP_JSON NEW_DIRECTORY");
var requests = new List<(string Package, string Platform, string Hash, int[] Indices)>();
foreach (var package in JObject.Parse(File.ReadAllText(args[1])).Properties())
foreach (var export in (JArray)package.Value)
foreach (var resource in export["LoadedMaterialResources"] as JArray ?? new JArray()) {
    var map = resource["LoadedShaderMap"] ?? throw new InvalidDataException("Missing shader map");
    var hash = map.Value<string>("ResourceHash") ?? throw new InvalidDataException("Expected shared shader code");
    if (hash.Length != 40 || !hash.All(Uri.IsHexDigit)) throw new InvalidDataException("Invalid shader-map hash");
    var content = map["Content"] ?? throw new InvalidDataException("Missing shader content");
    var shaders = (content["Shaders"] as JArray ?? new JArray()).Concat(
        (content["OrderedMeshShaderMaps"] as JArray ?? new JArray()).SelectMany(mesh => (JArray)mesh["Shaders"]!));
    var indices = shaders.Where(shader => shader["Target"]?.Value<string>("Frequency") == "SF_Pixel" &&
            (shader.Value<string>("Type") ?? "").StartsWith("TBasePassPS", StringComparison.Ordinal))
        .Select(shader => shader.Value<int>("ResourceIndex")).Distinct().Order().ToArray();
    if (indices.Length == 0 || indices.Any(index => index < 0)) throw new InvalidDataException("No base-pass pixel shaders");
    requests.Add((package.Name, map.Value<string>("ShaderPlatform")!, hash, indices));
}
if (requests.Count == 0) throw new InvalidDataException("No material shader requests");
Directory.CreateDirectory(args[2]);
using var provider = new DefaultFileProvider(args[0], SearchOption.TopDirectoryOnly,
    new VersionContainer(EGame.GAME_UE5_6), StringComparer.OrdinalIgnoreCase);
provider.Initialize(); provider.Mount();
var readers = provider.MountedVfs.OfType<IoStoreReader>().ToArray();
var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
var rows = new JArray();
foreach (var reader in readers)
foreach (var chunk in reader.TocResource.ChunkIds.Where(chunk => chunk.ChunkType == (byte)EIoChunkType5.ShaderCodeLibrary)) {
    using var archive = new FByteArchive(reader.Name, reader.Read(chunk), reader.Versions);
    if (new FShaderCodeArchive(archive).SerializedShaders is not FIoStoreShaderCodeArchive library)
        throw new InvalidDataException("Expected UE5 IoStore shader library");
    foreach (var request in requests) {
        var mapIndex = Array.FindIndex(library.ShaderMapHashes, hash => hash.ToString().Equals(request.Hash, StringComparison.OrdinalIgnoreCase));
        if (mapIndex < 0) continue;
        if (!found.Add(request.Hash)) throw new InvalidDataException("Shader-map hash occurs in multiple libraries");
        var map = library.ShaderMapEntries[mapIndex];
        foreach (var resourceIndex in request.Indices) {
            if (resourceIndex >= map.NumShaders) throw new InvalidDataException("Shader resource index exceeds map");
            var shaderIndex = library.ShaderIndices[checked((int)map.ShaderIndicesOffset + resourceIndex)];
            var shader = library.ShaderEntries[shaderIndex];
            if (shader.Frequency.ToString() != "SF_Pixel") throw new InvalidDataException("Mapped shader is not a pixel shader");
            var group = library.ShaderGroupEntries[shader.ShaderGroupIndex];
            if (group.UncompressedSize == 0 || group.UncompressedSize > 128 * 1024 * 1024 || group.CompressedSize > group.UncompressedSize)
                throw new InvalidDataException("Shader group size invalid or exceeds 128 MiB");
            var groupId = library.ShaderGroupIoHashes[shader.ShaderGroupIndex];
            var owners = readers.Where(candidate => candidate.TryResolve(groupId, out _)).ToArray();
            if (owners.Length != 1) throw new InvalidDataException("Shader group missing or ambiguous");
            var stored = owners[0].Read(groupId);
            if (stored.Length != group.CompressedSize) throw new InvalidDataException("Shader group stored size mismatch");
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
    ["scope"]="Base-pass pixel shader bytecode extracted from native shared libraries. No material reconstruction or runtime change.",
    ["shaders"]=rows
}.ToString(Formatting.Indented) + "\n");
Console.WriteLine($"Extracted {rows.Count} pixel shaders from {found.Count} maps");

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
