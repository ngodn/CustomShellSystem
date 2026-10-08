#include "CSSClothBasisStudy.h"
#include "CSSRigAuditLibrary.h"
#include "Animation/MeshDeformerCollection.h"
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

bool UCSSRigAuditLibrary::CompileClothRuntimeGraph(UObject* Graph)
{
    auto* Deformer = Cast<UOptimusDeformer>(Graph);
    if (!IsRunningCommandlet() || !IsInGameThread() || !Deformer ||
        Deformer->GetPathName() != TEXT("/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphBody.DG_ClothMorphBody"))
        return false;
    return Deformer->Compile();
}

USkeletalMesh* UCSSRigAuditLibrary::CloneClothBasisMeshStudy(USkeletalMesh* Source, const FString& OutputPackage,
    bool bAllShapes, UMeshDeformerCollection* DeformerCollection, UMeshDeformer* RuntimeDeformer)
{
    const auto Fail = [](const TCHAR* Reason) -> USkeletalMesh*
    {
        UE_LOG(LogTemp, Error, TEXT("Cloth basis mesh study: %s"), Reason);
        return nullptr;
    };
    const FString SourcePackage = Source ? Source->GetOutermost()->GetName() : FString();
    const bool bSourceAllowed = SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/SK_EveW3") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/SK_GenessaW3") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_EveW3_Face") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_GenessaW3_Face");
    const FString ExpectedOutput = SourcePackage + (RuntimeDeformer ? TEXT("_BasisStudy3") :
        bAllShapes ? TEXT("_BasisStudy2") : TEXT("_BasisStudy1"));
    if (!IsRunningCommandlet() || !IsInGameThread() || !Source || !bSourceAllowed || OutputPackage != ExpectedOutput ||
        (bAllShapes && (!DeformerCollection || !DeformerCollection->GetPathName().StartsWith(
            RuntimeDeformer ? TEXT("/Game/CSS/eins0fx/Shared/Deformers/MDC_ClothMorphStudy5.") :
                TEXT("/Game/CSS/eins0fx/Shared/Deformers/MDC_ClothMorphStudy4.")))) ||
        (RuntimeDeformer && (!bAllShapes || !RuntimeDeformer->GetPathName().StartsWith(
            TEXT("/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy5.")))) ||
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
    const int32 ShapeCount = bAllShapes ? UE_ARRAY_COUNT(CSSClothBasisStudy::Shapes) : 1;
    TArray<FName> AttributeNames;
    for (int32 ShapeIndex = 0; ShapeIndex != ShapeCount; ++ShapeIndex)
    {
        const auto& Shape = CSSClothBasisStudy::Shapes[ShapeIndex];
        const auto ShapeMorph = Attributes.GetVertexMorphPositionDelta(Shape.Morph);
        if (!ShapeMorph.IsValid()) return Fail(TEXT("missing shape morph"));
        for (int32 Axis = 0; Axis != 3; ++Axis)
        {
            const FName Name(*FString::Printf(TEXT("%s%c"), Shape.Attribute, TEXT('X') + Axis));
            if (Description->VertexAttributes().HasAttribute(Name)) return Fail(TEXT("basis already exists"));
            auto Values = Description->VertexAttributes().RegisterAttribute<float>(Name, 1, 0.f, EMeshAttributeFlags::Lerpable);
            for (const auto Vertex : Skirt)
            {
                if (ShapeMorph[Vertex].ContainsNaN()) return Fail(TEXT("nonfinite morph delta"));
                Values[Vertex] = ShapeMorph[Vertex][Axis];
            }
            AttributeNames.Add(Name);
        }
    }
    Mesh->CommitMeshDescription(0);
    for (auto& Info : Mesh->GetLODInfo(0)->VertexAttributes)
        if (AttributeNames.Contains(Info.Name))
        {
            Info.EnabledForRender.Default = true;
            Info.DataType = ESkeletalMeshVertexAttributeDataType::Float;
        }
    if (DeformerCollection) Mesh->SetTargetMeshDeformers(DeformerCollection);
    if (RuntimeDeformer) Mesh->SetDefaultMeshDeformer(RuntimeDeformer);
    Mesh->Build();
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto* Render = Mesh->GetResourceForRendering();
    if (!Render || Render->LODRenderData.Num() != 1) return Fail(TEXT("missing built render data"));
    for (const FName Name : AttributeNames)
    {
        if (!Render->LODRenderData[0].VertexAttributeBuffers.GetAttributeBuffer(Name))
            return Fail(TEXT("basis missing from built render buffers"));
    }
    Mesh->MarkPackageDirty();
    return Mesh;
}

UObject* UCSSRigAuditLibrary::CloneClothBasisGraphStudy(UObject* Source, const FString& OutputPackage,
    bool bAllShapes, bool bRuntimeBindings)
{
    if (bRuntimeBindings && (!bAllShapes || OutputPackage != TEXT("/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy5")))
        return nullptr;
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
            const int32 ShapeCount = bAllShapes ? UE_ARRAY_COUNT(CSSClothBasisStudy::Shapes) : 1;
            FString BasisCode;
            for (int32 ShapeIndex = 0; ShapeIndex != ShapeCount; ++ShapeIndex)
            {
                const auto& Shape = CSSClothBasisStudy::Shapes[ShapeIndex];
                const FString PinPrefix = bAllShapes ? FString::Printf(TEXT("Basis%d"), ShapeIndex) : TEXT("Basis");
                for (int32 Axis = 0; Axis != 3; ++Axis)
                {
                    auto* Node = Graph->AddDataInterfaceNode(InterfaceClass, FVector2D(-900, (ShapeIndex * 4 + Axis) * 200));
                    if (!Node || Node->GetPins().IsEmpty() || !Graph->AddLink(ComponentOutput, Node->GetPins()[0]))
                        return Fail(TEXT("cannot bind vertex attribute component"));
                    const FString AttributeName = FString::Printf(TEXT("%s%c"), Shape.Attribute, TEXT('X') + Axis);
                    const auto* DataProperty = FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("DataInterfaceData"));
                    auto* Data = DataProperty ? DataProperty->GetObjectPropertyValue_InContainer(Node) : nullptr;
                    const auto* NameProperty = Data ? FindFProperty<FNameProperty>(Data->GetClass(), TEXT("AttributeName")) : nullptr;
                    if (!NameProperty) return Fail(TEXT("vertex attribute metadata unavailable"));
                    NameProperty->SetPropertyValue_InContainer(Data, *AttributeName);
                    if (NameProperty->GetPropertyValue_InContainer(Data) != FName(*AttributeName) ||
                        !Connect(Node->FindPin(TEXT("Value")), *FString::Printf(TEXT("%s%c"), *PinPrefix, TEXT('X') + Axis)))
                        return Fail(TEXT("cannot bind vertex attribute value"));
                }
                const FName WeightName(bRuntimeBindings ? *FString::Printf(TEXT("CSS_Morph_%s"), Shape.Morph) :
                    *FString(Shape.Attribute).Append(TEXT("Weight")));
                auto* Variable = Copy->AddVariable(FOptimusDataTypeRegistry::Get().FindType(*FDoubleProperty::StaticClass()), WeightName);
                auto* ValueNode = Variable ? Graph->AddVariableGetNode(Variable, FVector2D(-900, (ShapeIndex * 4 + 3) * 200)) : nullptr;
                if (!ValueNode || ValueNode->GetPins().Num() != 1 ||
                    !Connect(ValueNode->GetPins()[0], *FString(PinPrefix).Append(TEXT("Weight"))))
                    return Fail(TEXT("cannot bind instance weight"));
                BasisCode += FString::Printf(TEXT("MorphDelta += Read%sWeight() * float3(Read%sX(Index), Read%sY(Index), Read%sZ(Index));\n"),
                    *PinPrefix, *PinPrefix, *PinPrefix, *PinPrefix);
            }
            FString Text = Shader->GetShaderText();
            const FString Replacement = TEXT("float3 MorphDelta = ReadMorphDelta(Index);\n")
                TEXT("#if ENABLE_DEFORMER_CLOTH && !ENABLE_DEFORMER_MORPHTARGET\n") + BasisCode + TEXT("#endif\n");
            if (Text.ReplaceInline(TEXT("float3 MorphDelta = ReadMorphDelta(Index);"), *Replacement, ESearchCase::CaseSensitive) != 1)
                return Fail(TEXT("position kernel guard failed"));
            Shader->SetShaderText(Text);
            ++Edited;
        }
    }
    if (Edited != 1 || !Copy->Compile()) return Fail(TEXT("basis graph compile failed"));
    Copy->MarkPackageDirty();
    return Copy;
}
