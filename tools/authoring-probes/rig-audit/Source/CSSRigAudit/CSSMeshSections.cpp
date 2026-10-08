#include "CSSRigAuditLibrary.h"

#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"

namespace
{
TSharedPtr<FJsonValue> Numbers(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (double Value : Values) Result.Add(MakeShared<FJsonValueNumber>(Value));
    return MakeShared<FJsonValueArray>(MoveTemp(Result));
}
}

FString UCSSRigAuditLibrary::SnapshotMeshSections(USkeletalMesh* Mesh, const TArray<FName>& MaterialSlots)
{
    if (!IsRunningCommandlet() || !Mesh)
        return TEXT("ERROR: expected a commandlet mesh");
    const bool AllSlots = MaterialSlots.IsEmpty();
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const FMeshDescription* Description = Mesh->GetMeshDescription(0);
    if (!Description) return TEXT("ERROR: no source mesh description");
    const FSkeletalMeshConstAttributes Attributes(*Description);
    const auto Positions = Attributes.GetVertexPositions();
    const auto Weights = Attributes.GetVertexSkinWeights();
    const auto UVs = Attributes.GetVertexInstanceUVs();
    const auto Normals = Attributes.GetVertexInstanceNormals();
    const auto Colors = Attributes.GetVertexInstanceColors();
    const auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    if (!Weights.IsValid() || !Slots.IsValid()) return TEXT("ERROR: missing source attributes");
    TSet<FName> Requested(MaterialSlots);
    if (Requested.Num() != MaterialSlots.Num()) return TEXT("ERROR: duplicate requested slot");
    TSet<FName> Found;
    TArray<TSharedPtr<FJsonValue>> Groups;
    for (const FPolygonGroupID Group : Description->PolygonGroups().GetElementIDs())
    {
        const FName Slot = Slots[Group];
        if (!AllSlots && !Requested.Contains(Slot)) continue;
        if (!AllSlots && Found.Contains(Slot)) return TEXT("ERROR: ambiguous material slot");
        Found.Add(Slot);
        TSet<FVertexID> Used;
        TArray<TSharedPtr<FJsonValue>> Faces;
        for (const FTriangleID Triangle : Description->GetPolygonGroupTriangles(Group))
        {
            TArray<TSharedPtr<FJsonValue>> Corners;
            for (const FVertexInstanceID Corner : Description->GetTriangleVertexInstances(Triangle))
            {
                const FVertexID Vertex = Description->GetVertexInstanceVertex(Corner);
                Used.Add(Vertex);
                auto Row = MakeShared<FJsonObject>();
                Row->SetNumberField(TEXT("vertex"), Vertex.GetValue());
                Row->SetNumberField(TEXT("instance"), Corner.GetValue());
                const FVector3f Normal = Normals[Corner];
                const FVector4f Color = Colors[Corner];
                if (Normal.ContainsNaN() || Color.ContainsNaN()) return TEXT("ERROR: invalid corner data");
                Row->SetField(TEXT("normal"), Numbers({Normal.X, Normal.Y, Normal.Z}));
                Row->SetField(TEXT("color"), Numbers({Color.X, Color.Y, Color.Z, Color.W}));
                TArray<TSharedPtr<FJsonValue>> Channels;
                for (int32 Channel = 0; Channel < UVs.GetNumChannels(); ++Channel)
                {
                    const FVector2f UV = UVs.Get(Corner, Channel);
                    if (UV.ContainsNaN()) return TEXT("ERROR: invalid UV");
                    Channels.Add(Numbers({UV.X, UV.Y}));
                }
                Row->SetArrayField(TEXT("uv"), MoveTemp(Channels));
                Corners.Add(MakeShared<FJsonValueObject>(Row));
            }
            auto Face = MakeShared<FJsonObject>();
            Face->SetNumberField(TEXT("triangle"), Triangle.GetValue());
            Face->SetArrayField(TEXT("corners"), MoveTemp(Corners));
            Faces.Add(MakeShared<FJsonValueObject>(Face));
        }
        TArray<FVertexID> Ordered = Used.Array();
        Ordered.Sort([](FVertexID A, FVertexID B) { return A.GetValue() < B.GetValue(); });
        TArray<TSharedPtr<FJsonValue>> Vertices;
        for (const FVertexID Vertex : Ordered)
        {
            const FVector3f Position = Positions[Vertex];
            if (Position.ContainsNaN()) return TEXT("ERROR: invalid position");
            auto Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("vertex"), Vertex.GetValue());
            Row->SetField(TEXT("position"), Numbers({Position.X, Position.Y, Position.Z}));
            TArray<TSharedPtr<FJsonValue>> Influences;
            for (const auto Weight : Weights.Get(Vertex))
            {
                const int32 Bone = Weight.GetBoneIndex();
                if (Bone >= Mesh->GetRefSkeleton().GetRawBoneNum()) return TEXT("ERROR: invalid source bone");
                auto Influence = MakeShared<FJsonObject>();
                Influence->SetStringField(TEXT("bone"), Mesh->GetRefSkeleton().GetBoneName(Bone).ToString());
                Influence->SetNumberField(TEXT("weight"), Weight.GetRawWeight());
                Influences.Add(MakeShared<FJsonValueObject>(Influence));
            }
            Row->SetArrayField(TEXT("influences"), MoveTemp(Influences));
            Vertices.Add(MakeShared<FJsonValueObject>(Row));
        }
        TArray<TSharedPtr<FJsonValue>> Morphs;
        for (FName Name : Attributes.GetMorphTargetNames())
        {
            const auto Deltas = Attributes.GetVertexMorphPositionDelta(Name);
            TArray<TSharedPtr<FJsonValue>> Values;
            for (const FVertexID Vertex : Ordered)
            {
                const FVector3f Delta = Deltas[Vertex];
                if (Delta.ContainsNaN()) return TEXT("ERROR: invalid source morph");
                if (!Delta.IsZero()) Values.Add(Numbers({double(Vertex.GetValue()), Delta.X, Delta.Y, Delta.Z}));
            }
            auto Morph = MakeShared<FJsonObject>();
            Morph->SetStringField(TEXT("name"), Name.ToString());
            Morph->SetArrayField(TEXT("deltas"), MoveTemp(Values));
            Morphs.Add(MakeShared<FJsonValueObject>(Morph));
        }
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("polygon_group"), Group.GetValue());
        Row->SetStringField(TEXT("slot"), Slot.ToString());
        Row->SetArrayField(TEXT("vertices"), MoveTemp(Vertices));
        Row->SetArrayField(TEXT("triangles"), MoveTemp(Faces));
        Row->SetArrayField(TEXT("morphs"), MoveTemp(Morphs));
        Groups.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (!AllSlots && Found.Num() != Requested.Num()) return TEXT("ERROR: requested slot missing");
    if (Groups.IsEmpty()) return TEXT("ERROR: no source polygon groups");
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Root->SetNumberField(TEXT("total_vertices"), Description->Vertices().Num());
    Root->SetNumberField(TEXT("total_triangles"), Description->Triangles().Num());
    Root->SetArrayField(TEXT("groups"), MoveTemp(Groups));
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result));
    return Result;
}
