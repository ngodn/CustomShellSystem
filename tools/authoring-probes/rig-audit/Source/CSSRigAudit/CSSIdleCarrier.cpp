#include "CSSRigAuditLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

UBlendSpace* UCSSRigAuditLibrary::CloneConstantIdleCarrier(UBlendSpace* Source,
    UAnimSequence* Animation, const FString& OutputPackage)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Source || !Animation ||
        Source->GetClass() != UBlendSpace::StaticClass() ||
        !Animation->GetSkeleton() || Source->GetSkeleton() != Animation->GetSkeleton() ||
        Animation->IsValidAdditive() || Animation->bEnableRootMotion ||
        !Animation->Notifies.IsEmpty() || Animation->RateScale != 1.f ||
        Animation->GetPlayLength() <= 0 ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/")) ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage))
        return nullptr;
    const auto& Before = Source->GetBlendSamples();
    if (Before.IsEmpty() || !Before[0].Animation) return nullptr;
    for (const auto& Sample : Before)
        if (Sample.Animation != Before[0].Animation || Sample.RateScale != 1.f)
            return nullptr;
    auto* Property = FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(), TEXT("SampleData"));
    auto* Inner = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
    if (!Inner || Inner->Struct != FBlendSample::StaticStruct()) return nullptr;
    auto* Result = DuplicateObject<UBlendSpace>(Source, CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage));
    Result->SetFlags(RF_Public | RF_Standalone);
    Result->SetPreviewMesh(nullptr);
    auto& Samples = *Property->ContainerPtrToValuePtr<TArray<FBlendSample>>(Result);
    for (auto& Sample : Samples) Sample.Animation = Animation;
    Result->ValidateSampleData();
    Result->ResampleData();
    if (Samples.Num() != Before.Num() || Result->GetBlendSpaceData().IsEmpty()) return nullptr;
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
        if (Samples[Index].Animation != Animation || Samples[Index].RateScale != 1.f ||
            Samples[Index].SampleValue != Before[Index].SampleValue) return nullptr;
    Result->MarkPackageDirty();
    return Result;
}
