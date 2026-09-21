#include "CSSCopyAssetsCommandlet.h"
#include "AssetHeaderPatcher.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogCSSCopy, Log, All);

namespace
{
// Keep redirects for previously migrated dependencies, but write only the
// requested new packages. Never let dependency gathering add source writes.
struct FCopyContext : FAssetHeaderPatcher::FContext
{
    FCopyContext(const TMap<FString, FString>& Mapping, const TSet<FString>& Sources)
        : FContext(Mapping, false)
    {
        for (auto It = FilePathRenameMap.CreateIterator(); It; ++It)
        {
            if (!Sources.Contains(FPaths::ConvertRelativePathToFull(It.Key()))) It.RemoveCurrent();
        }
    }
};

bool PortablePackage(const FString& Name)
{
    if (!Name.StartsWith(TEXT("/Game/CSS/")) || Name.Len() > 96 || !FPackageName::IsValidLongPackageName(Name)) return false;
    TArray<FString> Parts;
    Name.Mid(1).ParseIntoArray(Parts, TEXT("/"), false);
    const FRegexPattern Component(TEXT("^[A-Za-z0-9_]{1,48}$"));
    const FRegexPattern Hash(TEXT("[0-9a-fA-F]{16,}"));
    for (const FString& Part : Parts)
    {
        FRegexMatcher Valid(Component, Part), HasHash(Hash, Part);
        const FString Upper = Part.ToUpper();
        if (!Valid.FindNext() || HasHash.FindNext() || Upper == TEXT("CON") || Upper == TEXT("PRN") ||
            Upper == TEXT("AUX") || Upper == TEXT("NUL") ||
            (Upper.Len() == 4 && (Upper.StartsWith(TEXT("COM")) || Upper.StartsWith(TEXT("LPT"))) && Upper[3] >= '1' && Upper[3] <= '9')) return false;
    }
    return true;
}
}

UCSSCopyAssetsCommandlet::UCSSCopyAssetsCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UCSSCopyAssetsCommandlet::Main(const FString& Params)
{
    FString Input, Text;
    TSharedPtr<FJsonObject> Json;
    if (!FParse::Value(*Params, TEXT("Input="), Input) || !FFileHelper::LoadFileToString(Text, *Input) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json)
    {
        UE_LOG(LogCSSCopy, Error, TEXT("Expected -Input=<explicit mapping and copy list>")); return 1;
    }
    const TSharedPtr<FJsonObject>* MappingJson;
    const TArray<TSharedPtr<FJsonValue>>* CopyJson;
    if (!Json->TryGetObjectField(TEXT("mapping"), MappingJson) || !Json->TryGetArrayField(TEXT("copy"), CopyJson)) return 1;
    TMap<FString, FString> Mapping;
    TSet<FString> Destinations, Requested, SourceFiles;
    for (const auto& Value : *CopyJson)
    {
        FString Source;
        if (!Value->TryGetString(Source) || Requested.Contains(Source)) return 1;
        Requested.Add(Source);
    }
    if (Requested.IsEmpty()) return 1;
    for (const auto& Pair : (*MappingJson)->Values)
    {
        FString Destination, SourceFile;
        if ((!Pair.Key.StartsWith(TEXT("/Game/CSS/")) && !Pair.Key.StartsWith(TEXT("/Game/CSSAuthoring/"))) ||
            !FPackageName::IsValidLongPackageName(Pair.Key) || !Pair.Value->TryGetString(Destination) ||
            !PortablePackage(Destination) || Pair.Key == Destination || Destinations.Contains(Destination.ToLower()) ||
            !FPackageName::DoesPackageExist(Pair.Key, &SourceFile))
        {
            UE_LOG(LogCSSCopy, Error, TEXT("Invalid or missing mapping source: %s"), *Pair.Key); return 1;
        }
        const FString TargetFile = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Destination, TEXT(".uasset")));
        const bool Exists = IFileManager::Get().FileExists(*TargetFile);
        if (TargetFile.Len() > 240 || (Requested.Contains(Pair.Key) ? Exists : !Exists))
        {
            UE_LOG(LogCSSCopy, Error, TEXT("Copy targets must be new; reference-only targets must exist; paths must be short: %s"), *TargetFile); return 1;
        }
        if (Requested.Contains(Pair.Key)) SourceFiles.Add(FPaths::ConvertRelativePathToFull(SourceFile));
        Mapping.Add(Pair.Key, Destination); Destinations.Add(Destination.ToLower());
    }
    if (SourceFiles.Num() != Requested.Num()) return 1;
    for (const auto& Pair : Mapping)
    {
        if (Mapping.Contains(Pair.Value))
        {
            UE_LOG(LogCSSCopy, Error, TEXT("A destination overlaps a source: %s"), *Pair.Value); return 1;
        }
    }
    FCopyContext Context(Mapping, SourceFiles);
    FAssetHeaderPatcher Patcher(MoveTemp(Context));
    int32 Expected = 0, Completed = 0;
    Patcher.PatchAsync(&Expected, &Completed).Wait();
    const auto Patched = Patcher.GetPatchedFiles();
    if (Patcher.HasErrors() || Patcher.GetPatchResult() != FAssetHeaderPatcher::EResult::Success ||
        Expected != Requested.Num() || Patched.Num() != Requested.Num())
    {
        for (const auto& Error : Patcher.GetErrorFiles())
            UE_LOG(LogCSSCopy, Error, TEXT("Header copy failed: %s (status %d)"), *Error.Key, static_cast<int32>(Error.Value));
        UE_LOG(LogCSSCopy, Error, TEXT("Expected %d copies, got %d. Inspect any partial candidate before retrying."), Requested.Num(), Patched.Num());
        return 1;
    }
    UE_LOG(LogCSSCopy, Display, TEXT("Copied %d packages with explicit reference remapping. Fresh load, structure, pose and cook checks remain required."), Patched.Num());
    return 0;
}
