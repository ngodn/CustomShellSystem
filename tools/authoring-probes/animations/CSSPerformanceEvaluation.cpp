#include "CSSPerformanceLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/ScopeExit.h"
#include "PreviewScene.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

FString UCSSPerformanceLibrary::EvaluatePerformanceLayer(USkeletalMesh* Mesh, UAnimBlueprint* Source,
    UAnimBlueprint* Layer, UAnimSequence* BaseClip, UAnimSequence* PerformanceClip)
{
    auto Fail = [](const TCHAR* Message) -> FString {
        UE_LOG(LogTemp, Error, TEXT("CSS performance evaluation: %s"), Message);
        return {};
    };
    if (!IsRunningCommandlet() || !Mesh || !Source || !Layer || !BaseClip || !PerformanceClip ||
        Source->TargetSkeleton != Mesh->GetSkeleton() || Layer->TargetSkeleton != Mesh->GetSkeleton() ||
        !VerifyPerformanceLayer(Source, Layer).StartsWith(TEXT("PASS:")) ||
        !BaseClip->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/")) ||
        !PerformanceClip->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/")) || BaseClip == PerformanceClip)
        return Fail(TEXT("Invalid isolated inputs"));
    for (auto* Clip : {BaseClip, PerformanceClip})
    {
        if (Clip->IsValidAdditive() || Clip->bEnableRootMotion || !Clip->Notifies.IsEmpty() ||
            Clip->GetPlayLength() <= 0.f || Clip->GetPlayLength() > 300.f)
            return Fail(TEXT("Unsupported clip settings"));
        Clip->BeginCacheDerivedDataForCurrentPlatform();
        Clip->WaitOnExistingCompression(true);
        if (!Clip->IsCompressedDataValid()) return Fail(TEXT("Missing compressed clip data"));
    }
    FMemMark Memory(FMemStack::Get());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto MakeComponent = [&](UAnimBlueprint* Blueprint) {
        auto* Component = NewObject<USkeletalMeshComponent>();
        Component->SetSkeletalMesh(Mesh);
        Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()), true);
        Scene.AddComponent(Component, FTransform::Identity);
        Component->InitAnim(true);
        Component->SetAnimation(BaseClip);
        Component->Stop();
        return Component;
    };
    auto* Original = MakeComponent(Source);
    auto* Candidate = MakeComponent(Layer);
    ON_SCOPE_EXIT {
        Original->CompleteParallelAnimationEvaluation(true);
        Candidate->CompleteParallelAnimationEvaluation(true);
        Scene.RemoveComponent(Original);
        Scene.RemoveComponent(Candidate);
        Original->DestroyComponent();
        Candidate->DestroyComponent();
    };
    auto* Instance = Candidate->GetPostProcessInstance();
    if (!Instance || !Original->GetPostProcessInstance()) return Fail(TEXT("Missing post-process instances"));
    auto* Sequence = FindFProperty<FObjectPropertyBase>(Instance->GetClass(), TEXT("CSSPerformanceSequence"));
    auto* Enabled = FindFProperty<FBoolProperty>(Instance->GetClass(), TEXT("CSSPerformanceEnabled"));
    auto* Time = FindFProperty<FFloatProperty>(Instance->GetClass(), TEXT("CSSPerformanceTime"));
    auto* Weight = FindFProperty<FFloatProperty>(Instance->GetClass(), TEXT("CSSPerformanceWeight"));
    if (!Sequence || !Enabled || !Time || !Weight) return Fail(TEXT("Missing performance controls"));
    const auto& Reference = Mesh->GetRefSkeleton();
    TSet<int32> BodyBones;
    for (const TCHAR* Name : {TEXT("root"), TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("neck_01"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")})
    {
        const int32 Bone = Reference.FindBoneIndex(Name);
        if (Bone == INDEX_NONE) return Fail(TEXT("Missing audited body bone"));
        BodyBones.Add(Bone);
    }
    struct FCase { const TCHAR* Name; bool Enabled, HasClip; float Fraction, Weight; bool Moving = false; };
    const FCase Cases[] {
        {TEXT("inactive"), false, false, 0.f, 0.f},
        {TEXT("active_start"), true, true, 0.f, 1.f},
        {TEXT("active_seek"), true, true, .37f, 1.f},
        {TEXT("held_end"), true, true, 1.f, 1.f},
        {TEXT("stopped"), false, true, 1.f, 0.f},
        {TEXT("null_clip_guard"), true, false, .5f, 1.f},
        {TEXT("restored"), false, false, 0.f, 0.f},
        {TEXT("zero_weight"), true, true, .37f, 0.f},
        {TEXT("full_clip"), true, true, 0.f, 1.f, true}
    };
    TArray<TSharedPtr<FJsonValue>> Results;
    for (const auto& Test : Cases)
    {
        const bool Active = Test.Enabled && Test.HasClip && Test.Weight > 0.f;
        const float ClipTime = Test.Fraction * PerformanceClip->GetPlayLength();
        Sequence->SetObjectPropertyValue_InContainer(Instance, Test.HasClip ? PerformanceClip : nullptr);
        Time->SetPropertyValue_InContainer(Instance, ClipTime);
        Weight->SetPropertyValue_InContainer(Instance, Test.Weight);
        Enabled->SetPropertyValue_InContainer(Instance, Test.Enabled);
        Original->SetAnimation(Active ? PerformanceClip : BaseClip);
        Original->Stop();
        double BodyPosition = 0, BodyAngle = 0, AllPosition = 0, AllAngle = 0;
        FString WorstBody, WorstAll;
        constexpr float Delta = 1.f / 60.f;
        const int32 Frames = Test.Moving ? FMath::CeilToInt(PerformanceClip->GetPlayLength() / Delta) + 1 : 180;
        for (int32 Frame = 0; Frame < Frames; ++Frame)
        {
            const float PerformanceTime = Test.Moving ? FMath::Min(Frame * Delta, PerformanceClip->GetPlayLength()) : ClipTime;
            const float BaseTime = Test.Moving ? FMath::Fmod(Frame * Delta, BaseClip->GetPlayLength()) : BaseClip->GetPlayLength() * .63f;
            Time->SetPropertyValue_InContainer(Instance, PerformanceTime);
            Original->SetPosition(Active ? PerformanceTime : BaseTime, false);
            Candidate->SetPosition(BaseTime, false);
            for (auto* Component : {Original, Candidate})
            {
                Component->TickAnimation(Delta, true);
                Component->RefreshBoneTransforms();
                Component->CompleteParallelAnimationEvaluation(true);
            }
            const auto A = Original->GetBoneSpaceTransformsView();
            const auto B = Candidate->GetBoneSpaceTransformsView();
            if (A.Num() != Reference.GetNum() || B.Num() != A.Num()) return Fail(TEXT("Incomplete evaluated pose"));
            for (int32 Bone = 0; Bone < Reference.GetRawBoneNum(); ++Bone)
            {
                if (A[Bone].ContainsNaN() || B[Bone].ContainsNaN() || !B[Bone].GetRotation().IsNormalized())
                    return Fail(TEXT("Invalid evaluated transform"));
                // Compare after the transition and initial secondary-motion settling.
                if (!Test.Moving && Frame < 150) continue;
                const double Position = FVector::Distance(A[Bone].GetTranslation(), B[Bone].GetTranslation());
                const double Angle = A[Bone].GetRotation().AngularDistance(B[Bone].GetRotation());
                if (Position > AllPosition) { AllPosition = Position; WorstAll = Reference.GetBoneName(Bone).ToString(); }
                AllAngle = FMath::Max(AllAngle, Angle);
                if (!BodyBones.Contains(Bone)) continue;
                if (Position > BodyPosition) { BodyPosition = Position; WorstBody = Reference.GetBoneName(Bone).ToString(); }
                BodyAngle = FMath::Max(BodyAngle, Angle);
            }
        }
        auto Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("case"), Test.Name);
        Result->SetNumberField(TEXT("frames"), Frames);
        Result->SetBoolField(TEXT("moving"), Test.Moving);
        Result->SetNumberField(TEXT("time"), ClipTime);
        Result->SetNumberField(TEXT("body_position_cm"), BodyPosition);
        Result->SetNumberField(TEXT("body_angle_rad"), BodyAngle);
        Result->SetNumberField(TEXT("all_position_cm"), AllPosition);
        Result->SetNumberField(TEXT("all_angle_rad"), AllAngle);
        Result->SetStringField(TEXT("worst_body"), WorstBody);
        Result->SetStringField(TEXT("worst_all"), WorstAll);
        Result->SetBoolField(TEXT("body_passed"), BodyPosition < .01 && BodyAngle < .001);
        Results.Add(MakeShared<FJsonValueObject>(Result));
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("cases"), Results);
    Report->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Report->SetStringField(TEXT("layer"), Layer->GetPathName());
    Report->SetStringField(TEXT("scope"), TEXT("Compressed component and post-process pose evaluation at 60 Hz. No world cloth-solver or gameplay acceptance."));
    FString Text;
    return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ? Text : FString();
}
