#include "CSSPerformanceLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AssetToolsModule.h"
#include "EdGraphSchema_K2.h"
#include "IAssetTools.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace
{
template<class T> T* Add(UEdGraph* Graph)
{
    auto* Node = NewObject<T>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    return Node;
}
UK2Node_VariableGet* Read(UEdGraph* Graph, FName Name)
{
    auto* Node = Add<UK2Node_VariableGet>(Graph);
    Node->VariableReference.SetSelfMember(Name);
    Node->ReconstructNode();
    return Node;
}
bool Link(UEdGraph* Graph, UEdGraphPin* From, UEdGraphPin* To)
{
    return From && To && Graph->GetSchema()->TryCreateConnection(From, To);
}
bool NodeSetting(UScriptStruct* Type, void* Node, FName Name, const TCHAR* Value)
{
    auto* Property = FindFProperty<FProperty>(Type, Name);
    return Property && Property->ImportText_Direct(Value,
        Property->ContainerPtrToValuePtr<void>(Node), nullptr, PPF_None);
}
bool ReadsIdleFlag(const UEdGraphNode* Node, int32 Depth = 0)
{
    if (!Node || Depth > 4) return false;
    if (const auto* Variable = Cast<UK2Node_VariableGet>(Node))
        return Variable->VariableReference.GetMemberName() == TEXT("CSSIdleEnabled");
    for (const auto* Pin : Node->Pins)
        if (Pin->Direction == EGPD_Input)
            for (const auto* Linked : Pin->LinkedTo)
                if (ReadsIdleFlag(Linked->GetOwningNode(), Depth + 1)) return true;
    return false;
}
}

UAnimBlueprint* UCSSPerformanceLibrary::CreatePerformanceLayer(UAnimBlueprint* Source,
    const FString& OutputPackage)
{
    auto Fail = [](const TCHAR* Why) -> UAnimBlueprint* {
        UE_LOG(LogTemp, Error, TEXT("CSS performance layer: %s"), Why);
        return nullptr;
    };
    static const TSet<FString> Sources {
        TEXT("/Game/CSS/AnimLab/ABP_Idle.ABP_Idle"),
        TEXT("/Game/CSS/EveTest/ABP_BikiniFeet2.ABP_BikiniFeet2"),
        TEXT("/Game/CSS/EveTest/ABP_KnitFeet1.ABP_KnitFeet1"),
        TEXT("/Game/CSS/UnholyGenessa/ABP_Wings2.ABP_Wings2"),
        TEXT("/Game/CSS/CWTest/ABP_BodyHairB4.ABP_BodyHairB4")
    };
    if (!IsRunningCommandlet() || !Source || !Sources.Contains(Source->GetPathName()) ||
        !Source->GeneratedClass || !Source->TargetSkeleton ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/Locomotion/Adapters/")) ||
        OutputPackage.Len() > 90 || !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FPackageName::DoesPackageExist(OutputPackage) || FindPackage(nullptr, *OutputPackage))
        return Fail(TEXT("Invalid source or existing output"));
    if (!FindFProperty<FBoolProperty>(Source->GeneratedClass.Get(), TEXT("CSSIdleEnabled")) ||
        !FindFProperty<FObjectPropertyBase>(Source->GeneratedClass.Get(), TEXT("CSSIdleSequence")))
        return Fail(TEXT("Source lacks the audited idle contract"));
    auto* Blueprint = Cast<UAnimBlueprint>(FAssetToolsModule::GetModule().Get().DuplicateAsset(
        FPackageName::GetLongPackageAssetName(OutputPackage),
        FPackageName::GetLongPackagePath(OutputPackage), Source));
    if (!Blueprint) return Fail(TEXT("Cannot duplicate post-process graph"));
    FEdGraphPinType SequenceType, BoolType, FloatType;
    SequenceType.PinCategory = UEdGraphSchema_K2::PC_Object;
    SequenceType.PinSubCategoryObject = UAnimSequence::StaticClass();
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CSSPerformanceSequence"), SequenceType) ||
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CSSPerformanceEnabled"), BoolType, TEXT("false")) ||
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CSSPerformanceTime"), FloatType, TEXT("0.0")) ||
        !FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CSSPerformanceWeight"), FloatType, TEXT("0.0")))
        return Fail(TEXT("Cannot add performance controls"));
    UEdGraph* Graph = nullptr;
    for (auto Candidate : Blueprint->FunctionGraphs)
        if (Candidate->GetFName() == TEXT("AnimGraph")) Graph = Candidate.Get();
    if (!Graph) return Fail(TEXT("Missing animation graph"));
    UAnimGraphNode_BlendListByBool* Idle = nullptr;
    for (auto Node : Graph->Nodes)
    {
        auto* Blend = Cast<UAnimGraphNode_BlendListByBool>(Node);
        auto* Active = Blend ? Blend->FindPin(TEXT("bActiveValue")) : nullptr;
        if (!Active || Active->LinkedTo.Num() != 1) continue;
        if (!ReadsIdleFlag(Active->LinkedTo[0]->GetOwningNode())) continue;
        if (Idle) return Fail(TEXT("Ambiguous existing idle branch"));
        Idle = Blend;
    }
    auto* IdleOutput = Idle ? Idle->FindPin(TEXT("Pose")) : nullptr;
    if (!IdleOutput || IdleOutput->LinkedTo.Num() != 1)
        return Fail(TEXT("Missing single idle-to-physics connection"));
    auto* Destination = IdleOutput->LinkedTo[0];
    auto* Evaluator = Add<UAnimGraphNode_SequenceEvaluator>(Graph);
    if (!NodeSetting(FAnimNode_SequenceEvaluator::StaticStruct(), &Evaluator->Node, TEXT("bShouldLoop"), TEXT("false")) ||
        !NodeSetting(FAnimNode_SequenceEvaluator::StaticStruct(), &Evaluator->Node, TEXT("bTeleportToExplicitTime"), TEXT("true")) ||
        !NodeSetting(FAnimNode_SequenceEvaluator::StaticStruct(), &Evaluator->Node, TEXT("bUseExplicitFrame"), TEXT("false")))
        return Fail(TEXT("Cannot configure explicit evaluation"));
    for (auto& Pin : Evaluator->ShowPinForProperties)
        if (Pin.PropertyName == TEXT("Sequence") || Pin.PropertyName == TEXT("ExplicitTime")) Pin.bShowPin = true;
    Evaluator->ReconstructNode();
    auto* Weighted = Add<UAnimGraphNode_TwoWayBlend>(Graph);
    if (!NodeSetting(FAnimNode_TwoWayBlend::StaticStruct(), &Weighted->BlendNode,
                     TEXT("bResetChildOnActivation"), TEXT("true")))
        return Fail(TEXT("Cannot configure activation"));
    auto* Sequence = Read(Graph, TEXT("CSSPerformanceSequence"));
    auto* Enabled = Read(Graph, TEXT("CSSPerformanceEnabled"));
    auto* Time = Read(Graph, TEXT("CSSPerformanceTime"));
    auto* Weight = Read(Graph, TEXT("CSSPerformanceWeight"));
    auto Call = [&](UClass* Owner, FName Name) {
        auto* Node = Add<UK2Node_CallFunction>(Graph);
        Node->SetFromFunction(Owner->FindFunctionByName(Name));
        Node->ReconstructNode();
        return Node;
    };
    // Sequence is a reflected strong reference. A thread-safe null comparison
    // avoids running UObject validity checks on an animation worker.
    auto* Valid = Call(UKismetMathLibrary::StaticClass(), TEXT("NotEqual_ObjectObject"));
    auto* Both = Call(UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    auto* Guard = Call(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"));
    Guard->FindPin(TEXT("B"))->DefaultValue = TEXT("0.0");
    IdleOutput->BreakLinkTo(Destination);
    if (!Link(Graph, Sequence->FindPin(TEXT("CSSPerformanceSequence")), Evaluator->FindPin(TEXT("Sequence"))) ||
        !Link(Graph, Time->FindPin(TEXT("CSSPerformanceTime")), Evaluator->FindPin(TEXT("ExplicitTime"))) ||
        !Link(Graph, Sequence->FindPin(TEXT("CSSPerformanceSequence")), Valid->FindPin(TEXT("A"))) ||
        !Link(Graph, Enabled->FindPin(TEXT("CSSPerformanceEnabled")), Both->FindPin(TEXT("A"))) ||
        !Link(Graph, Valid->FindPin(TEXT("ReturnValue")), Both->FindPin(TEXT("B"))) ||
        !Link(Graph, Both->FindPin(TEXT("ReturnValue")), Guard->FindPin(TEXT("bPickA"))) ||
        !Link(Graph, Weight->FindPin(TEXT("CSSPerformanceWeight")), Guard->FindPin(TEXT("A"))) ||
        !Link(Graph, Guard->FindPin(TEXT("ReturnValue")), Weighted->FindPin(TEXT("Alpha"))) ||
        !Link(Graph, IdleOutput, Weighted->FindPin(TEXT("A"))) ||
        !Link(Graph, Evaluator->FindPin(TEXT("Pose")), Weighted->FindPin(TEXT("B"))) ||
        !Link(Graph, Weighted->FindPin(TEXT("Pose")), Destination))
        return Fail(TEXT("Cannot connect performance branch"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
    if (Results.NumErrors || Results.NumWarnings || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
        return Fail(TEXT("Performance graph compilation failed"));
    UObject* Default = Blueprint->GeneratedClass->GetDefaultObject();
    auto* EnabledProperty = FindFProperty<FBoolProperty>(Blueprint->GeneratedClass.Get(), TEXT("CSSPerformanceEnabled"));
    auto* SequenceProperty = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass.Get(), TEXT("CSSPerformanceSequence"));
    auto* TimeProperty = FindFProperty<FFloatProperty>(Blueprint->GeneratedClass.Get(), TEXT("CSSPerformanceTime"));
    if (!EnabledProperty || EnabledProperty->GetPropertyValue_InContainer(Default) || !SequenceProperty ||
        SequenceProperty->GetObjectPropertyValue_InContainer(Default) || !TimeProperty ||
        TimeProperty->GetPropertyValue_InContainer(Default) != 0.0f || Blueprint->TargetSkeleton != Source->TargetSkeleton)
        return Fail(TEXT("Invalid default playback state or skeleton"));
    return Blueprint;
}

FString UCSSPerformanceLibrary::VerifyPerformanceLayer(UAnimBlueprint* Source, UAnimBlueprint* Layer)
{
    if (!IsRunningCommandlet() || !Source || !Layer || !Source->GeneratedClass || !Layer->GeneratedClass ||
        !Layer->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/Adapters/")) ||
        Source->TargetSkeleton != Layer->TargetSkeleton || Source->FunctionGraphs.Num() != Layer->FunctionGraphs.Num())
        return TEXT("ERROR: invalid comparison graphs");
    UObject* Before = Source->GeneratedClass->GetDefaultObject();
    UObject* After = Layer->GeneratedClass->GetDefaultObject();
    int32 Controls = 0, Nodes = 0;
    for (TFieldIterator<FProperty> It(Source->GeneratedClass.Get()); It; ++It)
    {
        if (!It->GetName().StartsWith(TEXT("CSS"))) continue;
        auto* Other = FindFProperty<FProperty>(Layer->GeneratedClass.Get(), It->GetFName());
        if (!Other || !It->SameType(Other) || !It->Identical(
                It->ContainerPtrToValuePtr<void>(Before), Other->ContainerPtrToValuePtr<void>(After)))
            return FString::Printf(TEXT("ERROR: changed control %s"), *It->GetName());
        ++Controls;
    }
    if (Controls < 2) return TEXT("ERROR: missing source controls");
    for (auto Original : Source->FunctionGraphs)
    {
        UEdGraph* Copy = nullptr;
        for (auto Candidate : Layer->FunctionGraphs)
            if (Candidate->GetFName() == Original->GetFName()) Copy = Candidate.Get();
        if (!Copy || Copy->Nodes.Num() != Original->Nodes.Num() + (Original->GetFName() == TEXT("AnimGraph") ? 9 : 0))
            return TEXT("ERROR: unexpected graph layout");
        for (auto OriginalNode : Original->Nodes)
        {
            UEdGraphNode* CopyNode = nullptr;
            for (auto Candidate : Copy->Nodes)
                // Blueprint duplication assigns new GUIDs, but preserves subobject names.
                if (Candidate->GetFName() == OriginalNode->GetFName()) CopyNode = Candidate.Get();
            if (!CopyNode || CopyNode->GetClass() != OriginalNode->GetClass())
                return FString::Printf(TEXT("ERROR: original graph node was lost: %s"), *OriginalNode->GetName());
            for (auto* Pin : OriginalNode->Pins)
            {
                auto* OtherPin = CopyNode->FindPin(Pin->PinName, Pin->Direction);
                if (!OtherPin || OtherPin->DefaultValue != Pin->DefaultValue ||
                    OtherPin->DefaultObject != Pin->DefaultObject)
                    return FString::Printf(TEXT("ERROR: node pin default changed: %s.%s"),
                        *OriginalNode->GetName(), *Pin->PinName.ToString());
            }
            ++Nodes;
        }
    }
    auto* Enabled = FindFProperty<FBoolProperty>(Layer->GeneratedClass.Get(), TEXT("CSSPerformanceEnabled"));
    auto* Sequence = FindFProperty<FObjectPropertyBase>(Layer->GeneratedClass.Get(), TEXT("CSSPerformanceSequence"));
    auto* Time = FindFProperty<FFloatProperty>(Layer->GeneratedClass.Get(), TEXT("CSSPerformanceTime"));
    auto* Weight = FindFProperty<FFloatProperty>(Layer->GeneratedClass.Get(), TEXT("CSSPerformanceWeight"));
    if (!Enabled || Enabled->GetPropertyValue_InContainer(After) || !Sequence ||
        Sequence->GetObjectPropertyValue_InContainer(After) || !Time || Time->GetPropertyValue_InContainer(After) != 0.f ||
        !Weight || Weight->GetPropertyValue_InContainer(After) != 0.f)
        return TEXT("ERROR: invalid inactive defaults");
    return FString::Printf(TEXT("PASS: %d existing CSS controls, %d original graph nodes, matching skeleton, inactive defaults"), Controls, Nodes);
}
