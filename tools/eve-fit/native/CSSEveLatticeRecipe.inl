static UPhysicsAsset* ImportEveLatticeRecipe(USkeletalMesh* Mesh, const FString& Output, const FString& Filename)
{
    auto Fail=[](const TCHAR* Why)->UPhysicsAsset* { UE_LOG(LogCSSEvePanel,Error,TEXT("%s"),Why); return nullptr; };
    FString Text;
    TSharedPtr<FJsonObject> Recipe;
    if (!FFileHelper::LoadFileToString(Text,*Filename) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Recipe))
        return Fail(TEXT("Cannot read lattice recipe"));
    FString SourcePath,Root;
    if (!Recipe->TryGetStringField(TEXT("source_asset"),SourcePath) ||
        SourcePath!=TEXT("/Game/CSS/EveTest/PA_CBody128.PA_CBody128") ||
        !Recipe->TryGetStringField(TEXT("root_bone"),Root)) return Fail(TEXT("Unexpected recipe source"));
    auto* Source=LoadObject<UPhysicsAsset>(nullptr,*SourcePath);
    if (!Source || Source->SkeletalBodySetups.Num()!=1 || !Source->ConstraintSetup.IsEmpty()) return Fail(TEXT("Invalid source collider"));
    const auto* Body=Source->SkeletalBodySetups[0].Get();
    if (Body->BoneName!=FName(*Root) || Body->AggGeom.SkinnedLevelSetElems.Num()!=1) return Fail(TEXT("Invalid source body"));
    const auto* Volume=Body->AggGeom.SkinnedLevelSetElems[0].WeightedLevelSet().GetReference();
    const int32 RootIndex=Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName);
    if (!Volume || RootIndex==INDEX_NONE) return Fail(TEXT("Invalid lattice root"));
    const FTransform RootReference=FTransform(FMatrix(Mesh->GetRefBasesInvMatrix()[RootIndex])).Inverse();
    const TSharedPtr<FJsonObject>* GridJson;
    const TArray<TSharedPtr<FJsonValue>> *CountsJson,*Nodes;
    if (!Recipe->TryGetObjectField(TEXT("grid"),GridJson) ||
        !(*GridJson)->TryGetArrayField(TEXT("counts"),CountsJson) || CountsJson->Num()!=3 ||
        !(*GridJson)->TryGetArrayField(TEXT("nodes"),Nodes)) return Fail(TEXT("Missing grid data"));
    Chaos::TVec3<int32> Counts;
    for (int32 I=0;I<3;++I)
    {
        double Value;
        if (!(*CountsJson)[I]->TryGetNumber(Value) || Value!=FMath::FloorToDouble(Value) || Value<1 || Value>128)
            return Fail(TEXT("Invalid grid counts"));
        Counts[I]=int32(Value);
    }
    const auto NodeCounts=Counts+Chaos::TVec3<int32>(1);
    if (Nodes->Num()!=NodeCounts.Product()) return Fail(TEXT("Missing lattice nodes"));
    Chaos::TUniformGrid<Chaos::FReal,3> Grid(Volume->GetGrid().MinCorner(),Volume->GetGrid().MaxCorner(),Counts);
    Chaos::TArrayND<Chaos::FWeightedLatticeInfluenceData,3> Influences(NodeCounts);
    TArray<FName> UsedBones;
    TArray<FTransform> Relative;
    TSet<int32> Seen;
    for (const auto& Value:*Nodes)
    {
        const auto Node=Value->AsObject();
        const TArray<TSharedPtr<FJsonValue>> *IndexJson,*Position,*Weights;
        if (!Node || !Node->TryGetArrayField(TEXT("index"),IndexJson) || IndexJson->Num()!=3 ||
            !Node->TryGetArrayField(TEXT("rest_cm"),Position) || Position->Num()!=3 ||
            !Node->TryGetArrayField(TEXT("weights"),Weights) || Weights->IsEmpty() || Weights->Num()>12)
            return Fail(TEXT("Malformed node"));
        Chaos::TVec3<int32> Index;
        FVector Rest;
        for (int32 I=0;I<3;++I)
        {
            double V,P;
            if (!(*IndexJson)[I]->TryGetNumber(V) || V!=FMath::FloorToDouble(V) || V<0 || V>Counts[I] ||
                !(*Position)[I]->TryGetNumber(P) || !FMath::IsFinite(P)) return Fail(TEXT("Invalid node coordinates"));
            Index[I]=int32(V); Rest[I]=P;
        }
        const int32 Flat=(Index.X*NodeCounts.Y+Index.Y)*NodeCounts.Z+Index.Z;
        if (Seen.Contains(Flat) || FVector::Distance(Rest,RootReference.TransformPosition(FVector(Grid.Node(Index))))>.001)
            return Fail(TEXT("Duplicate node or mismatched reference grid"));
        Seen.Add(Flat);
        auto& Influence=Influences(Index);
        double Total=0;
        TSet<FName> NodeBones;
        for (const auto& WeightValue:*Weights)
        {
            const TArray<TSharedPtr<FJsonValue>>* Pair;
            FString Name; double Weight;
            if (!WeightValue->TryGetArray(Pair) || Pair->Num()!=2 || !(*Pair)[0]->TryGetString(Name) ||
                !(*Pair)[1]->TryGetNumber(Weight) || !FMath::IsFinite(Weight) || Weight<=0 || Weight>1)
                return Fail(TEXT("Invalid node weight"));
            const FName Bone(*Name);
            const int32 BoneIndex=Mesh->GetRefSkeleton().FindBoneIndex(Bone);
            if (BoneIndex==INDEX_NONE || NodeBones.Contains(Bone)) return Fail(TEXT("Unknown or repeated bone"));
            NodeBones.Add(Bone);
            int32 UsedIndex=UsedBones.Find(Bone);
            if (UsedIndex==INDEX_NONE)
            {
                UsedIndex=UsedBones.Add(Bone);
                Relative.Add(RootReference*FTransform(FMatrix(Mesh->GetRefBasesInvMatrix()[BoneIndex])));
            }
            const int32 Slot=Influence.NumInfluences++;
            Influence.BoneIndices[Slot]=uint16(UsedIndex);
            Influence.BoneWeights[Slot]=float(Weight);
            Total+=Weight;
        }
        if (FMath::Abs(Total-1.)>1.e-5) return Fail(TEXT("Weights not normalized"));
    }
    auto Copy=Volume->GetEmbeddedObject()->CopyGeometry();
    auto* LevelSet=Copy->GetObject<Chaos::FLevelSet>();
    if (!LevelSet) return Fail(TEXT("Cannot copy source SDF"));
    TRefCountPtr<Chaos::FLevelSet> SDF(LevelSet);
    TRefCountPtr<Chaos::TWeightedLatticeImplicitObject<Chaos::FLevelSet>> Revised(
        new Chaos::TWeightedLatticeImplicitObject<Chaos::FLevelSet>(MoveTemp(SDF),MoveTemp(Grid),MoveTemp(Influences),MoveTemp(UsedBones),MoveTemp(Relative)));
    auto* Asset=DuplicateObject<UPhysicsAsset>(Source,CreatePackage(*Output),*FPackageName::GetShortName(Output));
    if (Asset->SkeletalBodySetups[0]==Source->SkeletalBodySetups[0]) return Fail(TEXT("Duplicate retained source body setup"));
    Asset->SetFlags(RF_Public|RF_Standalone);
    Asset->SkeletalBodySetups[0]->AggGeom.SkinnedLevelSetElems[0].SetWeightedLevelSet(MoveTemp(Revised));
    return Asset;
}
