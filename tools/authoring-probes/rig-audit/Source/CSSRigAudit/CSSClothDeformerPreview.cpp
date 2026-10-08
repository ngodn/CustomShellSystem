#include "CSSRigAuditLibrary.h"
#include "Animation/MeshDeformerInstance.h"
#include "AssetCompilingManager.h"
#include "ClothingSimulationInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"

FString UCSSRigAuditLibrary::StepClothDeformerStudy(USkeletalMeshComponent* Component,
    bool Initialize, bool ResetCloth)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Component ||
        !Component->GetWorld() || !Component->GetWorld()->IsPreviewWorld() ||
        !Component->GetSkeletalMeshAsset() ||
        !Component->GetSkeletalMeshAsset()->GetPathName().StartsWith(TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/")))
        return TEXT("ERROR: expected an isolated Genessa facial mesh preview");

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

    auto* World = Component->GetWorld();
    constexpr float Step = 1.f / 60.f;
    World->DeltaTimeSeconds = World->DeltaRealTimeSeconds = Step;
    World->TimeSeconds += Step;
    World->UnpausedTimeSeconds += Step;
    World->RealTimeSeconds += Step;
    Component->TickAnimation(Step, true);
    Component->RefreshBoneTransforms();
    if (ResetCloth) Component->ForceClothNextUpdateTeleportAndReset();
    FActorComponentTickFunction Tick;
    if (!Component->bDisableClothSimulation)
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
    Result->SetNumberField(TEXT("simulation_cloths"), Component->GetClothingSimulation()->GetNumCloths());
    int32 Particles = 0;
    for (const auto& Item : Component->GetCurrentClothingData_AnyThread())
    {
        if (Item.Value.Positions.Num() != Item.Value.Normals.Num())
            return TEXT("ERROR: cloth position/normal count differs");
        for (const auto& Position : Item.Value.Positions)
            if (Position.ContainsNaN()) return TEXT("ERROR: nonfinite cloth position");
        Particles += Item.Value.Positions.Num();
    }
    Result->SetNumberField(TEXT("particles"), Particles);
    FString Text;
    if (!FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text)))
        return TEXT("ERROR: cannot serialize preview");
    return Text;
}
