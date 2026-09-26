#include "CSSEveCollisionCommandlet.h"
#include "Animation/Skeleton.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

UCSSEveCollisionCommandlet::UCSSEveCollisionCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UCSSEveCollisionCommandlet::Main(const FString& Params)
{
    auto Fail = [](const TCHAR* Message) { UE_LOG(LogTemp, Error, TEXT("Eve collision: %s"), Message); return 1; };
    FString Input, Output, Text;
    if (FParse::Param(*Params, TEXT("Inspect")))
    {
        FString Report;
        if (!FParse::Value(*Params, TEXT("Output="), Output) || !Output.StartsWith(TEXT("/Game/CSS/EveTest/PA_")) ||
            !FParse::Value(*Params, TEXT("Report="), Report)) return Fail(TEXT("Inspect needs private Output asset and Report filename"));
        UPhysicsAsset* Asset = LoadObject<UPhysicsAsset>(nullptr,*Output);
        if (!Asset) return Fail(TEXT("Cannot load private collision asset"));
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& Body : Asset->SkeletalBodySetups)
        {
            auto Row = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(USkeletalBodySetup::StaticClass(), Body, Row, 0, CPF_Transient | CPF_Deprecated))
                return Fail(TEXT("Cannot inspect body"));
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        if (!FJsonSerializer::Serialize(Rows,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Report))
            return Fail(TEXT("Cannot write inspection report"));
        return 0;
    }
    if (!FParse::Value(*Params, TEXT("Input="), Input) || !FParse::Value(*Params, TEXT("Output="), Output) ||
        !Output.StartsWith(TEXT("/Game/CSS/EveTest/PA_")) || !FPackageName::IsValidLongPackageName(Output) ||
        FPackageName::DoesPackageExist(Output))
        return Fail(TEXT("Specify Input and an unused /Game/CSS/EveTest/PA_ package"));
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *Input) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
        return Fail(TEXT("Cannot read collision definition"));
    const TArray<TSharedPtr<FJsonValue>> *Spheres, *Connections;
    if (!Root->TryGetArrayField(TEXT("spheres"), Spheres) || Spheres->IsEmpty() || Spheres->Num() > 32 ||
        !Root->TryGetArrayField(TEXT("connections"), Connections) || Connections->Num() > 16)
        return Fail(TEXT("Expected 1..32 spheres and at most 16 connections"));
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/CSS/Shared/SKEL_Base.SKEL_Base"));
    if (!Skeleton) return Fail(TEXT("Shared skeleton missing"));
    struct FSphere { FName Bone; FVector Center; double Radius; };
    TArray<FSphere> Parsed;
    for (const auto& Value : *Spheres)
    {
        const TSharedPtr<FJsonObject>* Row;
        FString Bone;
        double Radius;
        const TArray<TSharedPtr<FJsonValue>>* Center;
        if (!Value->TryGetObject(Row) || !(*Row)->TryGetStringField(TEXT("bone"), Bone) ||
            !(*Row)->TryGetNumberField(TEXT("radius_cm"), Radius) || !FMath::IsFinite(Radius) || Radius <= 0 || Radius > 25 ||
            !(*Row)->TryGetArrayField(TEXT("local_center_cm"), Center) || Center->Num() != 3 ||
            Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*Bone)) == INDEX_NONE)
            return Fail(TEXT("Invalid sphere"));
        FVector Position;
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            double Number;
            if (!(*Center)[Axis]->TryGetNumber(Number) || !FMath::IsFinite(Number) || FMath::Abs(Number) > 200)
                return Fail(TEXT("Invalid sphere position"));
            Position[Axis] = Number;
        }
        Parsed.Add({FName(*Bone), Position, Radius});
    }
    TArray<TPair<int32,int32>> Pairs;
    TSet<int32> Connected;
    TSet<uint64> UniquePairs;
    for (const auto& Value : *Connections)
    {
        const TSharedPtr<FJsonObject>* Row;
        const TArray<TSharedPtr<FJsonValue>>* Indices;
        if (!Value->TryGetObject(Row) || !(*Row)->TryGetArrayField(TEXT("sphere_indices"), Indices) || Indices->Num() != 2)
            return Fail(TEXT("Invalid connection"));
        int32 Pair[2];
        for (int32 I = 0; I < 2; ++I)
        {
            double Number;
            if (!(*Indices)[I]->TryGetNumber(Number) || !FMath::IsFinite(Number) || Number < 0 || Number >= Parsed.Num() || FMath::FloorToDouble(Number) != Number)
                return Fail(TEXT("Invalid connection index"));
            Pair[I] = static_cast<int32>(Number);
        }
        const int32 A = FMath::Min(Pair[0],Pair[1]), B = FMath::Max(Pair[0],Pair[1]);
        const uint64 Key = (uint64(A) << 32) | uint64(B);
        if (A == B || Parsed[A].Bone != Parsed[B].Bone || UniquePairs.Contains(Key) ||
            FVector::Distance(Parsed[A].Center, Parsed[B].Center) <= FMath::Abs(Parsed[A].Radius-Parsed[B].Radius))
            return Fail(TEXT("Duplicate, cross-bone or degenerate capsule"));
        UniquePairs.Add(Key);
        Pairs.Emplace(A,B);
        Connected.Add(A); Connected.Add(B);
    }
    UPhysicsAsset* Asset = NewObject<UPhysicsAsset>(CreatePackage(*Output), *FPackageName::GetShortName(Output), RF_Public | RF_Standalone);
    TMap<FName, USkeletalBodySetup*> Bodies;
    for (const FSphere& Sphere : Parsed)
    {
        if (Bodies.Contains(Sphere.Bone)) continue;
        USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(Asset);
        Body->BoneName = Sphere.Bone;
        Body->PhysicsType = PhysType_Kinematic;
        Body->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Asset->SkeletalBodySetups.Add(Body);
        Bodies.Add(Sphere.Bone, Body);
    }
    for (int32 I = 0; I < Parsed.Num(); ++I)
    {
        if (Connected.Contains(I)) continue;
        FKSphereElem Sphere;
        Sphere.Center = Parsed[I].Center;
        Sphere.Radius = Parsed[I].Radius;
        Bodies[Parsed[I].Bone]->AggGeom.SphereElems.Add(Sphere);
    }
    for (const auto& Pair : Pairs)
    {
        const FSphere& A = Parsed[Pair.Key]; const FSphere& B = Parsed[Pair.Value];
        FKTaperedCapsuleElem Capsule;
        Capsule.Center = (A.Center+B.Center)*.5;
        Capsule.Length = FVector::Distance(A.Center,B.Center);
        Capsule.Rotation = FQuat::FindBetweenNormals(FVector::UpVector,(A.Center-B.Center).GetSafeNormal()).Rotator();
        Capsule.Radius0 = A.Radius;
        Capsule.Radius1 = B.Radius;
        Bodies[A.Bone]->AggGeom.TaperedCapsuleElems.Add(Capsule);
    }
    Asset->UpdateBodySetupIndexMap();
    Asset->UpdateBoundsBodiesArray();
    Asset->MarkPackageDirty();
    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    const FString File = FPackageName::LongPackageNameToFilename(Output,FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Asset->GetOutermost(),Asset,*File,Save)) return Fail(TEXT("Save failed"));
    UE_LOG(LogTemp, Display, TEXT("Eve collision saved %s: %d bodies, %d capsule connections. No mesh or skeleton saved."), *Output,Bodies.Num(),Pairs.Num());
    return 0;
}
