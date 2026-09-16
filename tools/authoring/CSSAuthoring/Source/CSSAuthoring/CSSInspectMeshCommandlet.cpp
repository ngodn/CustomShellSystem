#include "CSSInspectMeshCommandlet.h"

#include "Animation/MorphTarget.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCSSInspect, Log, All);

UCSSInspectMeshCommandlet::UCSSInspectMeshCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UCSSInspectMeshCommandlet::Main(const FString& Params)
{
    auto Fail = [](const FString& Why) {
        UE_LOG(LogCSSInspect, Error, TEXT("%s"), *Why);
        return 1;
    };
    FString AssetPath, OutputPath;
    if (!FParse::Value(*Params, TEXT("Mesh="), AssetPath))
        return Fail(TEXT("Specify -Mesh=/Game/Path/SK_Name"));
    FParse::Value(*Params, TEXT("Output="), OutputPath);

    // An object path needs its object name after the dot, the same rule the runtime uses.
    if (!AssetPath.Contains(TEXT(".")))
        AssetPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *AssetPath);
    if (!Mesh)
        return Fail(FString::Printf(TEXT("Could not load %s"), *AssetPath));

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Root->SetStringField(TEXT("skeleton"), Mesh->GetSkeleton() ? Mesh->GetSkeleton()->GetPathName() : TEXT(""));
    Root->SetNumberField(TEXT("bones"), Mesh->GetRefSkeleton().GetNum());

    TArray<TSharedPtr<FJsonValue>> Slots;
    for (const FSkeletalMaterial& Material : Mesh->GetMaterials())
        Slots.Add(MakeShared<FJsonValueString>(Material.MaterialSlotName.ToString()));
    Root->SetArrayField(TEXT("material_slots"), Slots);

    const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
    TArray<TSharedPtr<FJsonValue>> Lods;
    for (int32 I = 0; Model && I < Model->LODModels.Num(); ++I)
    {
        const FSkeletalMeshLODModel& Lod = Model->LODModels[I];
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("vertices"), Lod.NumVertices);
        Entry->SetNumberField(TEXT("triangles"), Lod.IndexBuffer.Num() / 3);
        Entry->SetNumberField(TEXT("sections"), Lod.Sections.Num());
        Lods.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("lods"), Lods);

    // The part this exists for: what a shape slider will actually move, and by how much.
    TArray<TSharedPtr<FJsonValue>> Morphs;
    for (UMorphTarget* Morph : Mesh->GetMorphTargets())
    {
        if (!Morph) continue;
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Morph->GetName());
        int32 Deltas = 0;
        double Largest = 0;
        TArray<TSharedPtr<FJsonValue>> Sample;
        if (Morph->GetMorphLODModels().Num() > 0)
        {
            const FMorphTargetLODModel& Lod = Morph->GetMorphLODModels()[0];
            Deltas = Lod.Vertices.Num();
            for (const FMorphTargetDelta& Delta : Lod.Vertices)
            {
                Largest = FMath::Max(Largest, static_cast<double>(Delta.PositionDelta.Size()));
                if (Sample.Num() < 8)
                {
                    TArray<TSharedPtr<FJsonValue>> Vector;
                    Vector.Add(MakeShared<FJsonValueNumber>(Delta.SourceIdx));
                    Vector.Add(MakeShared<FJsonValueNumber>(Delta.PositionDelta.X));
                    Vector.Add(MakeShared<FJsonValueNumber>(Delta.PositionDelta.Y));
                    Vector.Add(MakeShared<FJsonValueNumber>(Delta.PositionDelta.Z));
                    Sample.Add(MakeShared<FJsonValueArray>(Vector));
                }
            }
        }
        Entry->SetNumberField(TEXT("moved_vertices"), Deltas);
        Entry->SetNumberField(TEXT("largest_delta_cm"), Largest);
        Entry->SetArrayField(TEXT("sample"), Sample);
        Morphs.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("morph_targets"), Morphs);

    FString Text;
    const auto Writer = TJsonWriterFactory<>::Create(&Text);
    if (!FJsonSerializer::Serialize(Root, Writer))
        return Fail(TEXT("Could not serialize the report"));
    if (!OutputPath.IsEmpty() && !FFileHelper::SaveStringToFile(Text, *OutputPath))
        return Fail(FString::Printf(TEXT("Could not write %s"), *OutputPath));
    UE_LOG(LogCSSInspect, Display, TEXT("%s"), *Text);
    return 0;
}
