#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "CSSEveLatticeRecipe.inl"

static int32 CreateEveLevelSet(const FString& Params)
{
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogCSSEvePanel,Error,TEXT("%s"),Why); return 1; };
    FString Output, Report, Recipe;
    FParse::Value(*Params,TEXT("LatticeRecipe="),Recipe);
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
    else if (!Recipe.IsEmpty())
    {
        Asset=ImportEveLatticeRecipe(Mesh,Output,Recipe);
        if (!Asset) return 1;
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
    Data->SetStringField(TEXT("lattice_recipe"),Recipe);
    if (!Inspect && Recipe.IsEmpty())
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
            if (FParse::Param(*Params,TEXT("LatticeGeometry")))
            {
                const int32 RootIndex=Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName);
                if (RootIndex==INDEX_NONE) return Fail(TEXT("Invalid lattice root"));
                const FTransform RootReference=FTransform(FMatrix(Mesh->GetRefBasesInvMatrix()[RootIndex])).Inverse();
                const auto& Grid=Volume->GetGrid();
                const auto Counts=Grid.Counts();
                auto Geometry=MakeShared<FJsonObject>();
                Geometry->SetArrayField(TEXT("counts"),{MakeShared<FJsonValueNumber>(Counts.X),MakeShared<FJsonValueNumber>(Counts.Y),MakeShared<FJsonValueNumber>(Counts.Z)});
                TArray<TSharedPtr<FJsonValue>> Nodes;
                for (int32 X=0;X<=Counts.X;++X)
                    for (int32 Y=0;Y<=Counts.Y;++Y)
                        for (int32 Z=0;Z<=Counts.Z;++Z)
                        {
                            const Chaos::TVec3<int32> Index(X,Y,Z);
                            const FVector Position=RootReference.TransformPosition(FVector(Grid.Node(Index)));
                            auto Node=MakeShared<FJsonObject>();
                            Node->SetArrayField(TEXT("index"),{MakeShared<FJsonValueNumber>(X),MakeShared<FJsonValueNumber>(Y),MakeShared<FJsonValueNumber>(Z)});
                            Node->SetArrayField(TEXT("rest_cm"),{MakeShared<FJsonValueNumber>(Position.X),MakeShared<FJsonValueNumber>(Position.Y),MakeShared<FJsonValueNumber>(Position.Z)});
                            const auto& Influence=Volume->GetBoneData()(Index);
                            TArray<TSharedPtr<FJsonValue>> Weights;
                            for (int32 I=0;I<Influence.NumInfluences;++I)
                                Weights.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                                    MakeShared<FJsonValueString>(Volume->GetUsedBones()[Influence.BoneIndices[I]].ToString()),
                                    MakeShared<FJsonValueNumber>(Influence.BoneWeights[I])}));
                            Node->SetArrayField(TEXT("weights"),Weights);
                            Nodes.Add(MakeShared<FJsonValueObject>(Node));
                        }
                Geometry->SetArrayField(TEXT("nodes"),Nodes);
                Row->SetObjectField(TEXT("lattice_geometry"),Geometry);
            }
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
                TArray<TSharedPtr<FJsonValue>> Traces;
                TSet<int32> TraceIndices;
                FString TraceText;
                if (FParse::Value(*Params,TEXT("TraceSamples="),TraceText,false))
                {
                    TArray<FString> Parts;
                    TraceText.ParseIntoArray(Parts,TEXT(","));
                    if (Parts.Num()>32) return Fail(TEXT("Too many trace samples"));
                    for (const FString& Part:Parts)
                    {
                        int32 Index=-1;
                        if (!LexTryParseString(Index,*Part) || !Positions->IsValidIndex(Index)) return Fail(TEXT("Invalid trace index"));
                        TraceIndices.Add(Index);
                    }
                }
                auto JsonVector=[](const FVector& V) {
                    return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};
                };
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
                        if (TraceIndices.Contains(PointIndex))
                        {
                            const auto Cell=Grid.Cell(RestLocal);
                            const Chaos::FVec3 Alpha=(RestLocal-Grid.Node(Cell))/Grid.Dx();
                            const Chaos::FWeightedLatticeImplicitObject::FEmbeddingCoordinate Embedding(Cell,Alpha);
                            const auto& Offsets=Embedding.TetrahedronOffsets();
                            auto Trace=MakeShared<FJsonObject>();
                            Trace->SetNumberField(TEXT("body_index"),PointIndex);
                            Trace->SetBoolField(TEXT("empty_cell"),Query->GetEmptyCells()(Cell));
                            Trace->SetArrayField(TEXT("rest_cm"),JsonVector(Point));
                            Trace->SetArrayField(TEXT("skin_cm"),JsonVector(Sample));
                            Trace->SetArrayField(TEXT("lattice_cm"),JsonVector(LatticePoint));
                            if (Weights) Trace->SetArrayField(TEXT("skin_weights"),(*Weights)[PointIndex]->AsArray());
                            TArray<TSharedPtr<FJsonValue>> Corners;
                            for (int32 Corner=0;Corner<4;++Corner)
                            {
                                const auto Node=Cell+Offsets[Corner];
                                const auto& Influence=Query->GetBoneData()(Node);
                                auto CornerData=MakeShared<FJsonObject>();
                                const double Bary=Corner<3?Embedding.BarycentricCoordinate[Corner]:1.-Embedding.BarycentricCoordinate.X-Embedding.BarycentricCoordinate.Y-Embedding.BarycentricCoordinate.Z;
                                CornerData->SetNumberField(TEXT("barycentric_weight"),Bary);
                                CornerData->SetArrayField(TEXT("rest_cm"),JsonVector(BindRootInverse.InverseTransformPosition(FVector(Grid.Node(Node)))));
                                CornerData->SetArrayField(TEXT("posed_cm"),JsonVector(RootInverse.InverseTransformPosition(FVector(Query->GetDeformedPoints()(Node)))));
                                TArray<TSharedPtr<FJsonValue>> NodeWeights;
                                for (int32 I=0;I<Influence.NumInfluences;++I)
                                    NodeWeights.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                                        MakeShared<FJsonValueString>(Query->GetUsedBones()[Influence.BoneIndices[I]].ToString()),
                                        MakeShared<FJsonValueNumber>(Influence.BoneWeights[I])}));
                                CornerData->SetArrayField(TEXT("weights"),NodeWeights);
                                Corners.Add(MakeShared<FJsonValueObject>(CornerData));
                            }
                            Trace->SetArrayField(TEXT("corners"),Corners);
                            Traces.Add(MakeShared<FJsonValueObject>(Trace));
                        }
                    }
                    else LatticePositions.Add(MakeShared<FJsonValueNull>());
                    const Chaos::FVec3 Local(RootInverse.TransformPosition(Sample));
                    Chaos::FVec3 Normal(0),ClothNormal(0);
                    Chaos::FWeightedLatticeImplicitObject::FEmbeddingCoordinate Coordinate;
                    const double Phi=Query->PhiWithNormal(Local,Normal);
                    const double ClothPhi=Query->PhiWithNormalAndSurfacePoint(Local,ClothNormal,Coordinate,false);
                    if (InsideGrid && TraceIndices.Contains(PointIndex))
                    {
                        const auto Trace=Traces.Last()->AsObject();
                        TArray<Chaos::FWeightedLatticeImplicitObject::FEmbeddingCoordinate> Embeddings;
                        Query->GetEmbeddingCoordinates(Local,Embeddings,false);
                        TArray<TSharedPtr<FJsonValue>> Candidates;
                        for (const auto& Embedding:Embeddings)
                        {
                            const auto RestPosition=Embedding.UndeformedPosition(Grid);
                            Chaos::FVec3 RestNormal;
                            const double RestPhi=Query->GetEmbeddedObject()->PhiWithNormal(RestPosition,RestNormal);
                            auto Candidate=MakeShared<FJsonObject>();
                            Candidate->SetArrayField(TEXT("rest_cm"),JsonVector(BindRootInverse.InverseTransformPosition(FVector(RestPosition))));
                            Candidate->SetNumberField(TEXT("rest_phi_cm"),RestPhi);
                            Candidates.Add(MakeShared<FJsonValueObject>(Candidate));
                        }
                        Trace->SetArrayField(TEXT("query_embeddings"),Candidates);
                        Trace->SetNumberField(TEXT("cloth_phi_cm"),ClothPhi);
                        if (Coordinate.IsValid())
                        {
                            Trace->SetArrayField(TEXT("query_surface_rest_cm"),JsonVector(BindRootInverse.InverseTransformPosition(FVector(Coordinate.UndeformedPosition(Grid)))));
                            Trace->SetArrayField(TEXT("query_surface_posed_cm"),JsonVector(RootInverse.InverseTransformPosition(FVector(Coordinate.DeformedPosition(Query->GetDeformedPoints())))));
                        }
                    }
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
                Row->SetArrayField(TEXT("trace_samples"),Traces);
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
