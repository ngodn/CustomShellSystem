#include "CSSRigAuditLibrary.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/PoseSnapshot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "JsonObjectConverter.h"
#include "Misc/ScopeExit.h"
#include "PreviewScene.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

FString UCSSRigAuditLibrary::SnapshotHandPostProcess(USkeletalMesh* Mesh, UAnimBlueprint* Blueprint,
    const FString& SnapshotJson, const TArray<FName>& Curves, bool bCompatibleInput)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Mesh || !Blueprint ||
        !Blueprint->GeneratedClass || Blueprint->TargetSkeleton != Mesh->GetSkeleton())
        return TEXT("ERROR: invalid component assets or execution context");
    FPoseSnapshot Snapshot;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(SnapshotJson, &Snapshot) ||
        !Snapshot.bIsValid || Snapshot.BoneNames.Num() != Snapshot.LocalTransforms.Num())
        return TEXT("ERROR: invalid input snapshot");
    TMap<FName, int32> InputIndices;
    for (int32 Index = 0; Index < Snapshot.BoneNames.Num(); ++Index)
    {
        if (InputIndices.Contains(Snapshot.BoneNames[Index]) ||
            Snapshot.LocalTransforms[Index].ContainsNaN() ||
            !Snapshot.LocalTransforms[Index].GetRotation().IsNormalized())
            return TEXT("ERROR: duplicate or invalid input bone");
        InputIndices.Add(Snapshot.BoneNames[Index], Index);
    }
    const auto& Ref = Mesh->GetRefSkeleton();
    FMemMark Mark(FMemStack::Get());
    TStrongObjectPtr<UAnimSequence> Sequence(NewObject<UAnimSequence>(GetTransientPackage()));
    Sequence->SetSkeleton(Mesh->GetSkeleton());
    Sequence->SetRetargetSourceAsset(Mesh);
    Sequence->UpdateRetargetSourceAssetData();
    auto& Controller = Sequence->GetController();
    Controller.InitializeModel();
    TArray<TSharedPtr<FJsonValue>> Missing;
    {
        IAnimationDataController::FScopedBracket Edit(Controller,
            FText::FromString(TEXT("Construct isolated hand fixture")), false);
        Controller.SetFrameRate(FFrameRate(60, 1), false);
        Controller.SetNumberOfFrames(FFrameNumber(1), false);
        for (int32 Index = 0; Index < Ref.GetRawBoneNum(); ++Index)
        {
            const FName Name = Ref.GetBoneName(Index);
            const int32* Input = InputIndices.Find(Name);
            const FTransform& Pose = Input ? Snapshot.LocalTransforms[*Input] : Ref.GetRefBonePose()[Index];
            if (!Input) Missing.Add(MakeShared<FJsonValueString>(Name.ToString()));
            // Fixture rotations are retained; translation comes from this mesh's bind.
            const FVector3f Position(Ref.GetRefBonePose()[Index].GetTranslation());
            const FQuat4f Rotation(Pose.GetRotation());
            const FVector3f Scale(Pose.GetScale3D());
            if (!Controller.AddBoneCurve(Name, false) || !Controller.SetBoneTrackKeys(Name,
                {Position, Position}, {Rotation, Rotation}, {Scale, Scale}, false))
                return TEXT("ERROR: cannot construct transient animation");
        }
        Controller.NotifyPopulated();
    }
    Sequence->BeginCacheDerivedDataForCurrentPlatform();
    Sequence->WaitOnExistingCompression(true);
    if (!Sequence->IsCompressedDataValid()) return TEXT("ERROR: compression failed");
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    TStrongObjectPtr<USkeletalMeshComponent> Component(NewObject<USkeletalMeshComponent>(GetTransientPackage()));
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()), true);
    Component->SetDisablePostProcessBlueprint(false);
    Scene.AddComponent(Component.Get(), FTransform::Identity);
    ON_SCOPE_EXIT
    {
        Component->CompleteParallelAnimationEvaluation(true);
        Scene.RemoveComponent(Component.Get());
    };
    Component->InitAnim(true);
    Component->SetAnimation(Sequence.Get());
    Component->Stop();
    TArray<TSharedPtr<FJsonValue>> Samples;
    for (int32 Frame = 0; Frame < 7; ++Frame)
    {
        if (Frame == 6)
        {
            Component->InitAnim(true);
            Component->SetAnimation(Sequence.Get());
            Component->Stop();
        }
        auto* Instance = Component->GetPostProcessInstance();
        if (!Instance) return TEXT("ERROR: no post-process instance");
        auto* Enable = FindFProperty<FBoolProperty>(Instance->GetClass(), TEXT("CSSHandEnabled"));
        auto* Compatible = FindFProperty<FBoolProperty>(Instance->GetClass(), TEXT("CSSHandInputIsV43Compatible"));
        if (!Enable || !Compatible) return TEXT("ERROR: missing hand controls");
        const bool bEnabled = Frame == 1 || Frame == 2 || Frame == 4 || Frame == 6;
        Enable->SetPropertyValue_InContainer(Instance, bEnabled);
        Compatible->SetPropertyValue_InContainer(Instance, bCompatibleInput);
        Component->SetPosition(0.f, false);
        Component->TickAnimation(Frame == 0 ? 0.f : 1.f / 60.f, true);
        Component->RefreshBoneTransforms();
        const auto Poses = Component->GetBoneSpaceTransformsView();
        if (Poses.Num() != Ref.GetNum()) return TEXT("ERROR: incomplete component pose");
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("frame"), Frame);
        Row->SetBoolField(TEXT("hand_enabled"), bEnabled);
        TArray<TSharedPtr<FJsonValue>> Bones;
        for (int32 Index = 0; Index < Ref.GetRawBoneNum(); ++Index)
        {
            const auto& Pose = Poses[Index];
            if (Pose.ContainsNaN()) return TEXT("ERROR: nonfinite component output");
            auto Bone = MakeShared<FJsonObject>();
            Bone->SetStringField(TEXT("name"), Ref.GetBoneName(Index).ToString());
            const auto T = Pose.GetTranslation();
            const auto Q = Pose.GetRotation();
            const auto S = Pose.GetScale3D();
            TArray<TSharedPtr<FJsonValue>> Values;
            for (const double Value : {T.X, T.Y, T.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z})
                Values.Add(MakeShared<FJsonValueNumber>(Value));
            Bone->SetArrayField(TEXT("transform"), Values);
            Bones.Add(MakeShared<FJsonValueObject>(Bone));
        }
        Row->SetArrayField(TEXT("bones"), Bones);
        auto CurveValues = MakeShared<FJsonObject>();
        auto MorphWeights = MakeShared<FJsonObject>();
        for (const FName Name : Curves)
        {
            auto* Morph = Mesh->FindMorphTarget(Name);
            if (!Morph) return TEXT("ERROR: missing requested morph");
            float Value = 0.f;
            Component->GetCurveValue(Name, 0.f, Value);
            CurveValues->SetNumberField(Name.ToString(), Value);
            const int32* Weight = Component->ActiveMorphTargets.Find(Morph);
            if (Weight && !Component->MorphTargetWeights.IsValidIndex(*Weight))
                return TEXT("ERROR: invalid morph weight index");
            MorphWeights->SetNumberField(Name.ToString(), Weight ? Component->MorphTargetWeights[*Weight] : 0.f);
        }
        Row->SetObjectField(TEXT("curves"), CurveValues);
        Row->SetObjectField(TEXT("morph_weights"), MorphWeights);
        Samples.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("samples"), Samples);
    Report->SetArrayField(TEXT("input_missing_used_bind"), Missing);
    FString Result;
    return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Result))
        ? Result : TEXT("ERROR: cannot serialize component result");
}
