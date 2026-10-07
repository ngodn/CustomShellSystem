#include "CSSRigAuditLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

FString UCSSRigAuditLibrary::SnapshotAssetProperties(UObject* Asset)
{
    if (!IsRunningCommandlet() || !IsInGameThread() || !Asset ||
        (!Asset->IsA<UAnimBlueprint>() && !Asset->IsA<UAnimBoneCompressionSettings>()))
        return TEXT("ERROR: expected a commandlet animation blueprint or bone compression settings");
    TArray<UObject*> Objects;
    GetObjectsWithPackage(Asset->GetOutermost(), Objects, true, RF_Transient);
    Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
    auto Root = MakeShared<FJsonObject>();
    for (UObject* Object : Objects)
    {
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
