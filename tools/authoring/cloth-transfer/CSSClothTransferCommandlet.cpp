#include "CSSClothTransferCommandlet.h"
#include "AssetCompilingManager.h"
#include "ClothingAsset.h"
#include "ClothingAssetFactory.h"
#include "ClothLODData.h"
#include "ClothConfig.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CSSAuthoring)
DEFINE_LOG_CATEGORY_STATIC(LogCSSClothTransfer, Log, All);

namespace
{
int32 Fail(const TCHAR* Message)
{
    UE_LOG(LogCSSClothTransfer, Error, TEXT("%s"), Message);
    return 1;
}

bool SameTransitions(const TArray<FMeshToMeshVertData>& A, const TArray<FMeshToMeshVertData>& B)
{
    if (A.Num()!=B.Num()) return false;
    for (int32 I=0; I<A.Num(); ++I)
    {
        if (A[I].PositionBaryCoordsAndDist!=B[I].PositionBaryCoordsAndDist ||
            A[I].NormalBaryCoordsAndDist!=B[I].NormalBaryCoordsAndDist ||
            A[I].TangentBaryCoordsAndDist!=B[I].TangentBaryCoordsAndDist || A[I].Weight!=B[I].Weight) return false;
        for (int32 C=0; C<4; ++C)
            if (A[I].SourceMeshVertIndices[C]!=B[I].SourceMeshVertIndices[C]) return false;
    }
    return true;
}

TArray<TSharedPtr<FJsonValue>> Differences(const UClothingAssetCommon* A, const UClothingAssetCommon* B)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (TFieldIterator<FProperty> It(A->GetClass()); It; ++It)
    {
        const FProperty* P=*It;
        if (P->GetName()==TEXT("AssetGuid")) continue;
        for (int32 I=0; I<P->ArrayDim; ++I)
            if (!P->Identical_InContainer(A,B,I,PPF_DeepComparison))
                Result.Add(MakeShared<FJsonValueString>(P->GetName()));
    }
    if (A->LodData.Num()==B->LodData.Num())
        for (int32 I=0; I<A->LodData.Num(); ++I)
        {
            const auto& X=A->LodData[I]; const auto& Y=B->LodData[I];
            if (!SameTransitions(X.TransitionUpSkinData,Y.TransitionUpSkinData) ||
                !SameTransitions(X.TransitionDownSkinData,Y.TransitionDownSkinData))
                Result.Add(MakeShared<FJsonValueString>(TEXT("TransitionSkinData")));
            if (X.PhysicalMeshData.EuclideanTethers.Tethers!=Y.PhysicalMeshData.EuclideanTethers.Tethers ||
                X.PhysicalMeshData.GeodesicTethers.Tethers!=Y.PhysicalMeshData.GeodesicTethers.Tethers)
                Result.Add(MakeShared<FJsonValueString>(TEXT("Tethers")));
        }
    return Result;
}
}

UCSSClothTransferCommandlet::UCSSClothTransferCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 UCSSClothTransferCommandlet::Main(const FString& Params)
{
    FString Input, ReferencePath, Target, Output;
    if (!FParse::Value(*Params,TEXT("Source="),Input) || !FParse::Value(*Params,TEXT("Reference="),ReferencePath) ||
        !FParse::Value(*Params,TEXT("Output="),Output) ||
        !ReferencePath.StartsWith(TEXT("/Game/CSS/eins0fx/EveSB/")) ||
        !(Input.StartsWith(TEXT("/Game/CSSAuthoring/EveSurface20261014/")) || Input.StartsWith(TEXT("/Game/CSS/AnimLab/"))))
        return Fail(TEXT("Private source and accepted Eve reference required"));
    auto* Source=LoadObject<USkeletalMesh>(nullptr,*Input);
    auto* Reference=LoadObject<USkeletalMesh>(nullptr,*ReferencePath);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!Source || !Reference || Source->GetLODNum()!=1 || Reference->GetLODNum()!=1 ||
        Reference->GetMeshClothingAssets().Num()!=1 || Source->GetSkeleton()!=Reference->GetSkeleton())
        return Fail(TEXT("Expected compatible one-LOD meshes and one reference cloth asset"));
    auto* Original=Cast<UClothingAssetCommon>(Reference->GetMeshClothingAssets()[0]);
    if (!Original || Original->LodData.Num()!=1 || Original->LodMap!=TArray<int32>{0})
        return Fail(TEXT("Expected one bound common cloth LOD"));
    int32 Material=INDEX_NONE;
    for (const auto& S:Reference->GetImportedModel()->LODModels[0].Sections)
        if (S.HasClothingData())
        {
            if (Material!=INDEX_NONE || S.CorrespondClothAssetIndex!=0)
                return Fail(TEXT("Expected one reference cloth section"));
            Material=S.MaterialIndex;
        }
    if (Material==INDEX_NONE) return Fail(TEXT("Reference has no bound cloth section"));
    if (!Source->GetMaterials().IsValidIndex(Material) ||
        Source->GetMaterials()[Material].MaterialSlotName!=Reference->GetMaterials()[Material].MaterialSlotName)
        return Fail(TEXT("Source cloth material slot differs"));
    USkeletalMesh* Mesh=Source;
    const bool Create=FParse::Value(*Params,TEXT("Target="),Target);
    if (Create)
    {
        if (!Source->GetMeshClothingAssets().IsEmpty() || !Target.StartsWith(TEXT("/Game/CSSAuthoring/EveSurface20261014/")) ||
            !FPackageName::IsValidLongPackageName(Target) || FPackageName::DoesPackageExist(Target))
            return Fail(TEXT("New private target and cloth-free source required"));
        Mesh=DuplicateObject<USkeletalMesh>(Source,CreatePackage(*Target),*FPackageName::GetShortName(Target));
        Mesh->SetFlags(RF_Public|RF_Standalone);
        auto* Factory=NewObject<UClothingAssetFactory>();
        auto* Cloth=Cast<UClothingAssetCommon>(Factory->CreateFromExistingCloth(Mesh,Reference,Original));
        if (!Cloth) return Fail(TEXT("Could not clone accepted cloth"));
        Mesh->AddClothingAsset(Cloth);
        auto& LOD=Mesh->GetImportedModel()->LODModels[0];
        int32 Section=INDEX_NONE;
        for (int32 I=0; I<LOD.Sections.Num(); ++I)
            if (LOD.Sections[I].MaterialIndex==Material)
            {
                if (Section!=INDEX_NONE) return Fail(TEXT("Ambiguous target cloth section"));
                Section=I;
            }
        if (Section==INDEX_NONE) return Fail(TEXT("Missing target cloth section"));
        {
            // Match SkeletalMeshEditor::ApplyClothing: persist section data before the nested bind scope rebuilds.
            FScopedSkeletalMeshPostEditChange ScopedPostEditChange(Mesh);
            auto& User=LOD.UserSectionsData.FindOrAdd(LOD.Sections[Section].OriginalDataSectionIndex);
            if (!Cloth->BindToSkeletalMesh(Mesh,0,Section,0))
                return Fail(TEXT("Could not bind accepted simulation to corrected render section"));
            User.CorrespondClothAssetIndex=0;
            User.ClothingData.AssetGuid=Cloth->GetAssetGuid();
            User.ClothingData.AssetLodIndex=0;
            UE_LOG(LogCSSClothTransfer, Display, TEXT("Before rebuild: section=%d original=%d material=%d mappings=%d bound=%d"),
                Section,LOD.Sections[Section].OriginalDataSectionIndex,Material,
                LOD.Sections[Section].ClothMappingDataLODs.Num(),LOD.Sections[Section].HasClothingData());
        }
        FAssetCompilingManager::Get().FinishAllCompilation();
        Mesh->SetImportedBounds(Reference->GetImportedBounds());
    }
    if (Mesh->GetMeshClothingAssets().Num()!=1) return Fail(TEXT("Missing transferred cloth"));
    auto* Cloth=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);
    if (!Cloth || Cloth->GetClass()!=Original->GetClass()) return Fail(TEXT("Cloth class changed"));
    for (const auto& Config:Cloth->ClothConfigs)
        if (!Config.Value || Config.Value->GetTypedOuter<USkeletalMesh>()!=Mesh)
            return Fail(TEXT("Transferred config belongs to another mesh"));
    auto Changes=Differences(Original,Cloth);
    const auto& LOD=Mesh->GetImportedModel()->LODModels[0];
    int32 Bound=0,MappingVertices=0;
    for (const auto& S:LOD.Sections)
    {
        if (S.MaterialIndex==Material)
            UE_LOG(LogCSSClothTransfer, Display, TEXT("After rebuild/reload: original=%d material=%d mappings=%d bound=%d"),
                S.OriginalDataSectionIndex,Material,S.ClothMappingDataLODs.Num(),S.HasClothingData());
        if (S.HasClothingData())
        {
            if (S.MaterialIndex!=Material || S.CorrespondClothAssetIndex!=0 || S.ClothingData.AssetGuid!=Cloth->GetAssetGuid() ||
                S.ClothMappingDataLODs.Num()!=1 || S.ClothMappingDataLODs[0].Num()!=S.NumVertices)
                return Fail(TEXT("Invalid rebuilt render-to-simulation mapping"));
            ++Bound; MappingVertices+=S.NumVertices;
        }
    }
    if (Bound!=1) return Fail(TEXT("Cloth did not survive rebuild or reload"));
    if (Create && (LOD.NumVertices!=Source->GetImportedModel()->LODModels[0].NumVertices ||
        LOD.IndexBuffer.Num()!=Source->GetImportedModel()->LODModels[0].IndexBuffer.Num() ||
        Mesh->GetMorphTargets().Num()!=Source->GetMorphTargets().Num())) return Fail(TEXT("Render or morph counts changed"));
    auto Copy=Original->LodData[0];
    if (Copy.PhysicalMeshData.Vertices.IsEmpty()) return Fail(TEXT("Empty simulation"));
    auto* Type=FClothLODDataCommon::StaticStruct();
    if (!Type->CompareScriptStruct(&Original->LodData[0],&Copy,PPF_DeepComparison)) return Fail(TEXT("Cloth comparator rejects unchanged data"));
    Copy.PhysicalMeshData.Vertices[0].X+=1.f;
    if (Type->CompareScriptStruct(&Original->LodData[0],&Copy,PPF_DeepComparison)) return Fail(TEXT("Cloth comparator misses changed data"));
    auto Report=MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("passed"),Changes.IsEmpty());
    Report->SetArrayField(TEXT("cloth_differences_except_guid"),Changes);
    Report->SetNumberField(TEXT("render_vertices"),LOD.NumVertices);
    Report->SetNumberField(TEXT("render_triangles"),LOD.IndexBuffer.Num()/3);
    Report->SetNumberField(TEXT("morphs"),Mesh->GetMorphTargets().Num());
    Report->SetNumberField(TEXT("mapped_render_vertices"),MappingVertices);
    Report->SetNumberField(TEXT("simulation_vertices"),Cloth->LodData[0].PhysicalMeshData.Vertices.Num());
    Report->SetNumberField(TEXT("simulation_triangles"),Cloth->LodData[0].PhysicalMeshData.Indices.Num()/3);
    Report->SetBoolField(TEXT("changed_physical_vertex_rejected"),true);
    Report->SetStringField(TEXT("scope"),TEXT("Private editor transfer/reload only. Exact cooked geometry, mapping, cloth/config preservation and runtime remain unverified."));
    FString Text; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
    if (!FFileHelper::SaveStringToFile(Text,*Output) || !Changes.IsEmpty()) return Fail(TEXT("Cloth preservation check failed"));
    if (Create)
    {
        FSavePackageArgs Save; Save.TopLevelFlags=RF_Public|RF_Standalone; Save.SaveFlags=SAVE_NoError;
        const FString File=FPackageName::LongPackageNameToFilename(Target,FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*File,Save)) return Fail(TEXT("Could not save transferred mesh"));
    }
    return 0;
}
