#include "CSSEveMotionCommandlet.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_SpringBone.h"
#include "AnimGraphNode_AnimDynamics.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_ControlRig.h"
#include "ControlRigBlueprint.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/UnrealType.h"
#include "Components/SkinnedMeshComponent.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetSystemLibrary.h"

UCSSEveMotionCommandlet::UCSSEveMotionCommandlet()
{
    IsEditor = true;
    IsClient = IsServer = false;
    LogToConsole = true;
}

namespace
{
template<class T> T* AddNode(UEdGraph* Graph, int32 X)
{
    T* Node = NewObject<T>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Node->NodePosX = X;
    return Node;
}

bool ConnectPose(UEdGraph* Graph, UEdGraphNode* From, UEdGraphNode* To)
{
    for (auto* Out : From->Pins)
        if (Out->Direction == EGPD_Output)
            for (auto* In : To->Pins)
                if (In->Direction == EGPD_Input && Graph->GetSchema()->TryCreateConnection(Out, In))
                    return true;
    return false;
}
#include "CSSDynamicsRecipe.inl"
#include "CSSEveHeelVisibility.inl"
}

int32 UCSSEveMotionCommandlet::Main(const FString& Params)
{
    auto Fail = [](const FString& Why) { UE_LOG(LogTemp, Error, TEXT("Eve motion: %s"), *Why); return 1; };
    FString RecipePath, Text;
    FString Output = TEXT("/Game/CSS/EveTest/ABP_Holiday");
    FParse::Value(*Params, TEXT("Output="), Output);
    const bool Follow = FParse::Param(*Params, TEXT("Follow"));
    const bool Heels = FParse::Param(*Params, TEXT("Heels"));
    const bool ShoeVisibility = FParse::Param(*Params, TEXT("ShoeVisibility"));
    const bool Knit = FParse::Param(*Params, TEXT("Knit"));
    if ((Knit && (!Heels || !ShoeVisibility)) || (ShoeVisibility && !Heels) || (Follow && Heels) || (Heels ? Output != (Knit ? TEXT("/Game/CSS/EveTest/ABP_KnitFeet1") : ShoeVisibility ? TEXT("/Game/CSS/EveTest/ABP_BikiniFeet2") : TEXT("/Game/CSS/EveTest/ABP_BikiniFeet")) : Follow ? Output != TEXT("/Game/CSS/EveTest/ABP_HolidayFollow") :
        (Output != TEXT("/Game/CSS/EveTest/ABP_Holiday") && Output != TEXT("/Game/CSS/EveTest/ABP_Holiday2"))))
        return Fail(TEXT("Invalid private output"));
    if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath) || FPackageName::DoesPackageExist(Output))
        return Fail(TEXT("Expected a recipe and unused private output"));
    auto* Source = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary"));
    auto* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/CSS/EveTest/SK_Holiday.SK_Holiday"));
    if (!Source || !Source->GeneratedClass || !Source->TargetSkeleton || !Mesh)
        return Fail(TEXT("Missing current secondary graph or private Holiday reference"));
    TSharedPtr<FJsonObject> Recipe;
    if (!FFileHelper::LoadFileToString(Text, *RecipePath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe.IsValid())
        return Fail(TEXT("Invalid recipe"));
    auto* Blueprint = Cast<UAnimBlueprint>(FAssetToolsModule::GetModule().Get().DuplicateAsset(
        FPackageName::GetLongPackageAssetName(Output), TEXT("/Game/CSS/EveTest"), Source));
    if (!Blueprint || Blueprint->TargetSkeleton != Source->TargetSkeleton)
        return Fail(TEXT("Cannot preserve secondary graph skeleton"));
    if (ShoeVisibility)
        if (const FString Error = AddHeelVisibility(Blueprint, Knit ? 17 : 23); !Error.IsEmpty()) return Fail(Error);
    UEdGraph* Graph = nullptr;
    for (auto Candidate : Blueprint->FunctionGraphs)
        if (Candidate->GetFName() == TEXT("AnimGraph")) Graph = Candidate.Get();
    if (!Graph) return Fail(TEXT("Missing copied AnimGraph"));
    UAnimGraphNode_Root* Root = nullptr;
    TMap<FGuid, UClass*> OriginalNodes;
    for (auto Node : Graph->Nodes)
    {
        OriginalNodes.Add(Node->NodeGuid, Node->GetClass());
        if (auto* Found = Cast<UAnimGraphNode_Root>(Node.Get()))
        {
            if (Root) return Fail(TEXT("Multiple result nodes"));
            Root = Found;
        }
    }
    auto* Result = Root ? Root->FindPin(TEXT("Result")) : nullptr;
    if (!Result || Result->LinkedTo.Num() != 1) return Fail(TEXT("Unexpected result connection"));
    auto* ExistingOutput = Result->LinkedTo[0];
    ExistingOutput->BreakLinkTo(Result);
    int32 X = Root->NodePosX;
    TSet<FName> Seen;
    if (Heels)
    {
        const TArray<TSharedPtr<FJsonValue>>* Shoes = nullptr;
        if (!Recipe->TryGetArrayField(TEXT("shoes"), Shoes) || Shoes->Num() != 2)
            return Fail(TEXT("Expected two foot correction records"));
        auto* ToComponent = AddNode<UAnimGraphNode_LocalToComponentSpace>(Graph, X);
        X += 220;
        if (!Graph->GetSchema()->TryCreateConnection(ExistingOutput, ToComponent->FindPin(TEXT("LocalPose"))))
            return Fail(TEXT("Cannot connect heel input"));
        UEdGraphNode* Previous = ToComponent;
        const auto& Ref = Mesh->GetRefSkeleton();
        TArray<FTransform> Bind;
        for (int32 Index = 0; Index < Ref.GetRawBoneNum(); ++Index)
        {
            const int32 Parent = Ref.GetParentIndex(Index);
            Bind.Add(Parent >= 0 ? Ref.GetRefBonePose()[Index] * Bind[Parent] : Ref.GetRefBonePose()[Index]);
        }
        for (const auto& Value : *Shoes)
        {
            const TSharedPtr<FJsonObject>* Row;
            FString Name;
            double Degrees;
            const TArray<TSharedPtr<FJsonValue>>* Axis;
            if (!Value->TryGetObject(Row) || !(*Row)->TryGetStringField(TEXT("foot_bone"), Name) ||
                !(*Row)->TryGetNumberField(TEXT("angle_degrees"), Degrees) || !FMath::IsFinite(Degrees) || FMath::Abs(Degrees) > 45 ||
                !(*Row)->TryGetArrayField(TEXT("axis"), Axis) || Axis->Num() != 3 ||
                (Name != TEXT("foot_l") && Name != TEXT("foot_r")) || Seen.Contains(FName(*Name)))
                return Fail(TEXT("Invalid bounded foot correction"));
            FVector Direction((*Axis)[0]->AsNumber(), (*Axis)[1]->AsNumber(), (*Axis)[2]->AsNumber());
            if (Direction.ContainsNaN() || !Direction.Normalize()) return Fail(TEXT("Invalid heel axis"));
            const int32 Index = Ref.FindBoneIndex(FName(*Name));
            if (!Bind.IsValidIndex(Index)) return Fail(TEXT("Missing foot bind transform"));
            const FQuat Rotation = Bind[Index].GetRotation().Inverse() * FQuat(Direction, FMath::DegreesToRadians(Degrees)) * Bind[Index].GetRotation();
            auto* Node = AddNode<UAnimGraphNode_ModifyBone>(Graph, X);
            X += 220;
            Node->Node.BoneToModify.BoneName = FName(*Name);
            Node->Node.Rotation = Rotation.Rotator();
            Node->Node.RotationMode = BMM_Additive;
            Node->Node.RotationSpace = BCS_BoneSpace;
            Node->Node.TranslationMode = BMM_Ignore;
            Node->Node.ScaleMode = BMM_Ignore;
            Node->Node.Alpha = 1.f;
            if (auto* Pin = Node->FindPin(TEXT("Rotation")))
                Pin->DefaultValue = FString::Printf(TEXT("(Pitch=%.9f,Yaw=%.9f,Roll=%.9f)"), Node->Node.Rotation.Pitch, Node->Node.Rotation.Yaw, Node->Node.Rotation.Roll);
            if (auto* Pin = Node->FindPin(TEXT("Alpha"))) Pin->DefaultValue = TEXT("1.0");
            if (ShoeVisibility)
            {
                Node->Node.AlphaInputType = EAnimAlphaInputType::Bool;
                Node->Node.AlphaBoolBlend.BlendInTime = 0.f;
                Node->Node.AlphaBoolBlend.BlendOutTime = 0.f;
                Node->ReconstructNode();
                auto* Read = AddNode<UK2Node_VariableGet>(Graph, X);
                Read->VariableReference.SetSelfMember(TEXT("CSSHeelsEnabled"));
                Read->ReconstructNode();
                if (!Graph->GetSchema()->TryCreateConnection(Read->FindPin(TEXT("CSSHeelsEnabled")), Node->FindPin(TEXT("bAlphaBoolEnabled"))))
                    return Fail(TEXT("Cannot connect shoe visibility alpha"));
            }
            if (!ConnectPose(Graph, Previous, Node)) return Fail(TEXT("Cannot connect foot correction"));
            Previous = Node;
            Seen.Add(FName(*Name));
        }
        auto* ToLocal = AddNode<UAnimGraphNode_ComponentToLocalSpace>(Graph, X);
        if (!ConnectPose(Graph, Previous, ToLocal) || !ConnectPose(Graph, ToLocal, Root))
            return Fail(TEXT("Cannot connect heel output"));
    }
    else if (Follow)
    {
        auto* RigBP = LoadObject<UControlRigBlueprint>(nullptr, TEXT("/Game/CSS/EveTest/CR_HolidayFollow.CR_HolidayFollow"));
        const TSharedPtr<FJsonObject>* Drivers = nullptr;
        if (!RigBP || !RigBP->GeneratedClass || !Recipe->TryGetObjectField(TEXT("drivers"), Drivers) || (*Drivers)->Values.Num() != 11)
            return Fail(TEXT("Invalid private carrier rig or driver map"));
        auto* Rig = AddNode<UAnimGraphNode_ControlRig>(Graph, X);
        Rig->Node.SetControlRigClass(RigBP->GeneratedClass.Get());
        Rig->ReconstructNode();
        for (const auto& Pair : {TPair<FName,bool>(TEXT("bResetInputPoseToInitial"),true),
                                TPair<FName,bool>(TEXT("bTransferInputPose"),true),
                                TPair<FName,bool>(TEXT("bTransferInputCurves"),true),
                                TPair<FName,bool>(TEXT("bTransferPoseInGlobalSpace"),false)})
        {
            auto* Property = FindFProperty<FBoolProperty>(FAnimNode_ControlRigBase::StaticStruct(), Pair.Key);
            if (!Property) return Fail(TEXT("Missing pose transfer flag"));
            Property->SetPropertyValue_InContainer(&Rig->Node, Pair.Value);
        }
        auto* Filter = FindFProperty<FArrayProperty>(FAnimNode_ControlRigBase::StaticStruct(), TEXT("OutputBonesToTransfer"));
        if (!Filter) return Fail(TEXT("Missing native rig output filter"));
        auto& Bones = *Filter->ContainerPtrToValuePtr<TArray<FBoneReference>>(&Rig->Node);
        for (const auto& Pair : (*Drivers)->Values)
        {
            const FName Name(*Pair.Key);
            if (!Pair.Key.StartsWith(TEXT("CSS_Cloth_Skirt_")) || Mesh->GetRefSkeleton().FindBoneIndex(Name) == INDEX_NONE)
                return Fail(TEXT("Invalid carrier bone"));
            Seen.Add(Name);
        }
        // Transfer compensating child locals as well as the driven carriers.
        for (int32 Index = 0; Index < Mesh->GetRefSkeleton().GetRawBoneNum(); ++Index)
        {
            const FName Name = Mesh->GetRefSkeleton().GetBoneName(Index);
            if (!Name.ToString().StartsWith(TEXT("CSS_Cloth_Skirt_"))) continue;
            FBoneReference Bone; Bone.BoneName = Name; Bones.Add(Bone);
        }
        if (Bones.Num() != 24) return Fail(TEXT("Expected complete existing skirt output filter"));
        if (!Graph->GetSchema()->TryCreateConnection(ExistingOutput, Rig->FindPin(TEXT("Source"))) || !ConnectPose(Graph, Rig, Root))
            return Fail(TEXT("Cannot connect carrier rig"));
        X += 220;
    }
    else
    {
    auto* ToComponent = AddNode<UAnimGraphNode_LocalToComponentSpace>(Graph, X);
    X += 220;
    if (!Graph->GetSchema()->TryCreateConnection(ExistingOutput, ToComponent->FindPin(TEXT("LocalPose"))))
        return Fail(TEXT("Cannot connect existing secondary output"));
    UEdGraphNode* Previous = ToComponent;
    if (const FString Error = AddDynamicsChains(Recipe, Mesh, Graph, Previous, X, Seen); !Error.IsEmpty())
        return Fail(Error);
    if (Seen.Num() != 12) return Fail(TEXT("Expected twelve garment bones"));
    for (FName Name : Seen)
        if (!Name.ToString().StartsWith(TEXT("CSS_Cloth_Skirt_")))
            return Fail(TEXT("Recipe controls a non-skirt bone"));
    auto* ToLocal = AddNode<UAnimGraphNode_ComponentToLocalSpace>(Graph, X);
    if (!ConnectPose(Graph, Previous, ToLocal) || !ConnectPose(Graph, ToLocal, Root))
        return Fail(TEXT("Cannot connect garment output"));
    }
    Root->NodePosX = X + 220;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors || Results.NumWarnings || !Blueprint->GeneratedClass || Blueprint->Status == BS_Error)
        return Fail(TEXT("Combined graph compilation failed"));
    for (const auto& Pair : OriginalNodes)
    {
        bool Found = false;
        for (auto Node : Graph->Nodes)
            if (Node->NodeGuid == Pair.Key && Node->GetClass() == Pair.Value) Found = true;
        if (!Found) return Fail(TEXT("An existing graph node was lost"));
    }
    for (TFieldIterator<FProperty> It(Source->GeneratedClass); It; ++It)
    {
        if (!It->GetName().StartsWith(TEXT("CSS"))) continue;
        auto* Copy = FindFProperty<FProperty>(Blueprint->GeneratedClass, It->GetFName());
        if (!Copy || !Copy->SameType(*It)) return Fail(TEXT("Existing CSS control changed type"));
        FString Before, After;
        It->ExportText_InContainer(0, Before, Source->GeneratedClass->GetDefaultObject(), nullptr, nullptr, PPF_None);
        Copy->ExportText_InContainer(0, After, Blueprint->GeneratedClass->GetDefaultObject(), nullptr, nullptr, PPF_None);
        if (Before != After) return Fail(TEXT("Existing CSS control default changed: ") + It->GetName());
    }
    Blueprint->MarkPackageDirty();
    FSavePackageArgs Save;
    Save.TopLevelFlags = RF_Public | RF_Standalone;
    Save.SaveFlags = SAVE_NoError;
    const FString File = FPackageName::LongPackageNameToFilename(Output, FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *File, Save))
        return Fail(TEXT("Cannot save private graph"));
    UE_LOG(LogTemp, Display, TEXT("EVE_MOTION_SAVED original_nodes=%d skirt_bones=%d output=%s"),
        OriginalNodes.Num(), Seen.Num(), *Output);
    return 0;
}
