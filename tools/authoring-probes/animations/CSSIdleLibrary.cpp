#include "CSSIdleLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AssetToolsModule.h"
#include "EdGraphSchema_K2.h"
#include "IAssetTools.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace
{
template<class T> T* Add(UEdGraph* Graph)
{
    auto* Node=NewObject<T>(Graph);
    Graph->AddNode(Node,false,false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    return Node;
}
UK2Node_VariableGet* Read(UEdGraph* Graph,FName Name)
{
    auto* Node=Add<UK2Node_VariableGet>(Graph);
    Node->VariableReference.SetSelfMember(Name);
    Node->ReconstructNode();
    return Node;
}
bool Link(UEdGraph* Graph,UEdGraphPin* From,UEdGraphPin* To)
{
    return From && To && Graph->GetSchema()->TryCreateConnection(From,To);
}
}

UAnimBlueprint* UCSSIdleLibrary::CreateIdleLayer(UAnimBlueprint* Source,
    const FString& OutputPackage)
{
    auto Fail=[](const TCHAR* Why)->UAnimBlueprint* {
        UE_LOG(LogTemp,Error,TEXT("CSS idle layer: %s"),Why); return nullptr;
    };
    if (!IsRunningCommandlet() || !Source || !Source->GeneratedClass || !Source->TargetSkeleton ||
        Source->GetPathName()!=TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary") ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/AnimLab/ABP_Idle")) ||
        OutputPackage.Len()>80 || !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr,*OutputPackage))
        return Fail(TEXT("Invalid source or isolated output"));
    for (const auto Name:{TEXT("CSSIdleSequence"),TEXT("CSSIdleEnabled")})
        if (FindFProperty<FProperty>(Source->GeneratedClass.Get(),Name))
            return Fail(TEXT("Source already declares idle controls"));
    auto* Blueprint=Cast<UAnimBlueprint>(FAssetToolsModule::GetModule().Get().DuplicateAsset(
        FPackageName::GetLongPackageAssetName(OutputPackage),
        FPackageName::GetLongPackagePath(OutputPackage),Source));
    if (!Blueprint) return Fail(TEXT("Cannot copy post-process graph"));
    FEdGraphPinType SequenceType;
    SequenceType.PinCategory=UEdGraphSchema_K2::PC_Object;
    SequenceType.PinSubCategoryObject=UAnimSequence::StaticClass();
    FEdGraphPinType BoolType;
    BoolType.PinCategory=UEdGraphSchema_K2::PC_Boolean;
    if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint,TEXT("CSSIdleSequence"),SequenceType) ||
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint,TEXT("CSSIdleEnabled"),BoolType,TEXT("false")))
        return Fail(TEXT("Cannot add idle controls"));
    UEdGraph* Graph=nullptr;
    for (auto Candidate:Blueprint->FunctionGraphs)
        if (Candidate->GetFName()==TEXT("AnimGraph")) Graph=Candidate.Get();
    if (!Graph) return Fail(TEXT("Missing animation graph"));
    UAnimGraphNode_LinkedInputPose* Input=nullptr;
    for (auto Node:Graph->Nodes)
        if (auto* Found=Cast<UAnimGraphNode_LinkedInputPose>(Node))
        {
            if (Input) return Fail(TEXT("Expected one post-process input"));
            Input=Found;
        }
    auto* InputPin=Input?Input->FindPin(TEXT("Pose")):nullptr;
    if (!InputPin || InputPin->LinkedTo.Num()!=1)
        return Fail(TEXT("Expected one existing rig-chain connection"));
    auto* Destination=InputPin->LinkedTo[0];
    auto* Player=Add<UAnimGraphNode_SequencePlayer>(Graph);
    for (auto& Pin:Player->ShowPinForProperties)
        if (Pin.PropertyName==TEXT("Sequence")) Pin.bShowPin=true;
    Player->ReconstructNode();
    auto* Blend=Add<UAnimGraphNode_BlendListByBool>(Graph);
    // Restart the gesture on activation. Restore the game pose immediately on
    // release; never leave an idle blend covering the beginning of an attack.
    auto* ChildMode=FindFProperty<FProperty>(FAnimNode_BlendListBase::StaticStruct(),TEXT("ChildUpateMode"));
    if (!ChildMode || !ChildMode->ImportText_Direct(TEXT("ResetChildOnActivate"),
        ChildMode->ContainerPtrToValuePtr<void>(&Blend->Node),nullptr,PPF_None))
        return Fail(TEXT("Cannot configure child activation"));
    auto* Enter=Blend->FindPin(TEXT("BlendTime_0"));
    auto* Exit=Blend->FindPin(TEXT("BlendTime_1"));
    if (!Enter || !Exit) return Fail(TEXT("Missing blend-time pins"));
    Enter->DefaultValue=TEXT("0.18");
    Exit->DefaultValue=TEXT("0.0");
    auto* Sequence=Read(Graph,TEXT("CSSIdleSequence"));
    auto* Enabled=Read(Graph,TEXT("CSSIdleEnabled"));
    InputPin->BreakLinkTo(Destination);
    if (!Link(Graph,Sequence->FindPin(TEXT("CSSIdleSequence")),Player->FindPin(TEXT("Sequence"))) ||
        !Link(Graph,Enabled->FindPin(TEXT("CSSIdleEnabled")),Blend->FindPin(TEXT("bActiveValue"))) ||
        !Link(Graph,Player->FindPin(TEXT("Pose")),Blend->FindPin(TEXT("BlendPose_0"))) ||
        !Link(Graph,InputPin,Blend->FindPin(TEXT("BlendPose_1"))) ||
        !Link(Graph,Blend->FindPin(TEXT("Pose")),Destination))
        return Fail(TEXT("Cannot connect idle branch"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::None,&Results);
    if (Results.NumErrors || Results.NumWarnings || !Blueprint->GeneratedClass || Blueprint->Status==BS_Error)
        return Fail(TEXT("Idle graph compilation failed"));
    auto* Default=Blueprint->GeneratedClass->GetDefaultObject();
    auto* EnabledProperty=FindFProperty<FBoolProperty>(Blueprint->GeneratedClass.Get(),TEXT("CSSIdleEnabled"));
    auto* SequenceProperty=FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass.Get(),TEXT("CSSIdleSequence"));
    if (!EnabledProperty || EnabledProperty->GetPropertyValue_InContainer(Default) ||
        !SequenceProperty || SequenceProperty->GetObjectPropertyValue_InContainer(Default))
        return Fail(TEXT("Idle must default to inactive with no clip"));
    return Blueprint;
}
