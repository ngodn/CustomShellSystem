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
#include "Kismet2/BlueprintEditorUtils.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CSSBodyInputs)

FString UCSSBodyInputsLibrary::MeasureBodyCost(USkeletalMesh* Mesh, UAnimBlueprint* Blueprint,
    int32 Fps, bool BodyEnabled, int32 Frames)
{
    auto Fail=[](const FString& Why) { return FString(TEXT("ERROR: "))+Why; };
    if (!IsRunningCommandlet() || !IsInGameThread() || !Mesh || !Blueprint ||
        !Blueprint->GeneratedClass || Mesh->GetSkeleton()!=Blueprint->TargetSkeleton ||
        !Mesh->GetPathName().StartsWith(TEXT("/Game/CSS/")) ||
        (Fps!=30 && Fps!=60 && Fps!=120) || Frames<120 || Frames>3600)
        return Fail(TEXT("Invalid isolated timing fixture"));
    FMemMark Mark(FMemStack::Get());
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    auto* Component=NewObject<USkeletalMeshComponent>();
    Component->SetSkeletalMesh(Mesh);
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Component->SetOverridePostProcessAnimBP(TSubclassOf<UAnimInstance>(Blueprint->GeneratedClass.Get()),true);
    Component->SetDisablePostProcessBlueprint(false);
    Component->bDisableClothSimulation=true;
    Component->bEnableUpdateRateOptimizations=false;
    Scene.AddComponent(Component,FTransform::Identity);
    Component->InitAnim(true);
    auto* Instance=Component->GetPostProcessInstance();
    if (!Instance) return Fail(TEXT("No post-process instance"));
    auto* Enable=FindFProperty<FBoolProperty>(Instance->GetClass(),TEXT("CSSBodyEnabled"));
    auto* Filter=FindFProperty<FArrayProperty>(FAnimNode_ControlRigBase::StaticStruct(),TEXT("OutputBonesToTransfer"));
    if (!Enable || !Filter) return Fail(TEXT("Missing body contract"));
    Enable->SetPropertyValue_InContainer(Instance,BodyEnabled);
    TArray<TSharedPtr<FJsonValue>> Samples;
    UControlRig* Body=nullptr;
    int32 RigCount=0, PreviousSteps=-1;
    const int32 Warmup=2*Fps;
    for (int32 Frame=0; Frame<Warmup+Frames; ++Frame)
    {
        Component->SetWorldLocation(FVector(0,0,10*FMath::Sin(2*UE_PI*double(Frame)/Fps)));
        const double Begin=FPlatformTime::Seconds();
        Component->TickAnimation(Frame ? 1.f/Fps : 0.f,true);
        const double Updated=FPlatformTime::Seconds();
        // No tick function is supplied, so UE 5.6.1 evaluates synchronously.
        Component->RefreshBoneTransforms();
        const double End=FPlatformTime::Seconds();
        if (Component->GetPostProcessInstance()!=Instance || Enable->GetPropertyValue_InContainer(Instance)!=BodyEnabled)
            return Fail(TEXT("Instance or public enable changed"));
        if (!Body)
        {
            for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
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
            if (!Body) return Fail(TEXT("No body rig"));
        }
        auto* RigEnable=FindFProperty<FBoolProperty>(Body->GetClass(),TEXT("Enabled"));
        if (!RigEnable || RigEnable->GetPropertyValue_InContainer(Body)!=BodyEnabled)
            return Fail(TEXT("Rig enable transfer failed"));
        const int32 Steps=FCString::Atoi(*Body->GetVariableAsString(TEXT("TotalSteps")));
        if (PreviousSteps>=0 && (BodyEnabled ? Steps<=PreviousSteps : Steps!=PreviousSteps))
            return Fail(TEXT("Solver execution does not match enable state"));
        PreviousSteps=Steps;
        for (const FTransform& Bone : Component->GetComponentSpaceTransforms())
            if (Bone.ContainsNaN()) return Fail(TEXT("Non-finite output bone transform"));
        if (Frame>=Warmup)
        {
            auto Row=MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("frame"),Frame-Warmup);
            Row->SetNumberField(TEXT("update_ms"),(Updated-Begin)*1000);
            Row->SetNumberField(TEXT("refresh_ms"),(End-Updated)*1000);
            Row->SetNumberField(TEXT("total_ms"),(End-Begin)*1000);
            Row->SetNumberField(TEXT("steps"),Steps);
            Samples.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    auto Result=MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"),true);
    Result->SetBoolField(TEXT("body_enabled"),BodyEnabled);
    Result->SetNumberField(TEXT("fps"),Fps);
    Result->SetNumberField(TEXT("warmup_frames"),Warmup);
    Result->SetNumberField(TEXT("rig_count"),RigCount);
    Result->SetStringField(TEXT("body_class"),Body->GetClass()->GetPathName());
    Result->SetStringField(TEXT("limits"),Body->GetVariableAsString(TEXT("Limits")));
    Result->SetStringField(TEXT("travel"),Body->GetVariableAsString(TEXT("MaxTravel")));
    Result->SetStringField(TEXT("mesh"),Mesh->GetPathName());
    Result->SetArrayField(TEXT("samples"),Samples);
    FString Json;
    FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
    return Json;
}

bool UCSSBodyInputsLibrary::SetBodyMovementLimits(UBlueprint* Blueprint,
    const TArray<float>& Angles, const TArray<float>& Travel)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Blueprint ||
        !Blueprint->GeneratedClass || !Blueprint->GetPathName().StartsWith(TEXT("/Game/CSS/")) ||
        Angles.Num()!=7 || Travel.Num()!=7) return false;
    for (float Value : Angles) if (!FMath::IsFinite(Value) || Value<=0 || Value>PI) return false;
    for (float Value : Travel) if (!FMath::IsFinite(Value) || Value<=0 || Value>50) return false;
    UObject* Defaults=Blueprint->GeneratedClass->GetDefaultObject();
    const FName Names[]={TEXT("Limits"),TEXT("MaxTravel")};
    FArrayProperty* Fields[2]{};
    int32 Indices[2]{};
    for (int32 I=0; I<2; ++I)
    {
        Indices[I]=FBlueprintEditorUtils::FindNewVariableIndex(Blueprint,Names[I]);
        Fields[I]=FindFProperty<FArrayProperty>(Defaults->GetClass(),Names[I]);
        if (Indices[I]==INDEX_NONE || !Fields[I] || !CastField<FFloatProperty>(Fields[I]->Inner)) return false;
        FScriptArrayHelper Values(Fields[I],Fields[I]->ContainerPtrToValuePtr<void>(Defaults));
        if (Values.Num()!=7) return false;
    }
    const TArray<float>* Inputs[]={&Angles,&Travel};
    Blueprint->Modify();
    Defaults->Modify();
    for (int32 I=0; I<2; ++I)
    {
        FScriptArrayHelper Values(Fields[I],Fields[I]->ContainerPtrToValuePtr<void>(Defaults));
        for (int32 Index=0; Index<7; ++Index)
            Fields[I]->Inner->CopyCompleteValue(Values.GetRawPtr(Index),&(*Inputs[I])[Index]);
        FString Text;
        Fields[I]->ExportTextItem_Direct(Text,Fields[I]->ContainerPtrToValuePtr<void>(Defaults),
            nullptr,Defaults,PPF_None);
        // Rig recompilation reconstructs the CDO from these authored defaults.
        Blueprint->NewVariables[Indices[I]].DefaultValue=Text;
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return true;
}

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
