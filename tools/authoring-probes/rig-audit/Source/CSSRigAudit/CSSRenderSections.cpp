#include "CSSRigAuditLibrary.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "SkinnedAssetCompiler.h"

namespace
{
using FRows = TArray<TSharedPtr<FJsonValue>>;
TSharedPtr<FJsonValue> Array(std::initializer_list<double> Numbers)
{
    FRows Values;
    for (double Number : Numbers) Values.Add(MakeShared<FJsonValueNumber>(Number));
    return MakeShared<FJsonValueArray>(MoveTemp(Values));
}
}

FString UCSSRigAuditLibrary::SnapshotRenderSections(USkeletalMesh* Mesh, const TArray<FName>& MaterialSlots,
                                                  const TArray<FName>& MorphNames)
{
    if (!IsRunningCommandlet() || !Mesh || MaterialSlots.IsEmpty()) return TEXT("ERROR: commandlet mesh and slots required");
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto* Render = Mesh->GetResourceForRendering();
    if (!Render || Render->LODRenderData.Num() != 1) return TEXT("ERROR: expected one render LOD");
    const auto& Lod = Render->LODRenderData[0];
    const auto& Buffers = Lod.StaticVertexBuffers;
    const auto& Weights = Lod.SkinWeightVertexBuffer;
    const auto* IndexBuffer = Lod.MultiSizeIndexContainer.GetIndexBuffer();
    if (!IndexBuffer || !Weights.GetDataVertexBuffer()->GetWeightData()) return TEXT("ERROR: CPU data unavailable");
    TSet<FName> Requested(MaterialSlots), Found;
    if (Requested.Num() != MaterialSlots.Num()) return TEXT("ERROR: duplicate slot request");
    FRows Groups;
    for (const auto& Section : Lod.RenderSections)
    {
        if (!Mesh->GetMaterials().IsValidIndex(Section.MaterialIndex)) return TEXT("ERROR: material index out of range");
        const FName Slot = Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName;
        if (!Requested.Contains(Slot)) continue;
        Found.Add(Slot);
        auto Group = MakeShared<FJsonObject>();
        Group->SetStringField(TEXT("slot"), Slot.ToString());
        Group->SetBoolField(TEXT("disabled"), Section.bDisabled);
        Group->SetNumberField(TEXT("cloth_asset"), Section.CorrespondClothAssetIndex);
        Group->SetStringField(TEXT("cloth_guid"), Section.ClothingData.AssetGuid.ToString());
        Group->SetNumberField(TEXT("cloth_lod"), Section.ClothingData.AssetLodIndex);
        const uint32 End = Section.BaseVertexIndex+Section.NumVertices;
        if (End < Section.BaseVertexIndex || End > Lod.GetNumVertices()) return TEXT("ERROR: invalid vertex range");
        FRows Vertices, Skin, Indices;
        for (uint32 Index = Section.BaseVertexIndex; Index < End; ++Index)
        {
            const auto P = Buffers.PositionVertexBuffer.VertexPosition(Index);
            const auto N = Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index);
            const auto T = Buffers.StaticMeshVertexBuffer.VertexTangentX(Index);
            const auto B = Buffers.StaticMeshVertexBuffer.VertexTangentY(Index);
            FRows Values;
            for (double Number : {double(P.X), double(P.Y), double(P.Z), double(N.X), double(N.Y), double(N.Z),
                                  double(T.X), double(T.Y), double(T.Z), double(B.X), double(B.Y), double(B.Z)})
            {
                if (!FMath::IsFinite(Number)) return TEXT("ERROR: nonfinite vertex data");
                Values.Add(MakeShared<FJsonValueNumber>(Number));
            }
            for (uint32 Channel = 0; Channel < Buffers.StaticMeshVertexBuffer.GetNumTexCoords(); ++Channel)
            {
                const auto UV = Buffers.StaticMeshVertexBuffer.GetVertexUV(Index, Channel);
                if (UV.ContainsNaN()) return TEXT("ERROR: nonfinite render UV");
                Values.Add(MakeShared<FJsonValueNumber>(UV.X));
                Values.Add(MakeShared<FJsonValueNumber>(UV.Y));
            }
            const FColor Color = Buffers.ColorVertexBuffer.GetNumVertices() ? Buffers.ColorVertexBuffer.VertexColor(Index) : FColor::White;
            for (uint8 Value : {Color.R, Color.G, Color.B, Color.A}) Values.Add(MakeShared<FJsonValueNumber>(Value));
            Vertices.Add(MakeShared<FJsonValueArray>(MoveTemp(Values)));
            uint32 Offset, Count;
            Weights.GetVertexInfluenceOffsetCount(Index, Offset, Count);
            FRows Influences;
            for (uint32 Influence = 0; Influence < Count; ++Influence)
            {
                const auto Weight = Weights.GetBoneWeight(Index, Influence);
                if (!Weight) continue;
                const auto Bone = Weights.GetBoneIndex(Index, Influence);
                if (!Section.BoneMap.IsValidIndex(Bone)) return TEXT("ERROR: invalid influence bone");
                Influences.Add(Array({double(Section.BoneMap[Bone]), double(Weight)}));
            }
            Skin.Add(MakeShared<FJsonValueArray>(MoveTemp(Influences)));
        }
        for (uint32 Index = Section.BaseIndex; Index < Section.BaseIndex+Section.NumTriangles*3; ++Index)
        {
            if (Index >= static_cast<uint32>(IndexBuffer->Num())) return TEXT("ERROR: invalid triangle range");
            const uint32 Vertex = IndexBuffer->Get(Index);
            if (Vertex < Section.BaseVertexIndex || Vertex >= End) return TEXT("ERROR: triangle outside section");
            Indices.Add(MakeShared<FJsonValueNumber>(Vertex-Section.BaseVertexIndex));
        }
        Group->SetArrayField(TEXT("vertices"), MoveTemp(Vertices));
        Group->SetArrayField(TEXT("weights"), MoveTemp(Skin));
        Group->SetArrayField(TEXT("indices"), MoveTemp(Indices));
        FRows ClothLods;
        for (const auto& ClothLod : Section.ClothMappingDataLODs)
        {
            FRows Mappings;
            for (const auto& Entry : ClothLod)
            {
                const auto P = Entry.PositionBaryCoordsAndDist;
                const auto N = Entry.NormalBaryCoordsAndDist;
                const auto T = Entry.TangentBaryCoordsAndDist;
                if (P.ContainsNaN() || N.ContainsNaN() || T.ContainsNaN() || !FMath::IsFinite(Entry.Weight))
                    return TEXT("ERROR: nonfinite cloth mapping");
                Mappings.Add(Array({P.X, P.Y, P.Z, P.W, N.X, N.Y, N.Z, N.W, T.X, T.Y, T.Z, T.W,
                    double(Entry.SourceMeshVertIndices[0]), double(Entry.SourceMeshVertIndices[1]),
                    double(Entry.SourceMeshVertIndices[2]), double(Entry.SourceMeshVertIndices[3]), Entry.Weight}));
            }
            ClothLods.Add(MakeShared<FJsonValueArray>(MoveTemp(Mappings)));
        }
        Group->SetArrayField(TEXT("cloth_mappings"), MoveTemp(ClothLods));
        FRows Morphs;
        for (FName Name : MorphNames)
        {
            const auto* Morph = Mesh->FindMorphTarget(Name);
            if (!Morph || !Morph->HasDataForLOD(0)) return TEXT("ERROR: requested morph unavailable");
            FRows Deltas;
            for (const auto& Delta : Morph->GetMorphLODModels()[0].Vertices)
            {
                if (Delta.SourceIdx < Section.BaseVertexIndex || Delta.SourceIdx >= End) continue;
                const auto P = Delta.PositionDelta;
                const auto N = Delta.TangentZDelta;
                if (P.ContainsNaN() || N.ContainsNaN()) return TEXT("ERROR: nonfinite render morph");
                Deltas.Add(Array({double(Delta.SourceIdx-Section.BaseVertexIndex), P.X, P.Y, P.Z, N.X, N.Y, N.Z}));
            }
            auto Shape = MakeShared<FJsonObject>();
            Shape->SetStringField(TEXT("name"), Name.ToString());
            Shape->SetArrayField(TEXT("deltas"), MoveTemp(Deltas));
            Morphs.Add(MakeShared<FJsonValueObject>(Shape));
        }
        Group->SetArrayField(TEXT("morphs"), MoveTemp(Morphs));
        Groups.Add(MakeShared<FJsonValueObject>(Group));
    }
    if (Found.Num() != Requested.Num()) return TEXT("ERROR: requested slot missing");
    auto Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("groups"), MoveTemp(Groups));
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result));
    return Result;
}
