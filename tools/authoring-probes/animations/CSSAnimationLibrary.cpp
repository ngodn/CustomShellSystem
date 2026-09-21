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
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/MorphTarget.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AnimNode_ControlRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "PreviewScene.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

UAnimSequence* UCSSAnimationLibrary::ShiftLoop(UAnimSequence* Source,
    int32 StartFrame, const FString& OutputPackage)
{
    if (!IsRunningCommandlet() || !Source || !Source->GetSkeleton() ||
        !Source->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/")) ||
        Source->IsValidAdditive() || Source->RateScale != 1.f || Source->bEnableRootMotion ||
        !Source->Notifies.IsEmpty() || !Source->AuthoredSyncMarkers.IsEmpty() ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/RT_P1_")) || OutputPackage.Len() > 100 ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage)) return nullptr;
    const IAnimationDataModel* Model = Source->GetDataModel();
    if (!Model || !Model->GetCurveData().FloatCurves.IsEmpty() ||
        !Model->GetCurveData().TransformCurves.IsEmpty() || Model->GetNumberOfAttributes() != 0) return nullptr;
    const int32 Keys = Model->GetNumberOfKeys();
    if (Keys < 3 || Keys > 601 || StartFrame < 0 || StartFrame >= Keys - 1) return nullptr;
    TArray<FName> Names; Model->GetBoneTrackNames(Names);
    if (Names.IsEmpty() || Names.Num() > 1024) return nullptr;
    TArray<TArray<FTransform>> Tracks;
    for (const FName Name : Names)
    {
        auto& Track = Tracks.AddDefaulted_GetRef();
        Model->GetBoneTrackTransforms(Name, Track);
        if (Track.Num() != Keys) return nullptr;
        for (const auto& T : Track)
            if (T.ContainsNaN() || !T.GetRotation().IsNormalized()) return nullptr;
        // Only close an already near-periodic loop; never hide a bad clip cut.
        if (FVector::Distance(Track[0].GetTranslation(), Track.Last().GetTranslation()) > .01 ||
            Track[0].GetRotation().AngularDistance(Track.Last().GetRotation()) > FMath::DegreesToRadians(.1) ||
            !Track[0].GetScale3D().Equals(Track.Last().GetScale3D(), .0001)) return nullptr;
    }
    auto* Result = DuplicateObject<UAnimSequence>(Source, CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage));
    if (!Result || Result->GetDataModel() == Model) return nullptr;
    Result->SetFlags(RF_Public | RF_Standalone);
    auto& Edit = Result->GetController();
    Edit.OpenBracket(FText::FromString(TEXT("Align CSS loop start frames")), false);
    ON_SCOPE_EXIT { Edit.CloseBracket(false); };
    for (int32 Bone = 0; Bone < Names.Num(); ++Bone)
    {
        TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
        for (int32 Frame = 0; Frame < Keys; ++Frame)
        {
            const int32 Index = StartFrame ? (Frame + StartFrame) % (Keys - 1) : Frame;
            const auto& T = Tracks[Bone][Index];
            Positions.Add(FVector3f(T.GetTranslation())); Rotations.Add(FQuat4f(T.GetRotation()));
            Scales.Add(FVector3f(T.GetScale3D()));
        }
        if (!Edit.SetBoneTrackKeys(Names[Bone], Positions, Rotations, Scales, false)) return nullptr;
    }
    Result->MarkPackageDirty(); return Result;
}

UAnimSequence* UCSSAnimationLibrary::RetargetClip(USkeletalMesh* SourceMesh,
    USkeletalMesh* TargetMesh, UAnimSequence* Source, UIKRetargeter* Retargeter,
    const FString& OutputPackage, bool PreserveUnmappedAttachments)
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
    const auto& TargetRig = Processor.GetTargetSkeleton();
    TArray<int32> Attachments;
    if (PreserveUnmappedAttachments)
    {
        // Mirror the processor's mask collection through exported APIs;
        // FTargetSkeleton's direct mask accessors are not exported in 5.6.
        TSet<int32> Retargeted;
        for (const auto* Op : Processor.GetRetargetOpsByType(FIKRetargetOpBase::StaticStruct()))
            if (Op->IsEnabled() && Op->IsInitialized()) Op->CollectRetargetedBones(Retargeted);
        // UE 5.6's cached branch traversal assumes contiguous descendants.
        // CSS keeps game bone indices and appends extensions, so its extra hair
        // can miss parent propagation. Unmapped terminal branches must retain
        // their retarget-pose locals, then follow the animated parent normally.
        // Leave mapped bones, intermediate joints, root markers and virtual
        // bones alone. This changes exported tracks, never the shared skeleton.
        const int32 RawBones = TargetMesh->GetRefSkeleton().GetRawBoneNum();
        for (int32 Bone = 1; Bone < RawBones; ++Bone)
        {
            if (Retargeted.Contains(Bone)) continue;
            bool MappedAncestor = false, MappedDescendant = false;
            for (int32 Parent = TargetRig.ParentIndices[Bone]; Parent != INDEX_NONE;
                 Parent = TargetRig.ParentIndices[Parent])
                MappedAncestor |= Parent != 0 && Retargeted.Contains(Parent);
            for (int32 Child = Bone + 1; Child < RawBones && !MappedDescendant; ++Child)
                MappedDescendant = Retargeted.Contains(Child) && TargetRig.IsParentOf(Bone, Child);
            if (MappedAncestor && !MappedDescendant) Attachments.Add(Bone);
        }
    }
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
        for (const int32 Bone : Attachments)
            Local[Bone] = TargetRig.RetargetPoses.GetLocalRetargetPose()[Bone];
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

UBlendSpace* UCSSAnimationLibrary::CreateIdleCarrier(USkeletalMesh* Mesh,
    UAnimSequence* Animation, const FString& OutputPackage)
{
    if (!IsRunningCommandlet() || !Mesh || !Animation || Animation->GetSkeleton() != Mesh->GetSkeleton() ||
        Animation->IsValidAdditive() || Animation->RateScale != 1.f || Animation->GetPlayLength() <= 0 ||
        Animation->bEnableRootMotion || !Animation->Notifies.IsEmpty() ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/BS_")) || OutputPackage.Len() > 100 ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage))
        return nullptr;
    auto* Result = NewObject<UBlendSpace>(CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage), RF_Public | RF_Standalone);
    Result->SetSkeleton(Mesh->GetSkeleton());
    Result->SetPreviewMesh(Mesh);
    auto* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
    if (!Property || Property->Struct != FBlendParameter::StaticStruct() || Property->ArrayDim != 3)
        return nullptr;
    auto* Axes = Property->ContainerPtrToValuePtr<FBlendParameter>(Result);
    Axes[0].DisplayName = TEXT("Direction"); Axes[0].Min = -180; Axes[0].Max = 180; Axes[0].GridNum = 4;
    Axes[1].DisplayName = TEXT("Speed"); Axes[1].Min = 0; Axes[1].Max = 800; Axes[1].GridNum = 4;
    for (const FVector Point : {FVector(-180,0,0),FVector(180,0,0),FVector(-180,800,0),FVector(180,800,0)})
        if (Result->AddSample(Animation, Point) == INDEX_NONE) return nullptr;
    Result->ValidateSampleData();
    Result->ResampleData();
    if (Result->GetNumberOfBlendSamples() != 4 || Result->GetBlendSpaceData().IsEmpty()) return nullptr;
    Result->MarkPackageDirty();
    return Result;
}

UBlendSpace* UCSSAnimationLibrary::CreateMovementBlend(USkeletalMesh* Mesh,
    const TArray<UAnimSequence*>& Animations, const TArray<FVector>& Points,
    const TArray<float>& Rates, float MaxSpeed, const FString& OutputPackage)
{
    if (!IsRunningCommandlet() || !Mesh || Animations.Num() < 4 || Animations.Num() > 128 ||
        Points.Num() != Animations.Num() || Rates.Num() != Animations.Num() ||
        !FMath::IsFinite(MaxSpeed) || MaxSpeed <= 0 || MaxSpeed > 2000 ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/BS_")) || OutputPackage.Len() > 100 ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage)) return nullptr;
    for (int32 I = 0; I < Animations.Num(); ++I)
    {
        const auto* A = Animations[I]; const auto& P = Points[I];
        if (!A || A->GetSkeleton() != Mesh->GetSkeleton() || A->IsValidAdditive() ||
            A->RateScale != 1.f || A->GetPlayLength() <= 0 || A->bEnableRootMotion || !A->Notifies.IsEmpty() ||
            !FMath::IsFinite(Rates[I]) || Rates[I] < .05f || Rates[I] > 8.f ||
            P.ContainsNaN() || P.X < -180 || P.X > 180 || P.Y < 0 || P.Y > MaxSpeed || P.Z != 0)
            return nullptr;
        for (int32 J = 0; J < I; ++J) if (P.Equals(Points[J], .001)) return nullptr;
    }
    for (const FVector Corner : {FVector(-180,0,0),FVector(180,0,0),
                                 FVector(-180,MaxSpeed,0),FVector(180,MaxSpeed,0)})
        if (!Points.ContainsByPredicate([&](const FVector& P) { return P.Equals(Corner, .001); })) return nullptr;
    auto* AxesProperty = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
    auto* SamplesProperty = FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(), TEXT("SampleData"));
    auto* SampleType = SamplesProperty ? CastField<FStructProperty>(SamplesProperty->Inner) : nullptr;
    if (!AxesProperty || AxesProperty->Struct != FBlendParameter::StaticStruct() || AxesProperty->ArrayDim != 3 ||
        !SampleType || SampleType->Struct != FBlendSample::StaticStruct()) return nullptr;
    auto* Result = NewObject<UBlendSpace>(CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage), RF_Public | RF_Standalone);
    Result->SetSkeleton(Mesh->GetSkeleton()); Result->SetPreviewMesh(Mesh);
    auto* Axes = AxesProperty->ContainerPtrToValuePtr<FBlendParameter>(Result);
    Axes[0].DisplayName = TEXT("Direction"); Axes[0].Min = -180; Axes[0].Max = 180;
    Axes[0].GridNum = 8; Axes[0].bWrapInput = true; Axes[0].bSnapToGrid = false;
    Axes[1].DisplayName = TEXT("Speed"); Axes[1].Min = 0; Axes[1].Max = MaxSpeed;
    Axes[1].GridNum = 8; Axes[1].bSnapToGrid = false;
    Result->bAllowMarkerBasedSync = false;
    auto* ScaleAxis = FindFProperty<FByteProperty>(UBlendSpace::StaticClass(), TEXT("AxisToScaleAnimation"));
    if (!ScaleAxis || ScaleAxis->GetPropertyValue_InContainer(Result) != uint8(BSA_None)) return nullptr;
    auto& Samples = *SamplesProperty->ContainerPtrToValuePtr<TArray<FBlendSample>>(Result);
    for (int32 I = 0; I < Animations.Num(); ++I)
    {
        const int32 Index = Result->AddSample(Animations[I], Points[I]);
        if (!Samples.IsValidIndex(Index)) return nullptr;
        Samples[Index].RateScale = Rates[I];
    }
    Result->ValidateSampleData(); Result->ResampleData();
    if (Result->GetNumberOfBlendSamples() != Animations.Num() || Result->GetBlendSpaceData().IsEmpty()) return nullptr;
    Result->MarkPackageDirty(); return Result;
}

FString UCSSAnimationLibrary::InspectMovementBlend(UBlendSpace* Blend, const TArray<FVector>& Inputs)
{
    if (!IsRunningCommandlet() || !Blend || Blend->GetClass() != UBlendSpace::StaticClass() ||
        !Blend->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/BS_")) || Inputs.IsEmpty() || Inputs.Num() > 2048)
        return {};
    auto Report = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Queries;
    for (const auto& Input : Inputs)
    {
        if (Input.ContainsNaN()) return {};
        TArray<FBlendSampleData> Weights; int32 CachedIndex = INDEX_NONE;
        if (!Blend->GetSamplesFromBlendInput(Input, Weights, CachedIndex, false) || Weights.IsEmpty()) return {};
        auto Query = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Values;
        Query->SetNumberField(TEXT("direction"), Input.X); Query->SetNumberField(TEXT("speed"), Input.Y);
        double Total = 0;
        for (const auto& W : Weights)
        {
            if (!Blend->GetBlendSamples().IsValidIndex(W.SampleDataIndex) ||
                !FMath::IsFinite(W.TotalWeight) || W.TotalWeight < 0) return {};
            const auto& Sample = Blend->GetBlendSamples()[W.SampleDataIndex];
            if (!Sample.Animation) return {};
            auto Value = MakeShared<FJsonObject>();
            Value->SetNumberField(TEXT("index"), W.SampleDataIndex);
            Value->SetStringField(TEXT("animation"), Sample.Animation->GetPathName());
            Value->SetNumberField(TEXT("weight"), W.TotalWeight);
            Value->SetNumberField(TEXT("rate"), Sample.RateScale);
            Values.Add(MakeShared<FJsonValueObject>(Value)); Total += W.TotalWeight;
        }
        if (FMath::Abs(Total - 1.) > .00001) return {};
        Query->SetArrayField(TEXT("samples"), Values); Queries.Add(MakeShared<FJsonValueObject>(Query));
    }
    Report->SetArrayField(TEXT("queries"), Queries);
    FString Text;
    return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ? Text : FString();
}

FString UCSSAnimationLibrary::EvaluateClip(USkeletalMesh* Mesh, UAnimSequence* Animation,
    UAnimBlueprint* Blueprint, int32 Loops, UBlendSpace* Carrier, FVector BlendInput, bool AdvanceBlendClock)
{
    auto Fail = [](const TCHAR* Message) -> FString {
        UE_LOG(LogTemp, Error, TEXT("CSS EvaluateClip: %s"), Message);
        return {};
    };
    if (!IsRunningCommandlet() || !Mesh || !Animation || !Blueprint || !Blueprint->GeneratedClass ||
        Mesh->GetPathName() != TEXT("/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2") ||
        Blueprint->GetPathName() != TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary") ||
        !Animation->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/RT_")) ||
        Blueprint->TargetSkeleton != Mesh->GetSkeleton() || Animation->GetSkeleton() != Mesh->GetSkeleton() ||
        Loops < 1 || Loops > 3 || Animation->GetPlayLength() <= 0 || Animation->GetPlayLength()*Loops > 22)
        return Fail(TEXT("Invalid isolated component input"));
    if (AdvanceBlendClock && !Carrier) return Fail(TEXT("Advancing a blend requires a carrier"));
    if (Carrier)
    {
        if (Carrier->GetSkeleton() != Mesh->GetSkeleton() || Carrier->IsValidAdditive() ||
            !Carrier->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/BS_")) ||
            (!AdvanceBlendClock && Carrier->GetNumberOfBlendSamples() != 4) ||
            Carrier->GetNumberOfBlendSamples() < 1 || Carrier->GetNumberOfBlendSamples() > 128 || BlendInput.ContainsNaN())
            return Fail(TEXT("Invalid idle carrier"));
        for (const auto& Sample : Carrier->GetBlendSamples())
        {
            if (!Sample.Animation || Sample.Animation->GetSkeleton() != Mesh->GetSkeleton() ||
                Sample.Animation->IsValidAdditive() || Sample.Animation->bEnableRootMotion ||
                !Sample.Animation->Notifies.IsEmpty() || !FMath::IsFinite(Sample.RateScale) || Sample.RateScale <= 0 ||
                (!AdvanceBlendClock && (Sample.Animation != Animation || Sample.RateScale != 1.f)))
                return Fail(TEXT("Carrier has incompatible samples"));
            Sample.Animation->BeginCacheDerivedDataForCurrentPlatform();
            Sample.Animation->WaitOnExistingCompression(true);
            if (!Sample.Animation->IsCompressedDataValid()) return Fail(TEXT("Uncompressed carrier sample"));
        }
    }
    FMemMark Memory(FMemStack::Get());
    Animation->BeginCacheDerivedDataForCurrentPlatform();
    Animation->WaitOnExistingCompression(true);
    const auto* ForceRaw = IConsoleManager::Get().FindConsoleVariable(TEXT("a.ForceEvalRawData"));
    if (!Animation->IsCompressedDataValid() || !ForceRaw || ForceRaw->GetInt() != 0 ||
        Animation->GetSkeletonVirtualBoneGuid() != Mesh->GetSkeleton()->GetVirtualBoneGuid())
        return Fail(TEXT("Compressed evaluation preconditions failed"));
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto MakeComponent = [&](bool PostProcess) {
        auto* C = NewObject<USkeletalMeshComponent>();
        C->SetSkeletalMesh(Mesh);
        C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (PostProcess) C->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()), true);
        C->SetDisablePostProcessBlueprint(!PostProcess);
        Scene.AddComponent(C, FTransform::Identity);
        C->InitAnim(true);
        C->SetAnimation(Carrier ? static_cast<UAnimationAsset*>(Carrier) : Animation);
        if (Carrier) C->GetSingleNodeInstance()->SetBlendSpacePosition(BlendInput);
        if (AdvanceBlendClock) { C->GetSingleNodeInstance()->SetLooping(true); C->GetSingleNodeInstance()->SetPlaying(true); }
        else C->Stop();
        return C;
    };
    auto* Upstream = MakeComponent(false);
    auto* Component = MakeComponent(true);
    auto* Instance = Component->GetPostProcessInstance();
    if (!Instance) return Fail(TEXT("Missing post-process instance"));
    auto Defaults = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Instance->GetClass()); It; ++It)
    {
        if (!It->GetName().StartsWith(TEXT("CSS"))) continue;
        FString Value;
        It->ExportText_InContainer(0, Value, Instance, Instance, Instance, PPF_None);
        Defaults->SetStringField(It->GetName(), Value);
    }
    for (const auto& Pair : {TPair<FName,float>(TEXT("CSSStiffness"),200.f), TPair<FName,float>(TEXT("CSSDamping"),24.f)})
    {
        auto* P = FindFProperty<FFloatProperty>(Instance->GetClass(), Pair.Key);
        if (!P) return Fail(TEXT("Missing accepted hair controls"));
        P->SetPropertyValue_InContainer(Instance, Pair.Value);
    }
    const auto& Ref = Mesh->GetRefSkeleton();
    auto* Filter = FindFProperty<FArrayProperty>(FAnimNode_ControlRigBase::StaticStruct(), TEXT("OutputBonesToTransfer"));
    if (!Filter) return Fail(TEXT("Missing native rig output filter"));
    TSet<int32> ChangedBones;
    TArray<TSharedPtr<FJsonValue>> FilterReport;
    for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
    {
        if (It->Struct != FAnimNode_ControlRig::StaticStruct()) continue;
        auto* Node = It->ContainerPtrToValuePtr<FAnimNode_ControlRig>(Instance);
        const auto& Bones = *Filter->ContainerPtrToValuePtr<TArray<FBoneReference>>(Node);
        auto Entry = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Names;
        for (const auto& Bone : Bones)
        {
            const int32 Index = Ref.FindBoneIndex(Bone.BoneName);
            if (Index == INDEX_NONE || ChangedBones.Contains(Index)) return Fail(TEXT("Invalid or overlapping output filter"));
            ChangedBones.Add(Index);
            Names.Add(MakeShared<FJsonValueString>(Bone.BoneName.ToString()));
        }
        Entry->SetArrayField(TEXT("bones"), Names);
        FilterReport.Add(MakeShared<FJsonValueObject>(Entry));
    }
    if (FilterReport.Num() < 2 || FilterReport.Num() > 3 || ChangedBones.Num() < 36)
        return Fail(TEXT("Unexpected secondary rig layout"));
    auto VectorJson = [](FVector V) {
        auto O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("X"), V.X); O->SetNumberField(TEXT("Y"), V.Y); O->SetNumberField(TEXT("Z"), V.Z);
        return O;
    };
    auto PoseJson = [&](TArrayView<const FTransform> Pose) {
        auto O = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Names, Transforms;
        for (int32 Bone = 0; Bone < Ref.GetRawBoneNum(); ++Bone)
        {
            Names.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(Bone).ToString()));
            auto T = MakeShared<FJsonObject>();
            T->SetObjectField(TEXT("Translation"), VectorJson(Pose[Bone].GetTranslation()));
            T->SetObjectField(TEXT("Scale3D"), VectorJson(Pose[Bone].GetScale3D()));
            const FQuat Q = Pose[Bone].GetRotation();
            auto R = MakeShared<FJsonObject>();
            R->SetNumberField(TEXT("X"),Q.X);R->SetNumberField(TEXT("Y"),Q.Y);R->SetNumberField(TEXT("Z"),Q.Z);R->SetNumberField(TEXT("W"),Q.W);
            T->SetObjectField(TEXT("Rotation"), R);
            Transforms.Add(MakeShared<FJsonValueObject>(T));
        }
        O->SetBoolField(TEXT("bIsValid"), true);
        O->SetStringField(TEXT("SkeletalMeshName"), Mesh->GetName());
        O->SetArrayField(TEXT("BoneNames"), Names);O->SetArrayField(TEXT("LocalTransforms"), Transforms);
        return O;
    };
    constexpr int32 Fps = 60;
    const double Duration = Animation->GetPlayLength();
    const int32 Frames = FMath::RoundToInt(Duration*Loops*Fps);
    TArray<TSharedPtr<FJsonValue>> Samples;
    double UnaffectedPosition = 0, UnaffectedAngle = 0;
    for (int32 Frame = 0; Frame <= Frames; ++Frame)
    {
        const double Time = double(Frame)/Fps;
        const double ClipTime = Frame == Frames ? Duration : FMath::Fmod(Time, Duration);
        for (auto* C : {Upstream, Component})
        {
            // Single-node blend spaces expose normalized position, sequences seconds.
            if (!AdvanceBlendClock) C->SetPosition(float(Carrier ? ClipTime/Duration : ClipTime), false);
            C->TickAnimation(Frame ? 1.f/Fps : 0.f, true);
            C->RefreshBoneTransforms();
        }
        const auto A = Upstream->GetBoneSpaceTransformsView(), B = Component->GetBoneSpaceTransformsView();
        if (A.Num() != Ref.GetNum() || B.Num() != Ref.GetNum()) return Fail(TEXT("Incomplete component pose"));
        for (int32 Bone = 0; Bone < Ref.GetRawBoneNum(); ++Bone)
        {
            if (A[Bone].ContainsNaN() || B[Bone].ContainsNaN() || !B[Bone].GetRotation().IsNormalized())
                return Fail(TEXT("Invalid component transform"));
            if (ChangedBones.Contains(Bone)) continue;
            UnaffectedPosition = FMath::Max(UnaffectedPosition, FVector::Distance(A[Bone].GetTranslation(), B[Bone].GetTranslation()));
            UnaffectedAngle = FMath::Max(UnaffectedAngle, A[Bone].GetRotation().AngularDistance(B[Bone].GetRotation()));
            if (!A[Bone].GetScale3D().Equals(B[Bone].GetScale3D(), 0.0001)) return Fail(TEXT("Unexpected scale change"));
        }
        auto Sample = MakeShared<FJsonObject>();
        auto Pose = MakeShared<FJsonObject>();
        Pose->SetObjectField(TEXT("Snapshot"), PoseJson(B));
        Sample->SetObjectField(TEXT("pose"), Pose);
        Sample->SetObjectField(TEXT("upstream"), PoseJson(A));
        Sample->SetNumberField(TEXT("time"), Time);
        if (AdvanceBlendClock) Sample->SetNumberField(TEXT("blend_normalized_time"), Component->GetSingleNodeInstance()->GetCurrentTime());
        else Sample->SetNumberField(TEXT("clip_time"), ClipTime);
        auto Morphs = MakeShared<FJsonObject>();
        for (UMorphTarget* Morph : Mesh->GetMorphTargets())
        {
            const int32* Index = Component->ActiveMorphTargets.Find(Morph);
            if (!Index) continue;
            if (!Component->MorphTargetWeights.IsValidIndex(*Index)) return Fail(TEXT("Invalid morph weight index"));
            const float Weight = Component->MorphTargetWeights[*Index];
            if (!FMath::IsFinite(Weight)) return Fail(TEXT("Invalid morph weight"));
            if (!FMath::IsNearlyZero(Weight)) Morphs->SetNumberField(Morph->GetName(), Weight);
        }
        Sample->SetObjectField(TEXT("morphs"), Morphs);
        TArray<TSharedPtr<FJsonValue>> Rigs;
        for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
        {
            if (It->Struct != FAnimNode_ControlRig::StaticStruct()) continue;
            auto* Rig = It->ContainerPtrToValuePtr<FAnimNode_ControlRig>(Instance)->GetControlRig();
            if (!Rig) return Fail(TEXT("Missing live rig instance"));
            auto Values = MakeShared<FJsonObject>();
            for (const TCHAR* Name : {TEXT("Enabled"), TEXT("Stiffness"), TEXT("Damping"), TEXT("Gravity"),
                TEXT("Frequency"), TEXT("DampingRatio"), TEXT("MotionAmount"), TEXT("UseRegionSettings"),
                TEXT("SeenUpdate"), TEXT("TotalSteps"), TEXT("ResetCount"), TEXT("HandValid")})
                if (Rig->GetClass()->FindPropertyByName(Name)) Values->SetStringField(Name, Rig->GetVariableAsString(Name));
            Rigs.Add(MakeShared<FJsonValueObject>(Values));
        }
        Sample->SetArrayField(TEXT("rigs"), Rigs);
        Samples.Add(MakeShared<FJsonValueObject>(Sample));
    }
    if (UnaffectedPosition > 0.001 || UnaffectedAngle > 0.001) return Fail(TEXT("Post-process changed unlisted bones"));
    auto Report = MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("frames"), Samples);Report->SetNumberField(TEXT("fps"), Fps);
    Report->SetObjectField(TEXT("defaults"), Defaults);Report->SetArrayField(TEXT("filters"), FilterReport);
    Report->SetBoolField(TEXT("compressed_source"), true);Report->SetNumberField(TEXT("loops"), Loops);
    Report->SetStringField(TEXT("carrier"), Carrier ? Carrier->GetPathName() : TEXT(""));
    Report->SetBoolField(TEXT("advance_blend_clock"), AdvanceBlendClock);
    Report->SetNumberField(TEXT("unaffected_position_cm"), UnaffectedPosition);
    Report->SetNumberField(TEXT("unaffected_angle_rad"), UnaffectedAngle);
    FString Text;
    return FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ? Text : FString();
}
