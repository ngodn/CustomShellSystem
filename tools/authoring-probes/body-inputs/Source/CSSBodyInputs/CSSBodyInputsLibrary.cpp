#include "CSSBodyInputsLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_ControlRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "Engine/SkeletalMesh.h"
#include "PreviewScene.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CSSBodyInputs)

FString UCSSBodyInputsLibrary::CompareRestoreSnapshot(USkeletalMesh* Mesh, UAnimBlueprint* Blueprint)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Mesh || !Blueprint ||
        !Blueprint->GeneratedClass || Mesh->GetSkeleton()!=Blueprint->TargetSkeleton ||
        !Mesh->GetPathName().StartsWith(TEXT("/Game/CSS/"))) return TEXT("ERROR: invalid isolated fixture");
    FMemMark Mark(FMemStack::Get());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto* Component=NewObject<USkeletalMeshComponent>();
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()),true);
    Scene.AddComponent(Component,FTransform::Identity);
    Component->InitAnim(true);
    TStrongObjectPtr<UAnimInstance> Source(Component->GetPostProcessInstance());
    if (!Source) return TEXT("ERROR: missing source");
    const TCHAR* Names[]={TEXT("CSSBodyRegionFrequencies"),TEXT("CSSBodyRegionDampingRatios"),TEXT("CSSBodyRegionMotionAmounts")};
    TArray<FArrayProperty*> Fields;
    for (const TCHAR* Name : Names)
    {
        auto* Field=FindFProperty<FArrayProperty>(Source->GetClass(),Name);
        if (!Field || !CastField<FFloatProperty>(Field->Inner)) return TEXT("ERROR: missing regional float array");
        FScriptArrayHelper Values(Field,Field->ContainerPtrToValuePtr<void>(Source.Get()));
        if (Values.Num()!=7) return TEXT("ERROR: wrong regional array length");
        Fields.Add(Field);
    }
    auto Capture=[&](UAnimInstance* Instance) {
        TArray<float> Result;
        for (auto* Field : Fields)
        {
            FScriptArrayHelper Values(Field,Field->ContainerPtrToValuePtr<void>(Instance));
            for (int32 I=0;I<7;++I) Result.Add(CastFieldChecked<FFloatProperty>(Field->Inner)->GetPropertyValue(Values.GetRawPtr(I)));
        }
        return Result;
    };
    auto Apply=[&](UAnimInstance* Instance,const TArray<float>& Values) {
        int32 Index=0;
        for (auto* Field : Fields)
        {
            FScriptArrayHelper Array(Field,Field->ContainerPtrToValuePtr<void>(Instance));
            for (int32 I=0;I<7;++I) CastFieldChecked<FFloatProperty>(Field->Inner)->SetPropertyValue(Array.GetRawPtr(I),Values[Index++]);
        }
    };
    TArray<float> Bouncy={1.7f,1.7f,1.65f,1.65f,1.85f,1.85f,1.75f,
        .28f,.28f,.3f,.3f,.38f,.38f,.32f,1.8f,1.8f,1.8f,1.8f,1.35f,1.35f,1.5f};
    TArray<float> Firm={2.6f,2.6f,2.5f,2.5f,2.8f,2.8f,2.7f,
        .65f,.65f,.65f,.65f,.72f,.72f,.68f,.6f,.6f,.65f,.65f,.4f,.4f,.45f};
    TArray<TSharedPtr<FJsonValue>> Cases;
    for (bool Refresh : {false,true})
    {
        Apply(Source.Get(),Bouncy);
        TArray<float> Physics=Capture(Source.Get());
        Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()),true);
        TStrongObjectPtr<UAnimInstance> Active(Component->GetPostProcessInstance());
        if (!Active || Active.Get()==Source.Get()) return TEXT("ERROR: source instance was not replaced");
        Apply(Active.Get(),Firm);
        Apply(Source.Get(),Capture(Active.Get()));
        if (Refresh) Physics=Capture(Active.Get());
        Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()),true);
        auto* Restored=Component->GetPostProcessInstance();
        if (!Restored || Restored==Active.Get()) return TEXT("ERROR: active instance was not replaced");
        Apply(Restored,Capture(Source.Get()));
        Apply(Restored,Physics);
        auto Row=MakeShared<FJsonObject>();
        Row->SetBoolField(TEXT("refresh_before_replace"),Refresh);
        Row->SetBoolField(TEXT("firm_preserved"),Capture(Restored)==Firm);
        Row->SetNumberField(TEXT("chest_frequency"),Capture(Restored)[0]);
        Row->SetNumberField(TEXT("chest_damping"),Capture(Restored)[7]);
        Row->SetNumberField(TEXT("chest_motion"),Capture(Restored)[14]);
        Cases.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Result=MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("cases"),Cases);
    Result->SetStringField(TEXT("scope"),TEXT("Native instance replacement and regional snapshot ordering; not CSS core or a performance adapter"));
    FString Json;
    FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    return Json;
}

FString UCSSBodyInputsLibrary::Evaluate(USkeletalMesh* Mesh, UAnimBlueprint* Blueprint, int32 Fps)
{
    auto Fail=[](const FString& Why) { return FString(TEXT("ERROR: "))+Why; };
    if (!IsRunningCommandlet() || !IsInGameThread() || !Mesh || !Blueprint ||
        !Blueprint->GeneratedClass || Mesh->GetSkeleton()!=Blueprint->TargetSkeleton ||
        !Mesh->GetPathName().StartsWith(TEXT("/Game/CSS/")) ||
        (Fps!=30 && Fps!=60 && Fps!=120)) return Fail(TEXT("Invalid isolated fixture"));
    FMemMark Mark(FMemStack::Get());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto* Component=NewObject<USkeletalMeshComponent>();
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()),true);
    Component->SetDisablePostProcessBlueprint(false);
    Component->bDisableClothSimulation=true;
    Scene.AddComponent(Component,FTransform::Identity);
    Component->InitAnim(true);
    auto* Instance=Component->GetPostProcessInstance();
    if (!Instance) return Fail(TEXT("No post-process instance"));
    auto* Mode=FindFProperty<FBoolProperty>(Instance->GetClass(),TEXT("CSSBodyUseRegionSettings"));
    auto* Epoch=FindFProperty<FIntProperty>(Instance->GetClass(),TEXT("CSSBodyResetEpoch"));
    auto* Filter=FindFProperty<FArrayProperty>(FAnimNode_ControlRigBase::StaticStruct(),TEXT("OutputBonesToTransfer"));
    if (!Mode || !Epoch || !Filter) return Fail(TEXT("Missing body contract"));
    const TCHAR* Fields[]={TEXT("RegionFrequencies"),TEXT("RegionDampingRatios"),TEXT("RegionMotionAmounts"),TEXT("RegionEnabled")};
    TArray<FArrayProperty*> Arrays;
    for (int32 Field=0;Field<4;++Field)
    {
        auto* Property=FindFProperty<FArrayProperty>(Instance->GetClass(),FName(*(FString(TEXT("CSSBody"))+Fields[Field])));
        if (!Property || (Field<3 ? !CastField<FFloatProperty>(Property->Inner) : !CastField<FBoolProperty>(Property->Inner)))
            return Fail(TEXT("Wrong regional field type"));
        FScriptArrayHelper Values(Property,Property->ContainerPtrToValuePtr<void>(Instance));
        if (Values.Num()!=7) return Fail(TEXT("Wrong region count"));
        Arrays.Add(Property);
    }
    const float Firm[7][3]={{2.6f,.65f,.6f},{2.6f,.65f,.6f},{2.5f,.65f,.65f},{2.5f,.65f,.65f},
        {2.8f,.72f,.4f},{2.8f,.72f,.4f},{2.7f,.68f,.45f}};
    const float Natural[7][3]={{2.15f,.48f,1.f},{2.15f,.48f,1.f},{2.1f,.5f,1.f},{2.1f,.5f,1.f},
        {2.35f,.58f,.75f},{2.35f,.58f,.75f},{2.2f,.52f,.85f}};
    TArray<TSharedPtr<FJsonValue>> Samples;
    for (int32 Frame=0;Frame<=8*Fps;++Frame)
    {
        const int32 Phase=FMath::Min(Frame/(2*Fps),3);
        const auto& Expected=Phase==1 ? Natural : Firm;
        // Write once at each transition, so an update-time overwrite cannot be hidden.
        if (Frame%(2*Fps)==0 && Frame<8*Fps)
        {
            Mode->SetPropertyValue_InContainer(Instance,true);
            Epoch->SetPropertyValue_InContainer(Instance,Phase+1);
            for (int32 Field=0;Field<4;++Field)
            {
                FScriptArrayHelper Values(Arrays[Field],Arrays[Field]->ContainerPtrToValuePtr<void>(Instance));
                for (int32 Region=0;Region<7;++Region)
                    if (Field<3) CastFieldChecked<FFloatProperty>(Arrays[Field]->Inner)->SetPropertyValue(Values.GetRawPtr(Region),Expected[Region][Field]);
                    else CastFieldChecked<FBoolProperty>(Arrays[Field]->Inner)->SetPropertyValue(Values.GetRawPtr(Region),Phase!=2);
            }
        }
        const double Time=double(Frame)/Fps;
        Component->SetWorldLocation(FVector(0,0,Time<7 ? 10*FMath::Sin(2*UE_PI*Time) : 0));
        Component->TickAnimation(Frame ? 1.f/Fps : 0.f,true);
        Component->RefreshBoneTransforms();
        if (Component->GetPostProcessInstance()!=Instance) return Fail(TEXT("Unexpected instance replacement"));
        UControlRig* Body=nullptr;
        int32 RigCount=0;
        for (TFieldIterator<FStructProperty> It(Instance->GetClass());It;++It)
        {
            if (It->Struct!=FAnimNode_ControlRig::StaticStruct()) continue;
            ++RigCount;
            auto* Node=It->ContainerPtrToValuePtr<FAnimNode_ControlRig>(Instance);
            const auto& Bones=*Filter->ContainerPtrToValuePtr<TArray<FBoneReference>>(Node);
            if (Bones.Num()==7)
            {
                if (Body) return Fail(TEXT("Multiple body rig nodes"));
                Body=Node->GetControlRig();
            }
        }
        if (!Body) return Fail(TEXT("Body rig did not initialize"));
        auto* BodyMode=FindFProperty<FBoolProperty>(Body->GetClass(),TEXT("UseRegionSettings"));
        if (!Mode->GetPropertyValue_InContainer(Instance) || !BodyMode || !BodyMode->GetPropertyValue_InContainer(Body))
            return Fail(FString::Printf(TEXT("Region switch lost at frame %d"),Frame));
        for (int32 Field=0;Field<4;++Field)
        {
            auto* Variable=FindFProperty<FArrayProperty>(Body->GetClass(),Fields[Field]);
            if (!Variable || !Arrays[Field]->SameType(Variable) ||
                !Arrays[Field]->Identical(Arrays[Field]->ContainerPtrToValuePtr<void>(Instance),Variable->ContainerPtrToValuePtr<void>(Body),PPF_None))
                return Fail(FString::Printf(TEXT("Rig transfer differs: %s frame %d"),Fields[Field],Frame));
            FScriptArrayHelper Values(Arrays[Field],Arrays[Field]->ContainerPtrToValuePtr<void>(Instance));
            for (int32 Region=0;Region<7;++Region)
            {
                const bool Matches=Field<3 ?
                    CastFieldChecked<FFloatProperty>(Arrays[Field]->Inner)->GetPropertyValue(Values.GetRawPtr(Region))==Expected[Region][Field] :
                    CastFieldChecked<FBoolProperty>(Arrays[Field]->Inner)->GetPropertyValue(Values.GetRawPtr(Region))==(Phase!=2);
                if (!Matches) return Fail(FString::Printf(TEXT("Public input overwritten at frame %d"),Frame));
            }
        }
        auto Row=MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("frame"),Frame);
        Row->SetNumberField(TEXT("phase"),Phase);
        Row->SetNumberField(TEXT("rig_count"),RigCount);
        for (const TCHAR* Name : {TEXT("SeenUpdate"),TEXT("ResetCount"),TEXT("TotalSteps"),TEXT("Substeps")})
            Row->SetNumberField(Name,FCString::Atoi(*Body->GetVariableAsString(Name)));
        Samples.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Result=MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"),true);
    Result->SetNumberField(TEXT("fps"),Fps);
    Result->SetStringField(TEXT("mesh"),Mesh->GetPathName());
    Result->SetStringField(TEXT("blueprint"),Blueprint->GetPathName());
    Result->SetArrayField(TEXT("samples"),Samples);
    FString Json;
    FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    return Json;
}
