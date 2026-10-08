#include "CSSRigAuditLibrary.h"

#include "ClothingAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Serialization/JsonSerializer.h"
#include "SkinnedAssetCompiler.h"

FString UCSSRigAuditLibrary::SnapshotClothCustomData(USkeletalMesh* Mesh)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Mesh)
        return TEXT("ERROR: expected a commandlet mesh on the game thread");
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    using FRows = TArray<TSharedPtr<FJsonValue>>;
    bool Valid = true;
    auto Numbers = [&](std::initializer_list<double> Values) -> TSharedPtr<FJsonValue>
    {
        FRows Row;
        for (double Value : Values)
        {
            Valid &= FMath::IsFinite(Value);
            Row.Add(MakeShared<FJsonValueNumber>(Value));
        }
        return MakeShared<FJsonValueArray>(MoveTemp(Row));
    };
    auto Transitions = [&](const TArray<FMeshToMeshVertData>& Values)
    {
        FRows Rows;
        for (const auto& V : Values)
        {
            const auto P = V.PositionBaryCoordsAndDist;
            const auto N = V.NormalBaryCoordsAndDist;
            const auto T = V.TangentBaryCoordsAndDist;
            Rows.Add(Numbers({P.X, P.Y, P.Z, P.W, N.X, N.Y, N.Z, N.W, T.X, T.Y, T.Z, T.W,
                double(V.SourceMeshVertIndices[0]), double(V.SourceMeshVertIndices[1]),
                double(V.SourceMeshVertIndices[2]), double(V.SourceMeshVertIndices[3]), V.Weight}));
        }
        return Rows;
    };
    auto Tethers = [&](const FClothTetherData& Data)
    {
        FRows Batches;
        for (const auto& Batch : Data.Tethers)
        {
            FRows Rows;
            for (const auto& V : Batch)
                Rows.Add(Numbers({double(V.Get<0>()), double(V.Get<1>()), V.Get<2>()}));
            Batches.Add(MakeShared<FJsonValueArray>(MoveTemp(Rows)));
        }
        return Batches;
    };
    FRows Rows;
    for (const auto& Entry : Mesh->GetMeshClothingAssets())
    {
        const auto* Cloth = Cast<UClothingAssetCommon>(Entry.Get());
        if (!Cloth) return TEXT("ERROR: unsupported clothing asset");
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Cloth->GetName());
        FRows Lods;
        for (const auto& Lod : Cloth->LodData)
        {
            auto Value = MakeShared<FJsonObject>();
            Value->SetArrayField(TEXT("transition_up"), Transitions(Lod.TransitionUpSkinData));
            Value->SetArrayField(TEXT("transition_down"), Transitions(Lod.TransitionDownSkinData));
            Value->SetArrayField(TEXT("euclidean_tethers"), Tethers(Lod.PhysicalMeshData.EuclideanTethers));
            Value->SetArrayField(TEXT("geodesic_tethers"), Tethers(Lod.PhysicalMeshData.GeodesicTethers));
            Lods.Add(MakeShared<FJsonValueObject>(Value));
        }
        Row->SetArrayField(TEXT("lods"), MoveTemp(Lods));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (!Valid) return TEXT("ERROR: nonfinite cloth data");
    auto Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("assets"), MoveTemp(Rows));
    FString Result;
    if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result)))
        return TEXT("ERROR: cannot serialize cloth data");
    return Result;
}
