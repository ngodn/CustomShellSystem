#include "CSSAnimationLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimPose.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/RetargetOps/SpeedPlantingOp.h"
#include "UObject/Package.h"

UAnimSequence* UCSSAnimationLibrary::RetargetClip(USkeletalMesh* SourceMesh,
    USkeletalMesh* TargetMesh, UAnimSequence* Source, UIKRetargeter* Retargeter,
    const FString& OutputPackage)
{
    auto Fail = [](const TCHAR* Message) -> UAnimSequence* {
        UE_LOG(LogTemp, Error, TEXT("CSS RetargetClip: %s"), Message);
        return nullptr;
    };
    if (!IsRunningCommandlet() || !SourceMesh || !TargetMesh || !Source || !Retargeter ||
        Source->GetSkeleton() != SourceMesh->GetSkeleton() || !TargetMesh->GetSkeleton() ||
        Source->IsValidAdditive() || Source->RateScale != 1.f || Source->GetNumberOfSampledKeys() < 2 ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/")) || OutputPackage.Len() > 100 ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage))
        return Fail(TEXT("Invalid source, output or execution context"));
    const IAnimationDataModel* Model = Source->GetDataModel();
    if (!Model || !Model->GetCurveData().FloatCurves.IsEmpty() ||
        !Model->GetCurveData().TransformCurves.IsEmpty() || Model->GetNumberOfAttributes() != 0 ||
        !Source->Notifies.IsEmpty() ||
        !Retargeter->GetAllRetargetOpsOfType<FIKRetargetSpeedPlantingOp>().IsEmpty())
        return Fail(TEXT("This authoring probe accepts bone-only clips without speed planting"));

    // UE 5.6.1 batch conversion uses this processor, but its UI completion path
    // dereferences Slate in a commandlet. Keep the numerical path headless.
    FMemMark Memory(FMemStack::Get());
    FRetargetProfile Profile;
    Profile.FillProfileWithAssetSettings(Retargeter);
    FIKRetargetProcessor Processor;
    Processor.Initialize(SourceMesh, TargetMesh, Retargeter, Profile);
    if (!Processor.IsInitialized()) return Fail(TEXT("Processor did not initialize"));
    const FRetargetSkeleton& SourceRig = Processor.GetSkeleton(ERetargetSourceOrTarget::Source);
    const FRetargetSkeleton& TargetRig = Processor.GetSkeleton(ERetargetSourceOrTarget::Target);
    const int32 Frames = Source->GetNumberOfSampledKeys();
    TArray<FRawAnimSequenceTrack> Tracks;
    Tracks.SetNum(TargetRig.BoneNames.Num());
    for (auto& Track : Tracks)
    {
        Track.PosKeys.SetNum(Frames);
        Track.RotKeys.SetNum(Frames);
        Track.ScaleKeys.SetNum(Frames);
    }
    TArray<FTransform> SourcePose;
    SourcePose.SetNum(SourceRig.BoneNames.Num());
    FAnimPoseEvaluationOptions Options;
    Options.EvaluationType = EAnimDataEvalType::Raw;
    Options.OptionalSkeletalMesh = SourceMesh;
    Options.bExtractRootMotion = false;
    Options.bIncorporateRootMotionIntoPose = true;
    Processor.OnPlaybackReset();
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        FAnimPose Pose;
        UAnimPoseExtensions::GetAnimPoseAtFrame(Source, Frame, Options, Pose);
        if (!Pose.IsValid()) return Fail(TEXT("Invalid evaluated source pose"));
        for (int32 Bone = 0; Bone < SourcePose.Num(); ++Bone)
        {
            SourcePose[Bone] = UAnimPoseExtensions::GetBonePose(Pose,
                SourceRig.BoneNames[Bone], EAnimPoseSpaces::World);
            if (SourcePose[Bone].ContainsNaN()) return Fail(TEXT("Non-finite source pose"));
            SourcePose[Bone].SetScale3D(FVector::OneVector);
        }
        const float Delta = Frame ? Source->GetTimeAtFrame(Frame) - Source->GetTimeAtFrame(Frame - 1) : 0.f;
        Processor.ScaleSourcePose(SourcePose);
        const TArray<FTransform>& Global = Processor.RunRetargeter(SourcePose, Profile, Delta);
        TArray<FTransform> Local = Global;
        TargetRig.UpdateLocalTransformsBelowBone(0, Local, Global);
        if (Local.Num() != Tracks.Num()) return Fail(TEXT("Unexpected target bone count"));
        for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
        {
            const FTransform& Transform = Local[Bone];
            if (Transform.ContainsNaN() || !Transform.GetRotation().IsNormalized() ||
                !Transform.GetScale3D().Equals(FVector::OneVector, 0.001))
                return Fail(TEXT("Invalid target transform or changed bone scale"));
            Tracks[Bone].PosKeys[Frame] = FVector3f(Transform.GetTranslation());
            Tracks[Bone].RotKeys[Frame] = FQuat4f(Transform.GetRotation());
            Tracks[Bone].ScaleKeys[Frame] = FVector3f(Transform.GetScale3D());
        }
    }

    auto* Result = NewObject<UAnimSequence>(CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage), RF_Public | RF_Standalone);
    Result->SetSkeleton(TargetMesh->GetSkeleton());
    Result->SetPreviewMesh(TargetMesh);
    // Bind rotations to the fitted mesh, not the shared skeleton's reference pose.
    Result->SetRetargetSourceAsset(TargetMesh);
    Result->UpdateRetargetSourceAssetData();
    auto& Edit = Result->GetController();
    Edit.InitializeModel();
    Edit.OpenBracket(FText::FromString(TEXT("Headless CSS animation retarget")), false);
    ON_SCOPE_EXIT { Edit.CloseBracket(false); };
    Edit.SetFrameRate(Model->GetFrameRate(), false);
    Edit.SetNumberOfFrames(FFrameNumber(Frames - 1), false);
    if (Result->GetDataModel()->GetFrameRate() != Model->GetFrameRate() ||
        Result->GetDataModel()->GetNumberOfKeys() != Frames)
        return Fail(TEXT("Could not set sequence timing"));
    for (int32 Bone = 0; Bone < Tracks.Num(); ++Bone)
    {
        const auto& Track = Tracks[Bone];
        if (!Edit.AddBoneCurve(TargetRig.BoneNames[Bone], false) ||
            !Edit.SetBoneTrackKeys(TargetRig.BoneNames[Bone], Track.PosKeys,
                Track.RotKeys, Track.ScaleKeys, false))
            return Fail(TEXT("Could not write target bone keys"));
    }
    Edit.NotifyPopulated();
    Result->MarkPackageDirty();
    return Result;
}
