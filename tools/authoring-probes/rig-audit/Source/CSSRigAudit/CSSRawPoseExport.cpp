#include "CSSRigAuditLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimPose.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

FString UCSSRigAuditLibrary::ExportRawPoseKeys(UAnimSequence* Sequence, const FString& OutputFile)
{
    FString File = FPaths::ConvertRelativePathToFull(OutputFile);
    FPaths::CollapseRelativeDirectories(File);
    const FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    if (!PLATFORM_LITTLE_ENDIAN || !IsRunningCommandlet() || !IsInGameThread() || !Sequence ||
        !Sequence->GetDataModel() || Sequence->IsValidAdditive() || !File.StartsWith(Project) ||
        IFileManager::Get().FileExists(*File)) return TEXT("ERROR: invalid raw export request");
    const auto* Model = Sequence->GetDataModel();
    const int32 Keys = Model->GetNumberOfKeys();
    const FFrameRate Rate = Model->GetFrameRate();
    if (Keys < 2 || Rate.Numerator <= 0 || Rate.Denominator <= 0)
        return TEXT("ERROR: invalid raw animation timing");
    TUniquePtr<FArchive> Stream(IFileManager::Get().CreateFileWriter(*File, FILEWRITE_NoReplaceExisting));
    if (!Stream) return TEXT("ERROR: cannot create raw export");
    FAnimPoseEvaluationOptions Options;
    Options.EvaluationType = EAnimDataEvalType::Raw;
    Options.bShouldRetarget = false;
    TArray<FName> Names;
    for (int32 Frame = 0; Frame < Keys; ++Frame)
    {
        FAnimPose Pose;
        UAnimPoseExtensions::GetAnimPoseAtTime(Sequence,
            static_cast<double>(Frame) * Rate.Denominator / Rate.Numerator, Options, Pose);
        if (!Pose.IsValid()) return TEXT("ERROR: invalid raw pose");
        TArray<FName> Current;
        UAnimPoseExtensions::GetBoneNames(Pose, Current);
        if (Frame == 0) Names = Current;
        if (Names.IsEmpty() || Names != Current) return TEXT("ERROR: raw bone mapping changed");
        for (const FName Name : Names)
        {
            const auto& Transform = UAnimPoseExtensions::GetBonePose(Pose, Name, EAnimPoseSpaces::Local);
            if (Transform.ContainsNaN()) return TEXT("ERROR: nonfinite raw pose");
            const auto Q = Transform.GetRotation();
            const auto T = Transform.GetTranslation();
            const auto S = Transform.GetScale3D();
            double Values[]{Q.X, Q.Y, Q.Z, Q.W, T.X, T.Y, T.Z, S.X, S.Y, S.Z};
            Stream->Serialize(Values, sizeof(Values));
        }
        if (Stream->IsError()) return TEXT("ERROR: raw export write failed");
    }
    const int64 ExpectedBytes = static_cast<int64>(Keys) * Names.Num() * 10 * sizeof(double);
    if (Stream->Tell() != ExpectedBytes || !Stream->Close()) return TEXT("ERROR: incomplete raw export");
    TArray<TSharedPtr<FJsonValue>> BoneNames;
    for (const FName Name : Names) BoneNames.Add(MakeShared<FJsonValueString>(Name.ToString()));
    auto Report = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("names"), BoneNames);
    Report->SetNumberField(TEXT("frames"), Keys);
    Report->SetNumberField(TEXT("rate_numerator"), Rate.Numerator);
    Report->SetNumberField(TEXT("rate_denominator"), Rate.Denominator);
    Report->SetNumberField(TEXT("bytes"), ExpectedBytes);
    Report->SetStringField(TEXT("component_type"), TEXT("float64-le"));
    Report->SetStringField(TEXT("layout"), TEXT("frame,bone,qx-qy-qz-qw-tx-ty-tz-sx-sy-sz"));
    Report->SetBoolField(TEXT("should_retarget"), false);
    FString Result;
    if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Result)))
        return TEXT("ERROR: cannot serialize raw export metadata");
    return Result;
}
