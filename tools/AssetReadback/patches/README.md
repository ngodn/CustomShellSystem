# Skeletal vertex attribute readback

`ue56-skeletal-vertex-attributes.patch` applies to CUE4Parse commit
`bbb3551af4c0c1bf481bc2821a18bd3b72706ff4`, checked out at
`reference/CUE4Parse`. It changes only the skeletal vertex attribute reader.
Other local vendor changes are outside this patch.

UE 5.6.1's `FSkeletalMeshVertexAttributeData::Serialize` in
`Engine/Source/Runtime/Engine/Private/Rendering/SkeletalMeshAttributeVertexBuffer.cpp`
serializes the stride followed by `TResourceArray<uint8>::BulkSerialize`.
The original reader consumed the stride but skipped neither the bulk header nor
its payload. Any subsequent LOD fields were therefore read at the wrong offset
when attributes were present.

From the CSS repository root, inspect and apply to an unpatched checkout:

```sh
git -C reference/CUE4Parse apply --check ../../tools/AssetReadback/patches/ue56-skeletal-vertex-attributes.patch
git -C reference/CUE4Parse apply ../../tools/AssetReadback/patches/ue56-skeletal-vertex-attributes.patch
dotnet build tools/AssetReadback/AssetReadback.csproj -c Release
dotnet tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll --check-vertex-attributes
```

Use the repository's .NET 10 SDK. The fixture checks two known float values,
alignment of the following field, and rejection of malformed/truncated data.
The original reader failed the alignment check before the patch was applied.
`--mesh-digests` reports attribute payload hashes/counts and half-edge counts
for separate verification of actual cooked meshes. A fixture pass alone is
not proof that a cooked asset contains the expected data.
