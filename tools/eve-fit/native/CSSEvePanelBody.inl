#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/SkinnedTriangleMeshElem.h"
#include "Chaos/SkinnedTriangleMesh.h"

static UPhysicsAsset* MakePanelBodyCollision(const FString& Input, USkeletalMesh* Mesh)
{
    FString Text;
    TSharedPtr<FJsonObject> Json;
    if (!FFileHelper::LoadFileToString(Text,*Input) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json) || !Json.IsValid()) return nullptr;
    const TArray<TSharedPtr<FJsonValue>> *Points=nullptr,*Weights=nullptr,*Faces=nullptr;
    if (!Json->TryGetArrayField(TEXT("positions"),Points) || !Json->TryGetArrayField(TEXT("weights"),Weights) ||
        !Json->TryGetArrayField(TEXT("indices"),Faces) || Points->Num()!=Weights->Num() ||
        Points->Num()<3 || Points->Num()>100000 || Faces->Num()<1 || Faces->Num()>200000) return nullptr;
    const auto& Ref=Mesh->GetRefSkeleton();
    const auto& Inverse=Mesh->GetRefBasesInvMatrix();
    if (Inverse.Num()!=Ref.GetNum()) return nullptr;
    const FTransform RootInverse{FMatrix(Inverse[0])};
    FTransform RootReference=RootInverse.Inverse();
    TArray<FName> Names;
    TArray<FTransform> Relative;
    for (int32 Bone=0;Bone<Ref.GetNum();++Bone)
    {
        Names.Add(Ref.GetBoneName(Bone));
        Relative.Add(RootReference*FTransform(FMatrix(Inverse[Bone])));
    }
    TArray<Chaos::FVec3f> Positions;
    TArray<Chaos::FWeightedInfluenceData> BoneData;
    for (int32 I=0;I<Points->Num();++I)
    {
        const auto& P=(*Points)[I]->AsArray();
        if (P.Num()!=3) return nullptr;
        FVector Position(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber());
        if (Position.ContainsNaN() || Position.GetAbsMax()>1000.) return nullptr;
        Positions.Add(Chaos::FVec3f(RootInverse.TransformPosition(Position)));
        auto& Data=BoneData.AddDefaulted_GetRef();
        const auto& Row=(*Weights)[I]->AsArray();
        if (Row.IsEmpty() || Row.Num()>Chaos::FWeightedInfluenceData::MaxTotalInfluences) return nullptr;
        float Sum=0;
        for (const auto& Value:Row)
        {
            const auto& Pair=Value->AsArray();
            if (Pair.Num()!=2) return nullptr;
            const int32 Bone=Ref.FindBoneIndex(FName(*Pair[0]->AsString()));
            const float Weight=Pair[1]->AsNumber();
            if (Bone==INDEX_NONE || !FMath::IsFinite(Weight) || Weight<=0) return nullptr;
            Data.BoneIndices[Data.NumInfluences]=uint16(Bone);
            Data.BoneWeights[Data.NumInfluences++]=Weight;
            Sum+=Weight;
        }
        if (!FMath::IsNearlyEqual(Sum,1.f,.0001f)) return nullptr;
    }
    TArray<Chaos::TVec3<int32>> Triangles;
    for (const auto& Value:*Faces)
    {
        const auto& Row=Value->AsArray();
        if (Row.Num()!=3) return nullptr;
        Chaos::TVec3<int32> Triangle;
        for (int32 J=0;J<3;++J)
        {
            const double Index=Row[J]->AsNumber();
            if (!FMath::IsFinite(Index) || Index<0 || Index>=Positions.Num() || Index!=FMath::FloorToDouble(Index)) return nullptr;
            Triangle[J]=int32(Index);
        }
        if (Triangle[0]==Triangle[1] || Triangle[1]==Triangle[2] || Triangle[0]==Triangle[2]) return nullptr;
        Triangles.Add(Triangle);
    }
    Chaos::FTriangleMesh TriMesh(MoveTemp(Triangles),0,Positions.Num());
    TRefCountPtr<Chaos::FSkinnedTriangleMesh> Geometry(new Chaos::FSkinnedTriangleMesh(
        MoveTemp(TriMesh),MoveTemp(Positions),MoveTemp(BoneData),MoveTemp(Names),MoveTemp(RootReference),MoveTemp(Relative)));
    auto* Physics=NewObject<UPhysicsAsset>(GetTransientPackage(),NAME_None,RF_Transient);
    auto* Body=NewObject<USkeletalBodySetup>(Physics,NAME_None,RF_Transient);
    Body->BoneName=Ref.GetBoneName(0);
    auto& Shape=Body->AggGeom.SkinnedTriangleMeshElems.AddDefaulted_GetRef();
    Shape.SetSkinnedTriangleMesh(MoveTemp(Geometry));
    Physics->SkeletalBodySetups.Add(Body);
    Physics->UpdateBodySetupIndexMap();
    UE_LOG(LogCSSEvePanel,Display,TEXT("Transient body collider: %d vertices, %d triangles"),Points->Num(),Faces->Num());
    return Physics;
}
