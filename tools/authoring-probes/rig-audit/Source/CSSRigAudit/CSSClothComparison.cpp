#include "CSSRigAuditLibrary.h"

#include "ClothingAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Serialization/JsonSerializer.h"
#include "SkinnedAssetCompiler.h"
#include "UObject/UnrealType.h"

namespace
{
bool SameTransitions(const TArray<FMeshToMeshVertData>& A, const TArray<FMeshToMeshVertData>& B)
{
    if (A.Num() != B.Num()) return false;
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        const auto& X = A[Index];
        const auto& Y = B[Index];
        // Compare serialized fields, never the struct's padding.
        if (X.PositionBaryCoordsAndDist != Y.PositionBaryCoordsAndDist ||
            X.NormalBaryCoordsAndDist != Y.NormalBaryCoordsAndDist ||
            X.TangentBaryCoordsAndDist != Y.TangentBaryCoordsAndDist || X.Weight != Y.Weight) return false;
        for (int32 Corner = 0; Corner < 4; ++Corner)
            if (X.SourceMeshVertIndices[Corner] != Y.SourceMeshVertIndices[Corner]) return false;
    }
    return true;
}
}

FString UCSSRigAuditLibrary::CompareMeshCloth(USkeletalMesh* Source, USkeletalMesh* Candidate)
{
    if (!IsRunningCommandlet() || !Source || !Candidate || Source == Candidate)
        return TEXT("ERROR: commandlet and distinct meshes required");
    USkinnedAsset* Assets[]{Source, Candidate};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto& Before = Source->GetMeshClothingAssets();
    const auto& After = Candidate->GetMeshClothingAssets();
    if (Before.IsEmpty() || Before.Num() != After.Num()) return TEXT("ERROR: missing or different cloth counts");
    TArray<TSharedPtr<FJsonValue>> Rows;
    bool AllEqual = true;
    for (int32 Index = 0; Index < Before.Num(); ++Index)
    {
        const auto* A = Cast<UClothingAssetCommon>(Before[Index]);
        const auto* B = Cast<UClothingAssetCommon>(After[Index]);
        if (!A || !B || A->GetClass() != B->GetClass()) return TEXT("ERROR: unsupported or mismatched cloth class");
        TArray<TSharedPtr<FJsonValue>> Differences, Compared;
        auto Fail = [&](const FString& Field) { Differences.Add(MakeShared<FJsonValueString>(Field)); };
        for (TFieldIterator<FProperty> It(A->GetClass()); It; ++It)
        {
            const FProperty* Property = *It;
            Compared.Add(MakeShared<FJsonValueString>(Property->GetName()));
            for (int32 Element = 0; Element < Property->ArrayDim; ++Element)
                if (!Property->Identical_InContainer(A, B, Element, PPF_DeepComparison))
                    Fail(Property->GetName()+FString::Printf(TEXT("[%d]"), Element));
        }
        // These arrays have custom serializers and are invisible to reflection.
        if (A->LodData.Num() == B->LodData.Num())
        {
            for (int32 Lod = 0; Lod < A->LodData.Num(); ++Lod)
            {
                const auto& X = A->LodData[Lod];
                const auto& Y = B->LodData[Lod];
                const FString Prefix = FString::Printf(TEXT("LodData[%d]."), Lod);
                if (!SameTransitions(X.TransitionUpSkinData, Y.TransitionUpSkinData)) Fail(Prefix+TEXT("TransitionUpSkinData"));
                if (!SameTransitions(X.TransitionDownSkinData, Y.TransitionDownSkinData)) Fail(Prefix+TEXT("TransitionDownSkinData"));
                if (X.PhysicalMeshData.EuclideanTethers.Tethers != Y.PhysicalMeshData.EuclideanTethers.Tethers)
                    Fail(Prefix+TEXT("EuclideanTethers"));
                if (X.PhysicalMeshData.GeodesicTethers.Tethers != Y.PhysicalMeshData.GeodesicTethers.Tethers)
                    Fail(Prefix+TEXT("GeodesicTethers"));
            }
        }
        AllEqual &= Differences.IsEmpty();
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("index"), Index);
        Row->SetStringField(TEXT("source"), A->GetPathName());
        Row->SetStringField(TEXT("candidate"), B->GetPathName());
        Row->SetBoolField(TEXT("same_object"), A == B);
        Row->SetNumberField(TEXT("lods"), A->LodData.Num());
        Row->SetNumberField(TEXT("config_count"), A->ClothConfigs.Num());
        Row->SetArrayField(TEXT("compared_properties"), MoveTemp(Compared));
        Row->SetArrayField(TEXT("differences"), MoveTemp(Differences));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    // Validate the reflection comparison with detached copies of real data.
    const auto* First = CastChecked<UClothingAssetCommon>(Before[0]);
    FClothLODDataCommon LodCopy = First->LodData.IsEmpty() ? FClothLODDataCommon{} : First->LodData[0];
    if (LodCopy.PhysicalMeshData.Vertices.IsEmpty()) return TEXT("ERROR: no physical vertices for comparator check");
    auto* LodType = FClothLODDataCommon::StaticStruct();
    if (!LodType->CompareScriptStruct(&First->LodData[0], &LodCopy, PPF_DeepComparison))
        return TEXT("ERROR: comparator rejects unchanged physical data");
    LodCopy.PhysicalMeshData.Vertices[0].X += 1.0f;
    if (LodType->CompareScriptStruct(&First->LodData[0], &LodCopy, PPF_DeepComparison))
        return TEXT("ERROR: comparator misses altered physical data");
    auto Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("equal"), AllEqual);
    Root->SetBoolField(TEXT("physical_vertex_negative_check"), true);
    Root->SetArrayField(TEXT("assets"), MoveTemp(Rows));
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result));
    return Result;
}
