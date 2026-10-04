#include "CSSPoseLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AnimGraphNode_CopyPoseFromMesh.h"
#include "AnimGraphNode_Root.h"
#include "AssetToolsModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"

UAnimBlueprint* UCSSPoseLibrary::CreateCopyPoseTemplate(const FString& OutputPackage)
{
    if (!IsRunningCommandlet() ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/SharedAssets/Astral/ABP_CopyPose")) ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage))
        return nullptr;

    auto* Factory = NewObject<UAnimBlueprintFactory>();
    Factory->ParentClass = UAnimInstance::StaticClass();
    Factory->bTemplate = true;
    auto* Blueprint = Cast<UAnimBlueprint>(FAssetToolsModule::GetModule().Get().CreateAsset(
        FPackageName::GetLongPackageAssetName(OutputPackage),
        FPackageName::GetLongPackagePath(OutputPackage), UAnimBlueprint::StaticClass(), Factory));
    if (!Blueprint) return nullptr;
    UEdGraph* Graph = nullptr;
    for (auto Candidate : Blueprint->FunctionGraphs)
        if (Candidate->GetFName() == TEXT("AnimGraph")) Graph = Candidate.Get();
    if (!Graph) return nullptr;
    UAnimGraphNode_Root* Root = nullptr;
    for (auto Node : Graph->Nodes)
        if (auto* Found = Cast<UAnimGraphNode_Root>(Node))
        {
            if (Root) return nullptr;
            Root = Found;
        }
    if (!Root) return nullptr;
    auto* Copy = NewObject<UAnimGraphNode_CopyPoseFromMesh>(Graph);
    Graph->AddNode(Copy, false, false);
    Copy->CreateNewGuid();
    Copy->PostPlacedNewNode();
    Copy->Node.bUseAttachedParent = true;
    Copy->Node.bCopyCurves = true;
    Copy->Node.bCopyCustomAttributes = true;
    Copy->Node.bUseMeshPose = false;
    Copy->AllocateDefaultPins();
    auto* Output = Copy->FindPin(TEXT("Pose"));
    auto* Input = Root->FindPin(TEXT("Result"));
    if (!Output || !Input || !Graph->GetSchema()->TryCreateConnection(Output, Input)) return nullptr;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
    const auto* Generated = Cast<UAnimBlueprintGeneratedClass>(Blueprint->GeneratedClass);
    if (Results.NumErrors || Results.NumWarnings || Blueprint->Status == BS_Error ||
        !Blueprint->bIsTemplate || Blueprint->TargetSkeleton || !Generated || Generated->TargetSkeleton)
        return nullptr;
    return Blueprint;
}

FString UCSSPoseLibrary::CheckCopyPose(UAnimBlueprint* Blueprint, USkeletalMesh* SourceMesh,
    USkeletalMesh* VisualMesh, UAnimSequence* Sequence, bool bWithPostProcess)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"), false);
    auto Serialize = [&]() {
        FString Json;
        FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
        return Json;
    };
    if (!IsRunningCommandlet() || !GEditor || !Blueprint || !Blueprint->GeneratedClass ||
        !SourceMesh || !VisualMesh || !Sequence)
        return Serialize();
    auto* World = GEditor->GetEditorWorldContext().World();
    if (!World) return Serialize();
    auto* Actor = World->SpawnActor<AActor>();
    if (!Actor) return Serialize();
    ON_SCOPE_EXIT { World->DestroyActor(Actor); };
    auto MakeComponent = [&](USkeletalMesh* Mesh, USkeletalMeshComponent* Parent) {
        auto* Component = NewObject<USkeletalMeshComponent>(Actor);
        Actor->AddInstanceComponent(Component);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetVisibility(false);
        Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Component->SetDisablePostProcessBlueprint(!Parent || !bWithPostProcess);
        Component->SetSkeletalMeshAsset(Mesh);
        if (Parent)
        {
            Component->SetupAttachment(Parent);
            Component->SetAnimInstanceClass(Blueprint->GeneratedClass);
            Component->AddTickPrerequisiteComponent(Parent);
        }
        else Actor->SetRootComponent(Component);
        Component->RegisterComponent();
        return Component;
    };
    auto* Source = MakeComponent(SourceMesh, nullptr);
    Source->PlayAnimation(Sequence, true);
    auto* OriginalInstance = Source->GetAnimInstance();
    auto* Visual = MakeComponent(VisualMesh, Source);
    if (!OriginalInstance || !Visual->GetAnimInstance()) return Serialize();
    Result->SetBoolField(TEXT("post_process_enabled"), bWithPostProcess);
    Result->SetBoolField(TEXT("post_process_instance"), Visual->GetPostProcessInstance() != nullptr);
    if (Visual->GetPostProcessInstance())
        Result->SetStringField(TEXT("post_process_class"), Visual->GetPostProcessInstance()->GetClass()->GetPathName());
    if (bWithPostProcess && !Visual->GetPostProcessInstance()) return Serialize();
    const auto& SourceRef = SourceMesh->GetRefSkeleton();
    const auto& VisualRef = VisualMesh->GetRefSkeleton();
    int32 Matched = 0;
    int32 Extra = 0;
    int32 Samples = 0;
    double PositionError = 0;
    double RotationError = 0;
    double ScaleError = 0;
    double Motion = 0;
    double ExtraMotion = 0;
    TMap<FName, double> BoneErrors;
    TArray<FTransform> First;
    TArray<FTransform> FirstVisual;
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        Source->TickAnimation(1.f / 60.f, false);
        Source->RefreshBoneTransforms();
        Visual->TickAnimation(1.f / 60.f, false);
        Visual->RefreshBoneTransforms();
        const auto& SourcePose = Source->GetComponentSpaceTransforms();
        const auto& VisualPose = Visual->GetComponentSpaceTransforms();
        if (SourcePose.Num() != SourceRef.GetNum() || VisualPose.Num() != VisualRef.GetNum())
            return Serialize();
        if (Frame == 0)
        {
            First = SourcePose;
            FirstVisual = VisualPose;
        }
        for (int32 Bone = 0; Bone < VisualRef.GetNum(); ++Bone)
        {
            const int32 SourceBone = SourceRef.FindBoneIndex(VisualRef.GetBoneName(Bone));
            if (SourceBone == INDEX_NONE)
            {
                if (Frame == 0) ++Extra;
                ExtraMotion = FMath::Max(ExtraMotion,
                    FirstVisual[Bone].GetRotation().AngularDistance(VisualPose[Bone].GetRotation()));
                continue;
            }
            if (Frame == 0) ++Matched;
            const auto& A = SourcePose[SourceBone];
            const auto& B = VisualPose[Bone];
            auto& Error = BoneErrors.FindOrAdd(VisualRef.GetBoneName(Bone));
            Error = FMath::Max(Error, FVector::Dist(A.GetTranslation(), B.GetTranslation()));
            PositionError = FMath::Max(PositionError, FVector::Dist(A.GetTranslation(), B.GetTranslation()));
            RotationError = FMath::Max(RotationError, A.GetRotation().AngularDistance(B.GetRotation()));
            ScaleError = FMath::Max(ScaleError, FVector::Dist(A.GetScale3D(), B.GetScale3D()));
            Motion = FMath::Max(Motion, FVector::Dist(First[SourceBone].GetTranslation(), A.GetTranslation()));
            ++Samples;
        }
    }
    Result->SetStringField(TEXT("source"), SourceMesh->GetPathName());
    Result->SetStringField(TEXT("visual"), VisualMesh->GetPathName());
    Result->SetStringField(TEXT("animation"), Sequence->GetPathName());
    Result->SetBoolField(TEXT("different_skeletons"), SourceMesh->GetSkeleton() != VisualMesh->GetSkeleton());
    Result->SetNumberField(TEXT("matched_bones"), Matched);
    Result->SetNumberField(TEXT("extra_bones"), Extra);
    Result->SetNumberField(TEXT("samples"), Samples);
    Result->SetNumberField(TEXT("max_position_error_cm"), PositionError);
    Result->SetNumberField(TEXT("max_rotation_error_radians"), RotationError);
    Result->SetNumberField(TEXT("max_scale_error"), ScaleError);
    Result->SetNumberField(TEXT("motion_cm"), Motion);
    Result->SetNumberField(TEXT("extra_bone_motion_radians"), ExtraMotion);
    auto Differences = MakeShared<FJsonObject>();
    for (const auto& Pair : BoneErrors)
        if (Pair.Value > 0.001) Differences->SetNumberField(Pair.Key.ToString(), Pair.Value);
    Result->SetObjectField(TEXT("changed_bone_positions_cm"), Differences);
    const bool Preserved = Source->GetAnimInstance() == OriginalInstance && Source->GetSkeletalMeshAsset() == SourceMesh;
    Result->SetBoolField(TEXT("source_instance_preserved"), Preserved);
    Result->SetBoolField(TEXT("passed"), Preserved && Matched > 0 && Motion > 0.01 &&
        (bWithPostProcess ? ExtraMotion > 0.001 :
            PositionError < 0.001 && RotationError < 0.001 && ScaleError < 0.001));
    return Serialize();
}
