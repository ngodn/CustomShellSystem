#include "CSSRigAuditLibrary.h"
#include "IOptimusShaderTextProvider.h"
#include "IOptimusNodeSubGraphReferencer.h"
#include "OptimusDeformer.h"
#include "OptimusNode.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodeSubGraph.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

UObject* UCSSRigAuditLibrary::CloneClothMorphStudy(UObject* Source, const FString& OutputPackage)
{
    const auto Fail = [](const TCHAR* Reason) -> UObject*
    {
        UE_LOG(LogTemp, Error, TEXT("Cloth morph study: %s"), Reason);
        return nullptr;
    };
    const auto* Original = Cast<UOptimusDeformer>(Source);
    if (!IsRunningCommandlet() || !IsInGameThread() || !Original ||
        Original->GetPathName() != TEXT("/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals.DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals") ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy")) ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FindPackage(nullptr, *OutputPackage) || FPackageName::DoesPackageExist(OutputPackage))
        return Fail(TEXT("invalid source, execution context or output package"));

    auto* Package = CreatePackage(*OutputPackage);
    auto* Copy = Cast<UOptimusDeformer>(StaticDuplicateObject(Original, Package,
        *FPackageName::GetLongPackageAssetName(OutputPackage)));
    if (!Copy) return Fail(TEXT("duplicate failed"));

    const FString Before = TEXT("float3 FinalPosition = lerp(SkinnedPosition, LocalClothPosition.xyz, ClothWeight);");
    const FString After = Before + TEXT("\nFinalPosition += ClothWeight * mul((float3x3)BoneMatrix, MorphDelta);");
    int32 EditedKernels = 0;
    for (auto* Graph : Copy->GetGraphs())
    {
        if (!Graph) return Fail(TEXT("null graph"));
        // Expand only the private copy so engine function assets remain untouched.
        const TArray<UOptimusNode*> Nodes = Graph->GetAllNodes();
        for (auto* Node : Nodes)
        {
            if (!Graph->IsFunctionReference(Node)) continue;
            // UE 5.6.1 returns an empty array even after successful expansion.
            Graph->ExpandCollapsedNodes(Node);
            if (Graph->GetAllNodes().Contains(Node)) return Fail(TEXT("function expansion failed"));
        }
        for (auto* Node : Graph->GetAllNodes())
        {
            auto* Shader = Cast<IOptimusShaderTextProvider>(Node);
            if (!Shader) continue;
            FString Text = Shader->GetShaderText();
            if (!Text.Contains(Before, ESearchCase::CaseSensitive)) continue;
            if (Shader->IsShaderTextReadOnly() ||
                !Text.Contains(TEXT("float3 MorphDelta = ReadMorphDelta(Index);"), ESearchCase::CaseSensitive) ||
                Text.ReplaceInline(*Before, *After, ESearchCase::CaseSensitive) != 1)
                return Fail(TEXT("kernel source guard failed"));
            Shader->SetShaderText(Text);
            if (Shader->GetShaderText() != Text) return Fail(TEXT("shader writeback differs"));
            ++EditedKernels;
        }
    }
    if (EditedKernels != 1) return Fail(TEXT("expected exactly one position kernel"));
    if (!Copy->Compile()) return Fail(TEXT("graph compile failed"));
    Copy->MarkPackageDirty();
    return Copy;
}

FString UCSSRigAuditLibrary::SnapshotDeformer(UObject* Asset)
{
    auto* Deformer = Cast<UOptimusDeformer>(Asset);
    if (!IsRunningCommandlet() || !IsInGameThread() || !Deformer)
        return TEXT("ERROR: expected a commandlet deformer");
    TArray<TSharedPtr<FJsonValue>> Graphs;
    TSet<const UOptimusNodeGraph*> Visited;
    TArray<UOptimusNodeGraph*> Pending = Deformer->GetGraphs();
    while (!Pending.IsEmpty())
    {
        const auto* Graph = Pending.Pop();
        if (!Graph) return TEXT("ERROR: null deformer graph");
        if (Visited.Contains(Graph)) continue;
        Visited.Add(Graph);
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("path"), Graph->GetPathName());
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const auto* Node : Graph->GetAllNodes())
        {
            if (!Node) return TEXT("ERROR: null deformer node");
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Node->GetName());
            Entry->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
            if (const auto* Shader = Cast<IOptimusShaderTextProvider>(Node))
            {
                Entry->SetStringField(TEXT("source"), Shader->GetShaderText());
                Entry->SetStringField(TEXT("declarations"), Shader->GetDeclarations());
                Entry->SetBoolField(TEXT("read_only"), Shader->IsShaderTextReadOnly());
            }
            if (const auto* Reference = Cast<IOptimusNodeSubGraphReferencer>(Node))
            {
                auto* Referenced = Reference->GetReferencedSubGraph();
                if (!Referenced) return TEXT("ERROR: unresolved deformer function");
                Entry->SetStringField(TEXT("referenced_graph"), Referenced->GetPathName());
                Pending.Add(Referenced);
            }
            Nodes.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Row->SetArrayField(TEXT("nodes"), MoveTemp(Nodes));
        Graphs.Add(MakeShared<FJsonValueObject>(Row));
        Pending.Append(Graph->GetGraphs());
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset"), Deformer->GetPathName());
    Result->SetArrayField(TEXT("graphs"), MoveTemp(Graphs));
    FString Text;
    if (!FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Text)))
        return TEXT("ERROR: cannot serialize deformer");
    return Text;
}
