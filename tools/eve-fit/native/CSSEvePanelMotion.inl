#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothAssetInteractor.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "Components/PoseableMeshComponent.h"
#include "PreviewScene.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeExit.h"
#include "CSSEvePanelBody.inl"

static int32 EvaluateHolidayPanel(const FString& Params)
{
    auto Fail=[](const TCHAR* Message) { UE_LOG(LogCSSEvePanel,Error,TEXT("Panel motion: %s"),Message); return 1; };
    FString Input,Report,Text,BodyInput;
    FParse::Value(*Params,TEXT("Body="),BodyInput);
    const bool NoBodyCollision=FParse::Param(*Params,TEXT("NoBodyCollision"));
    const bool NoSelfCollision=FParse::Param(*Params,TEXT("NoSelfCollision"));
    const bool UseCCD=FParse::Param(*Params,TEXT("CCD"));
    int32 Substeps=0;
    FParse::Value(*Params,TEXT("Substeps="),Substeps);
    if (Substeps<0 || Substeps>16) return Fail(TEXT("Substeps must be 0 (asset default) or 1..16"));
    int32 Count=9;
    int32 Iterations=0;
    FParse::Value(*Params,TEXT("Iterations="),Iterations);
    if (Iterations<0 || Iterations>16) return Fail(TEXT("Iterations must be 0 (asset default) or 1..16"));
    FParse::Value(*Params,TEXT("Frames="),Count);
    if (!FParse::Value(*Params,TEXT("Input="),Input) || !FParse::Value(*Params,TEXT("Report="),Report) ||
        Count<2 || Count>300 || IFileManager::Get().FileExists(*Report)) return Fail(TEXT("Require input, unused report and 2..300 frames"));
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text,*Input) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid()) return Fail(TEXT("Invalid pose JSON"));
    const TArray<TSharedPtr<FJsonValue>>* Frames=nullptr;
    if (!Root->TryGetArrayField(TEXT("frames"),Frames) || Frames->Num()<Count) return Fail(TEXT("Missing frames"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CSS/EveTest/SK_Holiday.SK_Holiday"));
    auto* Asset=LoadObject<UChaosClothAsset>(nullptr,TEXT("/Game/CSS/EveTest/CA_Holiday.CA_Holiday"));
    if (!Mesh || !Asset) return Fail(TEXT("Private reference assets missing"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!Asset->HasValidClothSimulationModels() || Asset->GetClothCollections().Num()!=1) return Fail(TEXT("Invalid panel simulation"));
    const UE::Chaos::ClothAsset::FCollectionClothConstFacade Collection(Asset->GetClothCollections()[0]);
    UPhysicsAsset* OriginalPhysics=Asset->GetPhysicsAsset();
    ON_SCOPE_EXIT { Asset->SetPhysicsAsset(OriginalPhysics); };
    if (NoBodyCollision) Asset->SetPhysicsAsset(nullptr);
    if (!BodyInput.IsEmpty())
    {
        if (NoBodyCollision) return Fail(TEXT("Body and NoBodyCollision are mutually exclusive"));
        UPhysicsAsset* BodyPhysics=MakePanelBodyCollision(BodyInput,Mesh);
        if (!BodyPhysics) return Fail(TEXT("Invalid transient body collision input"));
        Asset->SetPhysicsAsset(BodyPhysics);
    }
    FMemMark Memory(FMemStack::Get());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto* Pose=NewObject<UPoseableMeshComponent>();
    Pose->SetSkinnedAssetAndUpdate(Mesh);
    Scene.AddComponent(Pose,FTransform::Identity);
    auto* Cloth=NewObject<UChaosClothComponent>();
    Cloth->SetAsset(Asset);
    Cloth->SetupAttachment(Pose);
    Cloth->SetEnableSimulation(true);
    Cloth->SetSimulateInEditor(true);
    Scene.AddComponent(Cloth,FTransform::Identity);
    Cloth->SetLeaderPoseComponent(Pose,true);
    if (Iterations)
    {
        auto* Interactor=Cloth->GetClothOutfitInteractor();
        if (!Interactor) return Fail(TEXT("Missing property interactor"));
        Interactor->SetIntValue(TEXT("NumIterations"),-1,Iterations);
        Interactor->SetIntValue(TEXT("MaxNumIterations"),-1,Iterations);
        if (Interactor->GetIntValue(TEXT("NumIterations"))!=Iterations) return Fail(TEXT("Iteration override failed"));
    }
    auto* Properties=Cloth->GetClothOutfitInteractor();
    if (!Properties) return Fail(TEXT("Missing property interactor"));
    if (UseCCD)
    {
        Properties->SetIntValue(TEXT("UseCCD"),-1,1);
        if (Properties->GetIntValue(TEXT("UseCCD"),0,-1)!=1) return Fail(TEXT("CCD override failed"));
    }
    if (Substeps)
    {
        Properties->SetIntValue(TEXT("NumSubsteps"),-1,Substeps);
        if (Properties->GetIntValue(TEXT("NumSubsteps"),0,-1)!=Substeps) return Fail(TEXT("Substep override failed"));
    }
    if (NoSelfCollision)
    {
        Cloth->WaitForExistingParallelClothSimulation_GameThread();
        Properties->SetIntValue(TEXT("UseSelfCollisions"),-1,0);
        if (Properties->GetIntValue(TEXT("UseSelfCollisions"),0,-1)!=0) return Fail(TEXT("Self-collision override failed"));
        Cloth->RecreateClothSimulationProxy();
    }
    const auto& Ref=Mesh->GetRefSkeleton();
    if (Pose->BoneSpaceTransforms.Num()!=Ref.GetNum()) return Fail(TEXT("Pose component has wrong bone count"));
    auto ReadVector=[](const TSharedPtr<FJsonObject>& O) {
        return FVector(O->GetNumberField(TEXT("X")),O->GetNumberField(TEXT("Y")),O->GetNumberField(TEXT("Z")));
    };
    TArray<TSharedPtr<FJsonValue>> Rows;
    double PreviousTime=0;
    for (int32 Index=0;Index<Count;++Index)
    {
        const auto Frame=(*Frames)[Index]->AsObject();
        const auto Snapshot=Frame->GetObjectField(TEXT("pose"))->GetObjectField(TEXT("Snapshot"));
        const auto& Names=Snapshot->GetArrayField(TEXT("BoneNames"));
        const auto& Transforms=Snapshot->GetArrayField(TEXT("LocalTransforms"));
        if (Names.Num()!=Transforms.Num()) return Fail(TEXT("Mismatched pose arrays"));
        TSet<int32> Seen;
        for (int32 I=0;I<Names.Num();++I)
        {
            const int32 Bone=Ref.FindBoneIndex(FName(*Names[I]->AsString()));
            if (Bone==INDEX_NONE) continue;
            if (Seen.Contains(Bone)) return Fail(TEXT("Duplicate pose bone"));
            Seen.Add(Bone);
            const auto Entry=Transforms[I]->AsObject();
            const auto Rotation=Entry->GetObjectField(TEXT("Rotation"));
            FQuat Q(Rotation->GetNumberField(TEXT("X")),Rotation->GetNumberField(TEXT("Y")),
                Rotation->GetNumberField(TEXT("Z")),Rotation->GetNumberField(TEXT("W")));
            FTransform T(Q,ReadVector(Entry->GetObjectField(TEXT("Translation"))),ReadVector(Entry->GetObjectField(TEXT("Scale3D"))));
            if (T.ContainsNaN() || !Q.IsNormalized()) return Fail(TEXT("Invalid recorded transform"));
            Pose->BoneSpaceTransforms[Bone]=T;
        }
        if (Seen.Num()!=Ref.GetNum()) return Fail(TEXT("Incomplete recorded pose"));
        Pose->MarkRefreshTransformDirty();
        Pose->RefreshBoneTransforms();
        const double Time=Frame->GetNumberField(TEXT("time"));
        const float Dt=Index?float(Time-PreviousTime):1.f/60.f;
        if (Dt<0.f || Dt>.1f) return Fail(TEXT("Unexpected pose interval"));
        if (!Index) Cloth->ForceNextUpdateTeleportAndReset();
        static_cast<UActorComponent*>(Cloth)->TickComponent(Dt,LEVELTICK_All,nullptr);
        Cloth->WaitForExistingParallelClothSimulation_GameThread();
        const auto* Proxy=Cloth->GetClothSimulationProxy();
        if (!Proxy) return Fail(TEXT("Missing simulation proxy"));
        const auto& Data=Proxy->GetCurrentSimulationData_AnyThread();
        const auto* Sim=Data.Find(0);
        if (!Sim || Sim->Positions.Num()!=Collection.GetNumSimVertices3D()) return Fail(TEXT("Unexpected simulation particle count"));
        TArray<TSharedPtr<FJsonValue>> Positions;
        for (const FVector3f& P:Sim->Positions)
        {
            const FVector World=Sim->Transform.TransformPosition(FVector(P));
            if (World.ContainsNaN() || World.GetAbsMax()>10000.) return Fail(TEXT("Unbounded simulation position"));
            TArray<TSharedPtr<FJsonValue>> XYZ={MakeShared<FJsonValueNumber>(World.X),MakeShared<FJsonValueNumber>(World.Y),MakeShared<FJsonValueNumber>(World.Z)};
            Positions.Add(MakeShared<FJsonValueArray>(XYZ));
        }
        auto Row=MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("frame"),Index); Row->SetNumberField(TEXT("time"),Time);
        Row->SetNumberField(TEXT("simulation_dt"),Dt);
        Row->SetNumberField(TEXT("dynamic_particles"),Proxy->GetNumDynamicParticles());
        Row->SetNumberField(TEXT("kinematic_particles"),Proxy->GetNumKinematicParticles());
        Row->SetNumberField(TEXT("iterations"),Proxy->GetNumIterations());
        Row->SetNumberField(TEXT("substeps"),Proxy->GetNumSubsteps());
        Row->SetNumberField(TEXT("simulation_ms"),Proxy->GetSimulationTime());
        Row->SetArrayField(TEXT("positions_cm"),Positions);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        UE_LOG(LogCSSEvePanel,Display,TEXT("Panel frame %d: %d dynamic, %d kinematic, %d positions"),Index,
            Proxy->GetNumDynamicParticles(),Proxy->GetNumKinematicParticles(),Positions.Num());
        PreviousTime=Time;
    }
    auto Result=MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("scope"),TEXT("Actual saved CA_Holiday Chaos component simulation on recorded original-graph poses. Old reference asset geometry/weights; no F11, morph, material or game acceptance."));
    Result->SetStringField(TEXT("source_motion"),Input);
    Result->SetStringField(TEXT("asset"),Asset->GetPathName());
    Result->SetStringField(TEXT("body_collision_input"),BodyInput);
    Result->SetNumberField(TEXT("requested_iterations"),Iterations);
    Result->SetNumberField(TEXT("requested_substeps"),Substeps);
    Result->SetNumberField(TEXT("ccd_property"),Properties->GetIntValue(TEXT("UseCCD"),0,-1));
    Result->SetBoolField(TEXT("diagnostic_no_body_collision"),NoBodyCollision);
    Result->SetBoolField(TEXT("diagnostic_no_self_collision"),NoSelfCollision);
    Result->SetStringField(TEXT("physics_asset_during_run"),GetPathNameSafe(Asset->GetPhysicsAsset()));
    Result->SetNumberField(TEXT("self_collision_property"),Properties->GetIntValue(TEXT("UseSelfCollisions"),0,-1));
    Result->SetArrayField(TEXT("frames"),Rows);
    Scene.RemoveComponent(Cloth); Scene.RemoveComponent(Pose);
    return FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text)) && FFileHelper::SaveStringToFile(Text,*Report)
        ?0:Fail(TEXT("Cannot save motion report"));
}
