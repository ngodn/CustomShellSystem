#include "CSSRigAuditLibrary.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> TransformValues(const FTransform& Value)
{
    const FVector Position = Value.GetTranslation();
    const FQuat Rotation = Value.GetRotation();
    const FVector Scale = Value.GetScale3D();
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const double Component : {Position.X, Position.Y, Position.Z, Rotation.X,
             Rotation.Y, Rotation.Z, Rotation.W, Scale.X, Scale.Y, Scale.Z})
        Result.Add(MakeShared<FJsonValueNumber>(Component));
    return Result;
}
}

FString UCSSRigAuditLibrary::SnapshotSkeleton(USkeleton* Skeleton)
{
    if (!IsRunningCommandlet() || !Skeleton) return TEXT("ERROR: expected a commandlet skeleton");
    const auto& Reference = Skeleton->GetReferenceSkeleton();
    TArray<TSharedPtr<FJsonValue>> Bones, VirtualBones, Sockets;
    for (int32 Index = 0; Index < Reference.GetRawBoneNum(); ++Index)
    {
        const FTransform& Pose = Reference.GetRawRefBonePose()[Index];
        if (Pose.ContainsNaN()) return TEXT("ERROR: non-finite reference pose");
        auto Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("name"), Reference.GetBoneName(Index).ToString());
        Bone->SetNumberField(TEXT("parent"), Reference.GetParentIndex(Index));
        Bone->SetNumberField(TEXT("retarget_mode"), Skeleton->GetBoneTranslationRetargetingMode(Index));
        Bone->SetArrayField(TEXT("local_transform"), TransformValues(Pose));
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    for (const FVirtualBone& Virtual : Skeleton->GetVirtualBones())
    {
        auto Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("name"), Virtual.VirtualBoneName.ToString());
        Bone->SetStringField(TEXT("source"), Virtual.SourceBoneName.ToString());
        Bone->SetStringField(TEXT("target"), Virtual.TargetBoneName.ToString());
        VirtualBones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    for (const USkeletalMeshSocket* Socket : Skeleton->Sockets)
    {
        if (!Socket) return TEXT("ERROR: null skeleton socket");
        const FTransform Transform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale);
        if (Transform.ContainsNaN()) return TEXT("ERROR: non-finite socket transform");
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Socket->SocketName.ToString());
        Row->SetStringField(TEXT("bone"), Socket->BoneName.ToString());
        Row->SetBoolField(TEXT("always_animated"), Socket->bForceAlwaysAnimated);
        Row->SetArrayField(TEXT("local_transform"), TransformValues(Transform));
        Sockets.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("bones"), MoveTemp(Bones));
    Root->SetArrayField(TEXT("virtual_bones"), MoveTemp(VirtualBones));
    Root->SetArrayField(TEXT("sockets"), MoveTemp(Sockets));
    FString Result;
    if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result)))
        return TEXT("ERROR: could not serialize skeleton snapshot");
    return Result;
}

FString UCSSRigAuditLibrary::SnapshotAnimation(UAnimSequence* Sequence)
{
    if (!IsRunningCommandlet() || !Sequence || !Sequence->GetDataModel())
        return TEXT("ERROR: expected an editor animation in a commandlet");
    const IAnimationDataModel* Model = Sequence->GetDataModel();
    TArray<FName> Names;
    Model->GetBoneTrackNames(Names);
    if (Names.IsEmpty()) return TEXT("ERROR: no animation tracks");
    TArray<TSharedPtr<FJsonValue>> Tracks;
    for (const FName Name : Names)
    {
        TArray<FTransform> Keys;
        Model->GetBoneTrackTransforms(Name, Keys);
        if (Keys.IsEmpty()) return TEXT("ERROR: empty bone track");
        FSHA1 Hash;
        for (const FTransform& Key : Keys)
        {
            if (Key.ContainsNaN()) return TEXT("ERROR: non-finite source transform");
            const FVector Position = Key.GetTranslation();
            const FQuat Rotation = Key.GetRotation();
            const FVector Scale = Key.GetScale3D();
            // Hash named components, never struct padding or an evaluated pose.
            const double Values[]{Position.X, Position.Y, Position.Z,
                Rotation.X, Rotation.Y, Rotation.Z, Rotation.W, Scale.X, Scale.Y, Scale.Z};
            Hash.Update(reinterpret_cast<const uint8*>(Values), sizeof(Values));
        }
        Hash.Final();
        uint8 Bytes[FSHA1::DigestSize];
        Hash.GetHash(Bytes);
        auto Track = MakeShared<FJsonObject>();
        Track->SetStringField(TEXT("bone"), Name.ToString());
        Track->SetNumberField(TEXT("keys"), Keys.Num());
        Track->SetStringField(TEXT("transform_sha1"), BytesToHex(Bytes, sizeof(Bytes)));
        Tracks.Add(MakeShared<FJsonValueObject>(Track));
    }
    auto Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("tracks"), MoveTemp(Tracks));
    Root->SetNumberField(TEXT("frames"), Model->GetNumberOfFrames());
    Root->SetNumberField(TEXT("keys"), Model->GetNumberOfKeys());
    Root->SetNumberField(TEXT("rate_numerator"), Model->GetFrameRate().Numerator);
    Root->SetNumberField(TEXT("rate_denominator"), Model->GetFrameRate().Denominator);
    Root->SetNumberField(TEXT("float_curves"), Model->GetNumberOfFloatCurves());
    Root->SetNumberField(TEXT("transform_curves"), Model->GetNumberOfTransformCurves());
    Root->SetNumberField(TEXT("attributes"), Model->GetNumberOfAttributes());
    Root->SetStringField(TEXT("model_guid"), Model->GenerateGuid().ToString());
    Root->SetStringField(TEXT("retarget_source"), Sequence->RetargetSource.ToString());
    TArray<TSharedPtr<FJsonValue>> ReferencePose;
    for (const FTransform& Pose : Sequence->RetargetSourceAssetReferencePose)
    {
        if (Pose.ContainsNaN()) return TEXT("ERROR: non-finite retarget reference pose");
        ReferencePose.Add(MakeShared<FJsonValueArray>(TransformValues(Pose)));
    }
    Root->SetArrayField(TEXT("retarget_reference_pose"), MoveTemp(ReferencePose));
    FString Result;
    if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result)))
        return TEXT("ERROR: could not serialize animation snapshot");
    return Result;
}
