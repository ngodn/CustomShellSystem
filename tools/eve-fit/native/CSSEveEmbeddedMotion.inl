#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "ClothingSystemRuntimeTypes.h"
#include "PreviewScene.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"

static int32 EvaluateEmbeddedEveCloth(const FString& Params)
{
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogCSSEveCloth,Error,TEXT("Embedded motion: %s"),Why); return 1; };
    FString Report,Clip=TEXT("Sprint");
    const bool Secondary=FParse::Param(*Params,TEXT("Secondary"));
    FParse::Value(*Params,TEXT("Clip="),Clip);
    if (!FParse::Value(*Params,TEXT("Report="),Report) || IFileManager::Get().FileExists(*Report) ||
        (Clip!=TEXT("Walk") && Clip!=TEXT("Jog") && Clip!=TEXT("Sprint"))) return Fail(TEXT("Require unused report and supported clip"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CSS/EveTest/SK_PTailRun"));
    auto* Animation=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/CSS/Eve/Anim/AN_")+Clip));
    if (!Mesh || !Animation || Mesh->GetSkeleton()!=Animation->GetSkeleton() || Mesh->GetMeshClothingAssets().Num()!=1)
        return Fail(TEXT("Invalid private candidate or animation"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto* Wait=IConsoleManager::Get().FindConsoleVariable(TEXT("p.ClothPhysics.WaitForParallelClothTask"));
    if (!Wait || Wait->GetInt()!=0) return Fail(TEXT("Probe requires explicit end-of-step cloth waits"));
    FMemMark Memory(FMemStack::Get());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false).ShouldSimulatePhysics(true));
    auto* Component=NewObject<USkeletalMeshComponent>();
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetDisablePostProcessBlueprint(!Secondary);
    Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->bWaitForParallelClothTask=false;
    Scene.AddComponent(Component,FTransform::Identity);
    Component->InitAnim(true);
    Component->SetAnimation(Animation);
    Component->Stop();
    if (Secondary && !Component->GetPostProcessInstance()) return Fail(TEXT("Secondary graph did not initialize"));
    if (!Component->GetClothingSimulation()) return Fail(TEXT("Component has no cloth simulation"));
    auto Vec=[](const FVector& V) {
        auto O=MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("X"),V.X); O->SetNumberField(TEXT("Y"),V.Y); O->SetNumberField(TEXT("Z"),V.Z); return O;
    };
    const auto& Ref=Mesh->GetRefSkeleton();
    TArray<TSharedPtr<FJsonValue>> Frames;
    for (int32 Frame=0; Frame<65; ++Frame)
    {
        const double Time=Frame/60.;
        Component->SetPosition(float(FMath::Fmod(Time,double(Animation->GetPlayLength()))),false);
        Component->TickAnimation(Frame?1.f/60.f:0.f,true);
        Component->RefreshBoneTransforms();
        if (!Frame) Component->ForceClothNextUpdateTeleportAndReset();
        Component->TickClothing(1.f/60.f,Component->PrimaryComponentTick);
        Component->WaitForExistingParallelClothSimulation_GameThread();
        const auto& Sim=Component->GetCurrentClothingData_AnyThread();
        const auto* Data=Sim.Find(0);
        if (!Data || Data->Positions.Num()!=18 || Data->Normals.Num()!=18) return Fail(TEXT("Missing embedded simulation particles"));
        auto Row=MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("frame"),Frame); Row->SetNumberField(TEXT("time"),Time);
        TArray<TSharedPtr<FJsonValue>> Positions,Normals;
        for (int32 I=0; I<18; ++I)
        {
            const FVector P=Data->Transform.TransformPosition(FVector(Data->Positions[I]));
            const FVector N=Data->Transform.TransformVector(FVector(Data->Normals[I]));
            if (P.ContainsNaN() || N.ContainsNaN() || P.GetAbsMax()>10000) return Fail(TEXT("Invalid particle output"));
            Positions.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}));
            Normals.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(N.X),MakeShared<FJsonValueNumber>(N.Y),MakeShared<FJsonValueNumber>(N.Z)}));
        }
        Row->SetArrayField(TEXT("positions_cm"),Positions); Row->SetArrayField(TEXT("normals"),Normals);
        TArray<TSharedPtr<FJsonValue>> Names,Transforms;
        const auto Pose=Component->GetBoneSpaceTransformsView();
        for (int32 I=0; I<Ref.GetRawBoneNum(); ++I)
        {
            Names.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(I).ToString()));
            auto T=MakeShared<FJsonObject>();
            T->SetObjectField(TEXT("Translation"),Vec(Pose[I].GetTranslation()));
            T->SetObjectField(TEXT("Scale3D"),Vec(Pose[I].GetScale3D()));
            const auto Q=Pose[I].GetRotation(); auto R=Vec(FVector(Q.X,Q.Y,Q.Z)); R->SetNumberField(TEXT("W"),Q.W);
            T->SetObjectField(TEXT("Rotation"),R); Transforms.Add(MakeShared<FJsonValueObject>(T));
        }
        auto Snapshot=MakeShared<FJsonObject>(); Snapshot->SetArrayField(TEXT("BoneNames"),Names); Snapshot->SetArrayField(TEXT("LocalTransforms"),Transforms);
        auto PoseObject=MakeShared<FJsonObject>(); PoseObject->SetObjectField(TEXT("Snapshot"),Snapshot); Row->SetObjectField(TEXT("pose"),PoseObject);
        Frames.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Root=MakeShared<FJsonObject>(); Root->SetArrayField(TEXT("frames"),Frames);
    Root->SetStringField(TEXT("asset"),Mesh->GetPathName()); Root->SetStringField(TEXT("source_motion"),Report);
    Root->SetBoolField(TEXT("secondary_enabled"),Secondary);
    Root->SetStringField(TEXT("scope"),TEXT("Private skeletal component cloth tick, single-node clip; see secondary_enabled; no game acceptance"));
    FString Text;
    if (!FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Report)) return Fail(TEXT("Could not save report"));
    return 0;
}
