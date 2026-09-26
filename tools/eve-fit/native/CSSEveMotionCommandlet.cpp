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
}

int32 UCSSEveMotionCommandlet::Main(const FString& Params)
{
    auto Fail = [](const FString& Why) { UE_LOG(LogTemp, Error, TEXT("Eve motion: %s"), *Why); return 1; };
    FString RecipePath, Text;
    const FString Output = TEXT("/Game/CSS/EveTest/ABP_Holiday");
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
        TEXT("ABP_Holiday"), TEXT("/Game/CSS/EveTest"), Source));
    if (!Blueprint || Blueprint->TargetSkeleton != Source->TargetSkeleton)
        return Fail(TEXT("Cannot preserve secondary graph skeleton"));
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
    auto* ToComponent = AddNode<UAnimGraphNode_LocalToComponentSpace>(Graph, X);
    X += 220;
    if (!Graph->GetSchema()->TryCreateConnection(ExistingOutput, ToComponent->FindPin(TEXT("LocalPose"))))
        return Fail(TEXT("Cannot connect existing secondary output"));
    UEdGraphNode* Previous = ToComponent;
    TSet<FName> Seen;
    if (const FString Error = AddDynamicsChains(Recipe, Mesh, Graph, Previous, X, Seen); !Error.IsEmpty())
        return Fail(Error);
    if (Seen.Num() != 12) return Fail(TEXT("Expected twelve garment bones"));
    for (FName Name : Seen)
        if (!Name.ToString().StartsWith(TEXT("CSS_Cloth_Skirt_")))
            return Fail(TEXT("Recipe controls a non-skirt bone"));
    auto* ToLocal = AddNode<UAnimGraphNode_ComponentToLocalSpace>(Graph, X);
    if (!ConnectPose(Graph, Previous, ToLocal) || !ConnectPose(Graph, ToLocal, Root))
        return Fail(TEXT("Cannot connect garment output"));
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
