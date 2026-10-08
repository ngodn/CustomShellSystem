#include "CSSRigAuditLibrary.h"
#include "Animation/SkeletalMeshVertexAttribute.h"
#include "Engine/SkeletalMesh.h"
#include "IOptimusNodeAdderPinProvider.h"
#include "IOptimusShaderTextProvider.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "OptimusComputeDataInterface.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodePin.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

USkeletalMesh* UCSSRigAuditLibrary::CloneClothBasisMeshStudy(USkeletalMesh* Source, const FString& OutputPackage)
{
    const auto Fail = [](const TCHAR* Reason) -> USkeletalMesh*
    {
        UE_LOG(LogTemp, Error, TEXT("Cloth basis mesh study: %s"), Reason);
        return nullptr;
    };
    if (!IsRunningCommandlet() || !IsInGameThread() || !Source ||
        !Source->GetPathName().StartsWith(TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/")) ||
        !OutputPackage.StartsWith(TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/")) ||
        !OutputPackage.EndsWith(TEXT("_BasisStudy1")) ||
        !FPackageName::IsValidLongPackageName(OutputPackage) ||
        FindPackage(nullptr, *OutputPackage) || FPackageName::DoesPackageExist(OutputPackage))
        return Fail(TEXT("invalid commandlet source or private destination"));
    USkinnedAsset* SourceAssets[]{Source};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(SourceAssets);
    auto* Mesh = Cast<USkeletalMesh>(StaticDuplicateObject(Source, CreatePackage(*OutputPackage),
        *FPackageName::GetLongPackageAssetName(OutputPackage)));
    if (!Mesh || Mesh->GetLODNum() != 1) return Fail(TEXT("expected one-LOD private mesh"));
    auto* Description = Mesh->GetMeshDescription(0);
    if (!Description) return Fail(TEXT("missing source geometry"));
    FSkeletalMeshAttributes Attributes(*Description);
    const auto Morph = Attributes.GetVertexMorphPositionDelta(TEXT("PBMGlutesSize"));
    if (!Morph.IsValid()) return Fail(TEXT("missing glute morph"));
    const auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    FName SkirtSlot;
    int32 Matches = 0;
    for (const auto& Material : Mesh->GetMaterials())
        if (Material.MaterialSlotName == TEXT("UG_Skirt"))
        {
            SkirtSlot = Material.ImportedMaterialSlotName;
            ++Matches;
        }
    if (Matches != 1 || SkirtSlot.IsNone()) return Fail(TEXT("ambiguous skirt imported material slot"));
    TSet<FVertexID> Skirt, Other;
    for (const auto Group : Description->PolygonGroups().GetElementIDs())
    {
        auto& Used = Slots[Group] == SkirtSlot ? Skirt : Other;
        for (const auto Triangle : Description->GetPolygonGroupTriangles(Group))
            for (const auto Corner : Description->GetTriangleVertexInstances(Triangle))
                Used.Add(Description->GetVertexInstanceVertex(Corner));
    }
    const auto Shared = Skirt.Intersect(Other);
    int32 SharedNonzero = 0;
    float SharedMaximum = 0.f;
    for (const auto Vertex : Shared)
    {
        if (Morph[Vertex].ContainsNaN()) return Fail(TEXT("nonfinite shared morph delta"));
        SharedMaximum = FMath::Max(SharedMaximum, Morph[Vertex].Length());
        if (Morph[Vertex] != FVector3f::ZeroVector) ++SharedNonzero;
    }
    UE_LOG(LogTemp, Display, TEXT("Cloth basis study: shared vertices %d, nonzero glute deltas %d, maximum %g cm"),
        Shared.Num(), SharedNonzero, SharedMaximum);
    if (Skirt.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Cloth basis study: imported slot %s, skirt vertices %d, shared vertices %d"),
            *SkirtSlot.ToString(), Skirt.Num(), Skirt.Intersect(Other).Num());
        return Fail(TEXT("missing skirt vertices"));
    }
    for (int32 Axis = 0; Axis != 3; ++Axis)
    {
        const FName Name(*FString::Printf(TEXT("CSS_ClothGlute%c"), TEXT('X') + Axis));
        if (Description->VertexAttributes().HasAttribute(Name)) return Fail(TEXT("basis already exists"));
        auto Values = Description->VertexAttributes().RegisterAttribute<float>(Name, 1, 0.f, EMeshAttributeFlags::Lerpable);
        for (const auto Vertex : Skirt)
        {
            if (Morph[Vertex].ContainsNaN()) return Fail(TEXT("nonfinite morph delta"));
            Values[Vertex] = Morph[Vertex][Axis];
        }
    }
    Mesh->CommitMeshDescription(0);
    for (auto& Info : Mesh->GetLODInfo(0)->VertexAttributes)
        if (Info.Name.ToString().StartsWith(TEXT("CSS_ClothGlute")))
        {
            Info.EnabledForRender.Default = true;
            Info.DataType = ESkeletalMeshVertexAttributeDataType::Float;
        }
    Mesh->Build();
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto* Render = Mesh->GetResourceForRendering();
    if (!Render || Render->LODRenderData.Num() != 1) return Fail(TEXT("missing built render data"));
    for (int32 Axis = 0; Axis != 3; ++Axis)
    {
        const FName Name(*FString::Printf(TEXT("CSS_ClothGlute%c"), TEXT('X') + Axis));
        if (!Render->LODRenderData[0].VertexAttributeBuffers.GetAttributeBuffer(Name))
            return Fail(TEXT("basis missing from built render buffers"));
    }
    Mesh->MarkPackageDirty();
    return Mesh;
}

UObject* UCSSRigAuditLibrary::CloneClothBasisGraphStudy(UObject* Source, const FString& OutputPackage)
{
    auto* Copy = Cast<UOptimusDeformer>(CloneClothMorphStudy(Source, OutputPackage));
    if (!Copy) return nullptr;
    const auto Fail = [](const TCHAR* Reason) -> UObject*
    {
        UE_LOG(LogTemp, Error, TEXT("Cloth basis graph study: %s"), Reason);
        return nullptr;
    };
    auto* InterfaceClass = FindObject<UClass>(nullptr, TEXT("/Script/OptimusCore.OptimusSkinnedMeshVertexAttributeDataInterface"));
    if (!InterfaceClass || !InterfaceClass->IsChildOf(UOptimusComputeDataInterface::StaticClass()))
        return Fail(TEXT("vertex attribute interface unavailable"));
    int32 Edited = 0;
    for (auto* Graph : Copy->GetGraphs())
    {
        for (auto* Kernel : TArray<UOptimusNode*>(Graph->GetAllNodes()))
        {
            auto* Shader = Cast<IOptimusShaderTextProvider>(Kernel);
            if (!Shader || !Shader->GetShaderText().Contains(TEXT("float3 MorphDelta = ReadMorphDelta(Index);"))) continue;
            auto* Adder = Cast<IOptimusNodeAdderPinProvider>(Kernel);
            if (!Adder) return Fail(TEXT("position kernel cannot add bindings"));
            auto* Binding = Copy->GetPrimaryComponentBinding();
            auto* ComponentNode = Binding ? Graph->AddComponentBindingGetNode(Binding, FVector2D(-1200, 0)) : nullptr;
            if (!ComponentNode || ComponentNode->GetPins().Num() != 1)
                return Fail(TEXT("source component binding unavailable"));
            auto* ComponentOutput = ComponentNode->GetPins()[0];
            const auto Connect = [&](UOptimusNodePin* Output, FName Name) -> bool
            {
                if (!Output) return false;
                const auto Actions = Adder->GetAvailableAdderPinActions(Output, EOptimusNodePinDirection::Input);
                for (const auto& Action : Actions)
                    if (Action.bCanAutoLink)
                    {
                        const auto Added = Adder->TryAddPinFromPin(Action, Output, Name);
                        return Added.Num() == 1 && Graph->AddLink(Output, Added[0]);
                    }
                return false;
            };
            for (int32 Axis = 0; Axis != 3; ++Axis)
            {
                auto* Node = Graph->AddDataInterfaceNode(InterfaceClass, FVector2D(-900, Axis * 200));
                if (!Node || Node->GetPins().IsEmpty() || !Graph->AddLink(ComponentOutput, Node->GetPins()[0]))
                    return Fail(TEXT("cannot bind vertex attribute component"));
                const FString AttributeName = FString::Printf(TEXT("CSS_ClothGlute%c"), TEXT('X') + Axis);
                const auto* DataProperty = FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("DataInterfaceData"));
                auto* Data = DataProperty ? DataProperty->GetObjectPropertyValue_InContainer(Node) : nullptr;
                const auto* NameProperty = Data ? FindFProperty<FNameProperty>(Data->GetClass(), TEXT("AttributeName")) : nullptr;
                if (!NameProperty) return Fail(TEXT("vertex attribute metadata unavailable"));
                NameProperty->SetPropertyValue_InContainer(Data, *AttributeName);
                if (NameProperty->GetPropertyValue_InContainer(Data) != FName(*AttributeName) ||
                    !Connect(Node->FindPin(TEXT("Value")), *FString::Printf(TEXT("Basis%c"), TEXT('X') + Axis)))
                    return Fail(TEXT("cannot bind vertex attribute value"));
            }
            auto* Variable = Copy->AddVariable(FOptimusDataTypeRegistry::Get().FindType(*FDoubleProperty::StaticClass()), TEXT("CSS_ClothGluteWeight"));
            auto* ValueNode = Variable ? Graph->AddVariableGetNode(Variable, FVector2D(-900, 600)) : nullptr;
            if (!ValueNode || ValueNode->GetPins().Num() != 1 || !Connect(ValueNode->GetPins()[0], TEXT("BasisWeight")))
                return Fail(TEXT("cannot bind instance weight"));
            FString Text = Shader->GetShaderText();
            if (Text.ReplaceInline(TEXT("float3 MorphDelta = ReadMorphDelta(Index);"),
                TEXT("float3 MorphDelta = ReadMorphDelta(Index);\n"
                     "#if ENABLE_DEFORMER_CLOTH && !ENABLE_DEFORMER_MORPHTARGET\n"
                     "MorphDelta += ReadBasisWeight() * float3(ReadBasisX(Index), ReadBasisY(Index), ReadBasisZ(Index));\n"
                     "#endif\n"), ESearchCase::CaseSensitive) != 1)
                return Fail(TEXT("position kernel guard failed"));
            Shader->SetShaderText(Text);
            ++Edited;
        }
    }
    if (Edited != 1 || !Copy->Compile()) return Fail(TEXT("basis graph compile failed"));
    Copy->MarkPackageDirty();
    return Copy;
}
