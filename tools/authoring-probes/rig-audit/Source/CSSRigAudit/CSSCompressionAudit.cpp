#include "CSSRigAuditLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonSerializer.h"

FString UCSSRigAuditLibrary::CompareCompressedPoses(UAnimSequence* Source,
    UAnimSequence* Compressed, USkeletalMesh* Mesh)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Source || !Compressed || !Mesh ||
        !Source->GetDataModel() || Source->IsValidAdditive() || Compressed->IsValidAdditive() ||
        Source->GetSkeleton() != Compressed->GetSkeleton() || !Compressed->GetSkeleton() ||
        !Compressed->IsCompressedDataValid() ||
        Compressed->GetSkeletonVirtualBoneGuid() != Compressed->GetSkeleton()->GetVirtualBoneGuid())
        return TEXT("ERROR: invalid animation inputs or unavailable compression");
    const auto* ForceRaw = IConsoleManager::Get().FindConsoleVariable(TEXT("a.ForceEvalRawData"));
    if (!ForceRaw || ForceRaw->GetInt() != 0) return TEXT("ERROR: engine forces raw evaluation");
    const int32 Keys = Source->GetDataModel()->GetNumberOfKeys();
    const FFrameRate Rate = Source->GetDataModel()->GetFrameRate();
    if (Keys < 2 || Rate.Numerator <= 0 || Rate.Denominator <= 0 ||
        Compressed->GetNumberOfSampledKeys() != Keys || Compressed->GetSamplingFrameRate() != Rate)
        return TEXT("ERROR: animation timing mismatch");
    FAnimPoseEvaluationOptions RawOptions, CompressedOptions;
    RawOptions.OptionalSkeletalMesh = Mesh;
    RawOptions.EvaluationType = EAnimDataEvalType::Raw;
    CompressedOptions = RawOptions;
    CompressedOptions.EvaluationType = EAnimDataEvalType::Compressed;
    double Maxima[3]{};
    int32 WorstFrames[3]{};
    FName WorstBones[3];
    TArray<FName> Names;
    for (int32 Frame = 0; Frame < Keys; ++Frame)
    {
        FAnimPose RawPose, CompressedPose;
        const double Time = static_cast<double>(Frame) * Rate.Denominator / Rate.Numerator;
        UAnimPoseExtensions::GetAnimPoseAtTime(Source, Time, RawOptions, RawPose);
        UAnimPoseExtensions::GetAnimPoseAtTime(Compressed, Time, CompressedOptions, CompressedPose);
        if (!RawPose.IsValid() || !CompressedPose.IsValid()) return TEXT("ERROR: invalid evaluated pose");
        TArray<FName> RawNames, CompressedNames;
        UAnimPoseExtensions::GetBoneNames(RawPose, RawNames);
        UAnimPoseExtensions::GetBoneNames(CompressedPose, CompressedNames);
        if (Frame == 0) Names = RawNames;
        if (Names.Num() != Mesh->GetRefSkeleton().GetNum() || Names != RawNames || Names != CompressedNames)
            return TEXT("ERROR: evaluated bone mapping changed");
        for (const FName Name : Names)
        {
            const auto& A = UAnimPoseExtensions::GetBonePose(RawPose, Name, EAnimPoseSpaces::Local);
            const auto& B = UAnimPoseExtensions::GetBonePose(CompressedPose, Name, EAnimPoseSpaces::Local);
            if (A.ContainsNaN() || B.ContainsNaN()) return TEXT("ERROR: nonfinite evaluated pose");
            const FQuat QA = A.GetRotation(), QB = B.GetRotation();
            const double Values[]{FVector::Distance(A.GetTranslation(), B.GetTranslation()),
                FMath::Min((QA - QB).Size(), (QA + QB).Size()),
                FVector::Distance(A.GetScale3D(), B.GetScale3D())};
            for (int32 Index = 0; Index < 3; ++Index)
            {
                if (!FMath::IsFinite(Values[Index])) return TEXT("ERROR: nonfinite pose difference");
                if (Values[Index] > Maxima[Index])
                {
                    Maxima[Index] = Values[Index];
                    WorstFrames[Index] = Frame;
                    WorstBones[Index] = Name;
                }
            }
        }
    }
    const TCHAR* Fields[]{TEXT("position_cm"), TEXT("quaternion_distance"), TEXT("scale")};
    auto Errors = MakeShared<FJsonObject>();
    auto Worst = MakeShared<FJsonObject>();
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Errors->SetNumberField(Fields[Index], Maxima[Index]);
        if (Maxima[Index] > 0)
        {
            auto At = MakeShared<FJsonObject>();
            At->SetNumberField(TEXT("frame"), WorstFrames[Index]);
            At->SetStringField(TEXT("bone"), WorstBones[Index].ToString());
            Worst->SetObjectField(Fields[Index], At);
        }
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetNumberField(TEXT("frames"), Keys);
    Report->SetNumberField(TEXT("bones"), Names.Num());
    Report->SetBoolField(TEXT("compressed_passed"), Maxima[0] < .01 && Maxima[1] < .001 && Maxima[2] < .00001);
    Report->SetObjectField(TEXT("errors"), Errors);
    Report->SetObjectField(TEXT("worst"), Worst);
    FString Result;
    if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Result)))
        return TEXT("ERROR: cannot serialize compression comparison");
    return Result;
}
