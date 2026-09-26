#include "CSSEvePanelCommandlet.h"
#include "AssetCompilingManager.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogCSSEvePanel, Log, All);

UCSSEvePanelCommandlet::UCSSEvePanelCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UCSSEvePanelCommandlet::Main(const FString& Params)
{
    using namespace UE::Chaos::ClothAsset;
    auto Fail = [](const TCHAR* Message) { UE_LOG(LogCSSEvePanel, Error, TEXT("%s"), Message); return 1; };
    FString Input, Output, Text;
    if (FParse::Param(*Params,TEXT("Inspect")))
    {
        FString Report;
        if (!FParse::Value(*Params,TEXT("Output="),Output) || !Output.StartsWith(TEXT("/Game/CSS/EveTest/CA_")) ||
            !FParse::Value(*Params,TEXT("Report="),Report)) return Fail(TEXT("Inspect needs private Output and Report"));
        auto* Asset=LoadObject<UChaosClothAsset>(nullptr,*Output);
        if (!Asset) return Fail(TEXT("Cloth asset not found"));
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (Asset->GetClothCollections().Num()!=1 || !Asset->GetImportedModel()) return Fail(TEXT("Missing cloth collection or model"));
        const FCollectionClothConstFacade Cloth(Asset->GetClothCollections()[0]);
        auto Data=MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("asset"),Asset->GetPathName());
        Data->SetStringField(TEXT("skeleton"),GetPathNameSafe(Asset->GetSkeleton()));
        Data->SetStringField(TEXT("physics"),GetPathNameSafe(Asset->GetPhysicsAsset()));
        Data->SetNumberField(TEXT("simulations"),Asset->GetNumClothSimulationModels());
        Data->SetBoolField(TEXT("valid_simulation"),Asset->HasValidClothSimulationModels());
        Data->SetNumberField(TEXT("particles"),Cloth.GetNumSimVertices3D());
        Data->SetNumberField(TEXT("sim_faces"),Cloth.GetNumSimFaces());
        Data->SetNumberField(TEXT("render_patterns"),Cloth.GetNumRenderPatterns());
        Data->SetNumberField(TEXT("sim_morph_targets"),Cloth.GetNumSimMorphTargets());
        int32 Pinned=0, ZeroDistance=0;
        for (float Distance:Cloth.GetWeightMap(TEXT("MaxDistance")))
        {
            Pinned+=Distance<.1f;
            ZeroDistance+=Distance==0.f;
        }
        Data->SetNumberField(TEXT("pinned"),Pinned);
        Data->SetNumberField(TEXT("zero_distance"),ZeroDistance);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& Section:Asset->GetImportedModel()->LODModels[0].Sections)
        {
            auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("material"),GetPathNameSafe(Asset->GetMaterials()[Section.MaterialIndex].MaterialInterface));
            Row->SetNumberField(TEXT("vertices"),Section.NumVertices);
            Row->SetNumberField(TEXT("triangles"),Section.NumTriangles);
            Row->SetNumberField(TEXT("mapping_count"),Section.ClothMappingDataLODs.IsEmpty()?0:Section.ClothMappingDataLODs[0].Num());
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Data->SetArrayField(TEXT("sections"),Rows);
        if (!FJsonSerializer::Serialize(Data,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Report))
            return Fail(TEXT("Cannot save cloth report"));
        return 0;
    }
    if (!FParse::Value(*Params, TEXT("Proxy="), Input) ||
        !FParse::Value(*Params, TEXT("Output="), Output) ||
        !Output.StartsWith(TEXT("/Game/CSS/EveTest/CA_")) ||
        !FPackageName::IsValidLongPackageName(Output) || FPackageName::DoesPackageExist(Output))
        return Fail(TEXT("Require Proxy and unused /Game/CSS/EveTest/CA_ output"));
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *Input) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
        return Fail(TEXT("Invalid proxy JSON"));
    const FString Primary = TEXT("MI_CH_P_EVE_Christmas_01_01.001");
    const TSharedPtr<FJsonObject>* Slots = nullptr;
    const TSharedPtr<FJsonObject>* Proxy = nullptr;
    if (!Root->TryGetObjectField(TEXT("slots"), Slots) || !(*Slots)->TryGetObjectField(Primary, Proxy))
        return Fail(TEXT("Missing Holiday proxy"));
    const TArray<TSharedPtr<FJsonValue>> *P = nullptr, *N = nullptr, *W = nullptr, *T = nullptr;
    if (!(*Proxy)->TryGetArrayField(TEXT("positions"),P) || !(*Proxy)->TryGetArrayField(TEXT("normals"),N) ||
        !(*Proxy)->TryGetArrayField(TEXT("weights"),W) || !(*Proxy)->TryGetArrayField(TEXT("indices"),T) ||
        P->Num()<3 || P->Num()>5000 || N->Num()!=P->Num() || W->Num()!=P->Num() || T->Num()%3 || T->IsEmpty())
        return Fail(TEXT("Invalid proxy dimensions"));
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CSS/EveTest/SK_Holiday.SK_Holiday"));
    if (!Mesh) return Fail(TEXT("Private source mesh missing"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!Mesh->GetImportedModel() || Mesh->GetImportedModel()->LODModels.Num()!=1)
        return Fail(TEXT("Expected one imported LOD"));
    TArray<FVector3f> Positions, Normals;
    TArray<FVector2f> PatternPositions;
    TArray<int32> Triangles;
    float Top = -FLT_MAX;
    for (int32 I=0; I<P->Num(); ++I)
    {
        const auto& A=(*P)[I]->AsArray(); const auto& B=(*N)[I]->AsArray();
        if (A.Num()!=3 || B.Num()!=3) return Fail(TEXT("Invalid proxy vector"));
        FVector3f Pos(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());
        FVector3f Normal(B[0]->AsNumber(),B[1]->AsNumber(),B[2]->AsNumber());
        if (Pos.ContainsNaN() || Normal.ContainsNaN() || Normal.IsNearlyZero()) return Fail(TEXT("Nonfinite proxy"));
        Positions.Add(Pos); Normals.Add(Normal.GetSafeNormal());
        // Isotropic legacy constraints use the 3D rest mesh. This is not a tailored 2D pattern.
        PatternPositions.Add(FVector2f(Pos.X,Pos.Z));
        Top=FMath::Max(Top,Pos.Z);
    }
    for (const auto& Value:*T)
    {
        const double Number=Value->AsNumber();
        if (!FMath::IsFinite(Number) || Number<0 || Number>=Positions.Num() || Number!=FMath::FloorToDouble(Number))
            return Fail(TEXT("Invalid proxy triangle"));
        Triangles.Add(int32(Number));
    }
    auto Collection=MakeShared<FManagedArrayCollection>();
    FCollectionClothFacade Cloth(Collection);
    Cloth.DefineSchema();
    Cloth.AddGetSimPattern().Initialize(PatternPositions,Positions,Triangles,INDEX_NONE,Normals);
    Cloth.SetSkeletalMeshPathName(Mesh->GetPathName());
    Cloth.SetPhysicsAssetPathName(TEXT("/Game/CSS/EveTest/PA_Holiday.PA_Holiday"));
    Cloth.AddWeightMap(TEXT("MaxDistance"));
    int32 Pinned=0;
    for (int32 I=0; I<Positions.Num(); ++I)
    {
        float Sum=0;
        for (const auto& Influence:(*W)[I]->AsArray())
        {
            const auto& Pair=Influence->AsArray();
            if (Pair.Num()!=2) return Fail(TEXT("Invalid proxy influence"));
            const int32 Bone=Mesh->GetRefSkeleton().FindBoneIndex(FName(*Pair[0]->AsString()));
            const float Weight=Pair[1]->AsNumber();
            if (Bone==INDEX_NONE || !FMath::IsFinite(Weight) || Weight<=0) return Fail(TEXT("Invalid proxy bone weight"));
            Cloth.GetSimBoneIndices()[I].Add(Bone); Cloth.GetSimBoneWeights()[I].Add(Weight); Sum+=Weight;
        }
        if (!FMath::IsNearlyEqual(Sum,1.f,.001f)) return Fail(TEXT("Proxy weights not normalized"));
        const float Alpha=FMath::Clamp((Top-20.f-Positions[I].Z)/12.f,0.f,1.f);
        const float Distance=18.f*Alpha*Alpha*(3.f-2.f*Alpha);
        Cloth.GetWeightMap(TEXT("MaxDistance"))[I]=Distance;
        Pinned+=Distance<.1f;
    }
    const TSet<FString> Wanted={Primary,TEXT("MI_CH_P_EVE_Christmas_01_Decal.001"),
        TEXT("MI_EVE_HR_Christmas_01_Fur.001"),TEXT("MI_EVE_HR_15_Emissive1.001"),TEXT("MI_CH_P_EVE_Christmas_01_03.001")};
    TSet<FString> Found;
    const auto& Model=Mesh->GetImportedModel()->LODModels[0];
    for (const auto& Section:Model.Sections)
    {
        const auto& Material=Mesh->GetMaterials()[Section.MaterialIndex];
        const FString Slot=Material.MaterialSlotName.ToString();
        if (!Wanted.Contains(Slot)) continue;
        if (Found.Contains(Slot) || !Material.MaterialInterface) return Fail(TEXT("Duplicate or missing garment material"));
        Found.Add(Slot);
        auto Pattern=Cloth.AddGetRenderPattern();
        Pattern.SetNumRenderVertices(Section.SoftVertices.Num());
        Pattern.SetNumRenderFaces(Section.NumTriangles);
        Pattern.SetRenderMaterialPathName(Material.MaterialInterface->GetPathName());
        for (int32 I=0; I<Section.SoftVertices.Num(); ++I)
        {
            const auto& V=Section.SoftVertices[I];
            Pattern.GetRenderPosition()[I]=V.Position; Pattern.GetRenderNormal()[I]=V.TangentZ;
            Pattern.GetRenderTangentU()[I]=V.TangentX; Pattern.GetRenderTangentV()[I]=V.TangentY;
            Pattern.GetRenderColor()[I]=FLinearColor(V.Color);
            for (uint32 U=0; U<FMath::Min(uint32(MAX_TEXCOORDS),Model.NumTexCoords); ++U)
                Pattern.GetRenderUVs()[I].Add(V.UVs[U]);
            float Sum=0;
            for (int32 J=0; J<Section.MaxBoneInfluences; ++J)
            {
                if (!V.InfluenceWeights[J]) continue;
                if (!Section.BoneMap.IsValidIndex(V.InfluenceBones[J])) return Fail(TEXT("Invalid render bone map"));
                const float Weight=V.InfluenceWeights[J]/65535.f;
                Pattern.GetRenderBoneIndices()[I].Add(Section.BoneMap[V.InfluenceBones[J]]);
                Pattern.GetRenderBoneWeights()[I].Add(Weight); Sum+=Weight;
            }
            if (!FMath::IsNearlyEqual(Sum,1.f,.001f)) return Fail(TEXT("Render weights not normalized"));
        }
        for (uint32 I=0; I<Section.NumTriangles; ++I)
        {
            FIntVector3 Face;
            for (int32 J=0; J<3; ++J)
            {
                const int32 Index=int32(Model.IndexBuffer[Section.BaseIndex+I*3+J])-Section.BaseVertexIndex;
                if (!Section.SoftVertices.IsValidIndex(Index)) return Fail(TEXT("Invalid render triangle"));
                Face[J]=Index+Pattern.GetRenderVerticesOffset();
            }
            Pattern.GetRenderIndices()[I]=Face;
        }
    }
    if (Found.Num()!=5 || Pinned==0 || Pinned==Positions.Num()) return Fail(TEXT("Incomplete garment or anchors"));
    UChaosClothConfig* Config=NewObject<UChaosClothConfig>();
    UChaosClothSharedSimConfig* Shared=NewObject<UChaosClothSharedSimConfig>();
    Config->BendingStiffnessWeighted={.12f,.12f}; Config->AnimDriveStiffness={.05f,.05f};
    Config->DampingCoefficient=.15f; Config->CollisionThickness=.3f; Config->FrictionCoefficient=.3f;
    Config->bUseSelfCollisions=true; Config->SelfCollisionThickness=.35f;
    ::Chaos::FClothingSimulationConfig SimulationConfig;
    SimulationConfig.Initialize(Config,Shared);
    SimulationConfig.GetPropertyCollection(0)->CopyTo(&Collection.Get());
    FClothEngineTools::GenerateTethers(Collection,TEXT("MaxDistance"),true);
    UPackage* Package=CreatePackage(*Output);
    UChaosClothAsset* Asset=NewObject<UChaosClothAsset>(Package,*FPackageName::GetLongPackageAssetName(Output),RF_Public|RF_Standalone);
    FText Error,Verbose;
    Asset->Build({Collection},nullptr,&Error,&Verbose);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!Error.IsEmpty() || !Asset->HasValidClothSimulationModels() || Asset->GetNumClothSimulationModels()!=1 || Asset->GetMaterials().Num()!=5)
    {
        UE_LOG(LogCSSEvePanel,Error,TEXT("Build failed: %s %s"),*Error.ToString(),*Verbose.ToString());
        return 1;
    }
    UE_LOG(LogCSSEvePanel,Display,TEXT("Built one simulation, %d particles (%d pinned), five materials; motion untested."),Positions.Num(),Pinned);
    FSavePackageArgs Save;
    Save.TopLevelFlags=RF_Public|RF_Standalone;
    const FString Filename=FPackageName::LongPackageNameToFilename(Output,FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package,Asset,*Filename,Save)?0:Fail(TEXT("Save failed"));
}
