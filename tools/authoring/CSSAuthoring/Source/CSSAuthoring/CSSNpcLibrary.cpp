#include "CSSNpcLibrary.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"

bool UCSSNpcLibrary::ClearBossPreview(USkeleton* Target)
{
    if (!IsRunningCommandlet() || !Target ||
        !Target->GetPathName().StartsWith(TEXT("/Game/CSS/Bosses/")))
        return false;
    Target->SetPreviewMesh(nullptr);
    return Target->GetPreviewMesh(false) == nullptr;
}

int32 UCSSNpcLibrary::ConfigureAnimationReference(USkeleton* Target, USkeleton* Human)
{
    if (!IsRunningCommandlet() || !Target || !Human || Target == Human)
        return INDEX_NONE;
    const FString Path = Target->GetPathName();
    if (Path != TEXT("/Game/CSS/eins0fx/NPC/Zhirelle/SKEL_Body.SKEL_Body") &&
        Path != TEXT("/Game/CSS/eins0fx/NPC/Merrick/SKEL_Body.SKEL_Body"))
        return INDEX_NONE;
    if (Human->GetPathName() != TEXT("/Game/CSSAuthoring/DiagnosticReferences/SKEL_GameHumanMore1275_V1.SKEL_GameHumanMore1275_V1"))
        return INDEX_NONE;

    const auto& Source = Human->GetReferenceSkeleton();
    const auto& Before = Target->GetReferenceSkeleton();
    TArray<TPair<int32, FTransform>> Poses;
    const bool Seated = Path.Contains(TEXT("/Zhirelle/"));
    auto LegLength = [](const FReferenceSkeleton& Skeleton) {
        double Length = 0;
        for (const FName Bone : {FName(TEXT("calf_r")), FName(TEXT("foot_r"))})
        {
            const int32 Index = Skeleton.FindBoneIndex(Bone);
            if (Index == INDEX_NONE) return 0.0;
            Length += Skeleton.GetRawRefBonePose()[Index].GetTranslation().Size();
        }
        return Length;
    };
    const double SourceLeg = LegLength(Source);
    if (SourceLeg < 1) return INDEX_NONE;
    for (int32 Index = 0; Index < Before.GetRawBoneNum(); ++Index)
    {
        const FString Name = Before.GetBoneName(Index).ToString();
        bool Body = Name == TEXT("root") || Name == TEXT("pelvis") || Name == TEXT("head");
        for (const TCHAR* Prefix : {TEXT("spine_"), TEXT("neck_"), TEXT("clavicle_"),
             TEXT("upperarm_"), TEXT("lowerarm_"), TEXT("hand_"), TEXT("index_"),
             TEXT("middle_"), TEXT("thumb_"), TEXT("pinky_"), TEXT("ring_"),
             TEXT("thigh_"), TEXT("calf_"), TEXT("foot_"), TEXT("ball_"), TEXT("ik_")})
            Body |= Name.StartsWith(Prefix);
        const int32 SourceIndex = Source.FindBoneIndex(Before.GetBoneName(Index));
        if (!Body || SourceIndex == INDEX_NONE)
            continue;
        const FQuat Rotation = Source.GetRawRefBonePose()[SourceIndex].GetRotation();
        if (!Rotation.IsNormalized())
            return INDEX_NONE;
        FTransform Pose = Before.GetRawRefBonePose()[Index];
        Pose.SetRotation(Rotation);
        if (Seated && (Name == TEXT("calf_l") || Name == TEXT("foot_l") || Name == TEXT("ball_l")))
            Pose.SetTranslation(Source.GetRawRefBonePose()[SourceIndex].GetTranslation().GetSafeNormal() *
                                Pose.GetTranslation().Size());
        if (Seated && Name == TEXT("pelvis"))
        {
            FVector Position = Pose.GetTranslation();
            Position.Z = Source.GetRawRefBonePose()[SourceIndex].GetTranslation().Z * LegLength(Before) / SourceLeg;
            Pose.SetTranslation(Position);
        }
        Poses.Emplace(Index, Pose);
    }
    if (Poses.Num() < 60)
        return INDEX_NONE;
    // Mesh bind transforms stay untouched. Seated NPC rest rotations must not
    // become the destination reference for the player's standing locomotion.
    FReferenceSkeletonModifier Modifier(Target);
    for (const auto& Entry : Poses)
        Modifier.UpdateRefPoseTransform(Entry.Key, Entry.Value);
    return Poses.Num();
}
