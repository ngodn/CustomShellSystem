#include "CSSContactLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimPose.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonWriter.h"
#include "TwoBoneIK.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Rigs/FKControlRig.h"
#include "Sections/MovieSceneParameterSection.h"

UAnimSequence* UCSSContactLibrary::BakePelvisClearance(UAnimSequence* Source,
    const TArray<float>& Heights, const FString& OutputPackage)
{
    auto Fail = [](const TCHAR* Message) -> UAnimSequence* {
        UE_LOG(LogTemp, Error, TEXT("CSS BakePelvisClearance: %s"), Message);
        return nullptr;
    };
    if (!IsRunningCommandlet() || !Source || !Source->GetSkeleton() ||
        Source->IsValidAdditive() || Source->RateScale != 1.f ||
        !Source->Notifies.IsEmpty() || !Source->AuthoredSyncMarkers.IsEmpty() ||
        !Source->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/RT_")) ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/RT_")) || OutputPackage.Len() > 100 ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage))
        return Fail(TEXT("Invalid source, output or execution context"));
    const auto* Original = Source->GetDataModel();
    if (!Original || Original->GetNumberOfKeys() < 2 || Original->GetNumberOfKeys() > 10000 ||
        Original->GetNumberOfKeys() != Heights.Num() || !Original->GetFrameRate().IsValid() ||
        Original->GetNumberOfFloatCurves() ||
        !Original->GetTransformCurves().IsEmpty() || !Original->GetAttributes().IsEmpty())
        return Fail(TEXT("Expected a bone-only clip and one lift per key"));
    for (float Height : Heights)
        if (!FMath::IsFinite(Height) || Height < 0.f || Height > 50.f)
            return Fail(TEXT("Invalid or excessive lift"));
    const FReferenceSkeleton& Ref = Source->GetSkeleton()->GetReferenceSkeleton();
    const int32 Root = Ref.FindBoneIndex(TEXT("root")), Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
    if (Root == INDEX_NONE || Pelvis == INDEX_NONE || Ref.GetParentIndex(Root) != INDEX_NONE ||
        Ref.GetParentIndex(Pelvis) != Root)
        return Fail(TEXT("Expected pelvis directly under the animation root"));
    TArray<FName> Names; Original->GetBoneTrackNames(Names);
    if (!Names.Contains(TEXT("root")) || !Names.Contains(TEXT("pelvis")))
        return Fail(TEXT("Missing root or pelvis track"));
    TArray<FTransform> RootKeys, PelvisKeys;
    Original->GetBoneTrackTransforms(TEXT("root"), RootKeys);
    Original->GetBoneTrackTransforms(TEXT("pelvis"), PelvisKeys);
    if (RootKeys.Num() != Heights.Num() || PelvisKeys.Num() != Heights.Num())
        return Fail(TEXT("Incomplete root or pelvis keys"));
    TArray<FVector3f> Positions;
    for (int32 Key = 0; Key < Heights.Num(); ++Key)
    {
        if (RootKeys[Key].ContainsNaN() || PelvisKeys[Key].ContainsNaN() ||
            !RootKeys[Key].GetRotation().IsNormalized() ||
            !RootKeys[Key].GetScale3D().Equals(FVector::OneVector, .000001))
            return Fail(TEXT("Root must have finite unit-scale transforms"));
        const FVector3f Position(PelvisKeys[Key].GetTranslation() +
            RootKeys[Key].InverseTransformVectorNoScale(FVector(0, 0, Heights[Key])));
        if (Position.ContainsNaN()) return Fail(TEXT("Lift exceeds finite float position range"));
        Positions.Add(Position);
    }
    auto* Result = DuplicateObject<UAnimSequence>(Source, CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage));
    if (!Result || Result->GetDataModel() == Original) return Fail(TEXT("Could not isolate data model"));
    Result->SetFlags(RF_Public | RF_Standalone);
    const FName ControlName = UFKControlRig::GetControlName(TEXT("pelvis"), ERigElementType::Bone);
    FTransformParameterNameAndCurves* PelvisCurves = nullptr;
    int32 Matches = 0;
    ForEachObjectWithOuter(Result->GetDataModelInterface().GetObject(), [&](UObject* Object) {
        if (auto* Section = Cast<UMovieSceneParameterSection>(Object))
            for (auto& Curves : Section->GetTransformParameterNamesAndCurves())
                if (Curves.ParameterName == ControlName) { PelvisCurves = &Curves; ++Matches; }
    }, true);
    if (Matches != 1) return Fail(TEXT("Expected one sequencer pelvis parameter"));
    // Check the model's frame clock before replacing its translation channels.
    for (int32 Key = 0; Key < Heights.Num(); ++Key)
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            float Value = 0.f;
            if (!PelvisCurves->Translation[Axis].Evaluate(FFrameTime(Key), Value) ||
                !FMath::IsNearlyEqual(double(Value), PelvisKeys[Key].GetTranslation()[Axis], .000001))
                return Fail(TEXT("Unexpected translation channel clock or values"));
        }
    auto& Edit = Result->GetController();
    Edit.OpenBracket(FText::FromString(TEXT("Bake CSS pelvis clearance")), false);
    ON_SCOPE_EXIT { Edit.CloseBracket(false); };
    {
        IAnimationDataModel::FEvaluationAndModificationLock Lock(*Result->GetDataModel());
        TArray<FFrameNumber> Times;
        for (int32 Key = 0; Key < Heights.Num(); ++Key) Times.Add(Key);
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            TArray<FMovieSceneFloatValue> Values;
            for (const FVector3f& Position : Positions)
            {
                auto& Value = Values.AddDefaulted_GetRef();
                Value.Value = Position[Axis]; Value.InterpMode = RCIM_Linear;
            }
            // SetBoneTrackKeys also rewrites Euler rotations, including near gimbal lock.
            PelvisCurves->Translation[Axis].Reset();
            PelvisCurves->Translation[Axis].SetKeysOnly(Times, Values);
        }
    }
    Edit.NotifyPopulated();
    const auto* Baked = Result->GetDataModel();
    TArray<FName> BakedNames; Baked->GetBoneTrackNames(BakedNames);
    if (Names != BakedNames || Original->GetNumberOfKeys() != Baked->GetNumberOfKeys() ||
        Original->GetFrameRate() != Baked->GetFrameRate() || Original->GetPlayLength() != Baked->GetPlayLength())
        return Fail(TEXT("Track coverage or timing changed"));
    for (FName Name : Names)
    {
        TArray<FTransform> Before, After;
        Original->GetBoneTrackTransforms(Name, Before); Baked->GetBoneTrackTransforms(Name, After);
        if (Before.Num() != After.Num()) return Fail(TEXT("Track key count changed"));
        for (int32 Key = 0; Key < Before.Num(); ++Key)
            if (Name != TEXT("pelvis") ? !Before[Key].Equals(After[Key], 0.) :
                (!Before[Key].GetRotation().Equals(After[Key].GetRotation(), 0.) ||
                 !Before[Key].GetScale3D().Equals(After[Key].GetScale3D(), 0.) ||
                 !After[Key].GetTranslation().Equals(FVector(Positions[Key]), .000001)))
                return Fail(TEXT("Unexpected authored transform change"));
    }
    Result->MarkPackageDirty();
    return Result;
}

FQuat UCSSContactLibrary::SequencerRotationReference(FQuat Rotation)
{
    const FVector3f Euler = FQuat4f(Rotation).Euler();
    return FRotator::MakeFromEuler(FVector(Euler)).Quaternion();
}

FString UCSSContactLibrary::ReadAuthoredTrack(UAnimSequence* Source, FName Bone)
{
    if (!IsRunningCommandlet() || !Source || Bone.IsNone() ||
        !Source->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/"))) return {};
    const auto* Model = Source->GetDataModel();
    if (!Model || Model->GetNumberOfKeys() < 2 || Model->GetNumberOfKeys() > 10000) return {};
    TArray<FName> Names;
    Model->GetBoneTrackNames(Names);
    if (!Names.Contains(Bone)) return {};
    TArray<FTransform> Keys;
    Model->GetBoneTrackTransforms(Bone, Keys);
    FString Result;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("model"), Source->GetDataModelInterface().GetObject()->GetClass()->GetName());
    Writer->WriteArrayStart(TEXT("keys"));
    for (const auto& Key : Keys)
    {
        if (Key.ContainsNaN()) return {};
        Writer->WriteObjectStart();
        for (const auto& Pair : {TPair<FString, FVector>(TEXT("Translation"), Key.GetTranslation()),
                                TPair<FString, FVector>(TEXT("Scale3D"), Key.GetScale3D())})
        {
            Writer->WriteObjectStart(Pair.Key);
            Writer->WriteValue(TEXT("X"), Pair.Value.X);
            Writer->WriteValue(TEXT("Y"), Pair.Value.Y);
            Writer->WriteValue(TEXT("Z"), Pair.Value.Z);
            Writer->WriteObjectEnd();
        }
        const FQuat Q = Key.GetRotation();
        Writer->WriteObjectStart(TEXT("Rotation"));
        Writer->WriteValue(TEXT("X"), Q.X); Writer->WriteValue(TEXT("Y"), Q.Y);
        Writer->WriteValue(TEXT("Z"), Q.Z); Writer->WriteValue(TEXT("W"), Q.W);
        Writer->WriteObjectEnd();
        Writer->WriteObjectEnd();
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();
    return Result;
}

UAnimSequence* UCSSContactLibrary::BakeSoleContact(USkeletalMesh* Mesh,
    UAnimSequence* Source, const TArray<float>& LeftHeights, const TArray<float>& RightHeights,
    float SmoothingCm, const FString& OutputPackage, bool PreserveSourceKneePlane)
{
    return BakeContactWithKneeDirections(Mesh, Source, LeftHeights, RightHeights,
        SmoothingCm, OutputPackage, PreserveSourceKneePlane, {}, {});
}

UAnimSequence* UCSSContactLibrary::BakeContactWithKneeDirections(USkeletalMesh* Mesh,
    UAnimSequence* Source, const TArray<float>& LeftHeights, const TArray<float>& RightHeights,
    float SmoothingCm, const FString& OutputPackage, bool PreserveSourceKneePlane,
    const TArray<FVector>& LeftDirections, const TArray<FVector>& RightDirections)
{
    auto Fail = [](const TCHAR* Message) -> UAnimSequence* {
        UE_LOG(LogTemp, Error, TEXT("CSS BakeSoleContact: %s"), Message);
        return nullptr;
    };
    if (!IsRunningCommandlet() || !Mesh || !Source || !Source->GetSkeleton() ||
        Source->GetSkeleton() != Mesh->GetSkeleton() || Source->IsValidAdditive() ||
        Source->RateScale != 1.f || !Source->Notifies.IsEmpty() || !Source->AuthoredSyncMarkers.IsEmpty() ||
        !Source->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/RT_")) ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/RT_")) || OutputPackage.Len() > 100 ||
        !FPackageName::IsValidLongPackageName(OutputPackage) || FPackageName::DoesPackageExist(OutputPackage) ||
        FindPackage(nullptr, *OutputPackage) || !FMath::IsFinite(SmoothingCm) ||
        SmoothingCm <= 0.f || SmoothingCm > .5f)
        return Fail(TEXT("Invalid assets, output, smoothing or execution context"));
    const auto* Model = Source->GetDataModel();
    if (!Model || Model->GetNumberOfFloatCurves() || !Model->GetTransformCurves().IsEmpty() ||
        !Model->GetAttributes().IsEmpty()) return Fail(TEXT("Expected bone-only source"));
    const int32 Keys = Model->GetNumberOfKeys();
    if (Keys < 2 || Keys > 10000 || LeftHeights.Num() != Keys || RightHeights.Num() != Keys)
        return Fail(TEXT("Measurements do not cover every source key"));
    for (const auto* Directions : {&LeftDirections, &RightDirections})
    {
        if (!Directions->IsEmpty() && Directions->Num() != Keys)
            return Fail(TEXT("Knee directions do not cover every source key"));
        for (const FVector& Direction : *Directions)
            if (Direction.ContainsNaN() || (!Direction.IsZero() &&
                !FMath::IsNearlyEqual(Direction.SizeSquared(), 1., .0001)))
                return Fail(TEXT("Knee directions must be zero or finite unit vectors"));
    }
    for (const auto* Heights : {&LeftHeights, &RightHeights})
        for (float Height : *Heights)
            if (!FMath::IsFinite(Height) || Height < -25.f || Height > 500.f)
                return Fail(TEXT("Invalid or excessive sole displacement"));
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    const TArray<FName> Names = {TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
                                TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")};
    TArray<int32> Indices;
    TArray<TArray<FTransform>> Tracks;
    TArray<FName> SourceNames; Model->GetBoneTrackNames(SourceNames);
    for (FName Name : Names)
    {
        const int32 Index = Ref.FindBoneIndex(Name);
        if (Index == INDEX_NONE || !SourceNames.Contains(Name)) return Fail(TEXT("Missing leg track"));
        Indices.Add(Index);
        auto& Track = Tracks.AddDefaulted_GetRef();
        Model->GetBoneTrackTransforms(Name, Track);
        if (Track.Num() != Keys) return Fail(TEXT("Incomplete source leg keys"));
    }
    for (int32 Side : {0, 3})
        if (Ref.GetParentIndex(Indices[Side + 1]) != Indices[Side] ||
            Ref.GetParentIndex(Indices[Side + 2]) != Indices[Side + 1] ||
            Ref.GetParentIndex(Indices[Side]) == INDEX_NONE)
            return Fail(TEXT("Unexpected leg hierarchy"));
    // The audited rig has hand virtual bones only. Reject rigs needing rebaked leg virtual tracks.
    for (const auto& VB : Source->GetSkeleton()->GetVirtualBones())
        for (FName Name : {VB.SourceBoneName, VB.TargetBoneName})
            for (int32 Index = Ref.FindBoneIndex(Name); Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
                if (Index == Indices[0] || Index == Indices[3])
                    return Fail(TEXT("Leg-dependent virtual bones need a separate bake"));
    FAnimPoseEvaluationOptions Options;
    Options.EvaluationType = EAnimDataEvalType::Raw;
    Options.OptionalSkeletalMesh = Mesh;
    Options.bExtractRootMotion = false;
    Options.bIncorporateRootMotionIntoPose = true;
    for (int32 Frame = 0; Frame < Keys; ++Frame)
    {
        FAnimPose Pose;
        UAnimPoseExtensions::GetAnimPoseAtFrame(Source, Frame, Options, Pose);
        if (!Pose.IsValid()) return Fail(TEXT("Invalid source pose"));
        for (int32 Side : {0, 3})
        {
            const auto& Directions = Side == 0 ? LeftDirections : RightDirections;
            const FVector Direction = Directions.IsEmpty() ? FVector::ZeroVector : Directions[Frame];
            const bool HasDirection = !Direction.IsZero();
            const double Depth = -(Side == 0 ? LeftHeights[Frame] : RightHeights[Frame]);
            // C1 positive-part approximation avoids a derivative jump as the sole reaches the floor.
            const double Lift = Depth <= -SmoothingCm ? 0. : Depth >= SmoothingCm ? Depth :
                FMath::Square(Depth + SmoothingCm) / (4. * SmoothingCm);
            if (Lift == 0. && !HasDirection) continue;
            FTransform Hip = UAnimPoseExtensions::GetBonePose(Pose, Names[Side], EAnimPoseSpaces::World);
            FTransform Knee = UAnimPoseExtensions::GetBonePose(Pose, Names[Side + 1], EAnimPoseSpaces::World);
            FTransform Foot = UAnimPoseExtensions::GetBonePose(Pose, Names[Side + 2], EAnimPoseSpaces::World);
            auto Parent = UAnimPoseExtensions::GetBonePose(Pose,
                Ref.GetBoneName(Ref.GetParentIndex(Indices[Side])), EAnimPoseSpaces::World);
            for (const auto* T : {&Hip, &Knee, &Foot, &Parent})
                if (T->ContainsNaN() || !T->GetScale3D().Equals(FVector::OneVector, .0001))
                    return Fail(TEXT("Leg solve requires finite unit-scale transforms"));
            const FVector Goal = Foot.GetTranslation() + FVector(0, 0, Lift);
            FVector Pole = Knee.GetTranslation();
            const double Upper = FVector::Distance(Hip.GetTranslation(), Pole);
            const double Lower = FVector::Distance(Pole, Foot.GetTranslation());
            const double Reach = FVector::Distance(Hip.GetTranslation(), Goal);
            const FVector Axis = (Goal - Hip.GetTranslation()).GetSafeNormal();
            FVector Bend = Pole - Hip.GetTranslation();
            if (PreserveSourceKneePlane)
            {
                // Moving the foot goal can cross a nearly straight knee. Measure
                // its original plane before projecting onto the new goal axis.
                const FVector SourceAxis = (Foot.GetTranslation() - Hip.GetTranslation()).GetSafeNormal();
                Bend -= FVector::DotProduct(Bend, SourceAxis) * SourceAxis;
                Bend -= FVector::DotProduct(Bend, Axis) * Axis;
                Pole = Hip.GetTranslation() + Bend.GetSafeNormal() * Upper;
            }
            if (HasDirection)
            {
                Bend = Direction - FVector::DotProduct(Direction, Axis) * Axis;
                if (Bend.SizeSquared() < .0001)
                    return Fail(TEXT("Knee direction is parallel to the corrected leg"));
                Pole = Hip.GetTranslation() + Bend.GetSafeNormal() * Upper;
            }
            if (Reach >= Upper + Lower || Reach <= FMath::Abs(Upper - Lower))
                return Fail(TEXT("Unreachable foot goal"));
            if ((Bend - FVector::DotProduct(Bend, Axis) * Axis).SizeSquared() < 1.e-8)
            {
                // A straight source leg has no knee plane. Borrow the nearest measured
                // plane in hip-local space instead of letting IK choose an arbitrary axis.
                bool Found = false;
                for (int32 Distance = 1; Distance < Keys && !Found; ++Distance)
                    for (int32 Neighbor : {Frame - Distance, Frame + Distance})
                    {
                        if (Neighbor < 0 || Neighbor >= Keys || Found) continue;
                        FAnimPose Adjacent;
                        UAnimPoseExtensions::GetAnimPoseAtFrame(Source, Neighbor, Options, Adjacent);
                        if (!Adjacent.IsValid()) return Fail(TEXT("Invalid neighboring pose"));
                        const auto H = UAnimPoseExtensions::GetBonePose(Adjacent, Names[Side], EAnimPoseSpaces::World);
                        const auto K = UAnimPoseExtensions::GetBonePose(Adjacent, Names[Side + 1], EAnimPoseSpaces::World);
                        const auto F = UAnimPoseExtensions::GetBonePose(Adjacent, Names[Side + 2], EAnimPoseSpaces::World);
                        const FVector Along = (F.GetTranslation() - H.GetTranslation()).GetSafeNormal();
                        const FVector ToKnee = K.GetTranslation() - H.GetTranslation();
                        FVector Plane = ToKnee - FVector::DotProduct(ToKnee, Along) * Along;
                        if (Plane.SizeSquared() < .0025) continue;
                        Plane = Hip.TransformVectorNoScale(H.InverseTransformVectorNoScale(Plane.GetSafeNormal()));
                        Plane -= FVector::DotProduct(Plane, Axis) * Axis;
                        if (Plane.SizeSquared() < .0001) continue;
                        Pole = Hip.GetTranslation() + Plane.GetSafeNormal() * Upper;
                        Found = true;
                    }
                if (!Found) return Fail(TEXT("No measured knee plane for straight leg"));
            }
            const FQuat FootRotation = Foot.GetRotation();
            AnimationCore::SolveTwoBoneIK(Hip, Knee, Foot, Pole, Goal, false, 1., 1.);
            if (!Foot.GetTranslation().Equals(Goal, .001) || !Foot.GetRotation().Equals(FootRotation, .000001))
                return Fail(TEXT("Foot goal or orientation was not preserved"));
            Tracks[Side][Frame] = Hip.GetRelativeTransform(Parent);
            Tracks[Side + 1][Frame] = Knee.GetRelativeTransform(Hip);
            Tracks[Side + 2][Frame] = Foot.GetRelativeTransform(Knee);
        }
    }
    auto* Result = DuplicateObject<UAnimSequence>(Source, CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage));
    if (!Result || Result->GetDataModel() == Model) return Fail(TEXT("Could not isolate source model"));
    Result->SetFlags(RF_Public | RF_Standalone);
    auto& Edit = Result->GetController();
    Edit.OpenBracket(FText::FromString(TEXT("Bake measured CSS sole contact")), false);
    ON_SCOPE_EXIT { Edit.CloseBracket(false); };
    for (int32 Bone = 0; Bone < Names.Num(); ++Bone)
    {
        TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
        for (const auto& T : Tracks[Bone])
        {
            if (T.ContainsNaN() || !T.GetRotation().IsNormalized()) return Fail(TEXT("Invalid solved track"));
            Positions.Add(FVector3f(T.GetTranslation())); Scales.Add(FVector3f(T.GetScale3D()));
            Rotations.Add(FQuat4f(T.GetRotation()));
        }
        if (!Edit.SetBoneTrackKeys(Names[Bone], Positions, Rotations, Scales, false))
            return Fail(TEXT("Could not write solved leg track"));
    }
    Edit.NotifyPopulated();
    const auto* Baked = Result->GetDataModel();
    TArray<FName> BakedNames; Baked->GetBoneTrackNames(BakedNames);
    if (SourceNames != BakedNames || Baked->GetNumberOfKeys() != Keys ||
        Baked->GetFrameRate() != Model->GetFrameRate() || Baked->GetPlayLength() != Model->GetPlayLength())
        return Fail(TEXT("Bake changed track coverage or timing"));
    for (FName Name : SourceNames)
    {
        if (Names.Contains(Name)) continue;
        TArray<FTransform> Original, Copy;
        Model->GetBoneTrackTransforms(Name, Original);
        Baked->GetBoneTrackTransforms(Name, Copy);
        if (Original.Num() != Copy.Num()) return Fail(TEXT("Unrelated track key count changed"));
        for (int32 Key = 0; Key < Original.Num(); ++Key)
            if (!Original[Key].Equals(Copy[Key], 0.)) return Fail(TEXT("Unrelated bone motion changed"));
    }
    Result->MarkPackageDirty();
    return Result;
}
