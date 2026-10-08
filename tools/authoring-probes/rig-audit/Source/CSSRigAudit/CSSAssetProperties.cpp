#include "CSSRigAuditLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

FString UCSSRigAuditLibrary::SnapshotAssetProperties(UObject* Asset)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Asset ||
        (!Asset->IsA<UAnimBlueprint>() && !Asset->IsA<UAnimBoneCompressionSettings>() &&
         !Asset->IsA<UMaterialInterface>() && !Asset->IsA<UTexture>()))
        return TEXT("ERROR: expected a commandlet animation blueprint, codec, material or texture");
    TArray<UObject*> Objects;
    GetObjectsWithPackage(Asset->GetOutermost(), Objects, true, RF_Transient);
    Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
    auto Root = MakeShared<FJsonObject>();
    for (UObject* Object : Objects)
    {
        // Recompilation discards this generated struct when it has no fields
        // (FAnimBlueprintCompilerContext::BuildDataArea). It stores no pose state.
        const auto* Struct = Cast<UScriptStruct>(Object);
        if (Struct && Cast<UAnimBlueprintGeneratedClass>(Struct->GetOuter()) &&
            Struct->GetFName() == UAnimBlueprintGeneratedClass::GetMutablesStructName() &&
            !TFieldIterator<FProperty>(Struct)) continue;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
        auto Properties = MakeShared<FJsonObject>();
        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
            TArray<TSharedPtr<FJsonValue>> Values;
            for (int32 Index = 0; Index < It->ArrayDim; ++Index)
            {
                FString Value;
                It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(Object, Index),
                    nullptr, Object, PPF_None);
                Values.Add(MakeShared<FJsonValueString>(Value));
            }
            Properties->SetArrayField(It->GetName(), MoveTemp(Values));
        }
        Row->SetObjectField(TEXT("properties"), Properties);
        if (auto* Texture = Cast<UTexture>(Object))
        {
            // Reflected properties omit the source pixels stored in bulk data.
            auto& Source = Texture->Source;
            if (Source.GetNumBlocks() != 1 || Source.GetNumLayers() != 1 || !Source.HasPayloadData())
                return TEXT("ERROR: texture audit requires one populated source block and layer");
            auto Pixels = MakeShared<FJsonObject>();
            Pixels->SetNumberField(TEXT("width"), Source.GetSizeX());
            Pixels->SetNumberField(TEXT("height"), Source.GetSizeY());
            Pixels->SetNumberField(TEXT("slices"), Source.GetNumSlices());
            Pixels->SetNumberField(TEXT("format"), Source.GetFormat());
            TArray<TSharedPtr<FJsonValue>> Mips;
            for (int32 Mip = 0; Mip < Source.GetNumMips(); ++Mip)
            {
                TArray64<uint8> Data;
                if (!Source.GetMipData(Data, Mip)) return TEXT("ERROR: cannot read texture source mip");
                auto Value = MakeShared<FJsonObject>();
                Value->SetStringField(TEXT("bytes"), LexToString(Data.Num()));
                Value->SetStringField(TEXT("sha1"), FSHA1::HashBuffer(Data.GetData(), Data.Num()).ToString());
                Mips.Add(MakeShared<FJsonValueObject>(Value));
            }
            Pixels->SetArrayField(TEXT("mips"), MoveTemp(Mips));
            Row->SetObjectField(TEXT("source_pixels"), Pixels);
        }
        if (const auto* Node = Cast<UEdGraphNode>(Object))
        {
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) return TEXT("ERROR: null graph pin");
                auto Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("id"), Pin->PinId.ToString());
                Item->SetStringField(TEXT("name"), Pin->PinName.ToString());
                Item->SetNumberField(TEXT("direction"), Pin->Direction);
                Item->SetStringField(TEXT("default"), Pin->DefaultValue);
                Item->SetStringField(TEXT("default_object"), GetPathNameSafe(Pin->DefaultObject));
                Item->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
                FString Type;
                FEdGraphPinType::StaticStruct()->ExportText(Type, &Pin->PinType, nullptr, nullptr, PPF_None, nullptr);
                Item->SetStringField(TEXT("type"), Type);
                TArray<TSharedPtr<FJsonValue>> Links;
                for (const UEdGraphPin* Link : Pin->LinkedTo)
                {
                    if (!Link || !Link->GetOwningNode()) return TEXT("ERROR: invalid graph link");
                    Links.Add(MakeShared<FJsonValueString>(Link->GetOwningNode()->GetPathName() + TEXT(":") + Link->PinId.ToString()));
                }
                Item->SetArrayField(TEXT("links"), MoveTemp(Links));
                Pins.Add(MakeShared<FJsonValueObject>(Item));
            }
            Row->SetArrayField(TEXT("pins"), MoveTemp(Pins));
        }
        Root->SetObjectField(Object->GetPathName(), Row);
    }
    if (Objects.IsEmpty()) return TEXT("ERROR: empty asset package");
    FString Result;
    if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result)))
        return TEXT("ERROR: cannot serialize asset properties");
    return Result;
}
