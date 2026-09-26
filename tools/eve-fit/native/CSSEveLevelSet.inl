#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

static int32 CreateEveLevelSet(const FString& Params)
{
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogCSSEvePanel,Error,TEXT("%s"),Why); return 1; };
    FString Output, Report;
    const bool Inspect=FParse::Param(*Params,TEXT("InspectLevelSet"));
    if (!FParse::Value(*Params,TEXT("Output="),Output) ||
        !Output.StartsWith(TEXT("/Game/CSS/EveTest/PA_CBody")) ||
        !FPackageName::IsValidLongPackageName(Output) || (!Inspect && FPackageName::DoesPackageExist(Output)) ||
        !FParse::Value(*Params,TEXT("Report="),Report) || IFileManager::Get().FileExists(*Report))
        return Fail(TEXT("Require unused private PA_CBody output and report"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CSS/EveTest/SK_CBody.SK_CBody"));
    if (!Mesh) return Fail(TEXT("Missing verified body-only mesh"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    const auto* OriginalPhysics=Mesh->GetPhysicsAsset();
    FPhysAssetCreateParams Settings;
    Settings.GeomType=EFG_SkinnedLevelSet;
    Settings.VertWeight=EVW_AnyWeight;
    Settings.MinBoneSize=.01f;
    Settings.bCreateConstraints=false;
    Settings.LevelSetResolution=64;
    Settings.LatticeResolution=16;
    FParse::Value(*Params,TEXT("Grid="),Settings.LevelSetResolution);
    FParse::Value(*Params,TEXT("Lattice="),Settings.LatticeResolution);
    if (Settings.LevelSetResolution<8 || Settings.LevelSetResolution>128 ||
        Settings.LatticeResolution<8 || Settings.LatticeResolution>64)
        return Fail(TEXT("Grid or lattice outside trial bounds"));
    UPhysicsAsset* Asset=nullptr;
    if (Inspect)
    {
        const FString ObjectPath=Output+TEXT(".")+FPackageName::GetShortName(Output);
        Asset=LoadObject<UPhysicsAsset>(nullptr,*ObjectPath);
        if (!Asset) return Fail(TEXT("Cannot reload saved collider"));
    }
    else
    {
        auto* Package=CreatePackage(*Output);
        Asset=NewObject<UPhysicsAsset>(Package,*FPackageName::GetShortName(Output),RF_Public|RF_Standalone);
        FText Error;
        if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Asset,Mesh,Settings,Error,false,false))
            return Fail(*Error.ToString());
    }
    if (Mesh->GetPhysicsAsset()!=OriginalPhysics) return Fail(TEXT("Source physics changed"));
    auto Data=MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mesh"),Mesh->GetPathName());
    Data->SetStringField(TEXT("asset"),Asset->GetPathName());
    Data->SetStringField(TEXT("scope"),TEXT("Private collider generation only; coverage, motion and cost unverified"));
    Data->SetBoolField(TEXT("fresh_load"),Inspect);
    if (!Inspect)
    {
        Data->SetNumberField(TEXT("requested_grid"),Settings.LevelSetResolution);
        Data->SetNumberField(TEXT("requested_lattice"),Settings.LatticeResolution);
    }
    TArray<TSharedPtr<FJsonValue>> Bodies;
    int32 Count=0;
    for (const USkeletalBodySetup* Body:Asset->SkeletalBodySetups)
    {
        for (const auto& Element:Body->AggGeom.SkinnedLevelSetElems)
        {
            const auto* Volume=Element.WeightedLevelSet().GetReference();
            if (!Volume || Volume->GetUsedBones().IsEmpty()) return Fail(TEXT("Empty weighted level set"));
            auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("root_bone"),Body->BoneName.ToString());
            Row->SetStringField(TEXT("level_set_grid"),Element.LevelSetGridResolution().ToString());
            Row->SetStringField(TEXT("lattice_grid"),Element.LatticeGridResolution().ToString());
            TArray<TSharedPtr<FJsonValue>> Bones;
            for (FName Bone:Volume->GetUsedBones())
            {
                if (Mesh->GetRefSkeleton().FindBoneIndex(Bone)==INDEX_NONE) return Fail(TEXT("Unresolved collider bone"));
                Bones.Add(MakeShared<FJsonValueString>(Bone.ToString()));
            }
            Row->SetArrayField(TEXT("bones"),Bones);
            FString Samples;
            if (FParse::Value(*Params,TEXT("Samples="),Samples))
            {
                FString Source;
                TSharedPtr<FJsonObject> Input;
                const TArray<TSharedPtr<FJsonValue>>* Positions=nullptr;
                if (!Inspect || !FFileHelper::LoadFileToString(Source,*Samples) ||
                    !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source),Input) ||
                    !Input->TryGetArrayField(TEXT("positions"),Positions) || Positions->Num()>100000)
                    return Fail(TEXT("Sampling requires saved collider and valid positions JSON"));
                const auto& Ref=Mesh->GetRefSkeleton();
                const auto& Inv=Mesh->GetRefBasesInvMatrix();
                const int32 RootIndex=Ref.FindBoneIndex(Body->BoneName);
                if (RootIndex==INDEX_NONE || Inv.Num()!=Ref.GetNum()) return Fail(TEXT("Missing collider bind transforms"));
                TArray<FTransform> Pose;
                for (int32 Bone=0;Bone<Ref.GetNum();++Bone)
                    Pose.Add(FTransform(FMatrix(Inv[Bone])).Inverse());
                FString Motion;
                int32 FrameIndex=-1;
                const TArray<TSharedPtr<FJsonValue>>* Weights=nullptr;
                if (FParse::Value(*Params,TEXT("SampleMotion="),Motion))
                {
                    FParse::Value(*Params,TEXT("SampleFrame="),FrameIndex);
                    FString MotionText;
                    TSharedPtr<FJsonObject> MotionJson;
                    const TArray<TSharedPtr<FJsonValue>>* Frames=nullptr;
                    if (!FFileHelper::LoadFileToString(MotionText,*Motion) ||
                        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MotionText),MotionJson) ||
                        !MotionJson->TryGetArrayField(TEXT("frames"),Frames) || !Frames->IsValidIndex(FrameIndex) ||
                        !Input->TryGetArrayField(TEXT("weights"),Weights) || Weights->Num()!=Positions->Num())
                        return Fail(TEXT("Invalid motion frame or sample skin weights"));
                    const auto Snapshot=(*Frames)[FrameIndex]->AsObject()->GetObjectField(TEXT("pose"))->GetObjectField(TEXT("Snapshot"));
                    const auto& Names=Snapshot->GetArrayField(TEXT("BoneNames"));
                    const auto& Locals=Snapshot->GetArrayField(TEXT("LocalTransforms"));
                    if (Names.Num()!=Locals.Num()) return Fail(TEXT("Mismatched pose arrays"));
                    TSet<int32> Seen;
                    auto VectorOf=[](const TSharedPtr<FJsonObject>& O) {
                        return FVector(O->GetNumberField(TEXT("X")),O->GetNumberField(TEXT("Y")),O->GetNumberField(TEXT("Z")));
                    };
                    for (int32 I=0;I<Names.Num();++I)
                    {
                        const int32 Bone=Ref.FindBoneIndex(FName(*Names[I]->AsString()));
                        if (Bone==INDEX_NONE) continue;
                        if (Seen.Contains(Bone)) return Fail(TEXT("Duplicate sampled pose bone"));
                        Seen.Add(Bone);
                        const auto Local=Locals[I]->AsObject();
                        const auto Q=Local->GetObjectField(TEXT("Rotation"));
                        const FQuat Rotation(Q->GetNumberField(TEXT("X")),Q->GetNumberField(TEXT("Y")),Q->GetNumberField(TEXT("Z")),Q->GetNumberField(TEXT("W")));
                        const FVector Scale=VectorOf(Local->GetObjectField(TEXT("Scale3D")));
                        if (!Rotation.IsNormalized() || !Scale.Equals(FVector::OneVector,.0001)) return Fail(TEXT("Invalid sampled pose rotation or scale"));
                        Pose[Bone]=FTransform(Rotation,VectorOf(Local->GetObjectField(TEXT("Translation"))),Scale);
                        if (Pose[Bone].ContainsNaN()) return Fail(TEXT("Nonfinite sampled pose"));
                    }
                    if (Seen.Num()!=Ref.GetNum()) return Fail(TEXT("Incomplete sampled pose"));
                    for (int32 Bone=0;Bone<Ref.GetNum();++Bone)
                    {
                        const int32 Parent=Ref.GetParentIndex(Bone);
                        if (Parent>=Bone) return Fail(TEXT("Unexpected bone ordering"));
                        if (Parent>=0) Pose[Bone]=Pose[Bone]*Pose[Parent];
                    }
                }
                const FTransform RootInverse=Pose[RootIndex].Inverse();
                TArray<FTransform> Relative;
                for (FName Bone:Volume->GetUsedBones())
                    Relative.Add(Pose[Ref.FindBoneIndex(Bone)]*RootInverse);
                // Deform only a transient copy, using the same relative-bone convention as Chaos cloth.
                auto Copy=Volume->DeepCopyGeometry();
                auto* Query=Copy->GetObject<Chaos::TWeightedLatticeImplicitObject<Chaos::FLevelSet>>();
                if (!Query) return Fail(TEXT("Cannot copy weighted level set"));
                Query->DeformPoints(Relative);
                Query->UpdateSpatialHierarchy();
                TArray<TSharedPtr<FJsonValue>> Results;
                TArray<TSharedPtr<FJsonValue>> SamplePositions;
                TArray<TSharedPtr<FJsonValue>> LatticePositions;
                const FTransform BindRootInverse{FMatrix(Inv[RootIndex])};
                for (int32 PointIndex=0;PointIndex<Positions->Num();++PointIndex)
                {
                    const auto& V=(*Positions)[PointIndex]->AsArray();
                    if (V.Num()!=3) return Fail(TEXT("Invalid sample position"));
                    const FVector Point(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());
                    if (Point.ContainsNaN() || Point.GetAbsMax()>1000.) return Fail(TEXT("Invalid sample extent"));
                    FVector Sample=Point;
                    if (Weights)
                    {
                        Sample=FVector::ZeroVector;
                        double Sum=0;
                        for (const auto& Value:(*Weights)[PointIndex]->AsArray())
                        {
                            const auto& Pair=Value->AsArray();
                            if (Pair.Num()!=2) return Fail(TEXT("Invalid sample influence"));
                            const int32 Bone=Ref.FindBoneIndex(FName(*Pair[0]->AsString()));
                            const double Weight=Pair[1]->AsNumber();
                            if (Bone==INDEX_NONE || !FMath::IsFinite(Weight) || Weight<=0) return Fail(TEXT("Invalid sample bone weight"));
                            Sample+=Weight*Pose[Bone].TransformPosition(FTransform(FMatrix(Inv[Bone])).TransformPosition(Point));
                            Sum+=Weight;
                        }
                        if (!FMath::IsNearlyEqual(Sum,1.,.0001)) return Fail(TEXT("Unnormalized sample weights"));
                    }
                    SamplePositions.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                        MakeShared<FJsonValueNumber>(Sample.X),MakeShared<FJsonValueNumber>(Sample.Y),MakeShared<FJsonValueNumber>(Sample.Z)}));
                    const Chaos::FVec3 RestLocal(BindRootInverse.TransformPosition(Point));
                    const auto& Grid=Query->GetGrid();
                    bool InsideGrid=true;
                    for (int32 Axis=0;Axis<3;++Axis)
                        InsideGrid &= RestLocal[Axis]>Grid.MinCorner()[Axis] && RestLocal[Axis]<Grid.MaxCorner()[Axis];
                    if (InsideGrid)
                    {
                        const FVector LatticePoint=RootInverse.InverseTransformPosition(FVector(Query->GetDeformedPoint(RestLocal)));
                        LatticePositions.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                            MakeShared<FJsonValueNumber>(LatticePoint.X),MakeShared<FJsonValueNumber>(LatticePoint.Y),MakeShared<FJsonValueNumber>(LatticePoint.Z)}));
                    }
                    else LatticePositions.Add(MakeShared<FJsonValueNull>());
                    const Chaos::FVec3 Local(RootInverse.TransformPosition(Sample));
                    Chaos::FVec3 Normal(0),ClothNormal(0);
                    Chaos::FWeightedLatticeImplicitObject::FEmbeddingCoordinate Coordinate;
                    const double Phi=Query->PhiWithNormal(Local,Normal);
                    const double ClothPhi=Query->PhiWithNormalAndSurfacePoint(Local,ClothNormal,Coordinate,false);
                    const FVector WorldNormal=RootInverse.InverseTransformVectorNoScale(FVector(Normal));
                    TArray<TSharedPtr<FJsonValue>> Values={MakeShared<FJsonValueNumber>(Phi),MakeShared<FJsonValueNumber>(ClothPhi),
                        MakeShared<FJsonValueNumber>(WorldNormal.X),MakeShared<FJsonValueNumber>(WorldNormal.Y),MakeShared<FJsonValueNumber>(WorldNormal.Z)};
                    Results.Add(MakeShared<FJsonValueArray>(Values));
                }
                Row->SetStringField(TEXT("sample_scope"),TEXT("Single pose, [phi_cm, cloth_phi_cm, normal_x, normal_y, normal_z]; cloth query excludes empty cells; no cloth simulation"));
                Row->SetStringField(TEXT("sample_motion"),Motion);
                Row->SetNumberField(TEXT("sample_frame"),FrameIndex);
                Row->SetArrayField(TEXT("sample_positions_cm"),SamplePositions);
                Row->SetArrayField(TEXT("sample_lattice_positions_cm"),LatticePositions);
                Row->SetArrayField(TEXT("samples"),Results);
            }
            Bodies.Add(MakeShared<FJsonValueObject>(Row));
            ++Count;
        }
    }
    if (Count!=1 || !Asset->ConstraintSetup.IsEmpty()) return Fail(TEXT("Unexpected collider count or constraints"));
    Data->SetArrayField(TEXT("bodies"),Bodies);
    if (!Inspect)
    {
        FSavePackageArgs Save;
        Save.TopLevelFlags=RF_Public|RF_Standalone;
        Save.SaveFlags=SAVE_NoError;
        const FString Filename=FPackageName::LongPackageNameToFilename(Output,FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Save)) return Fail(TEXT("Collider save failed"));
    }
    FString Text;
    if (!FJsonSerializer::Serialize(Data,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Report))
        return Fail(TEXT("Collider report failed"));
    return 0;
}
