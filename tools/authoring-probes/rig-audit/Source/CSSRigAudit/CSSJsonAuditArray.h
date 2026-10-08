#pragma once

#include "Hash/Blake3.h"
#include "Serialization/JsonSerializer.h"

// Large mesh arrays are hashed a row at a time. The ordinary snapshot remains
// available for visual inspection and independent comparison of the digests.
class FJsonAuditArray
{
    class FHashArchive final : public FArchive
    {
    public:
        FBlake3 Hash;
        uint64 Bytes = 0;

        virtual void Serialize(void* Data, int64 Size) override
        {
            check(Size >= 0);
            Hash.Update(Data, static_cast<uint64>(Size));
            Bytes += static_cast<uint64>(Size);
        }
    };

    using FWriter = TJsonWriter<UTF8CHAR, TCondensedJsonPrintPolicy<UTF8CHAR>>;
    FHashArchive Archive;
    TSharedPtr<FWriter> Writer;
    TArray<TSharedPtr<FJsonValue>> Rows;
    uint64 Count = 0;
    bool bValid = true;

public:
    explicit FJsonAuditArray(bool bDigest)
    {
        if (bDigest)
        {
            Writer = TJsonWriterFactory<UTF8CHAR, TCondensedJsonPrintPolicy<UTF8CHAR>>::Create(&Archive);
            Writer->WriteArrayStart();
        }
    }

    void Add(TSharedPtr<FJsonValue> Row)
    {
        ++Count;
        if (Writer)
            bValid &= FJsonSerializer::Serialize(Row, FString(), Writer.ToSharedRef(), false);
        else
            Rows.Add(MoveTemp(Row));
    }

    TSharedPtr<FJsonValue> Finish()
    {
        if (!Writer) return MakeShared<FJsonValueArray>(MoveTemp(Rows));
        Writer->WriteArrayEnd();
        bValid &= Writer->Close();
        check(bValid && !Archive.IsError());
        auto Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("count"), static_cast<double>(Count));
        Result->SetNumberField(TEXT("utf8_bytes"), static_cast<double>(Archive.Bytes));
        Result->SetStringField(TEXT("blake3"), LexToString(Archive.Hash.Finalize()));
        return MakeShared<FJsonValueObject>(Result);
    }
};
