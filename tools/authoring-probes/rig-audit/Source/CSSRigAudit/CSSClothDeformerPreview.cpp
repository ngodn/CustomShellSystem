#include "CSSRigAuditLibrary.h"
#include "CSSClothBasisStudy.h"
#include "Animation/MeshDeformer.h"
#include "Animation/MeshDeformerInstance.h"
#include "AssetCompilingManager.h"
#include "ClothingSimulationInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "OptimusDeformerInstance.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool IsClothPreviewMesh(const USkeletalMesh* Mesh)
{
    if (!Mesh) return false;
    const FString Path = Mesh->GetPathName();
    return Path.StartsWith(TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/")) ||
        Path == TEXT("/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony_BasisStudy3.SK_Peony_BasisStudy3") ||
        Path == TEXT("/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony_Face_BasisStudy3.SK_Peony_Face_BasisStudy3");
}
}

bool UCSSRigAuditLibrary::SetClothStudyDeformer(USkeletalMeshComponent* Component, UMeshDeformer* Deformer)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Component ||
        !Component->GetWorld() || !Component->GetWorld()->IsPreviewWorld() ||
        !IsClothPreviewMesh(Component->GetSkeletalMeshAsset()))
        return false;
    Component->SetMeshDeformer(Deformer);
    return (Component->GetMeshDeformerInstance() != nullptr) == (Deformer != nullptr);
}

bool UCSSRigAuditLibrary::RequestClothDeformerReadback(USkeletalMeshComponent* Component, const FString& Label)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Component ||
        !Component->GetWorld() || !Component->GetWorld()->IsPreviewWorld() ||
        !IsClothPreviewMesh(Component->GetSkeletalMeshAsset()) || !Component->GetMeshDeformerInstance() ||
        Label.IsEmpty() || Label.Len() > 96)
        return false;
    for (TCHAR Character : Label)
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-')) return false;

    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DeformerReadback"));
    const FString Output = FPaths::Combine(Directory, Label + TEXT(".json"));
    const FString Pending = Output + TEXT(".pending");
    if (FPaths::FileExists(Output) || FPaths::FileExists(Pending) ||
        !IFileManager::Get().MakeDirectory(*Directory, true)) return false;
    auto Request = MakeUnique<FMeshDeformerGeometryReadbackRequest>();
    // The callback can run after component teardown. Capture no UObject or stack references.
    Request->VertexDataArraysCallback_AnyThread = [Output, Pending](const FMeshDeformerGeometryReadbackVertexDataArrays& Data)
    {
        auto Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("lod"), Data.LODIndex);
        Result->SetNumberField(TEXT("vertices"), Data.Positions.Num());
        bool Valid = Data.LODIndex == 0 && !Data.Positions.IsEmpty() &&
            Data.Normals.Num() == Data.Positions.Num() && Data.Tangents.Num() == Data.Positions.Num();
        TArray<TSharedPtr<FJsonValue>> Vertices;
        if (Valid)
        {
            Vertices.Reserve(Data.Positions.Num());
            for (int32 Index = 0; Index < Data.Positions.Num(); ++Index)
            {
                const FVector3f Values[] = {Data.Positions[Index], Data.Normals[Index], Data.Tangents[Index]};
                TArray<TSharedPtr<FJsonValue>> Row;
                Row.Reserve(9);
                for (const auto& Value : Values)
                {
                    Valid &= !Value.ContainsNaN();
                    Row.Add(MakeShared<FJsonValueNumber>(Value.X));
                    Row.Add(MakeShared<FJsonValueNumber>(Value.Y));
                    Row.Add(MakeShared<FJsonValueNumber>(Value.Z));
                }
                if (!Valid) break;
                Vertices.Add(MakeShared<FJsonValueArray>(MoveTemp(Row)));
            }
        }
        Result->SetBoolField(TEXT("passed"), Valid);
        if (Valid) Result->SetArrayField(TEXT("position_normal_tangent"), MoveTemp(Vertices));
        FString Text;
        if (!FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text)) ||
            !FFileHelper::SaveStringToFile(Text, *Pending) ||
            !IFileManager::Get().Move(*Output, *Pending, false))
            UE_LOG(LogTemp, Error, TEXT("Cannot save deformer readback: %s"), *Output);
    };
    return Component->RequestReadbackRenderGeometry(MoveTemp(Request));
}

FString UCSSRigAuditLibrary::StepClothDeformerStudy(USkeletalMeshComponent* Component,
    bool Initialize, bool ResetCloth, bool DisableCloth, bool AdvanceSimulation)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Component ||
        !Component->GetWorld() || !Component->GetWorld()->IsPreviewWorld() ||
        !IsClothPreviewMesh(Component->GetSkeletalMeshAsset()) ||
        (!AdvanceSimulation && (Initialize || ResetCloth)))
        return TEXT("ERROR: expected an isolated supported cloth mesh preview");

    if (Initialize)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
        Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        Component->SetDisablePostProcessBlueprint(true);
        Component->SetUpdateAnimationInEditor(true);
        Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Component->InitAnim(true);
        Component->SetComponentTickEnabled(false);
        Component->bDisableClothSimulation = false;
        Component->ClothBlendWeight = 1.f;
        Component->bCollideWithEnvironment = false;
        Component->bWaitForParallelClothTask = false;
        Component->SetUpdateClothInEditor(true);
        Component->ResumeClothingSimulation();
        Component->RecreateClothingActors();
        Component->ClothTickFunction.SetTickFunctionEnable(false);
    }
    if (Component->IsComponentTickEnabled() || Component->ClothTickFunction.IsTickFunctionEnabled() ||
        !Component->GetClothingSimulation()) return TEXT("ERROR: preview simulation not initialized");
    Component->bDisableClothSimulation = DisableCloth;

    auto* World = Component->GetWorld();
    constexpr float Step = 1.f / 60.f;
    if (AdvanceSimulation)
    {
        World->DeltaTimeSeconds = World->DeltaRealTimeSeconds = Step;
        World->TimeSeconds += Step;
        World->UnpausedTimeSeconds += Step;
        World->RealTimeSeconds += Step;
        Component->TickAnimation(Step, true);
    }
    if (auto* Deformer = Cast<UOptimusDeformerInstance>(Component->GetMeshDeformerInstance());
        Deformer && Deformer->GetName().StartsWith(TEXT("DG_ClothMorphStudy3_Instance")))
    {
        if (!Deformer->SetFloatVariable(TEXT("CSS_ClothGluteWeight"), Component->GetMorphTarget(TEXT("PBMGlutesSize"))))
            return TEXT("ERROR: basis study weight unavailable");
    }
    if (auto* Deformer = Cast<UOptimusDeformerInstance>(Component->GetMeshDeformerInstance());
        Deformer && Deformer->GetName().StartsWith(TEXT("DG_ClothMorphStudy4_Instance")))
    {
        for (const auto& Shape : CSSClothBasisStudy::Shapes)
            if (!Deformer->SetFloatVariable(*FString(Shape.Attribute).Append(TEXT("Weight")), Component->GetMorphTarget(Shape.Morph)))
                return TEXT("ERROR: shape basis study weight unavailable");
    }
    if (AdvanceSimulation) Component->RefreshBoneTransforms();
    if (ResetCloth) Component->ForceClothNextUpdateTeleportAndReset();
    FActorComponentTickFunction Tick;
    if (AdvanceSimulation && !Component->bDisableClothSimulation)
    {
        Component->TickClothing(Step, Tick);
        Component->WaitForExistingParallelClothSimulation_GameThread();
    }
    Component->UpdateBounds();
    Component->MarkRenderDynamicDataDirty();
    World->SendAllEndOfFrameUpdates();

    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("mesh"), Component->GetSkeletalMeshAsset()->GetPathName());
    Result->SetStringField(TEXT("deformer_instance"), GetPathNameSafe(Component->GetMeshDeformerInstance()));
    Result->SetBoolField(TEXT("cloth_disabled"), Component->bDisableClothSimulation);
    Result->SetNumberField(TEXT("glute_morph_weight"), Component->GetMorphTarget(TEXT("PBMGlutesSize")));
    Result->SetNumberField(TEXT("simulation_cloths"), Component->GetClothingSimulation()->GetNumCloths());
    FSHA1 ClothHash;
    const auto& ClothData = Component->GetCurrentClothingData_AnyThread();
    TArray<int32> ClothKeys;
    ClothData.GetKeys(ClothKeys);
    ClothKeys.Sort();
    int32 Particles = 0;
    for (const int32 Key : ClothKeys)
    {
        const auto& Item = ClothData.FindChecked(Key);
        if (Item.Positions.Num() != Item.Normals.Num())
            return TEXT("ERROR: cloth position/normal count differs");
        ClothHash.Update(reinterpret_cast<const uint8*>(&Key), sizeof(Key));
        ClothHash.Update(reinterpret_cast<const uint8*>(&Item.LODIndex), sizeof(Item.LODIndex));
        for (int32 Index = 0; Index != Item.Positions.Num(); ++Index)
        {
            const auto& Position = Item.Positions[Index];
            const auto& Normal = Item.Normals[Index];
            if (Position.ContainsNaN() || Normal.ContainsNaN()) return TEXT("ERROR: nonfinite cloth data");
            const float Values[]{Position.X, Position.Y, Position.Z, Normal.X, Normal.Y, Normal.Z};
            ClothHash.Update(reinterpret_cast<const uint8*>(Values), sizeof(Values));
        }
        for (const auto* Transform : {&Item.Transform, &Item.ComponentRelativeTransform})
        {
            if (Transform->ContainsNaN()) return TEXT("ERROR: nonfinite cloth transform");
            const auto T = Transform->GetTranslation();
            const auto R = Transform->GetRotation();
            const auto S = Transform->GetScale3D();
            const double Values[]{T.X, T.Y, T.Z, R.X, R.Y, R.Z, R.W, S.X, S.Y, S.Z};
            ClothHash.Update(reinterpret_cast<const uint8*>(Values), sizeof(Values));
        }
        Particles += Item.Positions.Num();
    }
    ClothHash.Final();
    FSHAHash Hash;
    ClothHash.GetHash(Hash.Hash);
    Result->SetStringField(TEXT("cloth_state_sha1"), Hash.ToString());
    Result->SetBoolField(TEXT("advanced_simulation"), AdvanceSimulation);
    Result->SetNumberField(TEXT("particles"), Particles);
    FString Text;
    if (!FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text)))
        return TEXT("ERROR: cannot serialize preview");
    return Text;
}
