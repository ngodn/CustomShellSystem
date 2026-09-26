#include "CSSEveClothCommandlet.h"

#include "AssetCompilingManager.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "ClothingAsset.h"
#include "ClothingAssetFactory.h"
#include "ClothLODData.h"
#include "Utils/ClothingMeshUtils.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "Chaos/TriangleMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogCSSEveCloth, Log, All);

UCSSEveClothCommandlet::UCSSEveClothCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UCSSEveClothCommandlet::Main(const FString& Params)
{
    auto Fail = [](const TCHAR* Message) {
        UE_LOG(LogCSSEveCloth, Error, TEXT("%s"), Message);
        return 1;
    };
    FString MeshPath, PhysicsPath, SlotList, Prefix;
    if (FParse::Param(*Params,TEXT("Inspect")))
    {
        FString Report, Text;
        if (!FParse::Value(*Params,TEXT("Mesh="),MeshPath) || !MeshPath.StartsWith(TEXT("/Game/CSS/EveTest/")) ||
            !FParse::Value(*Params,TEXT("Report="),Report)) return Fail(TEXT("Inspect needs private Mesh and Report"));
        USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,*MeshPath);
        if (!Mesh || !Mesh->GetImportedModel() || Mesh->GetImportedModel()->LODModels.Num()!=1)
            return Fail(TEXT("Cannot inspect mesh"));
        auto Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("skeleton"),GetPathNameSafe(Mesh->GetSkeleton()));
        Root->SetStringField(TEXT("mesh_physics"),GetPathNameSafe(Mesh->GetPhysicsAsset()));
        Root->SetStringField(TEXT("post_process"),GetPathNameSafe(Mesh->GetPostProcessAnimBlueprint()));
        Root->SetNumberField(TEXT("cloth_assets"),Mesh->GetMeshClothingAssets().Num());
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& Section : Mesh->GetImportedModel()->LODModels[0].Sections)
        {
            if (Section.CorrespondClothAssetIndex == INDEX_NONE) continue;
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("slot"),Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName.ToString());
            Row->SetNumberField(TEXT("asset_index"),Section.CorrespondClothAssetIndex);
            Row->SetStringField(TEXT("guid"),Section.ClothingData.AssetGuid.ToString());
            Row->SetNumberField(TEXT("vertices"),Section.SoftVertices.Num());
            Row->SetNumberField(TEXT("mapping_count"),Section.ClothMappingDataLODs.IsEmpty()?0:Section.ClothMappingDataLODs[0].Num());
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Root->SetArrayField(TEXT("sections"),Rows);
        if (!FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Report))
            return Fail(TEXT("Cannot save inspection"));
        return 0;
    }
    if (!FParse::Value(*Params, TEXT("Mesh="), MeshPath) ||
        !FParse::Value(*Params, TEXT("Physics="), PhysicsPath) ||
        !FParse::Value(*Params, TEXT("Prefix="), Prefix) ||
        !MeshPath.StartsWith(TEXT("/Game/CSS/EveTest/")) ||
        !PhysicsPath.StartsWith(TEXT("/Game/CSS/EveTest/PA_")) ||
        Prefix.IsEmpty())
        return Fail(TEXT("Specify private -Mesh/-Physics, -Prefix and one primary -Slots entry"));
    // FParse::Value stops at commas, so slot names are joined with '+'.
    FParse::Value(*Params, TEXT("Slots="), SlotList);
    TArray<FString> WantedSlots;
    SlotList.ParseIntoArray(WantedSlots, TEXT("+"), true);
    if (WantedSlots.Num() != 1) return Fail(TEXT("One coherent simulation surface is required"));
    FString AttachedText;
    FParse::Value(*Params, TEXT("Attachments="), AttachedText);
    TArray<FString> AttachedSlots;
    AttachedText.ParseIntoArray(AttachedSlots,TEXT("+"),true);
    if (AttachedSlots.Num() > 8 || AttachedSlots.Contains(WantedSlots[0]))
        return Fail(TEXT("Invalid attached slots"));
    // 2.0.2 (additive): optional -Config=<json> with per-slot Chaos settings, e.g.
    // {"Seductress_B.001": {"AnimDriveStiffness": .08, "DampingCoefficient": .12, "GravityScale": 1.1}}.
    // Absent keys keep the 1.0.2 values below.
    FString ConfigPath, ConfigText;
    TSharedPtr<FJsonObject> Config;
    if (FParse::Value(*Params, TEXT("Config="), ConfigPath))
    {
        if (!FFileHelper::LoadFileToString(ConfigText, *ConfigPath) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConfigText), Config) || !Config.IsValid())
            return Fail(TEXT("Invalid cloth config JSON"));
    }
    auto Setting = [&Config](const FString& Slot, const TCHAR* Key, double Default, double Min, double Max) -> double {
        if (!Config.IsValid()) return Default;
        const TSharedPtr<FJsonObject>* SlotConfig;
        if (!Config->TryGetObjectField(Slot, SlotConfig)) return Default;
        double Value;
        if (!(*SlotConfig)->TryGetNumberField(Key, Value)) return Default;
        return FMath::Clamp(Value, Min, Max);
    };
    FString ProxyPath, ProxyText;
    TSharedPtr<FJsonObject> Proxies;
    if (FParse::Value(*Params, TEXT("Proxy="), ProxyPath))
    {
        if (!FFileHelper::LoadFileToString(ProxyText, *ProxyPath) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProxyText), Proxies) || !Proxies.IsValid())
            return Fail(TEXT("Invalid cloth proxy JSON"));
    }
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
    if (Mesh && FParse::Param(*Params, TEXT("Rebuild")))
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
        const auto OldAssets = Mesh->GetMeshClothingAssets();
        for (const auto& Old : OldAssets) Old->UnbindFromSkeletalMesh(Mesh);
        Mesh->GetMeshClothingAssets().Reset();
        for (const auto& Old : OldAssets)
            Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    if (!Mesh || Mesh->GetLODNum() != 1 || !Mesh->GetMeshClothingAssets().IsEmpty())
        return Fail(TEXT("Expected a one-LOD authored mesh without cloth"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    Mesh->GetLODInfo(0)->BuildSettings.bUseFullPrecisionUVs = true;
    Mesh->PostEditChange();
    FAssetCompilingManager::Get().FinishAllCompilation();
    UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, *PhysicsPath);
    UPhysicsAsset* MeshPhysics = Mesh->GetPhysicsAsset();
    const auto OriginalPostProcess = Mesh->GetPostProcessAnimBlueprint();
    if (!Physics || !MeshPhysics || !OriginalPostProcess)
        return Fail(TEXT("Existing cloth collision and mesh secondary references are required"));
    USkeleton* Shared = LoadObject<USkeleton>(nullptr, TEXT("/Game/CSS/Shared/SKEL_Base.SKEL_Base"));
    if (!Shared) return Fail(TEXT("Shared skeleton missing"));
    const auto& Reference = Mesh->GetRefSkeleton();
    const auto& SharedReference = Shared->GetReferenceSkeleton();
    for (int32 I = 0; I < Reference.GetRawBoneNum(); ++I)
    {
        const int32 J = SharedReference.FindBoneIndex(Reference.GetBoneName(I));
        if (J == INDEX_NONE) return Fail(TEXT("Shared skeleton lacks mesh bone"));
        const int32 P = Reference.GetParentIndex(I), Q = SharedReference.GetParentIndex(J);
        if ((P == INDEX_NONE) != (Q == INDEX_NONE) ||
            (P != INDEX_NONE && Reference.GetBoneName(P) != SharedReference.GetBoneName(Q)))
            return Fail(TEXT("Shared skeleton parent mismatch"));
    }
    Mesh->SetSkeleton(Shared);
    TArray<int32> Sections;
    TSet<FString> FoundSlots;
    for (int32 I = 0; I < Mesh->GetImportedModel()->LODModels[0].Sections.Num(); ++I)
    {
        const auto& Section = Mesh->GetImportedModel()->LODModels[0].Sections[I];
        const FString Slot = Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName.ToString();
        if (WantedSlots.Contains(Slot))
        {
            Sections.Add(I);
            FoundSlots.Add(Slot);
        }
    }
    if (FoundSlots.Num() != WantedSlots.Num() || Sections.Num() != WantedSlots.Num())
        return Fail(TEXT("Every requested cloth slot must own exactly one section in this variant"));
    UClothingAssetFactory* Factory = NewObject<UClothingAssetFactory>();
    for (int32 Section : Sections)
    {
        FSkeletalMeshClothBuildParams Build;
        Build.AssetName = FString::Printf(TEXT("%s_Cloth_%d"), *Prefix, Section);
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
        const FString SlotName = Mesh->GetMaterials()[Mesh->GetImportedModel()->LODModels[0].Sections[Section].MaterialIndex].MaterialSlotName.ToString();
        if (Proxies.IsValid())
        {
            const auto& Slots = Proxies->GetObjectField(TEXT("slots"));
            if (Slots->HasField(SlotName))
            {
                const auto& Proxy = Slots->GetObjectField(SlotName);
                const auto& Positions = Proxy->GetArrayField(TEXT("positions"));
                const auto& Normals = Proxy->GetArrayField(TEXT("normals"));
                const auto& Weights = Proxy->GetArrayField(TEXT("weights"));
                const auto& Indices = Proxy->GetArrayField(TEXT("indices"));
                const int32 OriginalCount = Data.Vertices.Num();
                if (Positions.Num() < 3 || Positions.Num() > OriginalCount || Normals.Num() != Positions.Num() ||
                    Weights.Num() != Positions.Num() || Indices.IsEmpty() || Indices.Num() % 3)
                    return Fail(TEXT("Proxy dimensions are invalid"));
                Data.Reset(Positions.Num(), Indices.Num());
                for (int32 I = 0; I < Positions.Num(); ++I)
                {
                    const auto& P = Positions[I]->AsArray(); const auto& N = Normals[I]->AsArray();
                    if (P.Num() != 3 || N.Num() != 3) return Fail(TEXT("Proxy vector dimensions are invalid"));
                    Data.Vertices[I] = FVector3f(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
                    Data.Normals[I] = FVector3f(N[0]->AsNumber(), N[1]->AsNumber(), N[2]->AsNumber());
                    if (Data.Vertices[I].ContainsNaN() || Data.Normals[I].ContainsNaN() ||
                        !FMath::IsNearlyEqual(Data.Normals[I].Size(), 1.f, .01f))
                        return Fail(TEXT("Invalid proxy position or normal"));
                    const auto& W = Weights[I]->AsArray();
                    if (W.IsEmpty() || W.Num() > 8) return Fail(TEXT("Invalid proxy influence count"));
                    float Total = 0.f;
                    for (int32 J = 0; J < W.Num(); ++J)
                    {
                        const auto& Pair = W[J]->AsArray();
                        if (Pair.Num() != 2) return Fail(TEXT("Invalid proxy influence"));
                        const FName Name(*Pair[0]->AsString());
                        const int32 Bone = Cloth->UsedBoneNames.Find(Name);
                        const float Weight = Pair[1]->AsNumber();
                        if (Bone == INDEX_NONE || Weight <= 0 || Weight > 1 || !FMath::IsFinite(Weight))
                            return Fail(TEXT("Unknown proxy bone or invalid weight"));
                        Data.BoneData[I].BoneIndices[J] = Bone;
                        Data.BoneData[I].BoneWeights[J] = Weight;
                        Total += Weight;
                    }
                    if (!FMath::IsNearlyEqual(Total, 1.f, .0001f)) return Fail(TEXT("Proxy weights are not normalized"));
                }
                for (int32 I = 0; I < Indices.Num(); ++I)
                {
                    const double Index = Indices[I]->AsNumber();
                    if (!FMath::IsFinite(Index) || Index < 0 || Index >= Positions.Num() || FMath::FloorToDouble(Index) != Index)
                        return Fail(TEXT("Invalid proxy triangle index"));
                    Data.Indices[I] = static_cast<uint32>(Index);
                }
                for (int32 I = 0; I < Data.Indices.Num(); I += 3)
                {
                    const FVector3f A = Data.Vertices[Data.Indices[I]], B = Data.Vertices[Data.Indices[I+1]], C = Data.Vertices[Data.Indices[I+2]];
                    if (FVector3f::CrossProduct(B-A, C-A).SizeSquared() <= SMALL_NUMBER)
                        return Fail(TEXT("Degenerate proxy triangle"));
                }
                TSet<uint32> UsedVertices(Data.Indices);
                if (UsedVertices.Num() != Data.Vertices.Num())
                    return Fail(TEXT("Loose proxy vertices would break Chaos self-collision"));
                Data.CalculateNumInfluences();
                UE_LOG(LogCSSEveCloth, Display, TEXT("%s simulation proxy: %d -> %d vertices; render geometry unchanged"), *SlotName, OriginalCount, Data.Vertices.Num());
            }
        }
        if (Data.Vertices.IsEmpty() || Data.Vertices.Num() > 5000)
            return Fail(TEXT("Unexpected simulation vertex count"));
        // Pin every disconnected panel at its own upper edge (same rule as V1).
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
                const FString Slot = Mesh->GetMaterials()[Mesh->GetImportedModel()->LODModels[0].Sections[Section].MaterialIndex].MaterialSlotName.ToString();
                const bool Cuff = Slot == TEXT("Seductress_Cuffs");
                const bool Jewelry = Slot == TEXT("Seductress_C");
                const float Anchor = Setting(Slot, TEXT("Anchor"), Jewelry ? .02 : Cuff ? .8 : 2., 0., 20.);
                const float Falloff = Setting(Slot, TEXT("Falloff"), Jewelry ? 2. : Cuff ? 15. : 50., .5, 200.);
                const float Limit = Setting(Slot, TEXT("MaxDistance"), Jewelry ? .5 : Cuff ? 3. : 35., .1, 35.);
                const float Blend = FMath::Clamp((Top - Anchor - Data.Vertices[Index].Z) / Falloff, 0.f, 1.f);
                Distances[Index] = Limit * Blend * Blend * (3.f - 2.f * Blend);
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
        {
            const float Bending = Setting(SlotName, TEXT("BendingStiffness"), .08, 0., 1.);
            const float Drive = Setting(SlotName, TEXT("AnimDriveStiffness"), .18, 0., 1.);
            const float DriveDamping = Setting(SlotName, TEXT("AnimDriveDamping"), .3, 0., 1.);
            Config->BendingStiffnessWeighted = { Bending, Bending };
            Config->DampingCoefficient = Setting(SlotName, TEXT("DampingCoefficient"), .15, 0., 1.);
            Config->CollisionThickness = Setting(SlotName, TEXT("CollisionThickness"), .5, 0., 5.);
            Config->FrictionCoefficient = Setting(SlotName, TEXT("FrictionCoefficient"), .3, 0., 1.);
            Config->GravityScale = Setting(SlotName, TEXT("GravityScale"), 1., 0., 3.);
            Config->bUseSelfCollisions = Setting(SlotName, TEXT("SelfCollision"), SlotName == TEXT("Seductress_C") ? 0. : 1., 0., 1.) > .5;
            Config->SelfCollisionThickness = .35f;
            Config->AnimDriveStiffness = { Drive, Drive };
            Config->AnimDriveDamping = { DriveDamping, DriveDamping };
        }
        Cloth->ApplyParameterMasks(true, true);
        if (!Cloth->BindToSkeletalMesh(Mesh, 0, Section, 0))
            return Fail(TEXT("Cloth render mapping failed"));
        auto& Model = Mesh->GetImportedModel()->LODModels[0];
        auto& UserData = Model.UserSectionsData.FindOrAdd(Model.Sections[Section].OriginalDataSectionIndex);
        UserData.CorrespondClothAssetIndex = static_cast<int16>(Mesh->GetMeshClothingAssets().IndexOfByKey(Cloth));
        UserData.ClothingData.AssetGuid = Cloth->GetAssetGuid();
        UserData.ClothingData.AssetLodIndex = 0;
        UE_LOG(LogCSSEveCloth, Display, TEXT("Section %d: %d simulation vertices, %d pinned, %d connected pieces. Runtime motion not yet tested."),
            Section, Data.Vertices.Num(), Pinned, Components);
    }
    UClothingAssetCommon* SharedCloth = Mesh->GetMeshClothingAssets().Num()==1 ?
        Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]) : nullptr;
    if (!SharedCloth) return Fail(TEXT("Primary cloth missing"));
    TArray<int32> AttachedSections;
    auto& Model = Mesh->GetImportedModel()->LODModels[0];
    auto& ClothLOD = SharedCloth->LodData[0];
    for (const FString& Wanted : AttachedSlots)
    {
        int32 Found = INDEX_NONE;
        for (int32 I = 0; I < Model.Sections.Num(); ++I)
        {
            if (Mesh->GetMaterials()[Model.Sections[I].MaterialIndex].MaterialSlotName.ToString() != Wanted) continue;
            if (Found != INDEX_NONE) return Fail(TEXT("Attachment has multiple render sections"));
            Found = I;
        }
        if (Found == INDEX_NONE || AttachedSections.Contains(Found)) return Fail(TEXT("Missing or duplicate attachment"));
        auto& Section = Model.Sections[Found];
        TArray<FVector3f> Positions, Normals, Tangents;
        TArray<uint32> Indices;
        for (const auto& Vertex : Section.SoftVertices)
        {
            Positions.Add(Vertex.Position); Normals.Add(Vertex.TangentZ); Tangents.Add(Vertex.TangentX);
        }
        for (uint32 I = Section.BaseIndex; I < Section.BaseIndex+Section.NumTriangles*3; ++I)
        {
            const uint32 Index = Model.IndexBuffer[I]-Section.BaseVertexIndex;
            if (Index >= uint32(Positions.Num())) return Fail(TEXT("Invalid attached triangle index"));
            Indices.Add(Index);
        }
        ClothingMeshUtils::ClothMeshDesc Target(Positions,Normals,Tangents,Indices);
        ClothingMeshUtils::ClothMeshDesc Source(ClothLOD.PhysicalMeshData.Vertices,ClothLOD.PhysicalMeshData.Indices);
        Section.ClothMappingDataLODs.SetNum(1);
        ClothingMeshUtils::GenerateMeshToMeshVertData(Section.ClothMappingDataLODs[0],Target,Source,
            ClothLOD.PhysicalMeshData.FindWeightMap(EWeightMapTargetCommon::MaxDistance),
            ClothLOD.bSmoothTransition,ClothLOD.bUseMultipleInfluences,ClothLOD.SkinningKernelRadius);
        const int32 Expected = Positions.Num()*(ClothLOD.bUseMultipleInfluences ? 5 : 1);
        if (Section.ClothMappingDataLODs[0].Num() != Expected) return Fail(TEXT("Incomplete attached render mapping"));
        for (FName Name : SharedCloth->UsedBoneNames)
        {
            const int32 Bone = Mesh->GetRefSkeleton().FindBoneIndex(Name);
            if (Bone == INDEX_NONE) return Fail(TEXT("Attachment bone missing"));
            Section.BoneMap.AddUnique(Bone);
            Model.RequiredBones.AddUnique(Bone); Model.ActiveBoneIndices.AddUnique(Bone);
        }
        if (Section.BoneMap.Num()>256) return Fail(TEXT("Attachment bone map exceeds conservative limit"));
        Section.CorrespondClothAssetIndex = Model.Sections[Sections[0]].CorrespondClothAssetIndex;
        Section.ClothingData.AssetGuid = SharedCloth->GetAssetGuid();
        Section.ClothingData.AssetLodIndex = 0;
        auto& User = Model.UserSectionsData.FindOrAdd(Section.OriginalDataSectionIndex);
        User.CorrespondClothAssetIndex = Section.CorrespondClothAssetIndex;
        User.ClothingData = Section.ClothingData;
        AttachedSections.Add(Found);
    }
    Model.RequiredBones.Sort();
    Mesh->GetRefSkeleton().EnsureParentsExistAndSort(Model.ActiveBoneIndices);
    Mesh->PostEditChange();
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (int32 I : AttachedSections)
    {
        const auto& Section = Mesh->GetImportedModel()->LODModels[0].Sections[I];
        if (Mesh->GetSectionClothingAsset(0,I) != SharedCloth || Section.ClothMappingDataLODs.IsEmpty() ||
            Section.ClothMappingDataLODs[0].IsEmpty()) return Fail(TEXT("Shared attachment lost during rebuild"));
    }
    UE_LOG(LogCSSEveCloth,Display,TEXT("Verified %d sections sharing the primary simulation after rebuild"),AttachedSections.Num());
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
        if (Kinematic <= 0 || Kinematic >= Data.Vertices.Num())
            return Fail(TEXT("Cloth lost its fixed or movable particles"));
        UE_LOG(LogCSSEveCloth, Display, TEXT("Verified bound section %d after rebuild, %d Chaos kinematic vertices."), Section, Kinematic);
    }
    if (Mesh->GetPhysicsAsset() != MeshPhysics || Mesh->GetPostProcessAnimBlueprint() != OriginalPostProcess)
        return Fail(TEXT("Secondary asset assignment did not persist"));
    UE_LOG(LogCSSEveCloth, Display, TEXT("Bound %d cloth sections on %s; physics and post-process references set."), Sections.Num(), *MeshPath);
    Mesh->MarkPackageDirty();
    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    const FString Filename = FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Mesh->GetOutermost(), Mesh, *Filename, Save) ? 0 : 1;
}
