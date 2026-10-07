#include "CSSNativePerformanceLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "PreviewScene.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

UBlendSpace* UCSSNativePerformanceLibrary::BindNativeLocomotionBlend(UBlendSpace* Source,
    const TArray<UAnimSequence*>& Replacements, USkeleton* NativeSkeleton, const FString& OutputPackage)
{
    if (!IsRunningCommandlet() || !Source || Source->GetClass() != UBlendSpace::StaticClass() ||
        !Source->GetPathName().StartsWith(TEXT("/Game/CSS/Eve/Anim/BS_")) || !NativeSkeleton ||
        NativeSkeleton->GetPathName() != TEXT("/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.SKEL_Human_Skeleton") ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/Locomotion/NativeHuman/")) ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage)) return nullptr;
    const auto& Before = Source->GetBlendSamples();
    if (Before.IsEmpty() || Before.Num() > 128 || Replacements.Num() != Before.Num()) return nullptr;
    for (int32 I = 0; I < Before.Num(); ++I)
    {
        const auto* Clip = Replacements[I];
        if (!Before[I].Animation || !Clip || Clip->GetSkeleton() != NativeSkeleton ||
            !Clip->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/NativeHuman/")) ||
            Clip->IsValidAdditive() || Clip->bEnableRootMotion || !Clip->Notifies.IsEmpty() ||
            Clip->RateScale != Before[I].Animation->RateScale ||
            Clip->GetSamplingFrameRate() != Before[I].Animation->GetSamplingFrameRate() ||
            !FMath::IsNearlyEqual(Clip->GetPlayLength(), Before[I].Animation->GetPlayLength(), .000001f))
            return nullptr;
    }
    auto* Property = FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(), TEXT("SampleData"));
    auto* Inner = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
    if (!Inner || Inner->Struct != FBlendSample::StaticStruct()) return nullptr;
    auto* Result = DuplicateObject<UBlendSpace>(Source, CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage));
    Result->SetFlags(RF_Public | RF_Standalone);
    Result->SetSkeleton(NativeSkeleton);
    Result->SetPreviewMesh(nullptr);
    auto* Samples = Property->ContainerPtrToValuePtr<TArray<FBlendSample>>(Result);
    if (Samples->Num() != Before.Num()) return nullptr;
    for (int32 I = 0; I < Samples->Num(); ++I) (*Samples)[I].Animation = Replacements[I];
    Result->ValidateSampleData();
    if (Samples->Num() != Before.Num()) return nullptr;
    for (int32 I = 0; I < Samples->Num(); ++I)
        if ((*Samples)[I].Animation != Replacements[I] ||
            (*Samples)[I].SampleValue != Before[I].SampleValue ||
            (*Samples)[I].RateScale != Before[I].RateScale) return nullptr;
    Result->MarkPackageDirty();
    return Result;
}

FString UCSSNativePerformanceLibrary::EvaluateMontageOwnership(USkeletalMesh* ReferenceMesh,
    UAnimSequence* Animation)
{
    if (!IsRunningCommandlet() || !ReferenceMesh || !Animation ||
        !ReferenceMesh->GetPathName().StartsWith(TEXT("/Game/CSSAuthoring/DiagnosticReferences/")) ||
        !Animation->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/NativeHuman/")) ||
        Animation->IsValidAdditive() || Animation->bEnableRootMotion || !Animation->Notifies.IsEmpty())
        return TEXT("ERROR: invalid isolated montage fixture");
    auto Report = MakeShared<FJsonObject>();
    auto Cases = MakeShared<FJsonObject>();
    bool Passed = true;
    auto Check = [&](const TCHAR* Name, bool Value) { Cases->SetBoolField(Name, Value); Passed &= Value; };
    auto* Mesh = DuplicateObject<USkeletalMesh>(ReferenceMesh, GetTransientPackage());
    Mesh->SetSkeleton(Animation->GetSkeleton());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto* Component = NewObject<USkeletalMeshComponent>();
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimInstanceClass(UAnimInstance::StaticClass());
    Component->SetDisablePostProcessBlueprint(true);
    Scene.AddComponent(Component, FTransform::Identity);
    Component->InitAnim(true);
    auto* Instance = Component->GetAnimInstance();
    if (!Instance) return TEXT("ERROR: no preview animation instance");
    auto Tick = [&]() {
        Component->TickAnimation(.25f, false);
        Component->RefreshBoneTransforms();
        Component->FinalizeBoneTransform();
    };
    FMontageBlendSettings Blend(.18f);
    Blend.Blend.BlendOption = EAlphaBlendOption::HermiteCubic;
    auto Create = [&]() {
        auto* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage_WithBlendSettings(
            Animation, TEXT("DefaultSlot"), Blend, Blend, 1.f, 1, -1.f);
        if (Montage) Montage->bEnableAutoBlendOut = false;
        return Montage;
    };
    auto* Owned = Create();
    auto* Other = Create();
    if (!Owned || !Other || Owned == Other) return TEXT("ERROR: montage creation failed");
    Check(TEXT("start"), Instance->Montage_Play(Owned, 0.f, EMontagePlayReturnType::MontageLength, .5f, false) > 0.f);
    Check(TEXT("owns_active"), Instance->GetCurrentActiveMontage() == Owned && Instance->Montage_IsActive(Owned));
    Tick();
    Check(TEXT("blend_in"), Instance->GetActiveInstanceForMontage(Owned) &&
        FMath::IsNearlyEqual(Instance->GetActiveInstanceForMontage(Owned)->GetWeight(), 1.f));
    Check(TEXT("zero_rate_holds"), FMath::IsNearlyEqual(Instance->Montage_GetPosition(Owned), .5f));
    Instance->Montage_SetPosition(Owned, Animation->GetPlayLength() * .8f);
    Tick();
    Check(TEXT("seek_holds"), FMath::IsNearlyEqual(Instance->Montage_GetPosition(Owned), Animation->GetPlayLength() * .8f));
    Instance->Montage_SetPosition(Owned, Animation->GetPlayLength());
    Tick();
    Check(TEXT("endpoint_holds"), Instance->Montage_IsActive(Owned) &&
        FMath::IsNearlyEqual(Instance->Montage_GetPosition(Owned), Animation->GetPlayLength()));
    Check(TEXT("other_starts"), Instance->Montage_Play(Other, 0.f, EMontagePlayReturnType::MontageLength, 1.f, false) > 0.f);
    Check(TEXT("no_stop_all"), Instance->Montage_IsActive(Owned) && Instance->Montage_IsActive(Other));
    Check(TEXT("competition_visible"), Instance->GetCurrentActiveMontage() == Other);
    Instance->Montage_Stop(0.f, Owned);
    Tick();
    Check(TEXT("owned_stop_only"), !Instance->Montage_IsActive(Owned) && Instance->Montage_IsActive(Other) &&
        Instance->GetCurrentActiveMontage() == Other);
    Instance->Montage_Stop(0.f, Other);
    Tick();
    Check(TEXT("cleaned"), !Instance->IsAnyMontagePlaying());
    Check(TEXT("replay"), Instance->Montage_Play(Owned, 0.f, EMontagePlayReturnType::MontageLength, 0.f, false) > 0.f);
    Tick();
    Instance->Montage_Stop(.18f, Owned);
    Tick();
    Check(TEXT("blend_out_cleaned"), !Instance->IsAnyMontagePlaying());
    // The post-process setter retains the main instance pointer but destroys its
    // montages. This was the facial-idle interruption regression.
    Check(TEXT("handoff_gameplay_started"), Instance->Montage_Play(Other, 0.f,
        EMontagePlayReturnType::MontageLength, 1.f, false) > 0.f);
    Tick();
    Component->SetOverridePostProcessAnimBP(nullptr, true);
    Check(TEXT("old_restore_kept_pointer_but_lost_montage"),
        Component->GetAnimInstance() == Instance && !Instance->Montage_IsActive(Other));
    Instance->Montage_Play(Other, 0.f, EMontagePlayReturnType::MontageLength, 1.f, false);
    Tick();
    bool Restored = false;
    auto TryRestore = [&]() {
        if (Instance->IsAnyMontagePlaying()) return false;
        Component->SetOverridePostProcessAnimBP(nullptr, true);
        Restored = true;
        return true;
    };
    Check(TEXT("deferred_restore_preserves_gameplay"), !TryRestore() &&
        Instance->Montage_IsActive(Other) && FMath::IsNearlyEqual(Instance->Montage_GetPosition(Other), 1.f));
    Instance->Montage_Stop(.5f, Other);
    Check(TEXT("deferred_restore_preserves_blend_out"), !TryRestore() && !Restored);
    Tick();Tick();Tick();
    Check(TEXT("restore_after_gameplay_finished"), TryRestore() && Restored &&
        Component->GetAnimInstance() == Instance && !Instance->IsAnyMontagePlaying());
    auto Signatures = MakeShared<FJsonObject>();
    for (const auto& Pair : {TPair<UClass*,FName>(UAnimMontage::StaticClass(), TEXT("CreateSlotAnimationAsDynamicMontage_WithBlendSettings")),
        {UAnimInstance::StaticClass(), TEXT("Montage_Play")}, {UAnimInstance::StaticClass(), TEXT("Montage_Stop")},
        {UAnimInstance::StaticClass(), TEXT("Montage_SetPosition")}, {UAnimInstance::StaticClass(), TEXT("Montage_IsActive")}})
    {
        auto* Function = Pair.Key->FindFunctionByName(Pair.Value);
        if (!Function) return TEXT("ERROR: reflected montage function missing");
        auto Fields = MakeShared<FJsonObject>();
        for (TFieldIterator<FProperty> It(Function); It; ++It)
            if (It->HasAnyPropertyFlags(CPF_Parm)) Fields->SetNumberField(It->GetName(), It->GetSize());
        Signatures->SetObjectField(Pair.Value.ToString(), Fields);
    }
    Report->SetBoolField(TEXT("passed"), Passed);
    Report->SetObjectField(TEXT("cases"), Cases);
    Report->SetObjectField(TEXT("signatures"), Signatures);
    Report->SetStringField(TEXT("scope"), TEXT("Isolated engine montage lifecycle only. No game graph, visible mesh, cloth or live acceptance."));
    Scene.RemoveComponent(Component);
    Component->DestroyComponent();
    FString Result;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Result));
    return Result;
}
