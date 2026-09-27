FString AddHeelVisibility(UAnimBlueprint* Blueprint, int32 ShoeMaterial)
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CSSHeelsEnabled"), Type, TEXT("false")))
        return TEXT("Cannot add private heel visibility state");
    auto* Event = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, UAnimInstance::StaticClass(), TEXT("BlueprintUpdateAnimation"));
    UEdGraph* Graph = Event ? Event->GetGraph() : (Blueprint->UbergraphPages.IsEmpty() ? nullptr : Blueprint->UbergraphPages[0].Get());
    if (!Graph) return TEXT("Missing animation event graph");
    if (!Event)
    {
        int32 Y = 0;
        Event = FKismetEditorUtilities::AddDefaultEventNode(Blueprint, Graph, TEXT("BlueprintUpdateAnimation"), UAnimInstance::StaticClass(), Y);
    }
    auto Link = [Graph](UEdGraphPin* From, UEdGraphPin* To) {
        return From && To && Graph->GetSchema()->TryCreateConnection(From, To);
    };
    auto Call = [Graph](UClass* Owner, FName Name, int32 X) {
        auto* Node = AddNode<UK2Node_CallFunction>(Graph, X);
        Node->SetFromFunction(Owner->FindFunctionByName(Name));
        Node->ReconstructNode();
        return Node;
    };
    auto Set = [Graph](int32 X) {
        auto* Node = AddNode<UK2Node_VariableSet>(Graph, X);
        Node->VariableReference.SetSelfMember(TEXT("CSSHeelsEnabled"));
        Node->ReconstructNode();
        return Node;
    };
    auto* Exec = Event ? Event->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr;
    if (!Exec) return TEXT("Missing animation update execution pin");
    const auto Existing = Exec->LinkedTo;
    auto* Sequence = AddNode<UK2Node_ExecutionSequence>(Graph, 200);
    Exec->BreakAllPinLinks();
    if (!Link(Exec, Sequence->GetExecPin())) return TEXT("Cannot preserve animation event sequence");
    for (auto* Pin : Existing)
        if (!Link(Sequence->GetThenPinGivenIndex(0), Pin)) return TEXT("Cannot preserve existing animation update");
    auto* Component = Call(UAnimInstance::StaticClass(), TEXT("GetOwningComponent"), 400);
    auto* Valid = Call(UKismetSystemLibrary::StaticClass(), TEXT("IsValid"), 500);
    auto* Branch = AddNode<UK2Node_IfThenElse>(Graph, 600);
    auto* Query = Call(USkinnedMeshComponent::StaticClass(), TEXT("IsMaterialSectionShown"), 800);
    auto* Enabled = Set(1000);
    auto* Disabled = Set(1000);
    auto* MaterialPin = Query->FindPin(TEXT("MaterialID"));
    auto* LodPin = Query->FindPin(TEXT("LODIndex"));
    auto* FalsePin = Disabled->FindPin(TEXT("CSSHeelsEnabled"));
    if (!MaterialPin || !LodPin || !FalsePin) return TEXT("Missing shoe visibility parameters");
    MaterialPin->DefaultValue = FString::FromInt(ShoeMaterial);
    LodPin->DefaultValue = TEXT("0");
    FalsePin->DefaultValue = TEXT("false");
    if (!Link(Sequence->GetThenPinGivenIndex(1), Branch->GetExecPin()) ||
        !Link(Component->GetReturnValuePin(), Valid->FindPin(TEXT("Object"))) ||
        !Link(Valid->GetReturnValuePin(), Branch->GetConditionPin()) ||
        !Link(Branch->GetThenPin(), Query->GetExecPin()) ||
        !Link(Component->GetReturnValuePin(), Query->FindPin(UEdGraphSchema_K2::PN_Self)) ||
        !Link(Query->GetThenPin(), Enabled->GetExecPin()) ||
        !Link(Query->GetReturnValuePin(), Enabled->FindPin(TEXT("CSSHeelsEnabled"))) ||
        !Link(Branch->GetElsePin(), Disabled->GetExecPin()))
        return TEXT("Cannot connect private heel visibility update");
    return {};
}
