#include "CSSBindClothCommandlet.h"

#include "AssetCompilingManager.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "ClothingAsset.h"
#include "ClothingAssetFactory.h"
#include "ClothLODData.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogCSSCloth, Log, All);

UCSSBindClothCommandlet::UCSSBindClothCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UCSSBindClothCommandlet::Main(const FString& Params)
{
    auto Fail = [](const TCHAR* Message) {
        UE_LOG(LogCSSCloth, Error, TEXT("%s"), Message);
        return 1;
    };
    FString MeshPath, PhysicsPath, AnimPath;
    if (!FParse::Value(*Params, TEXT("Mesh="), MeshPath) ||
        !FParse::Value(*Params, TEXT("Physics="), PhysicsPath) ||
        !FParse::Value(*Params, TEXT("Anim="), AnimPath) ||
        !MeshPath.StartsWith(TEXT("/Game/CSSAuthoring/")) ||
        !PhysicsPath.StartsWith(TEXT("/Game/CSS/")) ||
        !AnimPath.StartsWith(TEXT("/Game/CSS/")))
        return Fail(TEXT("Specify authored -Mesh and staged -Physics paths"));
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
    if (!Mesh || Mesh->GetLODNum() != 1 || !Mesh->GetMeshClothingAssets().IsEmpty())
        return Fail(TEXT("Expected a one-LOD authored mesh without cloth"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    Mesh->GetLODInfo(0)->BuildSettings.bUseFullPrecisionUVs = true;
    Mesh->PostEditChange();
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto SaveAsset = [](UObject* Object) {
        FSavePackageArgs Save;
        Save.TopLevelFlags = RF_Public | RF_Standalone;
        Save.SaveFlags = SAVE_NoError;
        const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        return UPackage::SavePackage(Object->GetOutermost(), Object, *File, Save);
    };
    // These empty assets establish external imports. Packaging must exclude
    // them and supply the verified cooked originals at exactly these paths.
    auto PhysicsStub = [&SaveAsset](const FString& Path) -> UPhysicsAsset* {
        if (FPackageName::DoesPackageExist(Path)) return LoadObject<UPhysicsAsset>(nullptr, *Path);
        UPhysicsAsset* Asset = NewObject<UPhysicsAsset>(CreatePackage(*Path),
            *FPackageName::GetShortName(Path), RF_Public | RF_Standalone);
        return SaveAsset(Asset) ? Asset : nullptr;
    };
    UPhysicsAsset* Physics = PhysicsStub(PhysicsPath);
    UPhysicsAsset* MeshPhysics = PhysicsStub(TEXT("/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/PA_Sester_Genessa_V5_Capsules_NoCloth"));
    UAnimBlueprint* Anim = nullptr;
    if (FPackageName::DoesPackageExist(AnimPath)) Anim = LoadObject<UAnimBlueprint>(nullptr, *AnimPath);
    else
    {
        UAnimBlueprintFactory* AnimFactory = NewObject<UAnimBlueprintFactory>();
        AnimFactory->TargetSkeleton = Mesh->GetSkeleton();
        AnimFactory->ParentClass = UAnimInstance::StaticClass();
        Anim = Cast<UAnimBlueprint>(AnimFactory->FactoryCreateNew(UAnimBlueprint::StaticClass(),
            CreatePackage(*AnimPath), *FPackageName::GetShortName(AnimPath), RF_Public | RF_Standalone, nullptr, GWarn));
        if (Anim && !SaveAsset(Anim)) Anim = nullptr;
    }
    if (!Physics || !MeshPhysics || !Anim || !Anim->GeneratedClass ||
        !Anim->GeneratedClass->IsChildOf(UAnimInstance::StaticClass()))
        return Fail(TEXT("Could not create typed secondary import stubs"));
    Mesh->SetPhysicsAsset(MeshPhysics);
    Mesh->SetPostProcessAnimBlueprint(Anim->GeneratedClass.Get());
    TArray<int32> Sections;
    for (int32 I = 0; I < Mesh->GetImportedModel()->LODModels[0].Sections.Num(); ++I)
    {
        const auto& Section = Mesh->GetImportedModel()->LODModels[0].Sections[I];
        const FName Slot = Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName;
        if (Slot == TEXT("Seductress_B") || Slot == TEXT("Seductress_B.001"))
            Sections.Add(I);
    }
    if (Sections.Num() != 2)
        return Fail(TEXT("Expected exactly two Seductress skirt material sections"));
    UClothingAssetFactory* Factory = NewObject<UClothingAssetFactory>();
    for (int32 Section : Sections)
    {
        FSkeletalMeshClothBuildParams Build;
        Build.AssetName = FString::Printf(TEXT("CSS_Seductress_Skirt_%d"), Section);
        Build.LodIndex = 0;
        Build.SourceSection = Section;
        Build.bRemoveFromMesh = false;
        Build.PhysicsAsset = Physics;
        UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Factory->CreateFromSkeletalMesh(Mesh, Build));
        if (!Cloth || Cloth->LodData.Num() != 1)
            return Fail(TEXT("Native clothing factory failed"));
        Mesh->AddClothingAsset(Cloth);
        auto& LOD = Cloth->LodData[0];
        auto& Data = LOD.PhysicalMeshData;
        if (Data.Vertices.IsEmpty() || Data.Vertices.Num() > 5000)
            return Fail(TEXT("Unexpected skirt simulation vertex count"));
        // Pin every disconnected panel at its own upper edge. Decorative
        // pieces must not become unanchored particles when gravity starts.
        TArray<TArray<int32>> Neighbors;
        Neighbors.SetNum(Data.Vertices.Num());
        for (int32 I = 0; I < Data.Indices.Num(); I += 3)
            for (int32 J = 0; J < 3; ++J)
            {
                const int32 A = Data.Indices[I + J];
                const int32 B = Data.Indices[I + (J + 1) % 3];
                Neighbors[A].Add(B);
                Neighbors[B].Add(A);
            }
        TArray<bool> Seen;
        Seen.Init(false, Data.Vertices.Num());
        FPointWeightMap Distances(Data.Vertices.Num());
        Distances.Name = TEXT("CSS skirt anchors");
        Distances.CurrentTarget = static_cast<uint8>(EWeightMapTargetCommon::MaxDistance);
        Distances.bEnabled = true;
        int32 Components = 0, Pinned = 0;
        for (int32 Start = 0; Start < Data.Vertices.Num(); ++Start)
        {
            if (Seen[Start]) continue;
            TArray<int32> Connected{Start};
            Seen[Start] = true;
            float Top = Data.Vertices[Start].Z;
            for (int32 N = 0; N < Connected.Num(); ++N)
            {
                const int32 Index = Connected[N];
                Top = FMath::Max(Top, Data.Vertices[Index].Z);
                for (int32 Other : Neighbors[Index])
                    if (!Seen[Other]) { Seen[Other] = true; Connected.Add(Other); }
            }
            for (int32 Index : Connected)
            {
                const float Blend = FMath::Clamp((Top - 2.f - Data.Vertices[Index].Z) / 50.f, 0.f, 1.f);
                Distances[Index] = 35.f * Blend * Blend * (3.f - 2.f * Blend);
                Pinned += Distances[Index] == 0.f;
            }
            ++Components;
        }
        if (!Pinned || Pinned == Data.Vertices.Num())
            return Fail(TEXT("Cloth must contain both pinned and simulated vertices"));
        LOD.PointWeightMaps.Reset();
        LOD.PointWeightMaps.Add(MoveTemp(Distances));
        LOD.bSmoothTransition = true;
        UChaosClothConfig* Config = Cloth->GetClothConfig<UChaosClothConfig>();
        if (!Config)
            return Fail(TEXT("Chaos cloth configuration was not created"));
        Config->BendingStiffnessWeighted = { .08f, .08f };
        Config->DampingCoefficient = .06f;
        Config->CollisionThickness = .5f;
        Config->FrictionCoefficient = .3f;
        Config->bUseSelfCollisions = true;
        Config->SelfCollisionThickness = .35f;
        Config->AnimDriveStiffness = { .1f, .1f };
        Config->AnimDriveDamping = { .2f, .2f };
        Cloth->ApplyParameterMasks(true, true);
        if (!Cloth->BindToSkeletalMesh(Mesh, 0, Section, 0))
            return Fail(TEXT("Cloth render mapping failed"));
        // The editor records the binding in source section data as well as
        // the built section. Without this, the next rebuild unbinds cloth.
        auto& Model = Mesh->GetImportedModel()->LODModels[0];
        auto& UserData = Model.UserSectionsData.FindOrAdd(Model.Sections[Section].OriginalDataSectionIndex);
        UserData.CorrespondClothAssetIndex = static_cast<int16>(Mesh->GetMeshClothingAssets().IndexOfByKey(Cloth));
        UserData.ClothingData.AssetGuid = Cloth->GetAssetGuid();
        UserData.ClothingData.AssetLodIndex = 0;
        UE_LOG(LogCSSCloth, Display, TEXT("Section %d: %d simulation vertices, %d pinned, %d connected pieces. Runtime motion not yet tested."),
            Section, Data.Vertices.Num(), Pinned, Components);
    }
    Mesh->PostEditChange();
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (int32 Section : Sections)
    {
        UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Mesh->GetSectionClothingAsset(0, Section));
        if (!Cloth || Cloth->LodMap.Num() != 1 || Cloth->LodMap[0] != 0)
            return Fail(TEXT("Cloth binding did not survive the mesh rebuild"));
        auto& Data = Cloth->LodData[0].PhysicalMeshData;
        const auto* Distances = Data.FindWeightMap(EWeightMapTargetCommon::MaxDistance);
        if (!Distances || Distances->Num() != Data.Vertices.Num())
            return Fail(TEXT("Cloth lost its MaxDistance map"));
        int32 Kinematic = 0;
        for (int32 Index = 0; Index < Distances->Num(); ++Index)
        {
            const float Distance = (*Distances)[Index];
            if (!FMath::IsFinite(Distance) || Distance < 0 || Distance > 35)
                return Fail(TEXT("Invalid cloth displacement limit"));
            Kinematic += Distance < .1f;
        }
        // Chaos calculates masses from the weight map at runtime and does
        // not use the legacy InverseMasses/NumFixedVerts cache.
        if (Kinematic <= 0 || Kinematic >= Data.Vertices.Num())
            return Fail(TEXT("Cloth lost its fixed or movable particles"));
        UE_LOG(LogCSSCloth, Display, TEXT("Verified bound section %d after rebuild, %d Chaos kinematic vertices."), Section, Kinematic);
    }
    Mesh->MarkPackageDirty();
    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Mesh->GetOutermost(), Mesh, *Filename, Save) ? 0 : 1;
}
