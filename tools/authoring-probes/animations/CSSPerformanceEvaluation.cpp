#include "CSSPerformanceLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "ClothingSystemRuntimeTypes.h"
#include "HAL/IConsoleManager.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeExit.h"
#include "PreviewScene.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

namespace
{
FString AuditIdleRepeat(USkeletalMesh* Mesh, UAnimBlueprint* Source, UAnimSequence* Base, UAnimSequence* Clip)
{
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false)
        .SetCreatePhysicsScene(true).ShouldSimulatePhysics(true));
    auto* Component = NewObject<USkeletalMeshComponent>();
    Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->bWaitForParallelClothTask = false;
    auto* Wait = IConsoleManager::Get().FindConsoleVariable(TEXT("p.ClothPhysics.WaitForParallelClothTask"));
    if (!Wait) return {};
    const int32 PreviousWait = Wait->GetInt();
    Wait->Set(0, ECVF_SetByCode);
    ON_SCOPE_EXIT {
        Component->CompleteParallelAnimationEvaluation(true);
        Component->WaitForExistingParallelClothSimulation_GameThread();
        Scene.RemoveComponent(Component);
        Component->DestroyComponent();
        Wait->Set(PreviousWait, ECVF_SetByCode);
    };
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Source->GeneratedClass.Get()), true);
    Scene.AddComponent(Component, FTransform::Identity);
    Component->InitAnim(true);
    Component->SetAnimation(Base);
    Component->Stop();
    auto* Instance = Component->GetPostProcessInstance();
    auto* Sequence = Instance ? FindFProperty<FObjectPropertyBase>(Instance->GetClass(), TEXT("CSSIdleSequence")) : nullptr;
    auto* Enabled = Instance ? FindFProperty<FBoolProperty>(Instance->GetClass(), TEXT("CSSIdleEnabled")) : nullptr;
    auto* Player = Instance ? FindFProperty<FStructProperty>(Instance->GetClass(), TEXT("AnimGraphNode_SequencePlayer")) : nullptr;
    if (!Sequence || !Enabled || !Player || Player->Struct != FAnimNode_SequencePlayer::StaticStruct()) return {};
    auto* Node = Player->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(Instance);
    Sequence->SetObjectPropertyValue_InContainer(Instance, Clip);
    Enabled->SetPropertyValue_InContainer(Instance, true);
    constexpr float Delta = 1.f / 60.f;
    const int32 Frames = FMath::CeilToInt(Clip->GetPlayLength() / Delta) * 3 + 10;
    TArray<TSharedPtr<FJsonValue>> Samples;
    float PreviousTime = -1.f;
    int32 Wraps = 0, ClothFrames = 0, InvalidFrame = INDEX_NONE;
    const bool ResetAtWrap = FPlatformMisc::GetEnvironmentVariable(TEXT("CSS_IDLE_REPEAT_RESET")) == TEXT("1");
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Component->SetPosition(FMath::Fmod(Frame * Delta, Base->GetPlayLength()), false);
        Component->TickAnimation(Delta, true);
        Component->RefreshBoneTransforms();
        Component->CompleteParallelAnimationEvaluation(true);
        const float Time = Node->GetAccumulatedTime();
        const bool Wrapped = PreviousTime >= 0.f && Time < PreviousTime;
        if (Wrapped) ++Wraps;
        PreviousTime = Time;
        if (Wrapped && ResetAtWrap) Component->ForceClothNextUpdateTeleportAndReset();
        // This commandlet waits explicitly after dispatch; no tick-graph completion handle is needed.
        Component->TickClothing(Delta, Component->PrimaryComponentTick);
        Component->WaitForExistingParallelClothSimulation_GameThread();
        const auto& Cloth = Component->GetCurrentClothingData_AnyThread();
        int32 Particles = 0;
        double MaxParticleDistance = 0.;
        bool Finite = FMath::IsFinite(Time);
        for (const auto& Transform : Component->GetComponentSpaceTransforms())
            Finite = Finite && !Transform.ContainsNaN();
        for (const auto& Entry : Cloth)
        {
            Finite = Finite && !Entry.Value.Transform.ContainsNaN();
            for (const auto& Position : Entry.Value.Positions)
            {
                ++Particles;
                Finite = Finite && !Position.ContainsNaN();
                if (Finite) MaxParticleDistance = FMath::Max(MaxParticleDistance,
                    Entry.Value.Transform.TransformPosition(FVector(Position)).Size());
            }
            for (const auto& Normal : Entry.Value.Normals) Finite = Finite && !Normal.ContainsNaN();
        }
        if (Particles) ++ClothFrames;
        Component->UpdateBounds();
        const auto Bounds = Component->Bounds;
        Finite = Finite && !Bounds.Origin.ContainsNaN() && !Bounds.BoxExtent.ContainsNaN() && FMath::IsFinite(Bounds.SphereRadius);
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("frame"), Frame);
        Row->SetBoolField(TEXT("finite"), Finite);
        Row->SetBoolField(TEXT("wrapped"), Wrapped);
        Row->SetNumberField(TEXT("cloths"), Cloth.Num());
        Row->SetNumberField(TEXT("particles"), Particles);
        if (Finite)
        {
            Row->SetNumberField(TEXT("clip_time"), Time);
            Row->SetNumberField(TEXT("sphere_radius_cm"), Bounds.SphereRadius);
            Row->SetNumberField(TEXT("max_particle_distance_cm"), MaxParticleDistance);
        }
        Samples.Add(MakeShared<FJsonValueObject>(Row));
        if (!Finite) { InvalidFrame = Frame; break; }
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Report->SetStringField(TEXT("clip"), Clip->GetPathName());
    Report->SetNumberField(TEXT("expected_frames"), Frames);
    Report->SetNumberField(TEXT("cloth_frames"), ClothFrames);
    Report->SetNumberField(TEXT("wraps"), Wraps);
    Report->SetNumberField(TEXT("invalid_frame"), InvalidFrame);
    Report->SetBoolField(TEXT("looping"), Node->IsLooping());
    Report->SetBoolField(TEXT("reset_cloth_at_wrap"), ResetAtWrap);
    Report->SetArrayField(TEXT("samples"), Samples);
    Report->SetStringField(TEXT("scope"), TEXT("Original outfit idle graph plus isolated cloth at 60 Hz; no game map, GPU rendering or live customization."));
    FString Text;
    return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ? Text : FString();
}
}

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
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("CSS_IDLE_REPEAT_AUDIT")) == TEXT("1"))
        return AuditIdleRepeat(Mesh, Source, BaseClip, PerformanceClip);
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
    const bool CaptureFeet = FPlatformMisc::GetEnvironmentVariable(TEXT("CSS_PERFORMANCE_CAPTURE_FEET")) == TEXT("1");
    TArray<int32> ContactBones;
    TArray<TSharedPtr<FJsonValue>> ContactNames, ContactFrames;
    if (CaptureFeet)
        for (const TCHAR* Name : {TEXT("calf_l"), TEXT("foot_l"), TEXT("calf_r"), TEXT("foot_r")})
        {
            const int32 Bone = Reference.FindBoneIndex(Name);
            if (Bone == INDEX_NONE) return Fail(TEXT("Missing contact bone"));
            ContactBones.Add(Bone);
            ContactNames.Add(MakeShared<FJsonValueString>(Name));
        }
    TSet<int32> BodyBones;
    TSet<int32> FingerBones;
    for (const TCHAR* Name : {TEXT("root"), TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("neck_01"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")})
    {
        const int32 Bone = Reference.FindBoneIndex(Name);
        if (Bone == INDEX_NONE) return Fail(TEXT("Missing audited body bone"));
        BodyBones.Add(Bone);
    }
    for (int32 Bone = 0; Bone < Reference.GetRawBoneNum(); ++Bone)
    {
        const FString Name = Reference.GetBoneName(Bone).ToString();
        if (!Name.EndsWith(TEXT("_l")) && !Name.EndsWith(TEXT("_r"))) continue;
        for (const TCHAR* Prefix : {TEXT("thumb_"), TEXT("index_"), TEXT("middle_"), TEXT("ring_"), TEXT("pinky_")})
            if (Name.StartsWith(Prefix)) { FingerBones.Add(Bone); break; }
    }
    if (FingerBones.Num() != 38) return Fail(TEXT("Missing audited finger/metacarpal bones"));
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
        double BodyPosition = 0, BodyAngle = 0, BodyScale = 0, AllPosition = 0, AllAngle = 0;
        double FingerPosition = 0, FingerAngle = 0, FingerScale = 0;
        FString WorstBody, WorstAll, WorstFinger;
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
            if (CaptureFeet && Test.Moving)
            {
                const auto& Components = Candidate->GetComponentSpaceTransforms();
                if (Components.Num() != Reference.GetNum()) return Fail(TEXT("Incomplete contact pose"));
                TArray<TSharedPtr<FJsonValue>> Transforms;
                for (int32 Bone : ContactBones)
                {
                    const auto& Transform = Components[Bone];
                    if (Transform.ContainsNaN() || !Transform.GetRotation().IsNormalized())
                        return Fail(TEXT("Invalid contact pose"));
                    const auto Q = Transform.GetRotation();
                    const auto P = Transform.GetTranslation();
                    const auto S = Transform.GetScale3D();
                    TArray<TSharedPtr<FJsonValue>> Values;
                    for (double Value : {Q.X, Q.Y, Q.Z, Q.W, P.X, P.Y, P.Z, S.X, S.Y, S.Z})
                        Values.Add(MakeShared<FJsonValueNumber>(Value));
                    Transforms.Add(MakeShared<FJsonValueArray>(Values));
                }
                auto Row = MakeShared<FJsonObject>();
                Row->SetNumberField(TEXT("time"), PerformanceTime);
                Row->SetArrayField(TEXT("transforms"), Transforms);
                ContactFrames.Add(MakeShared<FJsonValueObject>(Row));
            }
            for (int32 Bone = 0; Bone < Reference.GetRawBoneNum(); ++Bone)
            {
                if (A[Bone].ContainsNaN() || B[Bone].ContainsNaN() || !B[Bone].GetRotation().IsNormalized())
                    return Fail(TEXT("Invalid evaluated transform"));
                // Compare after the transition and initial secondary-motion settling.
                if (!Test.Moving && Frame < 150) continue;
                const double Position = FVector::Distance(A[Bone].GetTranslation(), B[Bone].GetTranslation());
                const double Angle = A[Bone].GetRotation().AngularDistance(B[Bone].GetRotation());
                const double Scale = FVector::Distance(A[Bone].GetScale3D(), B[Bone].GetScale3D());
                if (Position > AllPosition) { AllPosition = Position; WorstAll = Reference.GetBoneName(Bone).ToString(); }
                AllAngle = FMath::Max(AllAngle, Angle);
                if (FingerBones.Contains(Bone))
                {
                    if (Position > FingerPosition) { FingerPosition = Position; WorstFinger = Reference.GetBoneName(Bone).ToString(); }
                    FingerAngle = FMath::Max(FingerAngle, Angle);
                    FingerScale = FMath::Max(FingerScale, Scale);
                }
                if (!BodyBones.Contains(Bone)) continue;
                if (Position > BodyPosition) { BodyPosition = Position; WorstBody = Reference.GetBoneName(Bone).ToString(); }
                BodyAngle = FMath::Max(BodyAngle, Angle);
                BodyScale = FMath::Max(BodyScale, Scale);
            }
        }
        auto Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("case"), Test.Name);
        Result->SetNumberField(TEXT("frames"), Frames);
        Result->SetBoolField(TEXT("moving"), Test.Moving);
        Result->SetNumberField(TEXT("time"), ClipTime);
        Result->SetNumberField(TEXT("body_position_cm"), BodyPosition);
        Result->SetNumberField(TEXT("body_angle_rad"), BodyAngle);
        Result->SetNumberField(TEXT("body_scale_error"), BodyScale);
        Result->SetNumberField(TEXT("finger_position_cm"), FingerPosition);
        Result->SetNumberField(TEXT("finger_angle_rad"), FingerAngle);
        Result->SetNumberField(TEXT("finger_scale_error"), FingerScale);
        Result->SetStringField(TEXT("worst_finger"), WorstFinger);
        Result->SetBoolField(TEXT("fingers_passed"), FingerPosition < .01 && FingerAngle < .001 && FingerScale < .001);
        Result->SetNumberField(TEXT("all_position_cm"), AllPosition);
        Result->SetNumberField(TEXT("all_angle_rad"), AllAngle);
        Result->SetStringField(TEXT("worst_body"), WorstBody);
        Result->SetStringField(TEXT("worst_all"), WorstAll);
        Result->SetBoolField(TEXT("body_passed"), BodyPosition < .01 && BodyAngle < .001 && BodyScale < .001);
        Results.Add(MakeShared<FJsonValueObject>(Result));
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("cases"), Results);
    Report->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Report->SetStringField(TEXT("layer"), Layer->GetPathName());
    Report->SetStringField(TEXT("performance"), PerformanceClip->GetPathName());
    Report->SetNumberField(TEXT("finger_bones_checked"), FingerBones.Num());
    if (CaptureFeet)
    {
        Report->SetArrayField(TEXT("contact_bones"), ContactNames);
        Report->SetArrayField(TEXT("contact_frames"), ContactFrames);
        Report->SetStringField(TEXT("contact_space"), TEXT("component; qx,qy,qz,qw,tx,ty,tz,sx,sy,sz; identity placement"));
    }
    Report->SetStringField(TEXT("scope"), TEXT("Compressed component and post-process pose evaluation at 60 Hz. No world cloth-solver or gameplay acceptance."));
    FString Text;
    return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ? Text : FString();
}
