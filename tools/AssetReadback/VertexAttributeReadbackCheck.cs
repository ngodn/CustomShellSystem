using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Exceptions;
using CUE4Parse.UE4.Readers;
using CUE4Parse.UE4.Versions;

internal static class VertexAttributeReadbackCheck
{
    public static void Run()
    {
        using var bytes = new MemoryStream();
        using (var writer = new BinaryWriter(bytes, System.Text.Encoding.UTF8, leaveOpen: true))
        {
            writer.Write(1); // Component count.
            writer.Write((int) EPixelFormat.PF_R32_FLOAT);
            writer.Write(4); // Component stride.
            writer.Write(4); // Attribute data stride.
            writer.Write(1); // BulkSerialize stores uint8 elements.
            writer.Write(8);
            writer.Write(-1.25f);
            writer.Write(2.5f);
            writer.Write(0xDEADBEEFu);
        }
        using var archive = new FByteArchive("UE56VertexAttributeFixture", bytes.ToArray(), new VersionContainer(EGame.GAME_UE5_6));
        var attribute = new FSkeletalMeshAttributeVertexBuffer(archive);
        if (archive.Position != 32 || archive.Read<uint>() != 0xDEADBEEFu || archive.Position != archive.Length)
            throw new InvalidDataException("Vertex attribute parser did not consume the complete bulk payload");
        if (attribute.ComponentCount != 1 || attribute.ComponentStride != 4 || attribute.ValueData.Stride != 4)
            throw new InvalidDataException("Vertex attribute metadata changed");
        if (attribute.ValueData.Data.Length != 8 || BitConverter.ToSingle(attribute.ValueData.Data, 0) != -1.25f ||
            BitConverter.ToSingle(attribute.ValueData.Data, 4) != 2.5f)
            throw new InvalidDataException("Vertex attribute values changed");
        var valid = bytes.ToArray();
        foreach (var (name, offset, value) in new[] {
            ("component count", 0, 0), ("pixel format", 4, -1),
            ("component stride", 8, 2), ("data stride", 12, 0),
            ("bulk element size", 16, 4), ("negative byte count", 20, -1),
            ("unaligned byte count", 20, 7), ("oversized byte count", 20, 100) })
        {
            var invalid = (byte[]) valid.Clone();
            System.Buffers.Binary.BinaryPrimitives.WriteInt32LittleEndian(invalid.AsSpan(offset, 4), value);
            ExpectRejected(name, invalid);
        }
        ExpectRejected("truncated payload", valid[..28]);
        Console.WriteLine("UE 5.6 vertex attribute payload, following-field alignment and nine invalid inputs passed");
    }

    private static void ExpectRejected(string name, byte[] bytes)
    {
        using var archive = new FByteArchive(name, bytes, new VersionContainer(EGame.GAME_UE5_6));
        try { _ = new FSkeletalMeshAttributeVertexBuffer(archive); }
        catch (ParserException) { return; }
        throw new InvalidDataException($"Vertex attribute parser accepted {name}");
    }
}
