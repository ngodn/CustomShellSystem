#include "CSSClothBasisStudy.h"
#include "CSSRigAuditLibrary.h"
#include "CSSJsonAuditArray.h"
#include "Animation/MorphTarget.h"
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
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshAttributeVertexBuffer.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

FString UCSSRigAuditLibrary::SnapshotVertexAttributeStorage(USkeletalMesh* Mesh)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !IsValid(Mesh))
        return TEXT("ERROR: commandlet mesh required");
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto* Model = Mesh->GetImportedModel();
    if (!Model || Model->LODModels.Num() != 1 || !Mesh->GetLODInfo(0))
        return TEXT("ERROR: expected one imported LOD");
    const auto& Lod = Model->LODModels[0];
    TArray<TSharedPtr<FJsonValue>> Rows;
    bool bValid = true;
    for (const auto& Info : Mesh->GetLODInfo(0)->VertexAttributes)
    {
        const auto* Attribute = Lod.VertexAttributes.Find(Info.Name);
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Info.Name.ToString());
        Row->SetBoolField(TEXT("enabled"), Info.IsEnabledForRender());
        Row->SetBoolField(TEXT("present"), Attribute != nullptr);
        bool bAttributeValid = Attribute && Attribute->ComponentCount >= 1 && Attribute->ComponentCount <= 4;
        if (Attribute)
        {
            Row->SetNumberField(TEXT("component_count"), Attribute->ComponentCount);
            Row->SetNumberField(TEXT("values"), Attribute->Values.Num());
            Row->SetBoolField(TEXT("has_data"), Attribute->Values.GetData() != nullptr);
            bAttributeValid &= static_cast<int64>(Lod.NumVertices) * Attribute->ComponentCount == Attribute->Values.Num();
            bAttributeValid &= Attribute->Values.Num() == 0 || Attribute->Values.GetData() != nullptr;
            if (bAttributeValid)
                for (float Value : Attribute->Values)
                    bAttributeValid &= FMath::IsFinite(Value);
        }
        Row->SetBoolField(TEXT("valid"), bAttributeValid);
        if (Info.IsEnabledForRender()) bValid &= bAttributeValid;
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("valid"), bValid);
    Result->SetNumberField(TEXT("imported_vertices"), Lod.NumVertices);
    Result->SetArrayField(TEXT("attributes"), MoveTemp(Rows));
    FString Output;
    const auto Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Result, Writer);
    return Output;
}

FString UCSSRigAuditLibrary::CheckClothBasisBuffers(USkeletalMesh* Mesh, const TArray<FName>& MaterialSlots)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Mesh || MaterialSlots.IsEmpty())
        return TEXT("ERROR: commandlet mesh and cloth slots required");
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto* Render = Mesh->GetResourceForRendering();
    if (!Render || Render->LODRenderData.Num() != 1) return TEXT("ERROR: expected one render LOD");
    const auto& Lod = Render->LODRenderData[0];
    TSet<FName> Requested(MaterialSlots), Found;
    if (Requested.Num() != MaterialSlots.Num()) return TEXT("ERROR: duplicate cloth slot");
    TArray<bool> Selected;
    Selected.Init(false, Lod.GetNumVertices());
    for (const auto& Section : Lod.RenderSections)
    {
        if (!Mesh->GetMaterials().IsValidIndex(Section.MaterialIndex)) return TEXT("ERROR: invalid material index");
        const FName Slot = Mesh->GetMaterials()[Section.MaterialIndex].MaterialSlotName;
        if (!Requested.Contains(Slot)) continue;
        if (!Section.HasClothingData()) return TEXT("ERROR: requested slot has no cloth binding");
        Found.Add(Slot);
        const uint32 End = Section.BaseVertexIndex + Section.NumVertices;
        if (End < Section.BaseVertexIndex || End > Lod.GetNumVertices()) return TEXT("ERROR: invalid section range");
        for (uint32 Index = Section.BaseVertexIndex; Index < End; ++Index) Selected[Index] = true;
    }
    if (Found.Num() != Requested.Num()) return TEXT("ERROR: missing cloth section");
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const auto& Shape : CSSClothBasisStudy::Shapes)
    {
        const auto* Morph = Mesh->FindMorphTarget(Shape.Morph);
        if (!Morph || Morph->GetMorphLODModels().Num() != 1) return TEXT("ERROR: missing shape render morph");
        TArray<FVector3f> Expected;
        Expected.Init(FVector3f::ZeroVector, Lod.GetNumVertices());
        for (const auto& Delta : Morph->GetMorphLODModels()[0].Vertices)
        {
            if (Delta.SourceIdx >= Lod.GetNumVertices() || Delta.PositionDelta.ContainsNaN())
                return TEXT("ERROR: invalid render morph delta");
            if (Selected[Delta.SourceIdx]) Expected[Delta.SourceIdx] = Delta.PositionDelta;
        }
        for (int32 Axis = 0; Axis != 3; ++Axis)
        {
            const FName Name(*FString::Printf(TEXT("%s%c"), Shape.Attribute, TEXT('X') + Axis));
            const auto* Buffer = Lod.VertexAttributeBuffers.GetAttributeBuffer(Name);
            if (!Buffer) return TEXT("ERROR: missing basis render buffer");
            const auto Cpu = Buffer->GetCPUData();
            if (!Cpu.Data || Cpu.PixelFormat != PF_R32_FLOAT || Cpu.Data->GetStride() != sizeof(float) ||
                Cpu.Data->GetNumVertices() != Lod.GetNumVertices() || !Cpu.Data->GetDataPointer())
                return TEXT("ERROR: invalid basis render buffer format");
            float MaximumError = 0.f;
            FJsonAuditArray Values(true);
            for (uint32 Index = 0; Index < Lod.GetNumVertices(); ++Index)
            {
                float Value;
                FMemory::Memcpy(&Value, Cpu.Data->GetDataPointer() + Index * sizeof(float), sizeof(float));
                if (!FMath::IsFinite(Value)) return TEXT("ERROR: nonfinite basis render value");
                if (!Selected[Index] && Value != 0.f) return TEXT("ERROR: basis affects a non-cloth vertex");
                MaximumError = FMath::Max(MaximumError, FMath::Abs(Value - Expected[Index][Axis]));
                Values.Add(MakeShared<FJsonValueNumber>(Value));
            }
            // Imported render morphs omit deltas below the existing 0.015 cm threshold.
            if (MaximumError > 0.02f) return TEXT("ERROR: basis differs from the render morph");
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), Name.ToString());
            Row->SetNumberField(TEXT("maximum_render_morph_error_cm"), MaximumError);
            Row->SetField(TEXT("values"), Values.Finish());
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    auto Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("buffers"), MoveTemp(Rows));
    Result->SetNumberField(TEXT("vertices"), Lod.GetNumVertices());
    Result->SetNumberField(TEXT("cloth_slots"), Found.Num());
    FString Output;
    const auto Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Result, Writer)) return TEXT("ERROR: cannot serialize basis check");
    return Output;
}

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
    const bool bPeony = SourcePackage == TEXT("/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/EveSB/Peony/Meshes/SK_Peony_Face");
    const bool bSourceAllowed = bPeony || SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/SK_EveW3") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/SK_GenessaW3") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_EveW3_Face") ||
        SourcePackage == TEXT("/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_GenessaW3_Face");
    const FString ExpectedOutput = SourcePackage + (RuntimeDeformer ? TEXT("_BasisStudy3") :
        bAllShapes ? TEXT("_BasisStudy2") : TEXT("_BasisStudy1"));
    const bool bBindingsAllowed = bPeony ?
        bAllShapes && DeformerCollection && RuntimeDeformer &&
        DeformerCollection->GetPathName() == TEXT("/Game/CSS/eins0fx/Shared/Deformers/MDC_ClothMorphBody.MDC_ClothMorphBody") &&
        RuntimeDeformer->GetPathName() == TEXT("/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphBody.DG_ClothMorphBody") :
        !(bAllShapes && (!DeformerCollection || !DeformerCollection->GetPathName().StartsWith(
            RuntimeDeformer ? TEXT("/Game/CSS/eins0fx/Shared/Deformers/MDC_ClothMorphStudy5.") :
                TEXT("/Game/CSS/eins0fx/Shared/Deformers/MDC_ClothMorphStudy4.")))) &&
        !(RuntimeDeformer && (!bAllShapes || !RuntimeDeformer->GetPathName().StartsWith(
            TEXT("/Game/CSS/eins0fx/Shared/Deformers/DG_ClothMorphStudy5."))));
    if (!IsRunningCommandlet() || !IsInGameThread() || !Source || !bSourceAllowed || OutputPackage != ExpectedOutput ||
        !bBindingsAllowed ||
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
    const TSet<FName> ExpectedSlots = bPeony ? TSet<FName>{TEXT("Peony_leg_panels_Lower_2_L"),
        TEXT("Peony_leg_panels_Lower_2_R")} : TSet<FName>{TEXT("UG_Skirt")};
    TSet<FName> SkirtSlots, FoundSlots;
    for (const auto& Material : Mesh->GetMaterials())
        if (ExpectedSlots.Contains(Material.MaterialSlotName))
        {
            if (FoundSlots.Contains(Material.MaterialSlotName) || Material.ImportedMaterialSlotName.IsNone() ||
                SkirtSlots.Contains(Material.ImportedMaterialSlotName))
                return Fail(TEXT("ambiguous cloth imported material slot"));
            FoundSlots.Add(Material.MaterialSlotName);
            SkirtSlots.Add(Material.ImportedMaterialSlotName);
        }
    if (FoundSlots.Num() != ExpectedSlots.Num()) return Fail(TEXT("missing cloth imported material slot"));
    TSet<FVertexID> Skirt, Other;
    for (const auto Group : Description->PolygonGroups().GetElementIDs())
    {
        auto& Used = SkirtSlots.Contains(Slots[Group]) ? Skirt : Other;
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
