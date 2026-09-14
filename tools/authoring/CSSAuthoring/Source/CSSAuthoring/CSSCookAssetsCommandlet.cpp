#include "CSSCookAssetsCommandlet.h"

#include "Windows/WindowsPlatformProperties.h"
#include "GenericWindowsTargetPlatformSettings.h"
#include "GenericWindowsTargetPlatformControls.h"
#include "Common/TargetPlatformBase.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "HAL/MemoryBase.h"
#include "HAL/MemoryMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/CommandLine.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCSSCookAssets, Log, All);

UCSSCookAssetsCommandlet::UCSSCookAssetsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UCSSCookAssetsCommandlet::Main(const FString& Params)
{
    // These are Epic's Windows target templates, not a renamed Linux target.
    // Shader compilation remains deliberately outside this commandlet's scope.
    if (!FParse::Param(FCommandLine::Get(), TEXT("NoShaderCompile")))
    {
        UE_LOG(LogCSSCookAssets, Error, TEXT("Run this experimental commandlet with -NoShaderCompile."));
        return 1;
    }
    using FProperties = FWindowsPlatformProperties<false, false, false>;
    TGenericWindowsTargetPlatformSettings<FProperties> Settings;
    TGenericWindowsTargetPlatformControls<FProperties> Controls(&Settings);
    FTargetPlatformMerged Windows(&Settings, &Controls);
    UDeviceProfile* Profile = UDeviceProfileManager::Get().FindProfile(TEXT("Windows"));
    if (!Profile)
    {
        UE_LOG(LogCSSCookAssets, Error, TEXT("Windows device profile is unavailable."));
        return 1;
    }
    Settings.RegisterTextureLODSettings(Profile);
    TArray<FName> ShaderFormats;
    Windows.GetAllTargetedShaderFormats(ShaderFormats);
    for (FName Format : ShaderFormats)
    {
        UE_LOG(LogCSSCookAssets, Display, TEXT("Windows target shader format: %s"), *Format.ToString());
    }
    UE_LOG(LogCSSCookAssets, Display, TEXT("Constructed target %s. This does not establish game compatibility."), *Windows.PlatformName());
    if (!FParse::Param(*Params, TEXT("Cook")))
    {
        UE_LOG(LogCSSCookAssets, Display, TEXT("Probe only. Add -Cook -List=<package-list.txt> -Output=<new-directory> to attempt cooking."));
        return 0;
    }

    FString ListPath;
    FString Output;
    TArray<FString> PackageNames;
    if (!FParse::Value(*Params, TEXT("List="), ListPath) ||
        !FParse::Value(*Params, TEXT("Output="), Output) ||
        !FFileHelper::LoadFileToStringArray(PackageNames, *ListPath))
    {
        UE_LOG(LogCSSCookAssets, Error, TEXT("A readable package list and unused output directory are required."));
        return 1;
    }
    Output = FPaths::ConvertRelativePathToFull(Output);
    if (IFileManager::Get().DirectoryExists(*Output) || IFileManager::Get().FileExists(*Output))
    {
        UE_LOG(LogCSSCookAssets, Error, TEXT("Refusing an existing output path: %s"), *Output);
        return 1;
    }
    TArray<FString> Validated;
    for (FString PackageName : PackageNames)
    {
        PackageName.TrimStartAndEndInline();
        if (PackageName.IsEmpty() || PackageName.StartsWith(TEXT("#"))) continue;
        if ((!PackageName.StartsWith(TEXT("/Game/CSSAuthoring/")) && !PackageName.StartsWith(TEXT("/Game/CSS/"))) ||
            !FPackageName::IsValidLongPackageName(PackageName))
        {
            UE_LOG(LogCSSCookAssets, Error, TEXT("Only explicit /Game/CSSAuthoring/ or /Game/CSS/ packages are allowed: %s"), *PackageName);
            return 1;
        }
        const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
        UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
        bool Allowed = Asset && (Asset->IsA<UTexture2D>() || Asset->IsA<USkeletalMesh>() || Asset->IsA<USkeleton>());
        if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset))
        {
            Allowed = Instance->Parent && !Instance->bHasStaticPermutationResource &&
                !Instance->HasStaticParameters() && !Instance->HasOverridenBaseProperties();
        }
        if (!Allowed)
        {
            UE_LOG(LogCSSCookAssets, Error, TEXT("Unsupported asset or shader permutation: %s"), *ObjectPath);
            return 1;
        }
        Validated.AddUnique(PackageName);
    }
    if (Validated.IsEmpty())
    {
        UE_LOG(LogCSSCookAssets, Error, TEXT("The package list is empty."));
        return 1;
    }

    const int32 InitialErrors = GWarn->GetNumErrors();
    UCookOnTheFlyServer* Cooker = NewObject<UCookOnTheFlyServer>();
    Cooker->AddToRoot();
    ON_SCOPE_EXIT
    {
        if (Cooker->IsInSession()) Cooker->CancelCookByTheBook();
        Cooker->RemoveFromRoot();
        // Destroy the cook server before the stack-owned target objects expire.
        CollectGarbage(RF_NoFlags);
    };
    Cooker->Initialize(ECookMode::CookByTheBook, ECookInitializationFlags::SkipEditorContent, Output);
    UCookOnTheFlyServer::FCookByTheBookStartupOptions Options;
    Options.TargetPlatforms.Add(&Windows);
    // Cook-by-the-book collects explicit package requests from CookMaps,
    // including non-map assets. CookPackages is not consumed by that collector.
    Options.CookMaps = Validated;
    Options.CookOptions = ECookByTheBookOptions::NoAlwaysCookMaps |
        ECookByTheBookOptions::NoDefaultMaps | ECookByTheBookOptions::NoGameAlwaysCookPackages |
        ECookByTheBookOptions::NoStartupPackages | ECookByTheBookOptions::NoInputPackages |
        ECookByTheBookOptions::SkipSoftReferences | ECookByTheBookOptions::SkipHardReferences |
        ECookByTheBookOptions::ForceDisableSaveGlobalShaders;
    Cooker->StartCookByTheBook(Options);
    const double Deadline = FPlatformTime::Seconds() + 300.0;
    while (Cooker->IsInSession())
    {
        if (FPlatformTime::Seconds() > Deadline)
        {
            UE_LOG(LogCSSCookAssets, Error, TEXT("Experimental asset cook exceeded its five-minute limit."));
            return 1;
        }
        uint32 Result = Cooker->TickCookByTheBook(0.1f);
        if (Result & UCookOnTheFlyServer::COSR_RequiresGC)
        {
            // Match UCookCommandlet::ConditionalCollectGarbage. The type and
            // package guard keep pending exports alive during cooker soft GC.
            const double GCStart = FPlatformTime::Seconds();
            FAssetRegistryModule::TickAssetRegistry(-1.0f);
            const FPlatformMemoryStats MemoryBefore = FPlatformMemory::GetStats();
            const int32 ObjectsBefore = GUObjectArray.GetObjectArrayNumMinusAvailable();
            FGenericMemoryStats AllocatorBefore;
            UE::Private::GMalloc->GetAllocatorStats(AllocatorBefore);
            Cooker->SetGarbageCollectType(Result);
            {
                ON_SCOPE_EXIT { Cooker->ClearGarbageCollectType(); };
                TGuardValue<bool> SoftGCGuard(UPackage::bSupportCookerSoftGC, true);
                Cooker->OnCookerStartCollectGarbage(Result);
                CollectGarbage(RF_NoFlags);
                Cooker->OnCookerEndCollectGarbage(Result);
                if (Cooker->NeedsDiagnosticSecondGC())
                {
                    Cooker->OnCookerStartCollectGarbage(Result);
                    CollectGarbage(RF_NoFlags);
                    Cooker->OnCookerEndCollectGarbage(Result);
                }
            }
            const FPlatformMemoryStats MemoryAfter = FPlatformMemory::GetStats();
            const int32 ObjectsAfter = GUObjectArray.GetObjectArrayNumMinusAvailable();
            FGenericMemoryStats AllocatorAfter;
            UE::Private::GMalloc->GetAllocatorStats(AllocatorAfter);
            const float Duration = static_cast<float>(FPlatformTime::Seconds() - GCStart);
            const bool DueToOOM = (Result & UCookOnTheFlyServer::COSR_RequiresGC_OOM) != 0;
            Cooker->EvaluateGarbageCollectionResults(DueToOOM, false, Result,
                ObjectsBefore, MemoryBefore, AllocatorBefore,
                ObjectsAfter, MemoryAfter, AllocatorAfter, Duration);
            UE_LOG(LogCSSCookAssets, Display, TEXT("Cook GC flags=0x%x, objects %d -> %d, %.3fs."),
                Result, ObjectsBefore, ObjectsAfter, Duration);
        }
    }
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Output, TEXT("*.uasset"), true, false);
    if (GWarn->GetNumErrors() != InitialErrors || Files.Num() != Validated.Num())
    {
        for (const FString& PackageName : Validated)
        {
            const FString ExpectedSuffix = TEXT("/Content/") + PackageName.Mid(6) + TEXT(".uasset");
            const bool Found = Files.ContainsByPredicate([&ExpectedSuffix](const FString& File)
                { return File.EndsWith(ExpectedSuffix); });
            if (!Found)
            {
                const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
                UObject* Remaining = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
                UE_LOG(LogCSSCookAssets, Error,
                    TEXT("Missing cooked package %s; source still loaded=%d. Inspect LogCook and LogSavePackage with Verbose logging for the save or suppression reason."),
                    *PackageName, Remaining != nullptr);
            }
        }
        UE_LOG(LogCSSCookAssets, Error, TEXT("Cook validation failed: expected %d packages, found %d; new errors %d."),
            Validated.Num(), Files.Num(), GWarn->GetNumErrors() - InitialErrors);
        return 1;
    }
    UE_LOG(LogCSSCookAssets, Display, TEXT("Cook emitted %d packages. Binary inspection and Windows game testing are still required."), Files.Num());
    return 0;
}
